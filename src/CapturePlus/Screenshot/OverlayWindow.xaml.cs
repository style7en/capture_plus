using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
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
    private double _dpiScale = 1.0;
    private double _screenW, _screenH;
    private NormRect? _lastSelection;
    private HwndSource? _hwndSource;

    private const int WM_DPICHANGED = 0x02E0;
    private const uint SWP_NOZORDER = 0x0004;
    private const uint SWP_NOACTIVATE = 0x0010;
    private const uint SWP_NOOWNERZORDER = 0x0200;

    public event Action<InputPhase, double, double>? SelectionInput;
    public event Action<ScreenshotAction>? ActionRequested;
    public event Action? Cancelled;

    public OverlayWindow(Bitmap source, Rectangle monitorBounds)
    {
        InitializeComponent();
        _source = source;
        _monitorBounds = monitorBounds;
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
        _hwndSource = (HwndSource)PresentationSource.FromVisual(this);
        _hwndSource.AddHook(WndProc);
        ApplyGeometry();
        SetupImage();
        RenderSelection(_lastSelection);
    }

    private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (msg == WM_DPICHANGED)
        {
            Dispatcher.BeginInvoke(DispatcherPriority.Render, ApplyGeometry);
        }
        return IntPtr.Zero;
    }

    private void ApplyGeometry()
    {
        var hwnd = _hwndSource!.Handle;
        SetWindowPos(hwnd, IntPtr.Zero,
            _monitorBounds.X, _monitorBounds.Y, _monitorBounds.Width, _monitorBounds.Height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

        _dpiScale = VisualTreeHelper.GetDpi(this).DpiScaleX;
        if (_dpiScale <= 0) _dpiScale = 1.0;

        _screenW = _monitorBounds.Width / _dpiScale;
        _screenH = _monitorBounds.Height / _dpiScale;
        Width = _screenW;
        Height = _screenH;
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

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter,
        int x, int y, int cx, int cy, uint uFlags);

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
                var r = new Rect(local.Value.X, local.Value.Y, local.Value.Width, local.Value.Height);
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
