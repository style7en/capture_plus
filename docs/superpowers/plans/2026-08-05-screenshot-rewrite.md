# Screenshot 模块重写实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重写 `Screenshot/` 模块：以物理虚拟屏幕坐标为唯一基准实现跨屏选区，集中选区状态、修复并发守卫、改为真异步、统一焦点/Esc 处理。功能与公开 API（`ScreenshotSession.StartAsync`、`ScreenshotAction`）不变。

**Architecture:** 每屏仍建一个 `OverlayWindow`（混合 DPI 下唯一可靠显示方案），但选区状态集中在 `ScreenshotSession` 持有的 `SelectionTracker`（物理虚拟坐标）；遮罩窗只上报换算后的物理坐标、按「选区∩本屏」绘制遮罩。裁剪直接从整张虚拟屏位图取。新增 `Core/RectMath` 与 `Core/SelectionTracker` 纯逻辑并配单元测试。

**Tech Stack:** C# .NET 8（WPF + WinForms），xUnit 纯逻辑测试，System.Drawing 截图。

**Spec:** `docs/superpowers/specs/2026-08-05-screenshot-rewrite-design.md`

**构建环境注意：** 本机仅装 .NET Runtime 无 SDK，`dotnet build`/`dotnet test` 需在装有 .NET 8 SDK 的机器上执行。每个任务都有明确的验证命令与预期输出。

---

## File Structure

```
src/CapturePlus/
├─ Core/
│   ├─ RectMath.cs                 新增：矩形求交/包含（纯逻辑）
│   └─ SelectionTracker.cs         新增：选区状态机（物理虚拟坐标，纯逻辑）
├─ Screenshot/
│   ├─ ScreenCapturer.cs           改：CaptureVirtualScreen 返回 VirtualCapture（含隐式转 Bitmap，保证中间态可编译）
│   ├─ DpiHelper.cs                增：GetDpiScaleForWindow(hwnd)
│   ├─ OverlayWindow.xaml          改：PreviewKeyDown + ImageBrush Stretch=Fill
│   ├─ OverlayWindow.xaml.cs       重写：输入上报 + 遮罩绘制 + 工具条，删除本地选区逻辑
│   └─ ScreenshotSession.cs        重写：选区状态机、整张位图、动作分发、_busy 守卫
tests/CapturePlus.Tests/Core/
├─ RectMathTests.cs                新增
└─ SelectionTrackerTests.cs        新增
```

任务顺序保证每次提交后项目可编译：
- Task 1-2 只加 `Core/` 纯逻辑与测试，不触碰现有调用方。
- Task 3 用「返回 `VirtualCapture` + 隐式转 `Bitmap`」保持旧 `ScreenshotSession` 可编译。
- Task 4 只加方法。
- Task 5-6 成对重写 OverlayWindow 与 ScreenshotSession（构造函数与事件签名一起改）。
- Task 7 全量验证。

---

### Task 1: Core/RectMath — TDD

**Files:**
- Create: `src/CapturePlus/Core/RectMath.cs`
- Test: `tests/CapturePlus.Tests/Core/RectMathTests.cs`

- [ ] **Step 1: Write the failing test**

Create `tests/CapturePlus.Tests/Core/RectMathTests.cs`:

```csharp
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `dotnet test tests/CapturePlus.Tests/CapturePlus.Tests.csproj`
Expected: FAIL — `RectMath` type not found (CS0103 / compile error).

- [ ] **Step 3: Implement RectMath**

Create `src/CapturePlus/Core/RectMath.cs`:

```csharp
using System;

namespace CapturePlus.Core;

public static class RectMath
{
    public static bool Contains(NormRect r, double x, double y)
        => x >= r.X && y >= r.Y && x < r.X + r.Width && y < r.Y + r.Height;

    public static bool Intersects(NormRect a, NormRect b)
        => a.X < b.X + b.Width && b.X < a.X + a.Width
           && a.Y < b.Y + b.Height && b.Y < a.Y + a.Height;

    public static NormRect Intersect(NormRect a, NormRect b)
    {
        double x1 = Math.Max(a.X, b.X);
        double y1 = Math.Max(a.Y, b.Y);
        double x2 = Math.Min(a.X + a.Width, b.X + b.Width);
        double y2 = Math.Min(a.Y + a.Height, b.Y + b.Height);
        return new NormRect(x1, y1, Math.Max(0, x2 - x1), Math.Max(0, y2 - y1));
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `dotnet test tests/CapturePlus.Tests/CapturePlus.Tests.csproj --filter RectMathTests`
Expected: 8 passing.

- [ ] **Step 5: Commit**

```bash
git add src/CapturePlus/Core/RectMath.cs tests/CapturePlus.Tests/Core/RectMathTests.cs
git commit -m "feat: RectMath helpers (intersect/contains)"
```

---

### Task 2: Core/SelectionTracker — TDD

**Files:**
- Create: `src/CapturePlus/Core/SelectionTracker.cs`
- Test: `tests/CapturePlus.Tests/Core/SelectionTrackerTests.cs`

- [ ] **Step 1: Write the failing test**

Create `tests/CapturePlus.Tests/Core/SelectionTrackerTests.cs`:

```csharp
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `dotnet test tests/CapturePlus.Tests/CapturePlus.Tests.csproj`
Expected: FAIL — `SelectionTracker` type not found.

- [ ] **Step 3: Implement SelectionTracker**

Create `src/CapturePlus/Core/SelectionTracker.cs`:

```csharp
using System;

namespace CapturePlus.Core;

public sealed class SelectionTracker
{
    private double _startX, _startY;

    public NormRect Rect { get; private set; }
    public bool IsDragging { get; private set; }
    public bool IsLocked { get; private set; }

    public void Begin(double x, double y)
    {
        if (IsLocked) return;
        _startX = x;
        _startY = y;
        Rect = new NormRect(x, y, 0, 0);
        IsDragging = true;
    }

    public void Update(double x, double y)
    {
        if (!IsDragging || IsLocked) return;
        Rect = SelectionNormalizer.Normalize(_startX, _startY, x - _startX, y - _startY);
    }

    public bool End()
    {
        if (!IsDragging || IsLocked) return false;
        IsDragging = false;
        if (SelectionNormalizer.IsValid(Rect.X, Rect.Y, Rect.Width, Rect.Height))
        {
            IsLocked = true;
            return true;
        }
        Reset();
        return false;
    }

    public void Reset()
    {
        IsDragging = false;
        IsLocked = false;
        Rect = default;
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `dotnet test tests/CapturePlus.Tests/CapturePlus.Tests.csproj --filter SelectionTrackerTests`
Expected: 7 passing.

- [ ] **Step 5: Commit**

```bash
git add src/CapturePlus/Core/SelectionTracker.cs tests/CapturePlus.Tests/Core/SelectionTrackerTests.cs
git commit -m "feat: SelectionTracker state machine (physical virtual coords)"
```

---

### Task 3: ScreenCapturer 返回 VirtualCapture

**Files:**
- Modify: `src/CapturePlus/Screenshot/ScreenCapturer.cs`

- [ ] **Step 1: Replace ScreenCapturer.cs**

Replace the entire contents of `src/CapturePlus/Screenshot/ScreenCapturer.cs` with:

```csharp
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Screenshot;

public readonly record struct VirtualCapture(Bitmap Bitmap, Rectangle VirtualBounds)
{
    public static implicit operator Bitmap(VirtualCapture cap) => cap.Bitmap;
}

public static class ScreenCapturer
{
    public static VirtualCapture CaptureVirtualScreen()
    {
        var bounds = SystemInformation.VirtualScreen;
        try
        {
            var bmp = new Bitmap(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb);
            using var g = Graphics.FromImage(bmp);
            g.CopyFromScreen(bounds.X, bounds.Y, 0, 0, bmp.Size, CopyPixelOperation.SourceCopy);
            return new VirtualCapture(bmp, bounds);
        }
        catch (Exception ex)
        {
            Logger.Error("CaptureVirtualScreen failed", ex);
            throw;
        }
    }

    public static Bitmap Crop(Bitmap source, NormRect rect)
    {
        var rx = (int)Math.Round(rect.X);
        var ry = (int)Math.Round(rect.Y);
        var rw = (int)Math.Round(rect.Width);
        var rh = (int)Math.Round(rect.Height);

        rx = Math.Max(0, Math.Min(rx, source.Width - 1));
        ry = Math.Max(0, Math.Min(ry, source.Height - 1));
        rw = Math.Max(1, Math.Min(rw, source.Width - rx));
        rh = Math.Max(1, Math.Min(rh, source.Height - ry));

        return source.Clone(new Rectangle(rx, ry, rw, rh), source.PixelFormat);
    }
}
```

- [ ] **Step 2: Build to confirm intermediate state compiles**

Run: `dotnet build src/CapturePlus/CapturePlus.csproj`
Expected: succeeds. 旧 `ScreenshotSession` 中的 `Bitmap full = ScreenCapturer.CaptureVirtualScreen();` 依靠 `VirtualCapture → Bitmap` 隐式转换继续可编译。

- [ ] **Step 3: Commit**

```bash
git add src/CapturePlus/Screenshot/ScreenCapturer.cs
git commit -m "feat: ScreenCapturer returns VirtualCapture (bitmap + bounds)"
```

---

### Task 4: DpiHelper 增加 GetDpiScaleForWindow

**Files:**
- Modify: `src/CapturePlus/Screenshot/DpiHelper.cs`

- [ ] **Step 1: Add GetDpiScaleForWindow**

In `src/CapturePlus/Screenshot/DpiHelper.cs`, add the `GetDpiForWindow` extern and a new public method. Replace the file contents with:

```csharp
using System.Runtime.InteropServices;

namespace CapturePlus.Screenshot;

public static class DpiHelper
{
    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    private static extern IntPtr MonitorFromRect(ref RECT lprc, uint dwFlags);

    [DllImport("shcore.dll")]
    private static extern int GetDpiForMonitor(IntPtr hmonitor, int dpiType, out uint dpiX, out uint dpiY);

    [DllImport("user32.dll")]
    private static extern uint GetDpiForSystem();

    [DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr hwnd);

    public static double SystemScale => GetDpiForSystem() / 96.0;

    public static double GetScaleForRect(int x, int y, int w, int h)
    {
        try
        {
            var rect = new RECT { Left = x, Top = y, Right = x + w, Bottom = y + h };
            var hmon = MonitorFromRect(ref rect, 2);
            if (hmon == IntPtr.Zero) return SystemScale;
            int hr = GetDpiForMonitor(hmon, 0, out uint dpiX, out _);
            if (hr != 0 || dpiX == 0) return SystemScale;
            return dpiX / 96.0;
        }
        catch { return SystemScale; }
    }

    public static double GetDpiScaleForWindow(IntPtr hwnd)
    {
        uint dpi = GetDpiForWindow(hwnd);
        return dpi > 0 ? dpi / 96.0 : 1.0;
    }
}
```

- [ ] **Step 2: Build**

Run: `dotnet build src/CapturePlus/CapturePlus.csproj`
Expected: succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/CapturePlus/Screenshot/DpiHelper.cs
git commit -m "feat: DpiHelper.GetDpiScaleForWindow"
```

---

### Task 5: OverlayWindow 重写（输入上报 + 遮罩绘制 + 工具条）

**Files:**
- Modify: `src/CapturePlus/Screenshot/OverlayWindow.xaml`
- Replace: `src/CapturePlus/Screenshot/OverlayWindow.xaml.cs`

本任务把选区的拥有权从遮罩窗移走：遮罩窗只负责把本地 DIP 坐标换算成物理虚拟坐标并上报，按「选区 ∩ 本屏」绘制遮罩空洞，以及显示工具条。

- [ ] **Step 1: Update OverlayWindow.xaml**

Replace the whole file `src/CapturePlus/Screenshot/OverlayWindow.xaml` with:

```xml
<Window x:Class="CapturePlus.Screenshot.OverlayWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="" WindowStyle="None" ResizeMode="NoResize"
        ShowInTaskbar="False" Topmost="True"
        WindowStartupLocation="Manual"
        Cursor="Cross" PreviewKeyDown="OnPreviewKeyDown">
    <Window.Background>
        <ImageBrush x:Name="BgBrush" Stretch="Fill"/>
    </Window.Background>
    <Window.Resources>
        <Style x:Key="TbBtn" TargetType="Button">
            <Setter Property="Foreground" Value="White"/>
            <Setter Property="Background" Value="Transparent"/>
            <Setter Property="BorderBrush" Value="Transparent"/>
            <Setter Property="Padding" Value="8,4"/>
            <Setter Property="FontSize" Value="13"/>
            <Setter Property="Template">
                <Setter.Value>
                    <ControlTemplate TargetType="Button">
                        <Border Background="{TemplateBinding Background}" CornerRadius="3" Padding="{TemplateBinding Padding}">
                            <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/>
                        </Border>
                        <ControlTemplate.Triggers>
                            <Trigger Property="IsMouseOver" Value="True">
                                <Setter Property="Background" Value="#FF3F3F46"/>
                            </Trigger>
                        </ControlTemplate.Triggers>
                    </ControlTemplate>
                </Setter.Value>
            </Setter>
        </Style>
    </Window.Resources>
    <Canvas x:Name="Root">
        <Path x:Name="MaskPath" Fill="#80000000"/>
        <Border x:Name="SelBorder" BorderBrush="White" BorderThickness="2" Background="Transparent"/>
        <Border x:Name="Toolbar" Background="#FF2D2D30" CornerRadius="4"
                Padding="6" BorderBrush="#FF555555" BorderThickness="1" Visibility="Collapsed">
            <StackPanel Orientation="Horizontal">
                <Button Content="复制图片" Margin="2,0" Click="OnCopy" Style="{StaticResource TbBtn}"/>
                <Button Content="保存图片" Margin="2,0" Click="OnSave" Style="{StaticResource TbBtn}"/>
                <Button Content="提取文字" Margin="2,0" Click="OnOcr" Style="{StaticResource TbBtn}"/>
                <Button Content="AI分析" Margin="2,0" Click="OnAi" Style="{StaticResource TbBtn}"/>
                <Button Content="翻译" Margin="2,0" Click="OnTranslate" Style="{StaticResource TbBtn}"/>
                <Button Content="✕" Margin="6,0,0,0" Click="OnCancel" Width="28" Style="{StaticResource TbBtn}"/>
            </StackPanel>
        </Border>
    </Canvas>
</Window>
```

要点：`KeyDown="OnKeyDown"` → `PreviewKeyDown="OnPreviewKeyDown"`；`ImageBrush Stretch="Uniform"` → `Stretch="Fill"`。

- [ ] **Step 2: Replace OverlayWindow.xaml.cs**

Replace the entire contents of `src/CapturePlus/Screenshot/OverlayWindow.xaml.cs` with:

```csharp
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using CapturePlus.Core;
using KeyEventArgs = System.Windows.Input.KeyEventArgs;
using MouseEventArgs = System.Windows.Input.MouseEventArgs;
using Point = System.Windows.Point;
using Rect = System.Windows.Rect;
using Size = System.Windows.Size;

namespace CapturePlus.Screenshot;

public enum InputPhase { Begin, Move, End }

public partial class OverlayWindow : Window
{
    private readonly Bitmap _source;
    private readonly Rectangle _monitorBounds;
    private readonly double _physW, _physH;
    private double _dpiScale = 1.0;
    private double _screenW, _screenH;
    private NormRect? _lastSelection;

    public event Action<InputPhase, double, double>? SelectionInput;
    public event Action<ScreenshotAction>? ActionRequested;
    public event Action? Cancelled;

    public OverlayWindow(Bitmap source, Rectangle monitorBounds, double dipX, double dipY,
        double physW, double physH, double estimatedScale)
    {
        InitializeComponent();
        _source = source;
        _monitorBounds = monitorBounds;
        _physW = physW;
        _physH = physH;
        _dpiScale = estimatedScale;
        _screenW = physW / estimatedScale;
        _screenH = physH / estimatedScale;

        Left = dipX; Top = dipY;
        Width = _screenW; Height = _screenH;
        Closed += (_, _) => _source.Dispose();
        SourceInitialized += OnSourceInitialized;

        SetupImage();
        RenderSelection(null);
    }

    private void SetupImage()
    {
        BgBrush.ImageSource = ToBitmapSource(_source);
    }

    private void OnSourceInitialized(object? sender, EventArgs e)
    {
        var hwnd = new WindowInteropHelper(this).Handle;
        _dpiScale = DpiHelper.GetDpiScaleForWindow(hwnd);

        _screenW = _physW / _dpiScale;
        _screenH = _physH / _dpiScale;
        Width = _screenW;
        Height = _screenH;
        SetupImage();
        RenderSelection(_lastSelection);
    }

    private static BitmapSource ToBitmapSource(Bitmap bmp)
    {
        var hBitmap = bmp.GetHbitmap();
        try
        {
            var src = Imaging.CreateBitmapSourceFromHBitmap(
                hBitmap, IntPtr.Zero, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
            src.Freeze();
            return src;
        }
        finally
        {
            DeleteObject(hBitmap);
        }
    }

    [DllImport("gdi32.dll")]
    private static extern bool DeleteObject(IntPtr hObject);

    private void OnPreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape) OnCancel(sender, e);
    }

    private void OnCancel(object sender, RoutedEventArgs e)
    {
        Cancelled?.Invoke();
        Close();
    }

    protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        base.OnMouseLeftButtonDown(e);
        var p = e.GetPosition(this);
        SelectionInput?.Invoke(InputPhase.Begin,
            _monitorBounds.X + p.X * _dpiScale,
            _monitorBounds.Y + p.Y * _dpiScale);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        if (Mouse.LeftButton != MouseButtonState.Pressed) return;
        var p = e.GetPosition(this);
        SelectionInput?.Invoke(InputPhase.Move,
            _monitorBounds.X + p.X * _dpiScale,
            _monitorBounds.Y + p.Y * _dpiScale);
    }

    protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
    {
        base.OnMouseLeftButtonUp(e);
        var p = e.GetPosition(this);
        SelectionInput?.Invoke(InputPhase.End,
            _monitorBounds.X + p.X * _dpiScale,
            _monitorBounds.Y + p.Y * _dpiScale);
    }

    public void RenderSelection(NormRect? sel)
    {
        _lastSelection = sel;
        NormRect? local = sel.HasValue ? LocalIntersection(sel.Value) : null;

        var full = new Rect(0, 0, _screenW, _screenH);
        var geometry = new StreamGeometry();
        using (var ctx = geometry.Open())
        {
            ctx.BeginFigure(full.TopLeft, true, true);
            ctx.LineTo(new Point(full.Right, full.Top), true, true);
            ctx.LineTo(full.BottomRight, true, true);
            ctx.LineTo(new Point(full.Left, full.Bottom), true, true);
            ctx.LineTo(full.TopLeft, true, true);

            if (local.HasValue)
            {
                var r = local.Value;
                ctx.BeginFigure(r.TopLeft, true, true);
                ctx.LineTo(new Point(r.Right, r.Top), true, true);
                ctx.LineTo(r.BottomRight, true, true);
                ctx.LineTo(new Point(r.Left, r.Bottom), true, true);
                ctx.LineTo(r.TopLeft, true, true);
            }
        }
        MaskPath.Data = geometry;

        if (local.HasValue)
        {
            SelBorder.Width = local.Value.Width; SelBorder.Height = local.Value.Height;
            Canvas.SetLeft(SelBorder, local.Value.X); Canvas.SetTop(SelBorder, local.Value.Y);
            SelBorder.Visibility = Visibility.Visible;
        }
        else
        {
            SelBorder.Visibility = Visibility.Collapsed;
        }
    }

    public NormRect ToLocalDip(NormRect phys)
    {
        return new NormRect(
            (phys.X - _monitorBounds.X) / _dpiScale,
            (phys.Y - _monitorBounds.Y) / _dpiScale,
            phys.Width / _dpiScale,
            phys.Height / _dpiScale);
    }

    private NormRect? LocalIntersection(NormRect sel)
    {
        var mb = new NormRect(_monitorBounds.X, _monitorBounds.Y, _monitorBounds.Width, _monitorBounds.Height);
        var inter = RectMath.Intersect(sel, mb);
        if (inter.Width <= 0 || inter.Height <= 0) return null;
        return ToLocalDip(inter);
    }

    public void ShowToolbar(NormRect localSel)
    {
        Toolbar.Visibility = Visibility.Visible;
        Toolbar.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
        Toolbar.Arrange(new Rect(0, 0, Toolbar.DesiredSize.Width, Toolbar.DesiredSize.Height));
        var tw = Toolbar.DesiredSize.Width; var th = Toolbar.DesiredSize.Height;

        var pos = ToolbarPlacement.Place(
            localSel.X, localSel.Y, localSel.Width, localSel.Height,
            0, 0, _screenW, _screenH, tw, th, 8);
        Canvas.SetLeft(Toolbar, pos.X);
        Canvas.SetTop(Toolbar, pos.Y);
    }

    private void OnCopy(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.CopyImage);
    private void OnSave(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.SaveImage);
    private void OnOcr(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.Ocr);
    private void OnAi(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.AiAnalysis);
    private void OnTranslate(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.Translate);

    private void Fire(ScreenshotAction action)
    {
        ActionRequested?.Invoke(action);
        Close();
    }
}

public enum ScreenshotAction { CopyImage, SaveImage, Ocr, AiAnalysis, Translate }
```

- [ ] **Step 3: Build（本任务预期失败，Task 6 完成前不保证可编译）**

Run: `dotnet build src/CapturePlus/CapturePlus.csproj`
Expected: FAIL（`ScreenshotSession` 仍引用旧的 OverlayWindow 构造函数与 `ActionRequested(Bitmap, ...)` 签名）。允许失败，Task 6 一并修复。

- [ ] **Step 4: Commit**

```bash
git add src/CapturePlus/Screenshot/OverlayWindow.xaml src/CapturePlus/Screenshot/OverlayWindow.xaml.cs
git commit -m "refactor: OverlayWindow reports input, renders shared selection mask"
```

（注：若你按子代理逐任务执行并要求每次提交都可编译，可将本任务与 Task 6 合并为一个提交。默认保持两个独立提交。）

---

### Task 6: ScreenshotSession 重写（选区状态机 + 动作分发）

**Files:**
- Replace: `src/CapturePlus/Screenshot/ScreenshotSession.cs`

- [ ] **Step 1: Replace ScreenshotSession.cs**

Replace the entire contents of `src/CapturePlus/Screenshot/ScreenshotSession.cs` with:

```csharp
using System.Drawing;
using System.Threading;
using System.Windows.Forms;
using CapturePlus.Core;
using CapturePlus.Features;
using CapturePlus.Logging;

namespace CapturePlus.Screenshot;

public sealed class ScreenshotSession
{
    private int _busy;
    private int _openCount;
    private readonly List<OverlayWindow> _overlays = new();
    private readonly List<OverlayInfo> _infos = new();
    private Bitmap? _full;
    private readonly SelectionTracker _tracker = new();

    private sealed record OverlayInfo(OverlayWindow Window, Rectangle Bounds);

    public async Task StartAsync()
    {
        if (Interlocked.CompareExchange(ref _busy, 1, 0) != 0) return;

        try
        {
            VirtualCapture cap;
            try
            {
                cap = await Task.Run(() => ScreenCapturer.CaptureVirtualScreen());
            }
            catch (Exception ex)
            {
                Logger.Error("Capture failed at session start", ex);
                TrayIconAdapter.ShowBalloon("截图失败，请重试", 1500);
                EndSession();
                return;
            }

            _full = cap.Bitmap;
            var screens = Screen.AllScreens;
            double primaryScale = DpiHelper.SystemScale;
            Logger.Info($"ScreenshotSession: primaryScale={primaryScale}, screens={screens.Length}, virtualScreen={cap.VirtualBounds}");

            _openCount = 0;
            foreach (var sc in screens)
            {
                double monitorScale = DpiHelper.GetScaleForRect(sc.Bounds.X, sc.Bounds.Y, sc.Bounds.Width, sc.Bounds.Height);

                var local = new Rectangle(
                    sc.Bounds.X - cap.VirtualBounds.X,
                    sc.Bounds.Y - cap.VirtualBounds.Y,
                    sc.Bounds.Width,
                    sc.Bounds.Height);
                Bitmap slice = ScreenCapturer.Crop(_full, new NormRect(local.X, local.Y, local.Width, local.Height));

                double dipX = sc.Bounds.X / primaryScale;
                double dipY = sc.Bounds.Y / primaryScale;

                Logger.Info($"  screen: phys=({sc.Bounds.X},{sc.Bounds.Y},{sc.Bounds.Width},{sc.Bounds.Height}) primaryScale={primaryScale} monitorScale={monitorScale} dipPos=({dipX},{dipY})");

                var overlay = new OverlayWindow(slice, sc.Bounds, dipX, dipY, sc.Bounds.Width, sc.Bounds.Height, monitorScale);
                overlay.SelectionInput += (phase, px, py) => OnSelectionInput(phase, px, py);
                overlay.ActionRequested += OnAction;
                overlay.Cancelled += CloseAllOverlays;
                overlay.Closed += (_, _) => OnOverlayClosed();

                _infos.Add(new OverlayInfo(overlay, sc.Bounds));
                _overlays.Add(overlay);
                overlay.Show();
                overlay.Activate();
                _openCount++;
            }

            if (_overlays.Count == 0) EndSession();
        }
        catch (Exception ex)
        {
            Logger.Error("Failed to start screenshot session", ex);
            CloseAllOverlays();
        }
    }

    private void OnSelectionInput(InputPhase phase, double px, double py)
    {
        switch (phase)
        {
            case InputPhase.Begin:
                _tracker.Begin(px, py);
                break;
            case InputPhase.Move:
                _tracker.Update(px, py);
                break;
            case InputPhase.End:
                if (_tracker.End()) ShowToolbar(px, py);
                break;
        }
        RenderAll();
    }

    private void RenderAll()
    {
        var sel = _tracker.IsDragging || _tracker.IsLocked ? _tracker.Rect : (NormRect?)null;
        foreach (var info in _infos)
        {
            info.Window.RenderSelection(sel);
        }
    }

    private void ShowToolbar(double releaseX, double releaseY)
    {
        var info = _infos.FirstOrDefault(i => RectMath.Contains(
            new NormRect(i.Bounds.X, i.Bounds.Y, i.Bounds.Width, i.Bounds.Height), releaseX, releaseY))
            ?? _infos.FirstOrDefault();
        if (info is null) return;
        info.Window.ShowToolbar(info.Window.ToLocalDip(_tracker.Rect));
    }

    private void OnAction(ScreenshotAction action)
    {
        try
        {
            var crop = ScreenCapturer.Crop(_full!, _tracker.Rect);
            switch (action)
            {
                case ScreenshotAction.CopyImage:
                    CopyImageService.Copy(crop);
                    break;
                case ScreenshotAction.SaveImage:
                    SaveImageService.Save(crop);
                    break;
                case ScreenshotAction.Ocr:
                    _ = ResultWindow.ShowOcrAsync(crop);
                    break;
                case ScreenshotAction.AiAnalysis:
                    _ = ResultWindow.ShowAiAnalysisAsync(crop);
                    break;
                case ScreenshotAction.Translate:
                    _ = ResultWindow.ShowTranslateAsync(crop);
                    break;
            }
        }
        catch (Exception ex)
        {
            Logger.Error($"Action {action} failed", ex);
        }
        finally
        {
            CloseAllOverlays();
        }
    }

    private void CloseAllOverlays()
    {
        var toClose = _overlays.ToList();
        _overlays.Clear();
        _infos.Clear();
        foreach (var o in toClose)
        {
            try { o.Close(); } catch { }
        }
        EndSession();
    }

    private void OnOverlayClosed()
    {
        if (_openCount > 0 && Interlocked.Decrement(ref _openCount) == 0) EndSession();
    }

    private void EndSession()
    {
        _openCount = 0;
        var bmp = Interlocked.Exchange(ref _full, null);
        bmp?.Dispose();
        Interlocked.Exchange(ref _busy, 0);
    }
}
```

- [ ] **Step 2: Build + run all tests**

Run: `dotnet build src/CapturePlus/CapturePlus.csproj && dotnet test tests/CapturePlus.Tests/CapturePlus.Tests.csproj`
Expected: build succeeds，全部测试通过（原有 Core 测试 + 新增 RectMathTests/SelectionTrackerTests）。

- [ ] **Step 3: Commit**

```bash
git add src/CapturePlus/Screenshot/ScreenshotSession.cs
git commit -m "refactor: ScreenshotSession owns shared selection state (cross-monitor)"
```

---

### Task 7: 全量验证

- [ ] **Step 1: 全量构建与测试**

Run: `dotnet build && dotnet test`
Expected: build 0 错误；全部测试通过。

预期测试数量：原有 7 个测试文件（AppSettings、SelectionNormalizer、ToolbarPlacement、SaveFileNameGenerator、PromptBuilder、ApiKeyRedactor、OcrTextJoiner）+ SmokeTest，另加新增的 RectMathTests(8) + SelectionTrackerTests(7)。

- [ ] **Step 2: 手动验证清单（需在 Windows 实机执行）**

1. 单屏拖拽选区、松手出工具条，5 项动作（复制/保存/OCR/AI/翻译）正常。
2. 双屏（同 DPI）跨屏拖拽：选区连续、裁剪完整、工具条出现在松手所在屏。
3. 混合 DPI（如 150% + 100%）跨屏拖拽：遮罩正确、裁剪清晰不模糊。
4. Esc 任意时刻取消；工具条 ✕ 取消。
5. 连按热键（Ctrl+Alt+A）：不重复进入截图模式、不卡死。
6. 启动后托盘、设置、热键均正常（回归）。

- [ ] **Step 3: 提交（如有修复改动）**

```bash
git add -A
git commit -m "fix: verification fixes"
```
若无改动则跳过本步。
