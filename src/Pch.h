#pragma once

#define _WIN32_WINNT 0x0601
#define WINVER       0x0601
#define _WIN32_IE    0x0600

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <winhttp.h>

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <utility>
