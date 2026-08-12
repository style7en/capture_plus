#pragma once
#include "Pch.h"

struct ApiSettings
{
    std::string baseUrl      = "https://api.openai.com/v1";
    std::string apiKey;
    std::string visionModel  = "gpt-4o";
    std::string textModel    = "gpt-4o-mini";
};

struct AppSettings
{
    std::string hotkey               = "Ctrl+Alt+A";
    ApiSettings  api;
    std::string translateTargetLanguage = "中文（简体）";
};

AppSettings LoadSettings();
void        SaveSettings(const AppSettings& s);
