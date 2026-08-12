#pragma once
#include "Pch.h"

namespace util {

std::wstring ToWide(const std::string& utf8);
std::string  ToUtf8(const std::wstring& w);
std::string  Trim(const std::string& s);
std::wstring GetAppDataDir();
std::string  Base64Encode(const unsigned char* data, size_t len);

bool SplitUrl(const std::string& url,
              std::string& host,
              std::string& path,
              bool& https,
              int& port);

}
