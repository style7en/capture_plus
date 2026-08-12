#include "Pch.h"
#include "AppSettings.h"
#include "Logger.h"
#include "TrayIcon.h"
#include "HotkeyManager.h"
#include "ScreenshotSession.h"
#include "SettingsWindow.h"
#include "ResultWindow.h"
#include "OverlayWindow.h"
#include "GdiUtil.h"
#include "Util.h"
#include "resource.h"

AppSettings       g_settings;
TrayIcon*         g_tray    = nullptr;
HotkeyManager*    g_hotkey  = nullptr;
ScreenshotSession* g_session = nullptr;
HWND              g_mainHwnd = nullptr;

static void startScreenshot()
{
    if (g_session) g_session->start();
}

static void openSettings()
{
    if (g_tray && g_hotkey)
        OpenSettings(g_mainHwnd, g_settings, *g_hotkey, *g_tray);
}

static void quitApp()
{
    PostQuitMessage(0);
}

static void setDpiAware()
{
    HMODULE u32 = LoadLibraryW(L"user32.dll");
    if (u32)
    {
        using SetCtx_t = BOOL(WINAPI*)(void*);
        auto setCtx = (SetCtx_t)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        if (setCtx)
        {
            void* PER_MONITOR_AWARE_V2 = (void*)(intptr_t)-4;
            if (setCtx(PER_MONITOR_AWARE_V2))
            {
                FreeLibrary(u32);
                return;
            }
        }
        auto p = (BOOL(WINAPI*)(void))GetProcAddress(u32, "SetProcessDPIAware");
        if (p) p();
        FreeLibrary(u32);
    }
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    setDpiAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\CapturePlus_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr, L"CapturePlus 已在后台运行。", L"CapturePlus",
                    MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    gdiutil::GdiStartup();
    g_settings = LoadSettings();

    g_tray = new TrayIcon();
    g_mainHwnd = g_tray->hwnd();
    g_tray->setOnScreenshot(startScreenshot);
    g_tray->setOnSettings(openSettings);
    g_tray->setOnExit(quitApp);
    g_tray->updateMenuLabel(L"截图 (" + util::ToWide(g_settings.hotkey) + L")");
    g_tray->showBalloon(L"CapturePlus 已启动，按 " +
                        util::ToWide(g_settings.hotkey) + L" 截图", 1500);

    g_hotkey = new HotkeyManager();
    g_hotkey->setCallback(startScreenshot);
    if (!g_hotkey->registerHotkey(g_settings.hotkey) && g_tray)
        g_tray->showBalloon(L"快捷键被占用，请在设置中更换", 2500);

    g_session = new ScreenshotSession();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ResultWindow::WaitForAiTasks(5000);
    delete g_session;
    delete g_hotkey;
    delete g_tray;
    OverlayWindow::shutdown();
    ShutdownSettingsFont();
    gdiutil::GdiShutdown();
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    CoUninitialize();
    return (int)msg.wParam;
}
