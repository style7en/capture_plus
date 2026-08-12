#include "Util.h"

namespace util {

std::wstring ToWide(const std::string& utf8)
{
    if (utf8.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &w[0], n);
    return w;
}

std::string ToUtf8(const std::wstring& w)
{
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::string Trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r' || s[b-1] == '\n')) b--;
    return s.substr(a, b - a);
}

std::wstring GetAppDataDir()
{
    wchar_t buf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf)))
        return std::wstring(buf) + L"\\CapturePlus";
    return L".";
}

std::string Base64Encode(const unsigned char* data, size_t len)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3)
    {
        unsigned int v = (unsigned)data[i] << 16;
        if (i + 1 < len) v |= (unsigned)data[i+1] << 8;
        if (i + 2 < len) v |= (unsigned)data[i+2];
        out.push_back(tbl[(v >> 18) & 0x3F]);
        out.push_back(tbl[(v >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? tbl[(v >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? tbl[v & 0x3F] : '=');
    }
    return out;
}

bool SplitUrl(const std::string& url,
              std::string& host, std::string& path,
              bool& https, int& port)
{
    std::string u = Trim(url);
    https = false;
    port = 0;
    if (u.rfind("https://", 0) == 0) { https = true; u = u.substr(8); }
    else if (u.rfind("http://", 0) == 0) { https = false; u = u.substr(7); }
    else { https = true; }

    size_t slash = u.find('/');
    std::string hostpart = (slash == std::string::npos) ? u : u.substr(0, slash);
    path = (slash == std::string::npos) ? "/" : u.substr(slash);
    if (path.empty() || path[0] != '/') path = "/" + path;

    size_t colon = hostpart.find(':');
    if (colon != std::string::npos)
    {
        port = std::atoi(hostpart.c_str() + colon + 1);
        host = hostpart.substr(0, colon);
    }
    else
    {
        host = hostpart;
        port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    }
    if (host.empty()) return false;
    return true;
}

}
