using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class SettingsOverlayTests
{
    [Fact]
    public void BothNull_ReturnsNull()
    {
        Assert.Null(SettingsOverlay.MergeJson(null, null));
    }

    [Fact]
    public void BaseOnly_ReturnsBase()
    {
        var baseJson = """{"Hotkey":"Ctrl+Alt+A"}""";
        var s = AppSettings.FromJson(SettingsOverlay.MergeJson(baseJson, null)!);
        Assert.Equal("Ctrl+Alt+A", s.Hotkey);
    }

    [Fact]
    public void OverlayOnly_ReturnsOverlay()
    {
        var overlayJson = """{"Hotkey":"Alt+A"}""";
        var s = AppSettings.FromJson(SettingsOverlay.MergeJson(null, overlayJson)!);
        Assert.Equal("Alt+A", s.Hotkey);
    }

    [Fact]
    public void OverlayOverridesScalar()
    {
        var merged = SettingsOverlay.MergeJson("""{"Hotkey":"Ctrl+Alt+A"}""", """{"Hotkey":"Alt+A"}""");
        var s = AppSettings.FromJson(merged!);
        Assert.Equal("Alt+A", s.Hotkey);
    }

    [Fact]
    public void OverlayKeepsBaseFieldsNotPresentInOverlay()
    {
        var baseJson = """{"Api":{"BaseUrl":"https://a","ApiKey":"secret","TextModel":"t1"}}""";
        var overlayJson = """{"Api":{"TextModel":"t2"}}""";
        var s = AppSettings.FromJson(SettingsOverlay.MergeJson(baseJson, overlayJson)!);
        Assert.Equal("https://a", s.Api.BaseUrl);
        Assert.Equal("secret", s.Api.ApiKey);
        Assert.Equal("t2", s.Api.TextModel);
    }

    [Fact]
    public void OverlayPartialFields_FillRestFromDefaults()
    {
        var baseJson = """{"Api":{"BaseUrl":"https://a","VisionModel":"v1"}}""";
        var merged = SettingsOverlay.MergeJson(baseJson, """{"Hotkey":"Ctrl+Shift+9"}""");
        var s = AppSettings.FromJson(merged!);
        Assert.Equal("Ctrl+Shift+9", s.Hotkey);
        Assert.Equal("https://a", s.Api.BaseUrl);
        Assert.Equal("v1", s.Api.VisionModel);
        Assert.Equal("gpt-4o-mini", s.Api.TextModel);
    }

    [Fact]
    public void EmptyOverlayObject_KeepsBaseIntact()
    {
        var baseJson = """{"Hotkey":"Alt+A","SaveDir":"C:\\x"}""";
        var merged = SettingsOverlay.MergeJson(baseJson, "{}");
        var s = AppSettings.FromJson(merged!);
        Assert.Equal("Alt+A", s.Hotkey);
        Assert.Equal(@"C:\x", s.SaveDir);
    }
}