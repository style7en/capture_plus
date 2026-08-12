#include "OverlayWindow.h"
#include "GdiUtil.h"
#include "Logger.h"

static const wchar_t* KC_OVERLAY = L"CapturePlus_OverlayWnd";

static HPEN   s_whitePen = nullptr;
static HCURSOR s_crossCursor = nullptr;
static bool s_classOk = false;

static RECT toClientRect(const NormRect& r, int ox, int oy, int w, int h);

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
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        dimmed_ = CreateCompatibleBitmap(screen, width_, height_);
        HBITMAP old = (HBITMAP)SelectObject(mem, dimmed_);
        HDC src = CreateCompatibleDC(screen);
        HBITMAP oldSrc = (HBITMAP)SelectObject(src, snapshot_);
        BitBlt(mem, 0, 0, width_, height_, src, 0, 0, SRCCOPY);
        SelectObject(src, oldSrc);
        DeleteDC(src);
        {
            Gdiplus::Graphics g(mem);
            Gdiplus::SolidBrush dim(Gdiplus::Color(140, 0, 0, 0));
            g.FillRectangle(&dim, 0, 0, width_, height_);
        }
        SelectObject(mem, old);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
    }

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
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

        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            if (self->inputCb_) self->inputCb_(InputPhase::Begin,
                self->originX_ + (short)LOWORD(lp), self->originY_ + (short)HIWORD(lp));
            return 0;

        case WM_MOUSEMOVE:
            if ((wp & MK_LBUTTON) && self->inputCb_)
                self->inputCb_(InputPhase::Move,
                    self->originX_ + (short)LOWORD(lp), self->originY_ + (short)HIWORD(lp));
            return 0;

        case WM_LBUTTONUP:
            if (GetCapture() == hwnd) ReleaseCapture();
            if (self->inputCb_) self->inputCb_(InputPhase::End,
                self->originX_ + (short)LOWORD(lp), self->originY_ + (short)HIWORD(lp));
            return 0;

        case WM_RBUTTONUP:
        case WM_KEYDOWN:
            if (msg == WM_KEYDOWN && wp != VK_ESCAPE) break;
            if (self->cancelCb_) self->cancelCb_();
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
    return gdiutil::CropBitmap(snapshot_, sx, sy, w, h);
}

void OverlayWindow::shutdown()
{
    if (s_whitePen)     { DeleteObject((HGDIOBJ)s_whitePen);     s_whitePen = nullptr; }
}
