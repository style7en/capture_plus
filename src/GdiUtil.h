#pragma once
#include "Pch.h"

namespace gdiutil {

struct BitmapGuard
{
    HBITMAP h;
    explicit BitmapGuard(HBITMAP b = nullptr) : h(b) {}
    ~BitmapGuard() { if (h) DeleteObject((HGDIOBJ)h); }
    BitmapGuard(const BitmapGuard&) = delete;
    BitmapGuard& operator=(const BitmapGuard&) = delete;
    HBITMAP release() { HBITMAP t = h; h = nullptr; return t; }
    operator HBITMAP() const { return h; }
};

void GdiStartup();
void GdiShutdown();

HBITMAP CaptureScreenRect(int x, int y, int w, int h);
HBITMAP CropBitmap(HBITMAP src, int x, int y, int w, int h);
HGLOBAL  HBitmapToDibGlobal(HBITMAP hbmp);

int  GetEncoderClsid(const wchar_t* format, CLSID* clsid);
bool SaveHBitmapToFile(HBITMAP hbmp, const wchar_t* path, const wchar_t* format);

}
