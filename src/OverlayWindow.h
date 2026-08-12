#pragma once
#include "Pch.h"
#include "SelectionTracker.h"

enum class InputPhase { Begin, Move, End };

class OverlayWindow
{
public:
    using InputCb = std::function<void(InputPhase, double, double)>;
    using CancelCb = std::function<void()>;

    OverlayWindow();
    ~OverlayWindow();

    bool create();
    void show();
    void close();

    void setInputCb(InputCb cb)    { inputCb_ = std::move(cb); }
    void setCancelCb(CancelCb cb)  { cancelCb_ = std::move(cb); }

    void renderSelection(const NormRect* sel);
    HBITMAP captureRect(const NormRect& sel) const;

    static void shutdown();
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

private:
    void onPaint(HDC hdc);

    HWND hwnd_ = nullptr;
    int  originX_ = 0, originY_ = 0, width_ = 0, height_ = 0;
    HBITMAP snapshot_ = nullptr;
    HBITMAP dimmed_  = nullptr;
    NormRect selection_;
    bool     hasSelection_ = false;

    InputCb  inputCb_;
    CancelCb cancelCb_;
};
