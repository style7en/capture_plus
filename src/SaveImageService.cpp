#include "SaveImageService.h"
#include "GdiUtil.h"
#include "Logger.h"
#include "Util.h"

namespace saveimage {

static std::wstring getKnownFolder(REFKNOWNFOLDERID rfid)
{
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(rfid, 0, nullptr, &path)) && path)
    {
        result = path;
        CoTaskMemFree(path);
    }
    return result;
}

static std::wstring resolveDir()
{
    std::wstring pics = getKnownFolder(FOLDERID_Pictures);
    if (!pics.empty())
    {
        pics += L"\\Screenshots";
        if (!PathFileExistsW(pics.c_str()))
        {
            if (CreateDirectoryW(pics.c_str(), nullptr))
                return pics;
        }
        else return pics;
    }
    std::wstring desktop = getKnownFolder(FOLDERID_Desktop);
    if (!desktop.empty()) return desktop;
    return L".";
}

void Save(HBITMAP hbmp)
{
    if (!hbmp) return;
    std::wstring dir = resolveDir();

    __int64 t = _time64(nullptr);
    struct tm lt;
    localtime_s(&lt, &t);
    wchar_t defName[MAX_PATH];
    swprintf(defName, MAX_PATH, L"CapturePlus_%04d%02d%02d_%02d%02d%02d.png",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             lt.tm_hour, lt.tm_min, lt.tm_sec);

    wchar_t filePath[MAX_PATH] = {0};
    wcsncpy(filePath, defName, MAX_PATH - 1);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrInitialDir = dir.c_str();
    ofn.lpstrFilter = L"PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg\0BMP (*.bmp)\0*.bmp\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn)) return;

    std::wstring ext = filePath;
    size_t dot = ext.find_last_of(L'.');
    const wchar_t* mime = L"image/png";
    if (dot != std::wstring::npos)
    {
        std::wstring e = ext.substr(dot);
        if (e == L".jpg" || e == L".jpeg") mime = L"image/jpeg";
        else if (e == L".bmp")             mime = L"image/bmp";
    }

    if (gdiutil::SaveHBitmapToFile(hbmp, filePath, mime))
    {
        std::wstring msg = L"已保存到 " + std::wstring(filePath);
        MessageBoxW(nullptr, msg.c_str(), L"CapturePlus", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxW(nullptr, L"保存失败。", L"CapturePlus", MB_OK | MB_ICONERROR);
    }
}

}
