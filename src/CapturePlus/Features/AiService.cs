using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using CapturePlus.Core;
using CapturePlus.Logging;
using CapturePlus.Models;

namespace CapturePlus.Features;

public sealed class AiService
{
    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(60) };

    public async Task<string> AnalyzeAsync(Bitmap image, AppSettings settings, CancellationToken ct)
        => await SendVisionAsync(image, PromptBuilder.AiAnalysis(), settings.Api.VisionModel, settings, ct);

    public async Task<string> AiOcrAsync(Bitmap image, AppSettings settings, CancellationToken ct)
        => await SendVisionAsync(image, PromptBuilder.AiOcr(), settings.Api.VisionModel, settings, ct);

    public async Task<string> TranslateAsync(string text, AppSettings settings, CancellationToken ct)
    {
        if (string.IsNullOrWhiteSpace(text))
            return "未识别到可翻译文字。";

        var msg = new ChatMessage
        {
            Role = "user",
            Content = PromptBuilder.Translate(settings.TranslateTargetLanguage, text)
        };
        var req = new ChatRequest { Model = settings.Api.TextModel, Messages = new() { msg } };
        return await SendAsync(req, settings, ct);
    }

    private async Task<string> SendVisionAsync(Bitmap image, string prompt, string model, AppSettings settings, CancellationToken ct)
    {
        var base64 = ToBase64Png(image);
        var req = new ChatRequest
        {
            Model = model,
            Messages = new()
            {
                new ChatMessage
                {
                    Role = "user",
                    Content = new List<ContentPart>
                    {
                        new() { Type = "text", Text = prompt },
                        new() { Type = "image_url", ImageUrl = new() { Url = $"data:image/png;base64,{base64}" } }
                    }
                }
            }
        };
        return await SendAsync(req, settings, ct);
    }

    private static async Task<string> SendAsync(ChatRequest req, AppSettings settings, CancellationToken ct)
    {
        var url = settings.Api.BaseUrl.TrimEnd('/') + "/chat/completions";
        using var msg = new HttpRequestMessage(HttpMethod.Post, url);
        msg.Headers.Authorization = new AuthenticationHeaderValue("Bearer", settings.Api.ApiKey);
        msg.Content = new StringContent(JsonSerializer.Serialize(req), Encoding.UTF8, "application/json");

        using var resp = await Http.SendAsync(msg, ct);
        var body = await resp.Content.ReadAsStringAsync(ct);
        if (!resp.IsSuccessStatusCode)
        {
            Logger.Warn($"AI HTTP {(int)resp.StatusCode}: {Truncate(body, 300)}");
            throw new Exception($"AI 服务返回 {(int)resp.StatusCode}");
        }

        var parsed = JsonSerializer.Deserialize<ChatResponse>(body);
        var content = parsed?.Choices?.FirstOrDefault()?.Message?.Content;
        return content?.Trim() ?? "";
    }

    private static string ToBase64Png(Bitmap bmp)
    {
        using var ms = new MemoryStream();
        bmp.Save(ms, ImageFormat.Png);
        return Convert.ToBase64String(ms.ToArray());
    }

    private static string Truncate(string s, int n)
        => s.Length <= n ? s : s[..n];
}
