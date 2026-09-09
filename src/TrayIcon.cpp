#include "TrayIcon.h"
#include "resource.h"
#include "Logger.h"

extern std::atomic<bool> g_modalDialogOpen;

static const wchar_t* KC_TRAY = L"CapturePlus_TrayWnd";

static HRESULT CALLBACK AboutCallback(HWND hwnd, UINT msg, WPARAM, LPARAM lp, LONG_PTR)
{
    if (msg == TDN_HYPERLINK_CLICKED && lp)
        ShellExecuteW(hwnd, L"open", (LPCWSTR)lp, nullptr, nullptr, SW_SHOWNORMAL);
    return S_OK;
}

TrayIcon::TrayIcon()
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = &TrayIcon::WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = KC_TRAY;
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(0, KC_TRAY, L"CapturePlus_Tray", 0,
                            0, 0, 0, 0, nullptr, nullptr, wc.hInstance, this);
    if (hwnd_) SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);

    menu_ = CreatePopupMenu();
    AppendMenuW(menu_, MF_STRING, 1, screenshotLabel_.c_str());
    AppendMenuW(menu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu_, MF_STRING, 2, L"设置");
    AppendMenuW(menu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu_, MF_STRING, 3, L"关于");
    AppendMenuW(menu_, MF_STRING, 4, L"退出");

    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd   = hwnd_;
    nid_.uID    = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_APP_TRAY;
    nid_.hIcon  = (HICON)LoadImageW(GetModuleHandleW(nullptr),
                                    MAKEINTRESOURCEW(IDI_APP),
                                    IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if (nid_.hIcon) ownsIcon_ = true;
    else nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy(nid_.szTip, L"CapturePlus", 127);

    Shell_NotifyIconW(NIM_ADD, &nid_);
}

TrayIcon::~TrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    if (ownsIcon_ && nid_.hIcon) DestroyIcon(nid_.hIcon);
    if (menu_) DestroyMenu(menu_);
    if (hwnd_) DestroyWindow(hwnd_);
}

void TrayIcon::showBalloon(const std::wstring& msg, int ms)
{
    nid_.uFlags |= NIF_INFO;
    nid_.dwInfoFlags = NIIF_NONE;
    nid_.uTimeout = (UINT)ms;
    wcsncpy(nid_.szInfoTitle, L"CapturePlus", 63);
    nid_.szInfoTitle[63] = L'\0';
    wcsncpy(nid_.szInfo, msg.c_str(), 255);
    nid_.szInfo[255] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::updateMenuLabel(const std::wstring& screenshotLabel)
{
    screenshotLabel_ = screenshotLabel;
    ModifyMenuW(menu_, 1, MF_BYCOMMAND | MF_STRING, 1, screenshotLabel_.c_str());
}

static TrayIcon* selfFromHwnd(HWND h)
{
    return (TrayIcon*)GetWindowLongPtrW(h, GWLP_USERDATA);
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = selfFromHwnd(hwnd);
    if (msg == WM_APP_TRAY && self)
    {
        self->handleTrayMessage(wp, lp);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void TrayIcon::showMenu()
{
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    int cmd = TrackPopupMenu(menu_, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN,
                             pt.x, pt.y, 0, hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    switch (cmd)
    {
        case 1: if (onScreenshot_) onScreenshot_(); break;
        case 2: if (onSettings_)   onSettings_();   break;
        case 3:
        {
            if (g_modalDialogOpen.exchange(true)) break;
            const wchar_t* content =
                L"CapturePlus v1.10\n\n"
                L"Windows 截图增强工具。常驻通知栏，按快捷键呼出截图，框选后提供五项操作：\n\n"
                L"  · 复制图片 — 选区位图复制到剪贴板\n"
                L"  · 保存图片 — 保存为 PNG / JPEG / BMP\n"
                L"  · 提取文字 — AI 视觉模型识别截图中的文字\n"
                L"  · AI 分析  — 分析截图内容，图中含提问则直接作答\n"
                L"  · 翻译     — 先 OCR 再翻译为目标语言\n\n"
                L"支持 OpenAI 兼容接口（DeepSeek、通义千问、Ollama 等）。\n"
                L"多显示器混合 DPI 自适应。\n\n"
                L"联系邮箱：zhangjia_86@126.com\n"
                L"<a href=\"https://github.com/style7en/capture_plus\">GitHub 仓库</a>";
            TASKDIALOGCONFIG cfg = { sizeof(cfg) };
            cfg.hInstance = GetModuleHandleW(nullptr);
            cfg.dwFlags = TDF_ENABLE_HYPERLINKS;
            cfg.pszWindowTitle = L"关于";
            cfg.pszMainIcon = MAKEINTRESOURCEW(IDI_APP);
            cfg.pszContent = content;
            cfg.pfCallback = AboutCallback;
            cfg.dwCommonButtons = TDCBF_OK_BUTTON;
            TaskDialogIndirect(&cfg, nullptr, nullptr, nullptr);
            g_modalDialogOpen = false;
            break;
        }
        case 4: if (onExit_)       onExit_();       break;
    }
}

void TrayIcon::handleTrayMessage(WPARAM wp, LPARAM lp)
{
    if (wp != nid_.uID) return;
    switch (lp)
    {
        case WM_LBUTTONDBLCLK:
            if (onScreenshot_) onScreenshot_();
            break;
        case WM_RBUTTONUP:
            showMenu();
            break;
    }
}
