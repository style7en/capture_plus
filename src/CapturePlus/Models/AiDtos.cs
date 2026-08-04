using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace CapturePlus.Models;

public sealed class ChatRequest
{
    [JsonPropertyName("model")] public string Model { get; set; } = "";
    [JsonPropertyName("messages")] public List<ChatMessage> Messages { get; set; } = new();
    [JsonPropertyName("max_tokens")] public int? MaxTokens { get; set; }
}

public sealed class ChatMessage
{
    [JsonPropertyName("role")] public string Role { get; set; } = "user";
    [JsonPropertyName("content")] public object Content { get; set; } = "";
}

public sealed class ContentPart
{
    [JsonPropertyName("type")] public string Type { get; set; } = "";
    [JsonPropertyName("text")] public string? Text { get; set; }
    [JsonPropertyName("image_url")] public ImageUrl? ImageUrl { get; set; }
}

public sealed class ImageUrl
{
    [JsonPropertyName("url")] public string Url { get; set; } = "";
}

public sealed class ChatResponse
{
    [JsonPropertyName("choices")] public List<Choice> Choices { get; set; } = new();
}

public sealed class Choice
{
    [JsonPropertyName("message")] public ChatMessageOut? Message { get; set; }
}

public sealed class ChatMessageOut
{
    [JsonPropertyName("content")] public string? Content { get; set; }
}
