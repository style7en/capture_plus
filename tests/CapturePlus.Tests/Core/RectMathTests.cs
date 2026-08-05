using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class RectMathTests
{
    [Fact]
    public void Contains_Inside_ReturnsTrue()
    {
        var r = new NormRect(10, 20, 100, 50);
        Assert.True(RectMath.Contains(r, 50, 40));
    }

    [Fact]
    public void Contains_TopLeftCorner_ReturnsTrue()
    {
        var r = new NormRect(10, 20, 100, 50);
        Assert.True(RectMath.Contains(r, 10, 20));
    }

    [Fact]
    public void Contains_RightEdgeExclusive_ReturnsFalse()
    {
        var r = new NormRect(10, 20, 100, 50);
        Assert.False(RectMath.Contains(r, 110, 40));
    }

    [Fact]
    public void Contains_Outside_ReturnsFalse()
    {
        var r = new NormRect(10, 20, 100, 50);
        Assert.False(RectMath.Contains(r, 9, 40));
        Assert.False(RectMath.Contains(r, 50, 70));
    }

    [Fact]
    public void Intersect_Overlap_ReturnsOverlap()
    {
        var a = new NormRect(0, 0, 100, 100);
        var b = new NormRect(50, 50, 100, 100);
        var r = RectMath.Intersect(a, b);
        Assert.Equal(50, r.X);
        Assert.Equal(50, r.Y);
        Assert.Equal(50, r.Width);
        Assert.Equal(50, r.Height);
    }

    [Fact]
    public void Intersect_Contained_ReturnsInner()
    {
        var a = new NormRect(0, 0, 200, 200);
        var b = new NormRect(50, 50, 100, 100);
        Assert.Equal(b, RectMath.Intersect(a, b));
    }

    [Fact]
    public void Intersect_Disjoint_ReturnsEmpty()
    {
        var a = new NormRect(0, 0, 100, 100);
        var b = new NormRect(200, 200, 50, 50);
        var r = RectMath.Intersect(a, b);
        Assert.Equal(0, r.Width);
        Assert.Equal(0, r.Height);
    }

    [Fact]
    public void Intersects_Disjoint_ReturnsFalse()
    {
        Assert.False(RectMath.Intersects(new NormRect(0, 0, 100, 100), new NormRect(200, 200, 50, 50)));
    }
}
