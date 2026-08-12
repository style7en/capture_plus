#pragma once
#include "Pch.h"

bool HotkeyParse(const std::string& text, UINT& mods, UINT& vk);
std::string HotkeyFormat(UINT mods, UINT vk);
bool HotkeyFromWParam(WPARAM wParam, UINT& mods, UINT& vk);

class HotkeyManager
{
public:
    using Callback = std::function<void()>;

    HotkeyManager();
    ~HotkeyManager();

    bool registerHotkey(const std::string& hotkey);
    bool reRegister(const std::string& hotkey);
    void unregister();

    void setCallback(Callback cb) { cb_ = std::move(cb); }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HWND    hwnd_ = nullptr;
    UINT    mods_ = 0;
    UINT    vk_   = 0;
    bool    registered_ = false;
    Callback cb_;
};
