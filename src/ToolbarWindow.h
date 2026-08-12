#pragma once
#include "Pch.h"
#include "SelectionTracker.h"

enum class ScreenshotAction { Copy, Save, Ocr, Ai, Translate };

class ToolbarWindow
{
public:
    using ActionCb = std::function<void(ScreenshotAction)>;
    using CancelCb = std::function<void()>;

    ToolbarWindow();
    ~ToolbarWindow();

    bool create(int x, int y);
    void show();
    void close();

    HWND hwnd() const { return hwnd_; }
    int  width() const  { return width_; }
    int  height() const { return height_; }

    void setActionCb(ActionCb cb) { actionCb_ = std::move(cb); }
    void setCancelCb(CancelCb cb) { cancelCb_ = std::move(cb); }

    static void computePosition(const NormRect& sel, int monX, int monY,
                                int monW, int monH, int margin,
                                int tw, int th,
                                int& outX, int& outY);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void layoutButtons();

    HWND hwnd_ = nullptr;
    HWND buttons_[6] = {};
    HFONT font_ = nullptr;
    int   width_ = 0, height_ = 0;
    int   scale_ = 100;

    ActionCb actionCb_;
    CancelCb cancelCb_;
};
