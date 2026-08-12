#pragma once
#include "Pch.h"
#include "OverlayWindow.h"
#include "SelectionTracker.h"
#include "ToolbarWindow.h"

class ScreenshotSession
{
public:
    ScreenshotSession();
    ~ScreenshotSession();

    void start();

private:
    void onInput(InputPhase phase, double x, double y);
    void renderOverlay();
    void showToolbar(double x, double y);
    void onAction(ScreenshotAction action);
    void closeWindows();
    void closeAll();

    OverlayWindow*    overlay_  = nullptr;
    ToolbarWindow*    toolbar_  = nullptr;
    SelectionTracker  tracker_;
    std::atomic<bool> busy_{ false };
};
