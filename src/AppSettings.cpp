#include "AppSettings.h"
#include "Json.h"
#include "Logger.h"
#include "Util.h"

static std::wstring ProgramSettingsPath()
{
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring d(buf);
    size_t slash = d.find_last_of(L"\\/");
    if (slash != std::wstring::npos) d = d.substr(0, slash);
    return d + L"\\settings.json";
}

static std::wstring SettingsFilePath()
{
    return util::GetAppDataDir() + L"\\settings.json";
}

static bool readText(const std::wstring& path, std::string& out)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(h, nullptr);
    if (sz == INVALID_FILE_SIZE) { CloseHandle(h); return false; }
    out.resize(sz);
    DWORD rd = 0;
    ReadFile(h, &out[0], sz, &rd, nullptr);
    CloseHandle(h);
    return rd == sz;
}

static void writeText(const std::wstring& path, const std::string& content)
{
    CreateDirectoryW(util::GetAppDataDir().c_str(), nullptr);
    std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wr;
    WriteFile(h, content.data(), (DWORD)content.size(), &wr, nullptr);
    CloseHandle(h);
    MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

static json::Value mergeObjects(const json::Value& base, const json::Value& overlay)
{
    if (!base.isObject() || !overlay.isObject())
        return overlay;
    json::Value out = base;
    for (auto& kv : overlay.object())
    {
        const json::Value* existing = base.find(kv.first);
        if (existing && existing->isObject() && kv.second.isObject())
            out.set(kv.first, mergeObjects(*existing, kv.second));
        else
            out.set(kv.first, kv.second);
    }
    return out;
}

static AppSettings fromJson(const json::Value& v)
{
    AppSettings s, def;
    if (!v.isObject()) return s;
    if (auto p = v.find("Hotkey"))              if (p->isString()) s.hotkey = p->asString();
    if (auto p = v.find("TranslateTargetLanguage")) if (p->isString()) s.translateTargetLanguage = p->asString();
    if (auto api = v.find("Api"))
    {
        if (auto p = api->find("BaseUrl"))      if (p->isString()) s.api.baseUrl = p->asString();
        if (auto p = api->find("ApiKey"))       if (p->isString()) s.api.apiKey = p->asString();
        if (auto p = api->find("VisionModel"))  if (p->isString()) s.api.visionModel = p->asString();
        if (auto p = api->find("TextModel"))    if (p->isString()) s.api.textModel = p->asString();
    }
    if (s.hotkey.empty())                   s.hotkey = def.hotkey;
    if (s.api.baseUrl.empty())              s.api.baseUrl = def.api.baseUrl;
    if (s.api.visionModel.empty())          s.api.visionModel = def.api.visionModel;
    if (s.translateTargetLanguage.empty())  s.translateTargetLanguage = def.translateTargetLanguage;
    return s;
}

static json::Value toJson(const AppSettings& s)
{
    json::Value root = json::Value::makeObject();
    root.set("Hotkey", s.hotkey);
    json::Value api = json::Value::makeObject();
    api.set("BaseUrl", s.api.baseUrl);
    api.set("ApiKey", s.api.apiKey);
    api.set("VisionModel", s.api.visionModel);
    api.set("TextModel", s.api.textModel);
    root.set("Api", std::move(api));
    root.set("TranslateTargetLanguage", s.translateTargetLanguage);
    return root;
}

static const std::vector<std::wstring> kTranslateLanguages = {
    L"English", L"中文（简体）", L"中文（繁體）", L"日本語", L"한국어",
    L"Français", L"Deutsch", L"Español", L"Italiano", L"Português",
    L"Русский", L"العربية", L"ไทย", L"Tiếng Việt",
};

const std::vector<std::wstring>& TranslateLanguageList()
{
    return kTranslateLanguages;
}

AppSettings LoadSettings()
{
    try
    {
        std::string baseJson, userJson;
        bool hasBase = readText(ProgramSettingsPath(), baseJson);
        bool hasUser = readText(SettingsFilePath(), userJson);

        if (!hasBase && !hasUser)
        {
            AppSettings def;
            SaveSettings(def);
            return def;
        }

        json::Value base, user;
        if (hasBase) json::parse(baseJson, base);
        if (hasUser) json::parse(userJson, user);

        json::Value merged = (hasBase && hasUser) ? mergeObjects(base, user)
                              : (hasUser ? user : base);

        AppSettings s = fromJson(merged);
        if (!hasUser) SaveSettings(s);
        return s;
    }
    catch (...)
    {
        logger::warn("Settings load failed, using defaults");
        return AppSettings();
    }
}

void SaveSettings(const AppSettings& s)
{
    try
    {
        writeText(SettingsFilePath(), json::write(toJson(s), true));
    }
    catch (const std::exception& e)
    {
        logger::error(std::string("Settings save failed: ") + e.what());
    }
}
