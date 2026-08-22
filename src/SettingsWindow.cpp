#include "SettingsWindow.h"
#include "GdiUtil.h"
#include "HotkeyManager.h"
#include "TrayIcon.h"
#include "Logger.h"
#include "Util.h"
#include "resource.h"

static UINT_PTR g_hotkeySubclassId = 100;

static HFONT g_dlgFont = nullptr;

static void setDlgFont(HWND dlg)
{
    if (!g_dlgFont)
    {
        HDC dc = GetDC(nullptr);
        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        if (dpi <= 0) dpi = 96;
        g_dlgFont = gdiutil::CreateUiFont(dpi);
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
            gdiutil::SetAppIcon(dlg);

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
            return TRUE;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wp))
            {
                case IDC_TOGGLE_APIKEY_BTN:
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
                    auto getEdit = [&](int id) -> std::string {
                        wchar_t buf[1024];
                        GetWindowTextW(GetDlgItem(dlg, id), buf, 1024);
                        return util::ToUtf8(buf);
                    };
                    ctx->settings->hotkey = HotkeyFormat(ctx->capturedMods, ctx->capturedVk);
                    ctx->settings->api.baseUrl     = getEdit(IDC_BASEURL_EDIT);
                    ctx->settings->api.apiKey      = getEdit(IDC_APIKEY_EDIT);
                    ctx->settings->api.visionModel = getEdit(IDC_VISIONMODEL_EDIT);
                    ctx->settings->api.textModel   = getEdit(IDC_TEXTMODEL_EDIT);

                    SaveSettings(*ctx->settings);
                    if (ctx->hotkey && !ctx->hotkey->reRegister(ctx->settings->hotkey))
                        MessageBoxW(dlg, L"该快捷键被占用，已保留原快捷键。",
                                    L"CapturePlus", MB_OK | MB_ICONWARNING);
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
