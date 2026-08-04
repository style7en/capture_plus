using CapturePlus.Core;
using System;
using Xunit;

namespace CapturePlus.Tests.Core;

public class SaveFileNameGeneratorTests
{
    [Fact]
    public void Generate_UsesPrefixAndTimestamp()
    {
        var dt = new DateTime(2026, 8, 4, 14, 5, 9);
        var name = SaveFileNameGenerator.Generate(dt);
        Assert.Equal("CapturePlus_20260804_140509", name);
    }

    [Theory]
    [InlineData("png", "CapturePlus_20260804_140509.png")]
    [InlineData("jpg", "CapturePlus_20260804_140509.jpg")]
    [InlineData("bmp", "CapturePlus_20260804_140509.bmp")]
    public void WithExtension_Appends(string ext, string expected)
    {
        var dt = new DateTime(2026, 8, 4, 14, 5, 9);
        Assert.Equal(expected, SaveFileNameGenerator.WithExtension(dt, ext));
    }
}
