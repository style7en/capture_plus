#include "ResultWindow.h"
#include "AiService.h"
#include "AppSettings.h"
#include "Logger.h"
#include "Util.h"
#include "resource.h"

static const wchar_t* KC_RESULT = L"CapturePlus_ResultWnd";
static bool s_classOk = false;

extern AppSettings g_settings;

static std::atomic<int> g_inFlightAi{ 0 };

void ResultWindow::WaitForAiTasks(int timeoutMs)
{
    int waited = 0;
    while (waited < timeoutMs)
    {
        if (g_inFlightAi.load() <= 0) return;
        Sleep(20);
        waited += 20;
    }
}

ResultWindow::ResultWindow(Mode mode, HBITMAP bmp) : mode_(mode)
{
    if (!s_classOk)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc   = &ResultWindow::WndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = KC_RESULT;
        s_classOk = RegisterClassW(&wc) != 0;
    }

    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    if (dpi <= 0) dpi = 96;
    int fontH = -MulDiv(9, dpi, 72);
    font_ = CreateFontW(fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, FF_DONTCARE, L"Microsoft YaHei");

    state_ = std::make_shared<Shared>();
    state_->bmp = bmp;
}

ResultWindow::~ResultWindow()
{
    if (font_) DeleteObject(font_);
    if (state_) { state_->closed = true; state_->hwnd = nullptr; }
}

bool ResultWindow::create()
{
    std::wstring title = L"提取文字";
    if (mode_ == Mode::Ai) title = L"AI 分析";
    else if (mode_ == Mode::Translate)
        title = L"翻译 → " + util::ToWide(g_settings.translateTargetLanguage);

    int W = 760, H = 560;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - W) / 2;
    int y = (sh - H) / 2;
    hwnd_ = CreateWindowExW(0, KC_RESULT, title.c_str(),
        WS_OVERLAPPEDWINDOW,
        x, y, W, H,
        nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) return false;
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    state_->hwnd = hwnd_;

    HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (hIcon)
    {
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
    }

    edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN,
        10, 10, W - 30, H - 70, hwnd_, (HMENU)1, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(edit_, WM_SETFONT, (WPARAM)font_, TRUE);

    int bw = 100, bh = 40;
    copyBtn_  = CreateWindowExW(0, L"BUTTON", L"复制",
        WS_CHILD | WS_VISIBLE, 0, 0, bw, bh,
        hwnd_, (HMENU)2, GetModuleHandleW(nullptr), nullptr);
    retryBtn_ = CreateWindowExW(0, L"BUTTON", L"重试",
        WS_CHILD | WS_VISIBLE, 0, 0, bw, bh,
        hwnd_, (HMENU)3, GetModuleHandleW(nullptr), nullptr);
    closeBtn_ = CreateWindowExW(0, L"BUTTON", L"关闭",
        WS_CHILD | WS_VISIBLE, 0, 0, bw, bh,
        hwnd_, (HMENU)4, GetModuleHandleW(nullptr), nullptr);
    for (HWND b : {copyBtn_, retryBtn_, closeBtn_})
        SendMessageW(b, WM_SETFONT, (WPARAM)font_, TRUE);

    EnableWindow(retryBtn_, FALSE);
    onLayout();
    return true;
}

void ResultWindow::show()
{
    if (hwnd_) { ShowWindow(hwnd_, SW_SHOW); SetForegroundWindow(hwnd_); }
    runAi();
}

static ResultWindow* selfFromHwnd(HWND h)
{
    return (ResultWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
}

LRESULT CALLBACK ResultWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = selfFromHwnd(hwnd);
    switch (msg)
    {
        case WM_COMMAND:
        {
            int id = LOWORD(wp);
            if (!self) break;
            if (id == 2)
            {
                int len = GetWindowTextLengthW(self->edit_);
                std::wstring txt(len + 1, 0);
                GetWindowTextW(self->edit_, &txt[0], len + 1);
                txt.resize(len);
                if (OpenClipboard(hwnd))
                {
                    EmptyClipboard();
                    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
                    if (g)
                    {
                        auto* p = (wchar_t*)GlobalLock(g);
                        if (p) { wcscpy(p, txt.c_str()); GlobalUnlock(g); }
                        if (!SetClipboardData(CF_UNICODETEXT, g))
                            GlobalFree(g);
                    }
                    CloseClipboard();
                }
            }
            else if (id == 3) { self->runAi(); }
            else if (id == 4) { DestroyWindow(hwnd); }
            return 0;
        }
        case WM_APP_AI_RESULT:
        {
            if (!self) return 0;
            std::wstring text;
            int kind = 0;
            {
                std::lock_guard<std::mutex> lk(self->state_->mtx);
                self->state_->ready = false;
                kind = self->state_->kind;
                text = std::move(self->state_->text);
            }
            if (kind == 0)
            {
                if (self->state_->bmp) { DeleteObject((HGDIOBJ)self->state_->bmp); self->state_->bmp = nullptr; }
                self->setResult(text);
                EnableWindow(self->retryBtn_, FALSE);
            }
            else
            {
                self->setError(text);
                EnableWindow(self->retryBtn_, TRUE);
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_SIZE:
            if (self) self->onLayout();
            return 0;
        case WM_GETMINMAXINFO:
        {
            auto* mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = 420;
            mmi->ptMinTrackSize.y = 260;
            return 0;
        }
        case WM_DESTROY:
            if (self)
            {
                self->state_->closed = true;
                self->state_->hwnd = nullptr;
                self->hwnd_ = nullptr;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                CloseCb cb = std::move(self->closeCb_);
                delete self;
                if (cb) cb();
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ResultWindow::onLayout()
{
    if (!hwnd_) return;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    int pad = 10;
    int bw = 100, bh = 40;
    int bottomH = bh + pad;
    int editH = H - bottomH - pad * 2;
    if (editH < 40) editH = 40;

    MoveWindow(edit_, pad, pad, W - pad * 2, editH, TRUE);

    int by = H - pad - bh;
    int cx = W - pad;
    cx -= bw; MoveWindow(closeBtn_, cx, by, bw, bh, TRUE); cx -= 8;
    cx -= bw; MoveWindow(retryBtn_, cx, by, bw, bh, TRUE); cx -= 8;
    cx -= bw; MoveWindow(copyBtn_,  cx, by, bw, bh, TRUE);
}

static std::wstring NormalizeLineBreaks(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == L'\n')
        {
            if (i == 0 || s[i - 1] != L'\r') out.push_back(L'\r');
            out.push_back(L'\n');
        }
        else if (s[i] != L'\r')
            out.push_back(s[i]);
        else if (i + 1 < s.size() && s[i + 1] == L'\n')
            out.push_back(L'\r');
    }
    return out;
}

void ResultWindow::setLoading(const std::wstring& msg)
{
    SetWindowTextW(edit_, NormalizeLineBreaks(msg).c_str());
    EnableWindow(retryBtn_, FALSE);
}

void ResultWindow::setResult(const std::wstring& text)
{
    SetWindowTextW(edit_, NormalizeLineBreaks(text).c_str());
    EnableWindow(retryBtn_, TRUE);
}

void ResultWindow::setError(const std::wstring& msg)
{
    setResult(msg);
}

void ResultWindow::runAi()
{
    if (!state_->bmp) return;
    state_->closed = false;
    setLoading(L"正在处理…");

    AppSettings settings = g_settings;
    auto state = state_;
    Mode mode = mode_;
    HWND target = hwnd_;
    g_inFlightAi.fetch_add(1);

    std::thread([state, mode, target, settings]() {
        struct Guard { ~Guard() { g_inFlightAi.fetch_sub(1); } } guard;
        std::wstring text;
        int kind = 0;
        try
        {
            AiService ai;
            std::string r;
            if (mode == Mode::Ocr)        r = ai.ocr(state->bmp, settings);
            else if (mode == Mode::Ai)    r = ai.analyze(state->bmp, settings);
            else                          r = ai.translate(ai.ocr(state->bmp, settings), settings);
            if (r.empty()) r = "（未返回内容）";
            text = util::ToWide(r);
        }
        catch (const std::exception& e)
        {
            kind = 1;
            text = util::ToWide(std::string("失败：") + e.what());
            logger::error("AI run failed: " + std::string(e.what()));
        }
        {
            std::lock_guard<std::mutex> lk(state->mtx);
            state->ready = true;
            state->kind = kind;
            state->text = std::move(text);
        }
        if (!state->closed.load() && IsWindow(target))
            PostMessageW(target, WM_APP_AI_RESULT, 0, 0);
    }).detach();
}
