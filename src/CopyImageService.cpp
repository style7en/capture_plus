#include "CopyImageService.h"
#include "GdiUtil.h"
#include "Logger.h"

namespace copyimage {

void Copy(HBITMAP hbmp)
{
    if (!hbmp) return;
    for (int attempt = 0; attempt < 3; attempt++)
    {
        HGLOBAL hDib = gdiutil::HBitmapToDibGlobal(hbmp);
        if (!hDib) { logger::warn("HBitmapToDibGlobal failed"); continue; }

        if (!OpenClipboard(nullptr))
        {
            GlobalFree(hDib);
            Sleep(100);
            continue;
        }
        EmptyClipboard();
        HANDLE ok = SetClipboardData(CF_DIB, hDib);
        CloseClipboard();

        if (ok)
        {
            return;
        }
        GlobalFree(hDib);
        logger::warn("SetClipboardData attempt " + std::to_string(attempt + 1) + " failed");
        Sleep(100);
    }
    MessageBoxW(nullptr, L"复制到剪贴板失败，请重试。", L"CapturePlus",
                MB_OK | MB_ICONWARNING);
}

}
