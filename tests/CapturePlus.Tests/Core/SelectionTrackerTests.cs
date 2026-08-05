using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class SelectionTrackerTests
{
    [Fact]
    public void Begin_SetsDragging_WithZeroRect()
    {
        var t = new SelectionTracker();
        t.Begin(100, 200);
        Assert.True(t.IsDragging);
        Assert.False(t.IsLocked);
        Assert.Equal(0, t.Rect.Width);
        Assert.Equal(0, t.Rect.Height);
    }

    [Fact]
    public void Update_BeforeBegin_Ignored()
    {
        var t = new SelectionTracker();
        t.Update(50, 50);
        Assert.False(t.IsDragging);
        Assert.Equal(0, t.Rect.Width);
    }

    [Fact]
    public void Begin_ThenUpdate_NormalizesRect()
    {
        var t = new SelectionTracker();
        t.Begin(100, 100);
        t.Update(50, 50);
        var r = t.Rect;
        Assert.Equal(50, r.X);
        Assert.Equal(50, r.Y);
        Assert.Equal(50, r.Width);
        Assert.Equal(50, r.Height);
    }

    [Fact]
    public void Update_AfterEndValid_Ignored()
    {
        var t = new SelectionTracker();
        t.Begin(0, 0);
        t.Update(100, 100);
        Assert.True(t.End());
        t.Update(200, 200);
        Assert.Equal(100, t.Rect.Width);
        Assert.True(t.IsLocked);
    }

    [Fact]
    public void End_TooSmall_Resets_ReturnsFalse()
    {
        var t = new SelectionTracker();
        t.Begin(0, 0);
        t.Update(5, 5);
        Assert.False(t.End());
        Assert.False(t.IsDragging);
        Assert.False(t.IsLocked);
        Assert.Equal(0, t.Rect.Width);
    }

    [Fact]
    public void End_Valid_Locks_ReturnsTrue()
    {
        var t = new SelectionTracker();
        t.Begin(0, 0);
        t.Update(100, 100);
        Assert.True(t.End());
        Assert.True(t.IsLocked);
        Assert.False(t.IsDragging);
        Assert.Equal(100, t.Rect.Width);
    }

    [Fact]
    public void Reset_ClearsState()
    {
        var t = new SelectionTracker();
        t.Begin(0, 0);
        t.Update(100, 100);
        t.End();
        t.Reset();
        Assert.False(t.IsDragging);
        Assert.False(t.IsLocked);
        Assert.Equal(0, t.Rect.Width);
    }
}
