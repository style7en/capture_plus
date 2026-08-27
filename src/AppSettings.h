#pragma once
#include "Pch.h"

struct ApiSettings
{
    std::string baseUrl      = "https://api.openai.com/v1";
    std::string apiKey;
    std::string visionModel  = "minimax";
    std::string textModel    = "deepseek";
};

struct AppSettings
{
    std::string hotkey               = "Alt+A";
    ApiSettings  api;
    std::string translateTargetLanguage = "中文（简体）";
};

AppSettings LoadSettings();
void        SaveSettings(const AppSettings& s);

const std::vector<std::wstring>& TranslateLanguageList();
