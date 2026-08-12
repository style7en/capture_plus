#include "HotkeyManager.h"
#include "Logger.h"
#include "Util.h"

struct ModEntry { const char* name; UINT flag; };
static const ModEntry g_mods[] = {
    { "Ctrl",  MOD_CONTROL },
    { "Alt",   MOD_ALT },
    { "Shift", MOD_SHIFT },
    { "Win",   MOD_WIN },
};

static const struct { const char* name; UINT vk; } g_keys[] = {
    { "A", 0x41 }, { "B", 0x42 }, { "C", 0x43 }, { "D", 0x44 }, { "E", 0x45 },
    { "F", 0x46 }, { "G", 0x47 }, { "H", 0x48 }, { "I", 0x49 }, { "J", 0x4A },
    { "K", 0x4B }, { "L", 0x4C }, { "M", 0x4D }, { "N", 0x4E }, { "O", 0x4F },
    { "P", 0x50 }, { "Q", 0x51 }, { "R", 0x52 }, { "S", 0x53 }, { "T", 0x54 },
    { "U", 0x55 }, { "V", 0x56 }, { "W", 0x57 }, { "X", 0x58 }, { "Y", 0x59 },
    { "Z", 0x5A },
    { "0", 0x30 }, { "1", 0x31 }, { "2", 0x32 }, { "3", 0x33 }, { "4", 0x34 },
    { "5", 0x35 }, { "6", 0x36 }, { "7", 0x37 }, { "8", 0x38 }, { "9", 0x39 },
    { "F1", 0x70 }, { "F2", 0x71 }, { "F3", 0x72 }, { "F4", 0x73 },
    { "F5", 0x74 }, { "F6", 0x75 }, { "F7", 0x76 }, { "F8", 0x77 },
    { "F9", 0x78 }, { "F10", 0x79 }, { "F11", 0x7A }, { "F12", 0x7B },
    { "Space", 0x20 }, { "Enter", 0x0D }, { "Esc", 0x1B }, { "Tab", 0x09 },
    { "Backspace", 0x08 }, { "Delete", 0x2E }, { "Home", 0x24 }, { "End", 0x23 },
    { "PageUp", 0x21 }, { "PageDown", 0x22 }, { "Left", 0x25 }, { "Right", 0x27 },
    { "Up", 0x26 }, { "Down", 0x28 },
};

static bool findKeyByName(const std::string& s, UINT& vk)
{
    for (auto& k : g_keys)
        if (_stricmp(s.c_str(), k.name) == 0) { vk = k.vk; return true; }
    return false;
}

static bool findModByName(const std::string& s, UINT& mod)
{
    for (auto& m : g_mods)
        if (_stricmp(s.c_str(), m.name) == 0) { mod = m.flag; return true; }
    return false;
}

bool HotkeyParse(const std::string& text, UINT& mods, UINT& vk)
{
    mods = 0; vk = 0;
    std::string s = util::Trim(text);
    if (s.empty()) return false;

    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); i++)
    {
        if (i == s.size() || s[i] == '+')
        {
            std::string part = util::Trim(s.substr(start, i - start));
            if (!part.empty()) parts.push_back(part);
            start = i + 1;
        }
    }
    if (parts.size() < 2) return false;

    for (size_t i = 0; i + 1 < parts.size(); i++)
    {
        UINT m;
        if (!findModByName(parts[i], m)) return false;
        mods |= m;
    }
    if (mods == 0) return false;
    return findKeyByName(parts.back(), vk);
}

std::string HotkeyFormat(UINT mods, UINT vk)
{
    std::string out;
    for (auto& m : g_mods)
        if (mods & m.flag) { if (!out.empty()) out += "+"; out += m.name; }
    for (auto& k : g_keys)
        if (vk == k.vk) { if (!out.empty()) out += "+"; out += k.name; break; }
    return out;
}

bool HotkeyFromWParam(WPARAM wParam, UINT& mods, UINT& vk)
{
    vk = (UINT)wParam;
    mods = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
    if (GetKeyState(VK_MENU)    & 0x8000) mods |= MOD_ALT;
    if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= MOD_SHIFT;
    if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) mods |= MOD_WIN;
    if (mods == 0) return false;
    if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN || vk == VK_RWIN)
        return false;
    for (auto& k : g_keys) if (vk == k.vk) return true;
    return false;
}

static const wchar_t* KC_HOTKEY = L"CapturePlus_HotkeyWnd";
static const int HOTKEY_ID = 0x9001;

HotkeyManager::HotkeyManager()
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = &HotkeyManager::WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = KC_HOTKEY;
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(0, KC_HOTKEY, L"", 0, 0, 0, 0, 0,
                            HWND_MESSAGE, nullptr, wc.hInstance, this);
    if (hwnd_) SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
}

HotkeyManager::~HotkeyManager()
{
    unregister();
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
}

bool HotkeyManager::registerHotkey(const std::string& hotkey)
{
    UINT mods, vk;
    if (!HotkeyParse(hotkey, mods, vk)) { logger::warn("Invalid hotkey: " + hotkey); return false; }
    registered_ = RegisterHotKey(hwnd_, HOTKEY_ID, mods, vk);
    if (registered_) { mods_ = mods; vk_ = vk; }
    else logger::warn("RegisterHotKey failed");
    return registered_;
}

bool HotkeyManager::reRegister(const std::string& hotkey)
{
    UINT mods, vk;
    if (!HotkeyParse(hotkey, mods, vk)) return false;
    if (registered_) UnregisterHotKey(hwnd_, HOTKEY_ID);
    registered_ = RegisterHotKey(hwnd_, HOTKEY_ID, mods, vk);
    if (registered_) { mods_ = mods; vk_ = vk; return true; }
    registered_ = (mods_ || vk_) && RegisterHotKey(hwnd_, HOTKEY_ID, mods_, vk_);
    return false;
}

void HotkeyManager::unregister()
{
    if (registered_) { UnregisterHotKey(hwnd_, HOTKEY_ID); registered_ = false; }
}

LRESULT CALLBACK HotkeyManager::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_HOTKEY && (int)wp == HOTKEY_ID)
    {
        auto* self = (HotkeyManager*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (self && self->cb_) self->cb_();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
