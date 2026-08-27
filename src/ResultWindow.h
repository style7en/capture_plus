#pragma once
#include "Pch.h"

class ResultWindow
{
public:
    enum class Mode { Ocr, Ai, Translate };

    ResultWindow(Mode mode, HBITMAP bmp);
    ~ResultWindow();

    static void WaitForAiTasks(int timeoutMs);
    static bool HasInFlightAi();

    bool create();
    void show();

private:
    struct Shared
    {
        HBITMAP bmp = nullptr;
        std::atomic<bool> closed{ false };
        std::atomic<bool> ocrDone{ false };
        std::mutex mtx;
        int  kind = 0;
        std::wstring text;
        std::string ocrText;
        ULONGLONG elapsedMs = 0;
        ~Shared() { if (bmp) DeleteObject((HGDIOBJ)bmp); }
    };

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void runAi();
    void onLanguageChanged();
    void onPreviewLink();
    void setLoading();
    void setResult(const std::wstring& text);
    void onLayout();

    HWND   hwnd_     = nullptr;
    HWND   edit_     = nullptr;
    HWND   copyBtn_  = nullptr;
    HWND   retryBtn_ = nullptr;
    HWND   closeBtn_ = nullptr;
    HWND   langLabel_ = nullptr;
    HWND   langCombo_ = nullptr;
    HWND   previewLink_ = nullptr;
    HWND   elapsedLabel_ = nullptr;
    HFONT  font_     = nullptr;
    Mode   mode_;
    bool   aiInflight_ = false;

    std::shared_ptr<Shared> state_;
};
