#include "AiService.h"
#include "GdiUtil.h"
#include "Json.h"
#include "PromptBuilder.h"
#include "Logger.h"
#include "Util.h"

std::string AiService::analyze(HBITMAP bmp, const AppSettings& s)
{
    return sendVision(bmp, prompts::aiAnalysis(), s.api.visionModel, s);
}

std::string AiService::ocr(HBITMAP bmp, const AppSettings& s)
{
    return sendVision(bmp, prompts::aiOcr(), s.api.visionModel, s);
}

static bool looksLikeTranslationRefusal(const std::string& r)
{
    static const char* const kPhrases[] = {
        "无需翻译", "原文已是", "already in", "no translation", "already translated"
    };
    std::string lower;
    lower.reserve(r.size());
    for (char c : r)
        lower.push_back((c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c);
    for (const char* ph : kPhrases)
        if (lower.find(ph) != std::string::npos) return true;
    return false;
}

std::string AiService::translate(const std::string& text, const AppSettings& s)
{
    if (text.empty()) return "未识别到可翻译文字。";
    std::string r = sendText(prompts::translate(s.translateTargetLanguage, text), s.api.textModel, s);
    if (looksLikeTranslationRefusal(r)) return text;
    return r;
}

std::string AiService::translateDirect(HBITMAP bmp, const AppSettings& s)
{
    std::string prompt = "请将这张图片中的文字翻译为" + s.translateTargetLanguage
        + "，只输出译文，不要添加解释。如果图片中没有文字，请说明。";
    return sendVision(bmp, prompt, s.api.visionModel, s);
}

static json::Value chatRequest(const std::string& model, json::Value content)
{
    json::Value msg = json::Value::makeObject();
    msg.set("role", json::Value("user"));
    msg.set("content", std::move(content));

    json::Value messages = json::Value::makeArray();
    messages.push(std::move(msg));

    json::Value req = json::Value::makeObject();
    req.set("model", json::Value(model));
    req.set("messages", std::move(messages));
    return req;
}

std::string AiService::sendVision(HBITMAP bmp, const std::string& prompt,
                                  const std::string& model, const AppSettings& s)
{
    std::string b64 = bitmapToBase64Png(bmp);

    json::Value content = json::Value::makeArray();
    json::Value textPart = json::Value::makeObject();
    textPart.set("type", json::Value("text"));
    textPart.set("text", json::Value(prompt));
    json::Value imgPart = json::Value::makeObject();
    imgPart.set("type", json::Value("image_url"));
    json::Value imgUrl = json::Value::makeObject();
    imgUrl.set("url", json::Value("data:image/png;base64," + b64));
    imgPart.set("image_url", std::move(imgUrl));
    content.push(std::move(textPart));
    content.push(std::move(imgPart));

    return sendTextImpl(chatRequest(model, std::move(content)), s);
}

std::string AiService::sendText(const std::string& prompt, const std::string& model,
                                const AppSettings& s)
{
    return sendTextImpl(chatRequest(model, json::Value(prompt)), s);
}

std::string AiService::sendTextImpl(const json::Value& req, const AppSettings& s)
{
    std::string host, path;
    bool https;
    int port;
    if (!util::SplitUrl(s.api.baseUrl, host, path, https, port))
        throw std::runtime_error("无效的 BaseUrl");

    std::string body = json::write(req);

    std::string headers = "Content-Type: application/json\r\n";
    if (!s.api.apiKey.empty())
        headers += "Authorization: Bearer " + s.api.apiKey + "\r\n";

    std::string resp = httpPost(host, port, path + "/chat/completions", https, headers, body);

    json::Value root;
    if (!json::parse(resp, root))
        throw std::runtime_error("AI 返回非 JSON: " + resp.substr(0, 200));

    const json::Value* choices = root.find("choices");
    if (!choices || !choices->isArray() || choices->size() == 0)
        throw std::runtime_error("AI 返回无 choices");

    const json::Value* content = choices->at(0)->find("message");
    if (content) content = content->find("content");
    if (!content || !content->isString())
        throw std::runtime_error("AI 返回无 content");

    std::string out = content->asString();
    return util::Trim(out);
}

std::string AiService::bitmapToBase64Png(HBITMAP bmp)
{
    if (!bmp) return "";

    IStream* raw = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &raw)) || !raw)
        throw std::runtime_error("CreateStreamOnHGlobal failed");

    struct Guard
    {
        IStream* stream;
        HGLOBAL  mem = nullptr;
        bool     locked = false;
        ~Guard()
        {
            if (locked) GlobalUnlock(mem);
            if (stream) stream->Release();
        }
    } guard{ raw };

    CLSID pngClsid;
    if (gdiutil::GetEncoderClsid(L"image/png", &pngClsid) < 0)
        throw std::runtime_error("No PNG encoder");

    {
        Gdiplus::Bitmap gbmp(bmp, nullptr);
        if (gbmp.Save(guard.stream, &pngClsid, nullptr) != Gdiplus::Ok)
            throw std::runtime_error("Bitmap.Save failed");
    }

    if (FAILED(GetHGlobalFromStream(guard.stream, &guard.mem)) || !guard.mem)
        throw std::runtime_error("GetHGlobalFromStream failed");

    auto* ptr = (const unsigned char*)GlobalLock(guard.mem);
    if (!ptr) throw std::runtime_error("GlobalLock failed");
    guard.locked = true;

    return util::Base64Encode(ptr, GlobalSize(guard.mem));
}

std::string AiService::httpPost(const std::string& host, int port,
                               const std::string& path, bool https,
                               const std::string& headers,
                               const std::string& body)
{
    std::wstring whost = util::ToWide(host);
    std::wstring wpath = util::ToWide(path);
    std::wstring wheaders = util::ToWide(headers);

    HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;
    struct HandleGuard {
        HINTERNET &s, &c, &r;
        ~HandleGuard() { if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s); }
    } guard{ hSession, hConnect, hRequest };

    hSession = WinHttpOpen(L"CapturePlus/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) throw std::runtime_error("WinHttpOpen failed");

    WinHttpSetTimeouts(hSession, 10000, 10000, 10000, 30000);

    hConnect = WinHttpConnect(hSession, whost.c_str(),
        (port ? port : (https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT)), 0);
    if (!hConnect) throw std::runtime_error("WinHttpConnect failed");

    hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) throw std::runtime_error("WinHttpOpenRequest failed");

    const wchar_t* hdrs = wheaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wheaders.c_str();
    BOOL ok = WinHttpSendRequest(hRequest, hdrs, (DWORD)-1,
        (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!ok)
        throw std::runtime_error("WinHttpSendRequest failed err=" + std::to_string(GetLastError()));

    if (!WinHttpReceiveResponse(hRequest, nullptr))
        throw std::runtime_error("WinHttpReceiveResponse failed err=" + std::to_string(GetLastError()));

    DWORD statusCode = 0;
    DWORD sz = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz, WINHTTP_NO_HEADER_INDEX);

    std::string resp;
    DWORD avail = 0;
    do
    {
        avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
        if (avail == 0) break;
        size_t old = resp.size();
        resp.resize(old + avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, &resp[old], avail, &read))
            break;
        resp.resize(old + read);
    } while (avail > 0);

    if (statusCode < 200 || statusCode >= 300)
    {
        std::string snippet = resp.size() > 300 ? resp.substr(0, 300) : resp;
        logger::warn("AI HTTP " + std::to_string(statusCode) + ": " + snippet);
        std::string brief = resp.size() > 200 ? resp.substr(0, 200) : resp;
        throw std::runtime_error("AI 服务返回 " + std::to_string(statusCode) +
                                 (brief.empty() ? std::string() : (": " + brief)));
    }
    return resp;
}
