using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class ToolbarPlacementTests
{
    // screen 0..1920 x, 0..1080 y. selection, toolbar 300x40, margin 8.
    [Fact]
    public void Place_BelowRight_WhenFits()
    {
        var p = ToolbarPlacement.Place(
            selX: 100, selY: 100, selW: 200, selH: 150,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        Assert.Equal(308, p.X);   // selX + selW + 8
        Assert.Equal(258, p.Y);   // selY + selH + 8
    }

    [Fact]
    public void Place_FlipsAbove_WhenBottomOverflows()
    {
        // selection bottom + toolbar > screen bottom
        var p = ToolbarPlacement.Place(
            selX: 100, selY: 1050, selW: 200, selH: 20,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        // below would be 1050+20+8=1078+40=1118 > 1080 → flip above: selY - 8 - 40
        // X fits to the right (308+300=608 < 1920), so X stays selX+selW+margin
        Assert.Equal(308, p.X);
        Assert.Equal(1002, p.Y);  // 1050 - 8 - 40
    }

    [Fact]
    public void Place_FlipsLeft_WhenRightOverflows()
    {
        var p = ToolbarPlacement.Place(
            selX: 1700, selY: 100, selW: 200, selH: 150,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        // right would be 1700+200+8=1908+300=2208 > 1920 → left: selX - 8 - 300
        Assert.Equal(1392, p.X);  // 1700 - 8 - 300
        Assert.Equal(258, p.Y);
    }

    [Fact]
    public void Place_Clamps_WhenBothOverflow()
    {
        var p = ToolbarPlacement.Place(
            selX: 1700, selY: 1050, selW: 200, selH: 20,
            screenLeft: 0, screenTop: 0, screenW: 1920, screenH: 1080,
            toolbarW: 300, toolbarH: 40, margin: 8);
        Assert.Equal(1392, p.X);   // flipped left
        Assert.Equal(1002, p.Y);   // flipped above
    }
}
