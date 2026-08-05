using System.Drawing;
using System.Drawing.Imaging;
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

public partial class OverlayWindow : Window
{
    private readonly Bitmap _source;
    private readonly double _physW, _physH;
    private double _dpiScale;
    private double _screenW, _screenH;
    private Point? _start;
    private NormRect _current;
    private bool _locked;

    public event Action<Bitmap, ScreenshotAction>? ActionRequested;
    public event Action? Cancelled;

    [DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr hwnd);

    public OverlayWindow(Bitmap source, double dipX, double dipY, double physW, double physH, double estimatedScale)
    {
        InitializeComponent();
        _source = source;
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
        UpdateMask(null);
    }

    private void SetupImage()
    {
        BgImage.Source = ToBitmapSource(_source, _dpiScale);
        Canvas.SetLeft(BgImage, 0);
        Canvas.SetTop(BgImage, 0);
    }

    private void OnSourceInitialized(object? sender, EventArgs e)
    {
        var hwnd = new WindowInteropHelper(this).Handle;
        uint dpi = GetDpiForWindow(hwnd);
        if (dpi > 0)
        {
            _dpiScale = dpi / 96.0;
        }

        _screenW = _physW / _dpiScale;
        _screenH = _physH / _dpiScale;
        Width = _screenW;
        Height = _screenH;
        SetupImage();
        UpdateMask(null);
    }

    private static BitmapSource ToBitmapSource(Bitmap bmp, double dpiScale)
    {
        var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
        var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
        try
        {
            double dpi = 96.0 * dpiScale;
            var src = BitmapSource.Create(
                bmp.Width, bmp.Height,
                dpi, dpi,
                PixelFormats.Bgra32,
                null,
                data.Scan0,
                data.Stride * bmp.Height,
                data.Stride);
            src.Freeze();
            return src;
        }
        finally
        {
            bmp.UnlockBits(data);
        }
    }

    private void OnKeyDown(object sender, KeyEventArgs e)
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
        if (_locked) return;
        _start = e.GetPosition(this);
        _current = new NormRect(_start.Value.X, _start.Value.Y, 0, 0);
        UpdateMask(_current);
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (_start is null || _locked) return;
        var p = e.GetPosition(this);
        double x0 = _start.Value.X, y0 = _start.Value.Y;
        var raw = SelectionNormalizer.Normalize(x0, y0, p.X - x0, p.Y - y0);
        _current = raw;
        UpdateMask(raw);
    }

    protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
    {
        if (_start is null || _locked) return;
        _locked = true;

        if (!SelectionNormalizer.IsValid(_current.X, _current.Y, _current.Width, _current.Height))
        {
            _locked = false;
            _start = null;
            UpdateMask(null);
            return;
        }
        ShowToolbar();
    }

    private void UpdateMask(NormRect? sel)
    {
        var full = new Rect(0, 0, _screenW, _screenH);
        var geometry = new StreamGeometry();
        using (var ctx = geometry.Open())
        {
            ctx.BeginFigure(full.TopLeft, true, true);
            ctx.LineTo(new Point(full.Right, full.Top), true, true);
            ctx.LineTo(full.BottomRight, true, true);
            ctx.LineTo(new Point(full.Left, full.Bottom), true, true);
            ctx.LineTo(full.TopLeft, true, true);
            if (sel.HasValue)
            {
                var s = sel.Value;
                var r = new Rect(s.X, s.Y, s.Width, s.Height);
                ctx.BeginFigure(r.TopLeft, true, true);
                ctx.LineTo(new Point(r.Right, r.Top), true, true);
                ctx.LineTo(r.BottomRight, true, true);
                ctx.LineTo(new Point(r.Left, r.Bottom), true, true);
                ctx.LineTo(r.TopLeft, true, true);
            }
        }
        MaskPath.Data = geometry;

        if (sel.HasValue)
        {
            var s = sel.Value;
            SelBorder.Width = s.Width; SelBorder.Height = s.Height;
            Canvas.SetLeft(SelBorder, s.X); Canvas.SetTop(SelBorder, s.Y);
            SelBorder.Visibility = Visibility.Visible;
        }
        else
        {
            SelBorder.Visibility = Visibility.Collapsed;
        }
    }

    private void ShowToolbar()
    {
        Toolbar.Visibility = Visibility.Visible;
        Toolbar.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
        Toolbar.Arrange(new Rect(0, 0, Toolbar.DesiredSize.Width, Toolbar.DesiredSize.Height));
        var tw = Toolbar.DesiredSize.Width; var th = Toolbar.DesiredSize.Height;

        var pos = ToolbarPlacement.Place(
            _current.X, _current.Y, _current.Width, _current.Height,
            0, 0, _screenW, _screenH, tw, th, 8);
        Canvas.SetLeft(Toolbar, pos.X);
        Canvas.SetTop(Toolbar, pos.Y);
    }

    private Bitmap CropCurrent()
    {
        var physicalRect = new NormRect(
            _current.X * _dpiScale,
            _current.Y * _dpiScale,
            _current.Width * _dpiScale,
            _current.Height * _dpiScale);
        return ScreenCapturer.Crop(_source, physicalRect);
    }

    private void OnCopy(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.CopyImage);
    private void OnSave(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.SaveImage);
    private void OnOcr(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.Ocr);
    private void OnAi(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.AiAnalysis);
    private void OnTranslate(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.Translate);

    private void Fire(ScreenshotAction action)
    {
        var crop = CropCurrent();
        ActionRequested?.Invoke(crop, action);
        Close();
    }
}

public enum ScreenshotAction { CopyImage, SaveImage, Ocr, AiAnalysis, Translate }
