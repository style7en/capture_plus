#pragma once
#include "Pch.h"
#include "AppSettings.h"
#include "Json.h"

class AiService
{
public:
    std::string analyze(HBITMAP bmp, const AppSettings& s);
    std::string ocr(HBITMAP bmp, const AppSettings& s);
    std::string translate(const std::string& text, const AppSettings& s);

private:
    std::string sendVision(HBITMAP bmp, const std::string& prompt,
                           const std::string& model, const AppSettings& s);
    std::string sendText(const std::string& prompt, const std::string& model,
                         const AppSettings& s);
    std::string sendTextImpl(const json::Value& req, const AppSettings& s);

    static std::string bitmapToBase64Png(HBITMAP bmp);
    static std::string httpPost(const std::string& host, int port,
                                const std::string& path, bool https,
                                const std::string& headers,
                                const std::string& body);
};
