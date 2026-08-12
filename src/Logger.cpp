#include "Logger.h"
#include "Util.h"

namespace logger {

static std::mutex g_mtx;
static std::wstring g_logDir;

static const std::wstring& logDir()
{
    if (g_logDir.empty())
    {
        g_logDir = util::GetAppDataDir() + L"\\logs";
        CreateDirectoryW(g_logDir.c_str(), nullptr);
    }
    return g_logDir;
}

static void writeLine(const char* level, const std::string& msg)
{
    try
    {
        __int64 t = _time64(nullptr);
        struct tm lt;
        localtime_s(&lt, &t);
        wchar_t fname[MAX_PATH];
        swprintf(fname, MAX_PATH, L"cpp-%04d-%02d-%02d.log",
                 lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
        std::wstring path = logDir() + L"\\" + fname;
        char head[64];
        snprintf(head, sizeof head, "[%02d:%02d:%02d] [%s] ",
                 lt.tm_hour, lt.tm_min, lt.tm_sec, level);
        std::string line = head + msg + "\r\n";
        std::lock_guard<std::mutex> lk(g_mtx);
        HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD written;
        WriteFile(h, line.data(), (DWORD)line.size(), &written, nullptr);
        CloseHandle(h);
    }
    catch (...) {}
}

void info(const std::string& msg) { writeLine("INFO", msg); }
void warn(const std::string& msg) { writeLine("WARN", msg); }
void error(const std::string& msg) { writeLine("ERROR", msg); }

}
