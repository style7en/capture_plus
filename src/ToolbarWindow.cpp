#include "ToolbarWindow.h"
#include "Logger.h"

static const wchar_t* KC_TOOLBAR = L"CapturePlus_ToolbarWnd";

enum { IDC_COPY, IDC_SAVE, IDC_OCR, IDC_AI, IDC_TRANSLATE, IDC_CANCEL };

static const struct { const wchar_t* label; int id; } kButtons[] = {
    { L"复制图片",   IDC_COPY },
    { L"保存图片",   IDC_SAVE },
    { L"提取文字",   IDC_OCR },
    { L"AI分析",     IDC_AI },
    { L"翻译",       IDC_TRANSLATE },
    { L"✕",          IDC_CANCEL },
};

static int getDpiScale()
{
    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    if (dpi <= 0) dpi = 96;
    return (int)(dpi * 100 / 96);
}

ToolbarWindow::ToolbarWindow()
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = &ToolbarWindow::WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = KC_TOOLBAR;
    RegisterClassW(&wc);

    scale_ = getDpiScale();
    int btnW = 58 * scale_ / 100;
    int btnH = 26 * scale_ / 100;
    int cancelW = 28 * scale_ / 100;
    int pad = 4 * scale_ / 100;
    width_  = 5 * btnW + cancelW + pad * 7;
    height_ = btnH + pad * 2;

    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    if (dpi <= 0) dpi = 96;
    int fontH = -MulDiv(9, dpi, 72);
    font_ = CreateFontW(fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        FF_DONTCARE, L"Microsoft YaHei");
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

    layoutButtons();
    return true;
}

void ToolbarWindow::layoutButtons()
{
    if (!hwnd_) return;
    int btnW = 58 * scale_ / 100;
    int btnH = 26 * scale_ / 100;
    int cancelW = 28 * scale_ / 100;
    int pad = 4 * scale_ / 100;

    int cx = pad;
    for (int i = 0; i < 6; i++)
    {
        int w = (i == 5) ? cancelW : btnW;
        if (!buttons_[i])
        {
            buttons_[i] = CreateWindowExW(0, L"BUTTON", kButtons[i].label,
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                cx, pad, w, btnH, hwnd_, (HMENU)(INT_PTR)kButtons[i].id,
                GetModuleHandleW(nullptr), nullptr);
            SendMessageW(buttons_[i], WM_SETFONT, (WPARAM)font_, TRUE);
        }
        else
            MoveWindow(buttons_[i], cx, pad, w, btnH, TRUE);
        cx += w + pad;
    }
}

void ToolbarWindow::show()
{
    if (!hwnd_) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    SetForegroundWindow(hwnd_);
}

void ToolbarWindow::hide()
{
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void ToolbarWindow::close()
{
    if (hwnd_)
    {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        for (auto& b : buttons_) b = nullptr;
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
        case WM_COMMAND:
        {
            int id = LOWORD(wp);
            if (!self) break;
            switch (id)
            {
                case IDC_COPY:     if (self->actionCb_) self->actionCb_(ScreenshotAction::Copy); break;
                case IDC_SAVE:     if (self->actionCb_) self->actionCb_(ScreenshotAction::Save); break;
                case IDC_OCR:      if (self->actionCb_) self->actionCb_(ScreenshotAction::Ocr); break;
                case IDC_AI:       if (self->actionCb_) self->actionCb_(ScreenshotAction::Ai); break;
                case IDC_TRANSLATE: if (self->actionCb_) self->actionCb_(ScreenshotAction::Translate); break;
                case IDC_CANCEL:   if (self->cancelCb_) self->cancelCb_(); break;
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE && self && self->cancelCb_) self->cancelCb_();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT r; GetClientRect(hwnd, &r);
            FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
            EndPaint(hwnd, &ps);
            return 0;
        }
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
