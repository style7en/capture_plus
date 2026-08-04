using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class OcrTextJoinerTests
{
    [Fact]
    public void Null_ReturnsEmpty()
    {
        Assert.Equal("", OcrTextJoiner.Join(null!));
    }

    [Fact]
    public void Empty_ReturnsEmpty()
    {
        Assert.Equal("", OcrTextJoiner.Join(Array.Empty<string>()));
    }

    [Fact]
    public void SingleLine_ReturnsAsIs()
    {
        Assert.Equal("hello", OcrTextJoiner.Join(new[] { "hello" }));
    }

    [Fact]
    public void MultipleLines_JoinedByNewline()
    {
        Assert.Equal("line1\nline2\nline3",
            OcrTextJoiner.Join(new[] { "line1", "line2", "line3" }));
    }

    [Fact]
    public void TrimsEachLine_AndSkipsEmpty()
    {
        Assert.Equal("a\nb",
            OcrTextJoiner.Join(new[] { " a ", "", "b", "   " }));
    }
}
