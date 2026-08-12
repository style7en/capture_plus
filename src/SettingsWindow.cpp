#include "SettingsWindow.h"
#include "HotkeyManager.h"
#include "TrayIcon.h"
#include "Logger.h"
#include "Util.h"
#include "resource.h"

static UINT_PTR g_hotkeySubclassId = 100;

static const wchar_t* const kLanguages[] = {
    L"中文（简体）", L"中文（繁體）", L"English", L"日本語",
    L"한국어", L"Français", L"Deutsch", L"Español", L"Русский",
};

static HFONT g_dlgFont = nullptr;

static void setDlgFont(HWND dlg)
{
    if (!g_dlgFont)
    {
        HDC dc = GetDC(nullptr);
        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        if (dpi <= 0) dpi = 96;
        int h = -MulDiv(9, dpi, 72);
        g_dlgFont = CreateFontW(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FF_DONTCARE, L"Microsoft YaHei");
    }
    if (g_dlgFont)
    {
        EnumChildWindows(dlg, [](HWND child, LPARAM lp) -> BOOL {
            SendMessageW(child, WM_SETFONT, (WPARAM)lp, TRUE);
            return TRUE;
        }, (LPARAM)g_dlgFont);
        SendMessageW(dlg, WM_SETFONT, (WPARAM)g_dlgFont, TRUE);
    }
}

static LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                       UINT_PTR id, DWORD_PTR ref)
{
    auto* ctx = (SettingsContext*)ref;
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
    {
        UINT mods, vk;
        if (HotkeyFromWParam(wp, mods, vk))
        {
            ctx->capturedMods = mods;
            ctx->capturedVk   = vk;
            std::string label = HotkeyFormat(mods, vk);
            SetWindowTextW(hwnd, util::ToWide(label).c_str());
        }
        return 0;
    }
    if (msg == WM_CHAR) return 0;
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* ctx = (SettingsContext*)GetWindowLongPtrW(dlg, GWLP_USERDATA);

    switch (msg)
    {
        case WM_INITDIALOG:
        {
            ctx = (SettingsContext*)lp;
            SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)ctx);
            setDlgFont(dlg);

            HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr),
                MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
            if (hIcon)
            {
                SendMessageW(dlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                SendMessageW(dlg, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
            }

            HWND hk = GetDlgItem(dlg, IDC_HOTKEY_EDIT);
            SetWindowSubclass(hk, HotkeyEditProc, g_hotkeySubclassId, (DWORD_PTR)ctx);

            UINT mods, vk;
            if (HotkeyParse(ctx->settings->hotkey, mods, vk))
            {
                ctx->capturedMods = mods;
                ctx->capturedVk   = vk;
            }
            SetWindowTextW(hk, util::ToWide(ctx->settings->hotkey).c_str());
            SetWindowTextW(GetDlgItem(dlg, IDC_BASEURL_EDIT),
                util::ToWide(ctx->settings->api.baseUrl).c_str());
            SetWindowTextW(GetDlgItem(dlg, IDC_APIKEY_EDIT),
                util::ToWide(ctx->settings->api.apiKey).c_str());
            SetWindowTextW(GetDlgItem(dlg, IDC_VISIONMODEL_EDIT),
                util::ToWide(ctx->settings->api.visionModel).c_str());
            SetWindowTextW(GetDlgItem(dlg, IDC_TEXTMODEL_EDIT),
                util::ToWide(ctx->settings->api.textModel).c_str());

            HWND combo = GetDlgItem(dlg, IDC_TRANSLATE_COMBO);
            std::wstring cur = util::ToWide(ctx->settings->translateTargetLanguage);
            int sel = -1;
            for (int i = 0; i < (int)(sizeof(kLanguages)/sizeof(kLanguages[0])); i++)
            {
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)kLanguages[i]);
                if (wcscmp(kLanguages[i], cur.c_str()) == 0) sel = i;
            }
            if (sel < 0) { SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)cur.c_str()); sel = (int)(sizeof(kLanguages)/sizeof(kLanguages[0])); }
            SendMessageW(combo, CB_SETCURSEL, sel, 0);
            return TRUE;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wp))
            {
                case IDC_TEST_BTN:
                {
                    HWND e = GetDlgItem(dlg, IDC_APIKEY_EDIT);
                    LONG style = GetWindowLongW(e, GWL_STYLE);
                    if (style & ES_PASSWORD) SendMessageW(e, EM_SETPASSWORDCHAR, 0, 0);
                    else                     SendMessageW(e, EM_SETPASSWORDCHAR, (WPARAM)L'*', 0);
                    SetWindowLongW(e, GWL_STYLE, style ^ ES_PASSWORD);
                    InvalidateRect(e, nullptr, TRUE);
                    return TRUE;
                }
                case IDC_SETTINGS_OK:
                {
                    wchar_t buf[MAX_PATH];
                    ctx->settings->hotkey = HotkeyFormat(ctx->capturedMods, ctx->capturedVk);
                    GetWindowTextW(GetDlgItem(dlg, IDC_BASEURL_EDIT), buf, MAX_PATH);
                    ctx->settings->api.baseUrl = util::ToUtf8(buf);
                    GetWindowTextW(GetDlgItem(dlg, IDC_APIKEY_EDIT), buf, MAX_PATH);
                    ctx->settings->api.apiKey = util::ToUtf8(buf);
                    GetWindowTextW(GetDlgItem(dlg, IDC_VISIONMODEL_EDIT), buf, MAX_PATH);
                    ctx->settings->api.visionModel = util::ToUtf8(buf);
                    GetWindowTextW(GetDlgItem(dlg, IDC_TEXTMODEL_EDIT), buf, MAX_PATH);
                    ctx->settings->api.textModel = util::ToUtf8(buf);
                    HWND combo = GetDlgItem(dlg, IDC_TRANSLATE_COMBO);
                    int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
                    if (sel >= 0)
                    {
                        wchar_t lang[64] = {0};
                        SendMessageW(combo, CB_GETLBTEXT, sel, (LPARAM)lang);
                        ctx->settings->translateTargetLanguage = util::ToUtf8(lang);
                    }

                    SaveSettings(*ctx->settings);
                    if (ctx->hotkey) ctx->hotkey->reRegister(ctx->settings->hotkey);
                    if (ctx->tray)
                        ctx->tray->updateMenuLabel(L"截图 (" +
                            util::ToWide(ctx->settings->hotkey) + L")");
                    EndDialog(dlg, IDOK);
                    return TRUE;
                }
                case IDC_SETTINGS_CANCEL:
                    EndDialog(dlg, IDCANCEL);
                    return TRUE;
            }
            break;
        }
        case WM_CLOSE:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void OpenSettings(HWND parent, AppSettings& settings, HotkeyManager& hotkey, TrayIcon& tray)
{
    SettingsContext ctx{ &settings, &hotkey, &tray, 0, 0 };
    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDR_SETTINGS_DIALOG),
                    parent, DlgProc, (LPARAM)&ctx);
}

void ShutdownSettingsFont()
{
    if (g_dlgFont) { DeleteObject((HGDIOBJ)g_dlgFont); g_dlgFont = nullptr; }
}
