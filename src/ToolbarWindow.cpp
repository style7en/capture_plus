#include "ToolbarWindow.h"
#include "GdiUtil.h"
#include "Logger.h"

static const wchar_t* KC_TOOLBAR = L"CapturePlus_ToolbarWnd";
static bool s_classOk = false;

enum { BTN_COPY, BTN_SAVE, BTN_OCR, BTN_AI, BTN_TRANSLATE, BTN_CANCEL };
static const int BTN_COUNT = 6;

static const wchar_t* const kLabels[BTN_COUNT] = {
    L"复制图片", L"保存图片", L"提取文字", L"AI分析", L"翻译", L"✕"
};

static const ScreenshotAction kActions[5] = {
    ScreenshotAction::Copy, ScreenshotAction::Save, ScreenshotAction::Ocr,
    ScreenshotAction::Ai,   ScreenshotAction::Translate,
};

static const COLORREF CLR_STRIP       = RGB(30, 30, 33);
static const COLORREF CLR_BTN         = RGB(55, 55, 60);
static const COLORREF CLR_BTN_HOVER   = RGB(85, 85, 92);
static const COLORREF CLR_BTN_DOWN    = RGB(105, 105, 113);
static const COLORREF CLR_CANCEL_H    = RGB(120, 42, 42);
static const COLORREF CLR_CANCEL_D    = RGB(150, 52, 52);
static const COLORREF CLR_TEXT        = RGB(235, 235, 235);
static const COLORREF CLR_EDGE        = RGB(72, 72, 78);

static int getDpiForPoint(int px, int py)
{
    HMONITOR hmon = MonitorFromPoint({ px, py }, MONITOR_DEFAULTTONEAREST);
    if (hmon)
    {
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        if (shcore)
        {
            using GetDpiForMonitor_t = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
            auto fn = (GetDpiForMonitor_t)GetProcAddress(shcore, "GetDpiForMonitor");
            if (fn)
            {
                UINT x = 0, y = 0;
                if (SUCCEEDED(fn(hmon, 0, &x, &y)) && x > 0)
                {
                    FreeLibrary(shcore);
                    return (int)x;
                }
            }
            FreeLibrary(shcore);
        }
    }
    return 96;
}

ToolbarWindow::ToolbarWindow(int px, int py)
{
    if (!s_classOk)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc   = &ToolbarWindow::WndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = KC_TOOLBAR;
        s_classOk = RegisterClassW(&wc) != 0;
    }

    int dpi = getDpiForPoint(px, py);
    scale_ = dpi * 100 / 96;
    btnW_    = 58 * scale_ / 100;
    btnH_    = 26 * scale_ / 100;
    cancelW_ = 28 * scale_ / 100;
    pad_     = 4 * scale_ / 100;
    width_  = 5 * btnW_ + cancelW_ + pad_ * 7;
    height_ = btnH_ + pad_ * 2;

    font_ = gdiutil::CreateUiFont(dpi);
}

ToolbarWindow::~ToolbarWindow()
{
    close();
    if (font_) DeleteObject(font_);
}

bool ToolbarWindow::create(int x, int y)
{
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        KC_TOOLBAR, L"", WS_POPUP | WS_BORDER,
        x, y, width_, height_,
        nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) { logger::error("Toolbar CreateWindow failed"); return false; }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    return true;
}

RECT ToolbarWindow::buttonRect(int i) const
{
    int cx = pad_;
    for (int j = 0; j < i; j++)
        cx += ((j == 5) ? cancelW_ : btnW_) + pad_;
    int w = (i == 5) ? cancelW_ : btnW_;
    RECT r = { cx, pad_, cx + w, pad_ + btnH_ };
    return r;
}

int ToolbarWindow::hitTest(int x, int y) const
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
        RECT r = buttonRect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return i;
    }
    return -1;
}

void ToolbarWindow::drawButton(HDC dc, int i, bool hover, bool pressed)
{
    RECT r = buttonRect(i);
    bool isCancel = (i == BTN_CANCEL);
    COLORREF bg;
    if (pressed)      bg = isCancel ? CLR_CANCEL_D : CLR_BTN_DOWN;
    else if (hover)   bg = isCancel ? CLR_CANCEL_H : CLR_BTN_HOVER;
    else              bg = CLR_BTN;

    HBRUSH brush = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, hover ? CLR_EDGE : bg);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    int radius = 5 * scale_ / 100;
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, CLR_TEXT);
    HGDIOBJ oldFont = SelectObject(dc, font_);
    DrawTextW(dc, kLabels[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
}

void ToolbarWindow::show()
{
    if (!hwnd_) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    SetForegroundWindow(hwnd_);
}

void ToolbarWindow::close()
{
    if (hwnd_)
    {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

static ToolbarWindow* selfFromHwnd(HWND h)
{
    return (ToolbarWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
}

LRESULT CALLBACK ToolbarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = selfFromHwnd(hwnd);
    switch (msg)
    {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT cr; GetClientRect(hwnd, &cr);
            HBRUSH bg = CreateSolidBrush(CLR_STRIP);
            FillRect(dc, &cr, bg);
            DeleteObject(bg);
            if (self)
            {
                for (int i = 0; i < BTN_COUNT; i++)
                    self->drawButton(dc, i, self->hoverIdx_ == i, self->pressedIdx_ == i);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (self)
            {
                int x = (short)LOWORD(lp);
                int y = (short)HIWORD(lp);
                int h = self->hitTest(x, y);
                if (h != self->hoverIdx_)
                {
                    self->hoverIdx_ = h;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                if (!self->trackingMouse_)
                {
                    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                    if (TrackMouseEvent(&tme)) self->trackingMouse_ = true;
                }
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (self)
            {
                self->pressedIdx_ = self->hitTest((short)LOWORD(lp), (short)HIWORD(lp));
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (self)
            {
                int idx = self->hitTest((short)LOWORD(lp), (short)HIWORD(lp));
                int pressed = self->pressedIdx_;
                self->pressedIdx_ = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
                if (idx >= 0 && idx == pressed)
                {
                    if (idx == BTN_CANCEL)
                    {
                        if (self->cancelCb_) self->cancelCb_();
                    }
                    else if (self->actionCb_)
                    {
                        self->actionCb_(kActions[idx]);
                    }
                }
            }
            return 0;
        case WM_MOUSELEAVE:
            if (self)
            {
                self->hoverIdx_ = -1;
                self->pressedIdx_ = -1;
                self->trackingMouse_ = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE && self && self->cancelCb_) self->cancelCb_();
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ToolbarWindow::computePosition(const NormRect& sel, int monX, int monY,
                                    int monW, int monH, int margin,
                                    int tw, int th,
                                    int& outX, int& outY)
{
    double x = sel.x + (sel.w - tw) / 2.0;
    double y = sel.y - margin - th;
    if (y < monY + margin) y = sel.y + sel.h + margin;
    if (y + th > monY + monH - margin) y = sel.y + margin;
    if (x < monX + margin) x = monX + margin;
    if (x + tw > monX + monW - margin) x = monX + monW - margin - tw;
    outX = (int)x;
    outY = (int)y;
}
