using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class ToolbarPlacementTests
{
    // screen 0..1920 x, 0..1080 y. toolbar 300x40, margin 8.
    [Fact]
    public void Place_CenteredAbove_WhenFits()
    {
        var p = ToolbarPlacement.Place(
            selX: 100, selY: 100, selW: 200, selH: 150,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        Assert.Equal(50, p.X);    // 100 + (200-300)/2
        Assert.Equal(52, p.Y);    // 100 - 8 - 40
    }

    [Fact]
    public void Place_FlipsBelow_WhenTopOverflows()
    {
        var p = ToolbarPlacement.Place(
            selX: 100, selY: 10, selW: 200, selH: 150,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        // y = 10 - 8 - 40 = -38 < 8 → flip below: 10 + 150 + 8
        Assert.Equal(50, p.X);
        Assert.Equal(168, p.Y);   // 10 + 150 + 8
    }

    [Fact]
    public void Place_ClampsLeft_WhenCenteredOverflowsLeft()
    {
        var p = ToolbarPlacement.Place(
            selX: 50, selY: 100, selW: 200, selH: 150,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        // x = 50 + (200-300)/2 = 0 < 8 → clamp to 8
        Assert.Equal(8, p.X);
        Assert.Equal(52, p.Y);
    }

    [Fact]
    public void Place_ClampsRight_WhenCenteredOverflowsRight()
    {
        var p = ToolbarPlacement.Place(
            selX: 1700, selY: 100, selW: 200, selH: 150,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        // x = 1700 + (200-300)/2 = 1650, 1650+300=1950 > 1920-8=1912 → clamp
        Assert.Equal(1612, p.X);  // 1920 - 8 - 300
        Assert.Equal(52, p.Y);
    }
}
