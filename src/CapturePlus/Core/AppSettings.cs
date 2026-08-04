using System.Text.Json;
using System.Text.Json.Serialization;

namespace CapturePlus.Core;

public sealed class AppSettings
{
    public string Hotkey { get; set; } = "Ctrl+Alt+A";
    public ApiSettings Api { get; set; } = new();
    public string TranslateTargetLanguage { get; set; } = "中文（简体）";
    public string OcrLanguage { get; set; } = "zh-CN";
    public string SaveDir { get; set; } = "";

    public static AppSettings Default => new();

    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public static string ToJson(AppSettings s)
        => JsonSerializer.Serialize(s, Options);

    public static AppSettings FromJson(string json)
    {
        try
        {
            var parsed = JsonSerializer.Deserialize<AppSettings>(json, Options);
            if (parsed is null) return Default;
            parsed.Api ??= new ApiSettings();
            if (string.IsNullOrEmpty(parsed.Hotkey)) parsed.Hotkey = Default.Hotkey;
            if (string.IsNullOrEmpty(parsed.Api.BaseUrl)) parsed.Api.BaseUrl = Default.Api.BaseUrl;
            if (parsed.Api.VisionModel is null) parsed.Api.VisionModel = Default.Api.VisionModel;
            if (parsed.Api.TextModel is null) parsed.Api.TextModel = Default.Api.TextModel;
            if (parsed.TranslateTargetLanguage is null) parsed.TranslateTargetLanguage = Default.TranslateTargetLanguage;
            if (parsed.OcrLanguage is null) parsed.OcrLanguage = Default.OcrLanguage;
            if (parsed.SaveDir is null) parsed.SaveDir = "";
            return parsed;
        }
        catch
        {
            return Default;
        }
    }
}

public sealed class ApiSettings
{
    public string BaseUrl { get; set; } = "https://api.openai.com/v1";
    public string ApiKey { get; set; } = "";
    public string VisionModel { get; set; } = "gpt-4o";
    public string TextModel { get; set; } = "gpt-4o-mini";
}
