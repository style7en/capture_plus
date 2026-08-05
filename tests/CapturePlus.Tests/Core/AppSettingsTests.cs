using CapturePlus.Core;
using System.IO;
using Xunit;

namespace CapturePlus.Tests.Core;

public class AppSettingsTests
{
    [Fact]
    public void Defaults_HaveExpectedValues()
    {
        var s = AppSettings.Default;
        Assert.Equal("Ctrl+Alt+A", s.Hotkey);
        Assert.Equal("https://api.openai.com/v1", s.Api.BaseUrl);
        Assert.Equal("", s.Api.ApiKey);
        Assert.Equal("gpt-4o", s.Api.VisionModel);
        Assert.Equal("gpt-4o-mini", s.Api.TextModel);
        Assert.Equal("中文（简体）", s.TranslateTargetLanguage);
        Assert.Equal("", s.SaveDir);
    }

    [Fact]
    public void RoundTrip_PreservesAllFields()
    {
        var s = AppSettings.Default;
        s.Api.ApiKey = "sk-test-123";
        s.Api.BaseUrl = "https://api.deepseek.com/v1";
        s.Api.VisionModel = "deepseek-chat";
        s.TranslateTargetLanguage = "English";
        s.SaveDir = @"D:\Shots";

        var json = AppSettings.ToJson(s);
        var back = AppSettings.FromJson(json);

        Assert.Equal("sk-test-123", back.Api.ApiKey);
        Assert.Equal("https://api.deepseek.com/v1", back.Api.BaseUrl);
        Assert.Equal("deepseek-chat", back.Api.VisionModel);
        Assert.Equal("English", back.TranslateTargetLanguage);
        Assert.Equal(@"D:\Shots", back.SaveDir);
    }

    [Fact]
    public void FromJson_CorruptString_ReturnsDefaults()
    {
        var back = AppSettings.FromJson("not valid json {{{");
        Assert.Equal(AppSettings.Default.Api.BaseUrl, back.Api.BaseUrl);
        Assert.Equal("", back.Api.ApiKey);
    }

    [Fact]
    public void FromJson_MissingFields_FillsDefaults()
    {
        var json = """{"Api":{"ApiKey":"sk-x"}}""";
        var back = AppSettings.FromJson(json);
        Assert.Equal("sk-x", back.Api.ApiKey);
        Assert.Equal("https://api.openai.com/v1", back.Api.BaseUrl);
        Assert.Equal("Ctrl+Alt+A", back.Hotkey);
    }
}
