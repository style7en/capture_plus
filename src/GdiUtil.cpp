#include "GdiUtil.h"
#include "Logger.h"
#include "Util.h"
#include "resource.h"

namespace gdiutil {

static ULONG_PTR g_gdiToken = 0;

void GdiStartup()
{
    Gdiplus::GdiplusStartupInput si;
    Gdiplus::GdiplusStartup(&g_gdiToken, &si, nullptr);
}

void GdiShutdown()
{
    if (g_gdiToken) { Gdiplus::GdiplusShutdown(g_gdiToken); g_gdiToken = 0; }
}

HFONT CreateUiFont(int dpi)
{
    int h = -MulDiv(9, dpi, 72);
    return CreateFontW(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       FF_DONTCARE, L"Microsoft YaHei");
}

void SetAppIcon(HWND hwnd)
{
    HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (hIcon)
    {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessageW(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
    }
}

HBITMAP CaptureScreenRect(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return nullptr;
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, screen, x, y, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return bmp;
}

HBITMAP CropBitmap(HBITMAP src, int x, int y, int w, int h)
{
    if (!src || w <= 0 || h <= 0) return nullptr;
    HDC screen = GetDC(nullptr);
    HDC srcDc = CreateCompatibleDC(screen);
    HBITMAP oldSrc = (HBITMAP)SelectObject(srcDc, src);
    HDC dstDc = CreateCompatibleDC(screen);
    HBITMAP result = CreateCompatibleBitmap(screen, w, h);
    HBITMAP oldDst = (HBITMAP)SelectObject(dstDc, result);
    BitBlt(dstDc, 0, 0, w, h, srcDc, x, y, SRCCOPY);
    SelectObject(dstDc, oldDst);
    SelectObject(srcDc, oldSrc);
    DeleteDC(dstDc);
    DeleteDC(srcDc);
    ReleaseDC(nullptr, screen);
    return result;
}

HGLOBAL HBitmapToDibGlobal(HBITMAP hbmp)
{
    if (!hbmp) return nullptr;

    BITMAP bm = {};
    if (!GetObject(hbmp, sizeof(bm), &bm)) return nullptr;

    BITMAPINFOHEADER bi = {};
    bi.biSize        = sizeof(BITMAPINFOHEADER);
    bi.biWidth       = bm.bmWidth;
    bi.biHeight      = bm.bmHeight;
    bi.biPlanes      = 1;
    bi.biBitCount    = 32;
    bi.biCompression = BI_RGB;

    DWORD imgSize = (DWORD)(bm.bmWidth * 4) * bm.bmHeight;
    DWORD total   = sizeof(BITMAPINFOHEADER) + imgSize;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, total);
    if (!h) return nullptr;

    auto* p = (BYTE*)GlobalLock(h);
    if (!p) { GlobalFree(h); return nullptr; }

    BITMAPINFO bi_full = {};
    bi_full.bmiHeader = bi;
    HDC dc = GetDC(nullptr);
    int rows = GetDIBits(dc, hbmp, 0, bm.bmHeight, p + sizeof(BITMAPINFOHEADER),
                         &bi_full, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);

    if (rows == 0)
    {
        GlobalUnlock(h);
        GlobalFree(h);
        return nullptr;
    }
    memcpy(p, &bi_full.bmiHeader, sizeof(BITMAPINFOHEADER));
    GlobalUnlock(h);
    return h;
}

int GetEncoderClsid(const wchar_t* format, CLSID* clsid)
{
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    auto buf = std::make_unique<BYTE[]>(size);
    auto* enc = (Gdiplus::ImageCodecInfo*)buf.get();
    Gdiplus::GetImageEncoders(num, size, enc);
    for (UINT i = 0; i < num; i++)
    {
        if (wcscmp(enc[i].MimeType, format) == 0)
        {
            *clsid = enc[i].Clsid;
            return (int)i;
        }
    }
    return -1;
}

bool SaveHBitmapToFile(HBITMAP hbmp, const wchar_t* path, const wchar_t* format)
{
    if (!hbmp || !path) return false;
    Gdiplus::Bitmap bmp(hbmp, nullptr);
    CLSID clsid;
    if (GetEncoderClsid(format, &clsid) < 0)
    {
        logger::error("No encoder for format");
        return false;
    }
    Gdiplus::Status st = bmp.Save(path, &clsid, nullptr);
    if (st != Gdiplus::Ok)
    {
        logger::error("Save bitmap failed, status=" + std::to_string((int)st));
        return false;
    }
    return true;
}

std::string HBitmapToBase64Png(HBITMAP hbmp)
{
    if (!hbmp) return "";

    IStream* raw = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &raw)) || !raw)
        throw std::runtime_error("CreateStreamOnHGlobal failed");

    struct Guard
    {
        IStream* stream;
        HGLOBAL  mem = nullptr;
        bool    locked = false;
        ~Guard()
        {
            if (locked) GlobalUnlock(mem);
            if (stream) stream->Release();
        }
    } guard{ raw };

    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gdiToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiToken, &gsi, nullptr) != Gdiplus::Ok)
        throw std::runtime_error("GdiplusStartup failed");
    struct GdiGuard { ULONG_PTR t; ~GdiGuard(){ if (t) Gdiplus::GdiplusShutdown(t); } } gdiGuard{ gdiToken };

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) < 0)
        throw std::runtime_error("No PNG encoder");

    {
        Gdiplus::Bitmap gbmp(hbmp, nullptr);
        if (gbmp.Save(guard.stream, &pngClsid, nullptr) != Gdiplus::Ok)
            throw std::runtime_error("Bitmap.Save failed");
    }

    if (FAILED(GetHGlobalFromStream(guard.stream, &guard.mem)) || !guard.mem)
        throw std::runtime_error("GetHGlobalFromStream failed");

    auto* ptr = (const unsigned char*)GlobalLock(guard.mem);
    if (!ptr) throw std::runtime_error("GlobalLock failed");
    guard.locked = true;

    return util::Base64Encode(ptr, GlobalSize(guard.mem));
}

}
