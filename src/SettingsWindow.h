#pragma once
#include "Pch.h"
#include "AppSettings.h"

class HotkeyManager;
class TrayIcon;

struct SettingsContext
{
    AppSettings*    settings;
    HotkeyManager*  hotkey;
    TrayIcon*       tray;
    UINT capturedMods = 0;
    UINT capturedVk   = 0;
};

void OpenSettings(HWND parent, AppSettings& settings, HotkeyManager& hotkey, TrayIcon& tray);
void ShutdownSettingsFont();
