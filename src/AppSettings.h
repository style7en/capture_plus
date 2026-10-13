#pragma once
#include "Pch.h"

struct ApiSettings
{
    std::string baseUrl      = "https://api.openai.com/v1";
    std::string apiKey;
    std::string visionModel  = "minimax";
    std::string textModel    = "deepseek";
    int         receiveTimeoutSec = 60;
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

// 归一化 AI 服务接收超时（秒）：非正值回落默认值，并限制在 1s~600s。
int NormalizeReceiveTimeoutSec(int sec);
