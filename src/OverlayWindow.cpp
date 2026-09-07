#include "OverlayWindow.h"
#include "GdiUtil.h"
#include "Logger.h"

static const wchar_t* KC_OVERLAY = L"CapturePlus_OverlayWnd";

static HPEN   s_whitePen = nullptr;
static HCURSOR s_crossCursor = nullptr;
static bool s_classOk = false;

static RECT toClientRect(const NormRect& r, int ox, int oy, int w, int h);

static int dpiForScreenPoint(int px, int py)
{
    using GetDpiForMonitor_t = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static GetDpiForMonitor_t fn = []() -> GetDpiForMonitor_t {
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        return shcore ? (GetDpiForMonitor_t)GetProcAddress(shcore, "GetDpiForMonitor") : nullptr;
    }();
    HMONITOR hmon = MonitorFromPoint({ px, py }, MONITOR_DEFAULTTONEAREST);
    UINT x = 0, y = 0;
    if (fn && hmon && SUCCEEDED(fn(hmon, 0, &x, &y)) && x > 0) return (int)x;
    return 96;
}

static int annotPenWidth(int dpi)
{
    int w = 3 * dpi / 96;
    return w < 2 ? 2 : w;
}

static void drawAnnotRect(Gdiplus::Graphics& g, const RECT& r, int dpi)
{
    int pw = annotPenWidth(dpi);
    Gdiplus::Rect gr(r.left + pw / 2, r.top + pw / 2,
                     (r.right - r.left) - pw, (r.bottom - r.top) - pw);
    if (gr.Width < 1 || gr.Height < 1) return;
    Gdiplus::Pen edge(Gdiplus::Color(255, 255, 0, 0), (Gdiplus::REAL)pw);
    g.DrawRectangle(&edge, gr);
}

static RECT normalizeRect(int x1, int y1, int x2, int y2)
{
    return { (std::min)(x1, x2), (std::min)(y1, y2), (std::max)(x1, x2), (std::max)(y1, y2) };
}

static void ensureAssets()
{
    if (s_whitePen) return;
    s_whitePen     = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    s_crossCursor  = LoadCursorW(nullptr, IDC_CROSS);
}

static bool registerClass()
{
    if (s_classOk) return true;
    ensureAssets();
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = &OverlayWindow::WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = s_crossCursor;
    wc.lpszClassName = KC_OVERLAY;
    s_classOk = RegisterClassW(&wc) != 0;
    return s_classOk;
}

OverlayWindow::OverlayWindow()
{
    registerClass();
}

OverlayWindow::~OverlayWindow()
{
    close();
}

bool OverlayWindow::create()
{
    originX_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
    originY_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
    width_   = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    height_  = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    snapshot_ = gdiutil::CaptureScreenRect(originX_, originY_, width_, height_);
    if (!snapshot_) logger::warn("Overlay snapshot capture failed");

    if (snapshot_)
    {
        dimmed_ = gdiutil::CropBitmap(snapshot_, 0, 0, width_, height_);
        if (dimmed_)
        {
            HDC screen = GetDC(nullptr);
            HDC mem = CreateCompatibleDC(screen);
            HBITMAP old = (HBITMAP)SelectObject(mem, dimmed_);
            Gdiplus::Graphics g(mem);
            Gdiplus::SolidBrush dim(Gdiplus::Color(140, 0, 0, 0));
            g.FillRectangle(&dim, 0, 0, width_, height_);
            SelectObject(mem, old);
            DeleteDC(mem);
            ReleaseDC(nullptr, screen);
        }
    }

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        KC_OVERLAY, L"", WS_POPUP,
        originX_, originY_, width_, height_,
        nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) { logger::error("Overlay CreateWindow failed"); return false; }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    return true;
}

void OverlayWindow::show()
{
    SetWindowPos(hwnd_, HWND_TOPMOST, originX_, originY_, width_, height_,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    SetForegroundWindow(hwnd_);
}

void OverlayWindow::close()
{
    if (hwnd_)
    {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (snapshot_)
    {
        DeleteObject((HGDIOBJ)snapshot_);
        snapshot_ = nullptr;
    }
    if (dimmed_)
    {
        DeleteObject((HGDIOBJ)dimmed_);
        dimmed_ = nullptr;
    }
}

static OverlayWindow* selfFromHwnd(HWND h)
{
    return (OverlayWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = selfFromHwnd(hwnd);
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            self->onPaint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) { SetCursor(s_crossCursor); return 1; }
            break;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            if (self->drawMode_)
            {
                self->drawing_ = true;
                self->dragStartX_ = (short)LOWORD(lp);
                self->dragStartY_ = (short)HIWORD(lp);
                self->dragRect_ = { self->dragStartX_, self->dragStartY_,
                                    self->dragStartX_, self->dragStartY_ };
                self->invalidateRectArea(self->dragRect_);
                return 0;
            }
            if (self->inputCb_) self->inputCb_(InputPhase::Begin,
                self->originX_ + (short)LOWORD(lp), self->originY_ + (short)HIWORD(lp));
            return 0;

        case WM_MOUSEMOVE:
            if (self->drawMode_)
            {
                if (self->drawing_ && (wp & MK_LBUTTON))
                {
                    RECT old = self->dragRect_;
                    self->dragRect_ = normalizeRect(self->dragStartX_, self->dragStartY_,
                                                    (short)LOWORD(lp), (short)HIWORD(lp));
                    if (self->hasSelection_)
                    {
                        RECT hole = toClientRect(self->selection_, self->originX_,
                                                 self->originY_, self->width_, self->height_);
                        RECT t = self->dragRect_;
                        IntersectRect(&self->dragRect_, &t, &hole);
                    }
                    self->invalidateRectArea(old);
                    self->invalidateRectArea(self->dragRect_);
                }
                return 0;
            }
            if ((wp & MK_LBUTTON) && self->inputCb_)
                self->inputCb_(InputPhase::Move,
                    self->originX_ + (short)LOWORD(lp), self->originY_ + (short)HIWORD(lp));
            return 0;

        case WM_LBUTTONUP:
            if (self->drawMode_)
            {
                if (self->drawing_)
                {
                    self->drawing_ = false;
                    if (GetCapture() == hwnd) ReleaseCapture();
                    RECT r = self->dragRect_;
                    self->dragRect_ = { 0, 0, 0, 0 };
                    if (r.right - r.left >= 4 && r.bottom - r.top >= 4)
                        self->rects_.push_back(r);
                    else
                        self->invalidateRectArea(r);
                }
                return 0;
            }
            if (GetCapture() == hwnd) ReleaseCapture();
            if (self->inputCb_) self->inputCb_(InputPhase::End,
                self->originX_ + (short)LOWORD(lp), self->originY_ + (short)HIWORD(lp));
            return 0;

        case WM_RBUTTONUP:
            if (self->drawMode_ && self->drawing_) return 0;
            if (self->drawMode_ && !self->rects_.empty())
            {
                RECT r = self->rects_.back();
                self->rects_.pop_back();
                self->invalidateRectArea(r);
                return 0;
            }
            if (self->cancelCb_) self->cancelCb_();
            return 0;

        case WM_KEYDOWN:
            if (wp != VK_ESCAPE) break;
            if (self->cancelCb_) self->cancelCb_();
            return 0;

        case WM_CAPTURECHANGED:
            if (self->drawing_)
            {
                self->drawing_ = false;
                RECT r = self->dragRect_;
                self->dragRect_ = { 0, 0, 0, 0 };
                self->invalidateRectArea(r);
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void OverlayWindow::renderSelection(const NormRect* sel)
{
    NormRect old = selection_;
    bool hadOld = hasSelection_;
    if (sel) { selection_ = *sel; hasSelection_ = true; }
    else     { hasSelection_ = false; }
    if (!hwnd_) return;

    if (!hadOld && !hasSelection_)
    {
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    RECT a = hadOld ? toClientRect(old, originX_, originY_, width_, height_)
                    : RECT{0, 0, 0, 0};
    RECT b = hasSelection_ ? toClientRect(selection_, originX_, originY_, width_, height_)
                           : RECT{0, 0, 0, 0};
    if (!hadOld)  { a.left = b.left; a.top = b.top; a.right = b.right; a.bottom = b.bottom; }
    if (!hasSelection_) { b.left = a.left; b.top = a.top; b.right = a.right; b.bottom = a.bottom; }

    int pad = 3;
    RECT dirty;
    dirty.left   = (std::min)(a.left, b.left) - pad;
    dirty.top    = (std::min)(a.top, b.top) - pad;
    dirty.right  = (std::max)(a.right, b.right) + pad;
    dirty.bottom = (std::max)(a.bottom, b.bottom) + pad;
    if (dirty.left < 0) dirty.left = 0;
    if (dirty.top < 0) dirty.top = 0;
    if (dirty.right > width_) dirty.right = width_;
    if (dirty.bottom > height_) dirty.bottom = height_;
    InvalidateRect(hwnd_, &dirty, FALSE);
}

void OverlayWindow::invalidateRectArea(const RECT& r)
{
    const int pad = 10;
    RECT dirty = { r.left - pad, r.top - pad, r.right + pad, r.bottom + pad };
    if (dirty.left < 0) dirty.left = 0;
    if (dirty.top < 0) dirty.top = 0;
    if (dirty.right > width_) dirty.right = width_;
    if (dirty.bottom > height_) dirty.bottom = height_;
    if (dirty.right <= dirty.left || dirty.bottom <= dirty.top) return;
    InvalidateRect(hwnd_, &dirty, FALSE);
}

static RECT toClientRect(const NormRect& r, int ox, int oy, int w, int h)
{
    int l = (int)r.x - ox;
    int t = (int)r.y - oy;
    int rr = l + (int)r.w;
    int bb = t + (int)r.h;
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (rr > w) rr = w;
    if (bb > h) bb = h;
    return { l, t, rr, bb };
}

void OverlayWindow::onPaint(HDC hdc)
{
    HBITMAP bg = dimmed_ ? dimmed_ : snapshot_;
    if (!bg)
    {
        RECT client = { 0, 0, width_, height_ };
        FillRect(hdc, &client, (HBRUSH)GetStockObject(BLACK_BRUSH));
        return;
    }

    RECT hole = { 0, 0, 0, 0 };
    bool hasHole = false;
    if (hasSelection_)
    {
        hole = toClientRect(selection_, originX_, originY_, width_, height_);
        hasHole = (hole.right > hole.left && hole.bottom > hole.top);
    }

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP old = (HBITMAP)SelectObject(mem, bg);
    BitBlt(hdc, 0, 0, width_, height_, mem, 0, 0, SRCCOPY);

    if (hasHole && snapshot_)
    {
        SelectObject(mem, snapshot_);
        BitBlt(hdc, hole.left, hole.top,
               hole.right - hole.left, hole.bottom - hole.top,
               mem, hole.left, hole.top, SRCCOPY);
    }

    SelectObject(mem, old);
    DeleteDC(mem);

    if (hasHole)
    {
        HGDIOBJ oldPen = SelectObject(hdc, s_whitePen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, hole.left - 1, hole.top - 1, hole.right + 1, hole.bottom + 1);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }

    if (!rects_.empty() || drawing_)
    {
        Gdiplus::Graphics g(hdc);
        for (const RECT& r : rects_)
            drawAnnotRect(g, r, dpiForScreenPoint(originX_ + (r.left + r.right) / 2,
                                                  originY_ + (r.top + r.bottom) / 2));
        if (drawing_)
            drawAnnotRect(g, dragRect_, dpiForScreenPoint(originX_ + (dragRect_.left + dragRect_.right) / 2,
                                                          originY_ + (dragRect_.top + dragRect_.bottom) / 2));
    }
}

HBITMAP OverlayWindow::captureRect(const NormRect& sel) const
{
    if (!snapshot_) return nullptr;
    int sx = (int)std::floor(sel.x) - originX_;
    int sy = (int)std::floor(sel.y) - originY_;
    int ex = (int)std::ceil(sel.x + sel.w) - originX_;
    int ey = (int)std::ceil(sel.y + sel.h) - originY_;
    int w = ex - sx;
    int h = ey - sy;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    HBITMAP bmp = gdiutil::CropBitmap(snapshot_, sx, sy, w, h);
    if (bmp && !rects_.empty())
    {
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        {
            Gdiplus::Graphics g(mem);
            for (const RECT& r : rects_)
            {
                RECT t = { r.left - sx, r.top - sy, r.right - sx, r.bottom - sy };
                drawAnnotRect(g, t, dpiForScreenPoint(originX_ + (r.left + r.right) / 2,
                                                      originY_ + (r.top + r.bottom) / 2));
            }
        }
        SelectObject(mem, old);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
    }
    return bmp;
}

void OverlayWindow::shutdown()
{
    if (s_whitePen)     { DeleteObject((HGDIOBJ)s_whitePen);     s_whitePen = nullptr; }
}
