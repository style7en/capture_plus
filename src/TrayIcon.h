#pragma once
#include "Pch.h"

class TrayIcon
{
public:
    using Callback = std::function<void()>;

    TrayIcon();
    ~TrayIcon();

    void setOnScreenshot(Callback cb) { onScreenshot_ = std::move(cb); }
    void setOnSettings(Callback cb)   { onSettings_   = std::move(cb); }
    void setOnExit(Callback cb)       { onExit_       = std::move(cb); }

    void showBalloon(const std::wstring& msg, int ms = 1500);
    void updateMenuLabel(const std::wstring& screenshotLabel);

    HWND hwnd() const { return hwnd_; }

private:
    void showMenu();
    void handleTrayMessage(WPARAM wp, LPARAM lp);

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    NOTIFYICONDATAW nid_ = {};
    HWND   hwnd_ = nullptr;
    HMENU  menu_ = nullptr;
    bool   ownsIcon_ = false;
    std::wstring screenshotLabel_ = L"截图";

    Callback onScreenshot_;
    Callback onSettings_;
    Callback onExit_;
};
