#pragma once
#include "Pch.h"
#include "ToolbarWindow.h"

class ResultWindow
{
public:
    enum class Mode { Ocr, Ai, Translate };

    ResultWindow(Mode mode, HBITMAP bmp);
    ~ResultWindow();

    using CloseCb = std::function<void()>;
    void setCloseCb(CloseCb cb) { closeCb_ = std::move(cb); }

    static void WaitForAiTasks(int timeoutMs);

    bool create();
    void show();

private:
    struct Shared
    {
        HBITMAP bmp = nullptr;
        HWND    hwnd = nullptr;
        std::atomic<bool> closed{ false };
        ~Shared() { if (bmp) DeleteObject((HGDIOBJ)bmp); }
    };

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void runAi();
    void setLoading(const std::wstring& msg);
    void setResult(const std::wstring& text);
    void setError(const std::wstring& msg);
    void onLayout();

    HWND   hwnd_     = nullptr;
    HWND   edit_     = nullptr;
    HWND   copyBtn_  = nullptr;
    HWND   retryBtn_ = nullptr;
    HWND   closeBtn_ = nullptr;
    HFONT  font_     = nullptr;
    Mode   mode_;

    std::shared_ptr<Shared> state_;
    CloseCb closeCb_;
};
