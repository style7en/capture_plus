using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class SelectionNormalizerTests
{
    [Theory]
    [InlineData(10, 20, 100, 50, 10, 20, 100, 50)]      // normal drag
    [InlineData(110, 70, -100, -50, 10, 20, 100, 50)]   // up-left drag
    [InlineData(10, 20, -30, -40, -20, -20, 30, 40)]    // negative origin ok
    [InlineData(5, 5, 0, 0, 5, 5, 0, 0)]                // zero size
    public void Normalize_ProducesPositiveSize(
        double x, double y, double w, double h,
        double ex, double ey, double ew, double eh)
    {
        var n = SelectionNormalizer.Normalize(x, y, w, h);
        Assert.Equal(ex, n.X);
        Assert.Equal(ey, n.Y);
        Assert.Equal(ew, n.Width);
        Assert.Equal(eh, n.Height);
    }

    [Fact]
    public void IsValid_TooSmall_ReturnsFalse()
    {
        Assert.False(SelectionNormalizer.IsValid(0, 0, 7, 100));
        Assert.False(SelectionNormalizer.IsValid(0, 0, 100, 7));
    }

    [Fact]
    public void IsValid_Boundary8_ReturnsTrue()
    {
        Assert.True(SelectionNormalizer.IsValid(0, 0, 8, 8));
    }
}
