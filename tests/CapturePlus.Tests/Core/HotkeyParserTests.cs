using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class HotkeyParserTests
{
    [Theory]
    [InlineData("Ctrl+Alt+A", HotkeyParser.ModControl | HotkeyParser.ModAlt, 0x41)]
    [InlineData("Ctrl+Shift+F12", HotkeyParser.ModControl | HotkeyParser.ModShift, 0x7B)]
    [InlineData("Win+Space", HotkeyParser.ModWin, 0x20)]
    [InlineData("Alt+5", HotkeyParser.ModAlt, 0x35)]
    [InlineData("Ctrl+Alt+Shift+Enter", HotkeyParser.ModControl | HotkeyParser.ModAlt | HotkeyParser.ModShift, 0x0D)]
    public void TryParse_ValidCombos(string text, uint expectedMods, uint expectedVk)
    {
        Assert.True(HotkeyParser.TryParse(text, out uint mods, out uint vk));
        Assert.Equal(expectedMods, mods);
        Assert.Equal(expectedVk, vk);
    }

    [Theory]
    [InlineData("ctrl+alt+a")]
    [InlineData(" Ctrl + Alt + A ")]
    public void TryParse_CaseInsensitiveAndTrims(string text)
    {
        Assert.True(HotkeyParser.TryParse(text, out uint mods, out uint vk));
        Assert.Equal(HotkeyParser.ModControl | HotkeyParser.ModAlt, mods);
        Assert.Equal(0x41u, vk);
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData("   ")]
    [InlineData("A")]
    [InlineData("Ctrl+")]
    [InlineData("Ctrl+X+Y")]
    [InlineData("Foo+A")]
    [InlineData("Ctrl+Unknown")]
    public void TryParse_Invalid_ReturnsFalse(string? text)
    {
        Assert.False(HotkeyParser.TryParse(text, out _, out _));
    }

    [Fact]
    public void Format_RendersCanonicalOrder()
    {
        Assert.Equal("Ctrl+Alt+A", HotkeyParser.Format(HotkeyParser.ModControl | HotkeyParser.ModAlt, 0x41));
        Assert.Equal("Win+F5", HotkeyParser.Format(HotkeyParser.ModWin, 0x74));
    }

    [Theory]
    [InlineData(HotkeyParser.ModControl | HotkeyParser.ModAlt, 0x41)]
    [InlineData(HotkeyParser.ModShift, 0x70)]
    public void Format_RoundTripsThroughParse(uint mods, uint vk)
    {
        var text = HotkeyParser.Format(mods, vk);
        Assert.True(HotkeyParser.TryParse(text, out uint m2, out uint v2));
        Assert.Equal(mods, m2);
        Assert.Equal(vk, v2);
    }

    [Theory]
    [InlineData(0x41, true)]
    [InlineData(0x7B, true)]
    [InlineData(0x20, true)]
    [InlineData(0x100, false)]
    public void IsSupported(uint vk, bool expected)
    {
        Assert.Equal(expected, HotkeyParser.IsSupported(vk));
    }
}
