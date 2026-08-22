#include "ResultWindow.h"
#include "AiService.h"
#include "AppSettings.h"
#include "GdiUtil.h"
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

bool ResultWindow::HasInFlightAi()
{
    return g_inFlightAi.load() > 0;
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
    font_ = gdiutil::CreateUiFont(dpi);

    state_ = std::make_shared<Shared>();
    state_->bmp = bmp;
}

ResultWindow::~ResultWindow()
{
    if (font_) DeleteObject(font_);
    if (state_) state_->closed = true;
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

    gdiutil::SetAppIcon(hwnd_);

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

    if (mode_ == Mode::Translate)
    {
        HINSTANCE inst = GetModuleHandleW(nullptr);
        langLabel_ = CreateWindowExW(0, L"STATIC", L"目标语言:",
            WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
            0, 0, 62, 20, hwnd_, nullptr, inst, nullptr);
        langCombo_ = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 200, 220, hwnd_, (HMENU)5, inst, nullptr);

        std::wstring cur = util::ToWide(g_settings.translateTargetLanguage);
        int sel = -1;
        int i = 0;
        for (const auto& lang : TranslateLanguageList())
        {
            SendMessageW(langCombo_, CB_ADDSTRING, 0, (LPARAM)lang.c_str());
            if (lang == cur) sel = i;
            i++;
        }
        if (sel < 0 && !cur.empty())
        {
            SendMessageW(langCombo_, CB_ADDSTRING, 0, (LPARAM)cur.c_str());
            sel = i;
        }
        if (sel >= 0) SendMessageW(langCombo_, CB_SETCURSEL, sel, 0);

        SendMessageW(langLabel_, WM_SETFONT, (WPARAM)font_, TRUE);
        SendMessageW(langCombo_, WM_SETFONT, (WPARAM)font_, TRUE);
    }

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
            else if (id == 5 && HIWORD(wp) == CBN_SELCHANGE) { self->onLanguageChanged(); }
            return 0;
        }
        case WM_APP_AI_RESULT:
        {
            if (!self) return 0;
            self->aiInflight_ = false;
            std::wstring text;
            int kind = 0;
            {
                std::lock_guard<std::mutex> lk(self->state_->mtx);
                kind = self->state_->kind;
                text = std::move(self->state_->text);
            }
            if (kind == 0)
            {
                if (self->state_->bmp && !self->keepBmp_)
                {
                    DeleteObject((HGDIOBJ)self->state_->bmp);
                    self->state_->bmp = nullptr;
                }
                self->setResult(text);
                EnableWindow(self->retryBtn_, FALSE);
            }
            else
            {
                self->setResult(text);
                EnableWindow(self->retryBtn_, TRUE);
            }
            if (self->langCombo_) EnableWindow(self->langCombo_, TRUE);
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
                self->hwnd_ = nullptr;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                delete self;
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

    if (langCombo_)
    {
        int comboH = (int)SendMessageW(langCombo_, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
        if (comboH <= 0) comboH = 24;
        int cy = by + (bh - comboH) / 2;
        if (cy < by) cy = by;

        int labelW = 66;
        int textH = comboH;
        {
            HDC dc = GetDC(hwnd_);
            HGDIOBJ oldFont = SelectObject(dc, font_);
            SIZE sz = {0, 0};
            if (GetTextExtentPoint32W(dc, L"目标语言:", 5, &sz))
            {
                if (sz.cx > 0) labelW = sz.cx + 6;
                if (sz.cy > 0) textH = sz.cy;
            }
            SelectObject(dc, oldFont);
            ReleaseDC(hwnd_, dc);
        }

        int labelCy = cy + (comboH - textH) / 2;
        MoveWindow(langLabel_, pad, labelCy, labelW, textH, TRUE);
        MoveWindow(langCombo_, pad + labelW + 8, cy, 200, 400, TRUE);
    }
}

static std::wstring NormalizeLineBreaks(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++)
    {
        wchar_t c = s[i];
        if (c == L'\r')
        {
            if (i + 1 < s.size() && s[i + 1] == L'\n') { out += L"\r\n"; i++; }
        }
        else if (c == L'\n') out += L"\r\n";
        else out.push_back(c);
    }
    return out;
}

void ResultWindow::setLoading(const std::wstring& msg)
{
    SetWindowTextW(edit_, NormalizeLineBreaks(msg).c_str());
    EnableWindow(retryBtn_, FALSE);
    if (langCombo_) EnableWindow(langCombo_, FALSE);
}

void ResultWindow::setResult(const std::wstring& text)
{
    SetWindowTextW(edit_, NormalizeLineBreaks(text).c_str());
}

void ResultWindow::onLanguageChanged()
{
    int sel = (int)SendMessageW(langCombo_, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    wchar_t lang[64] = {0};
    SendMessageW(langCombo_, CB_GETLBTEXT, sel, (LPARAM)lang);
    if (util::ToWide(g_settings.translateTargetLanguage) == lang) return;

    g_settings.translateTargetLanguage = util::ToUtf8(lang);
    SaveSettings(g_settings);
    SetWindowTextW(hwnd_, (L"翻译 -> " + std::wstring(lang)).c_str());
    runAi();
}

void ResultWindow::runAi()
{
    if (aiInflight_) return;
    bool needBmp = (mode_ != Mode::Translate) || !state_->ocrDone.load();
    if (needBmp && !state_->bmp) return;
    aiInflight_ = true;
    state_->closed = false;
    setLoading(L"正在处理…");

    AppSettings settings = g_settings;
    auto state = state_;
    Mode mode = mode_;
    HWND target = hwnd_;
    keepBmp_ = (mode == Mode::Translate && settings.api.textModel.empty());
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
            else
            {
                if (settings.api.textModel.empty())
                {
                    r = ai.translateDirect(state->bmp, settings);
                }
                else
                {
                    if (!state->ocrDone.load())
                    {
                        std::string ocr = ai.ocr(state->bmp, settings);
                        std::lock_guard<std::mutex> lk(state->mtx);
                        state->ocrText = ocr;
                        state->ocrDone = true;
                    }
                    std::string src;
                    {
                        std::lock_guard<std::mutex> lk(state->mtx);
                        src = state->ocrText;
                    }
                    r = ai.translate(src, settings);
                }
            }
            if (r.empty()) r = "（未返回内容）";
            text = util::ToWide(r);
        }
        catch (const std::exception& e)
        {
            kind = 1;
            text = util::ToWide(std::string("失败：") + e.what());
            logger::error("AI run failed: " + std::string(e.what()));
        }
        catch (...)
        {
            kind = 1;
            text = L"失败：发生未知错误";
            logger::error("AI run failed: unknown exception");
        }
        {
            std::lock_guard<std::mutex> lk(state->mtx);
            state->kind = kind;
            state->text = std::move(text);
        }
        if (!state->closed.load() && IsWindow(target))
            PostMessageW(target, WM_APP_AI_RESULT, 0, 0);
    }).detach();
}
