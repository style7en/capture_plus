using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using System.Windows.Threading;
using CapturePlus.Core;
using CapturePlus.Features;
using CapturePlus.Logging;

namespace CapturePlus.Screenshot;

public sealed class ScreenshotSession
{
    private int _busy;
    private readonly List<NativeDimOverlay> _overlays = new();
    private readonly List<(NativeDimOverlay Overlay, Rectangle Bounds)> _infos = new();
    private readonly SelectionTracker _tracker = new();
    private ToolbarWindow? _toolbar;

    public Task StartAsync()
    {
        if (Interlocked.CompareExchange(ref _busy, 1, 0) != 0) return Task.CompletedTask;

        try
        {
            var screens = Screen.AllScreens;
            for (int i = 0; i < screens.Length; i++)
            {
                var sc = screens[i];
                var overlay = new NativeDimOverlay(sc.Bounds);
                overlay.SelectionInput += (phase, px, py) => OnSelectionInput(phase, px, py);
                overlay.Cancelled += CloseAllOverlays;
                _infos.Add((overlay, sc.Bounds));
                _overlays.Add(overlay);
                overlay.Show();
                if (i == 0) overlay.Activate();
            }
            LogMem("overlays shown");

            if (_overlays.Count == 0) EndSession();
        }
        catch (Exception ex)
        {
            Logger.Error("Failed to start screenshot session", ex);
            CloseAllOverlays();
        }
        return Task.CompletedTask;
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
        foreach (var (overlay, _) in _infos)
            overlay.RenderSelection(sel);
    }

    private void ShowToolbar(double releaseX, double releaseY)
    {
        var bounds = FindMonitor(releaseX, releaseY);

        var dpi = GetDpiScale(bounds);
        var sel = _tracker.Rect;
        var selDip = new NormRect(sel.X / dpi, sel.Y / dpi, sel.Width / dpi, sel.Height / dpi);
        var monDip = new NormRect(bounds.X / dpi, bounds.Y / dpi, bounds.Width / dpi, bounds.Height / dpi);

        _toolbar = new ToolbarWindow();
        var root = (System.Windows.FrameworkElement)_toolbar.Content;
        root.Measure(new System.Windows.Size(double.PositiveInfinity, double.PositiveInfinity));
        var tw = root.DesiredSize.Width;
        var th = root.DesiredSize.Height;

        var pos = ToolbarPlacement.Place(
            selDip.X, selDip.Y, selDip.Width, selDip.Height,
            monDip.X, monDip.Y, monDip.Width, monDip.Height,
            tw, th, 8);

        _toolbar.Left = pos.X;
        _toolbar.Top = pos.Y;
        _toolbar.ActionRequested += OnAction;
        _toolbar.Cancelled += CloseAllOverlays;
        _toolbar.Show();
    }

    private Rectangle FindMonitor(double x, double y)
    {
        foreach (var (_, b) in _infos)
            if (RectMath.Contains(new NormRect(b.X, b.Y, b.Width, b.Height), x, y))
                return b;
        return _infos.Count > 0 ? _infos[0].Bounds : Rectangle.Empty;
    }

    private void OnAction(ScreenshotAction action)
    {
        try
        {
            _toolbar?.Hide();
            foreach (var (overlay, _) in _infos) overlay.Hide();
            try { DwmFlush(); } catch { }
            Thread.Sleep(20);
            try { DwmFlush(); } catch { }

            var crop = CaptureSelection(_tracker.Rect);
            switch (action)
            {
                case ScreenshotAction.CopyImage:
                    CopyImageService.Copy(crop);
                    crop.Dispose();
                    break;
                case ScreenshotAction.SaveImage:
                    SaveImageService.Save(crop);
                    crop.Dispose();
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

    private static Bitmap CaptureSelection(NormRect sel)
    {
        var x = (int)Math.Round(sel.X);
        var y = (int)Math.Round(sel.Y);
        var w = Math.Max(1, (int)Math.Round(sel.Width));
        var h = Math.Max(1, (int)Math.Round(sel.Height));
        var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(bmp))
            g.CopyFromScreen(x, y, 0, 0, bmp.Size, CopyPixelOperation.SourceCopy);
        return bmp;
    }

    private void CloseAllOverlays()
    {
        _toolbar?.Close();
        _toolbar = null;
        foreach (var o in _overlays.ToList())
        {
            try { o.Close(); } catch { }
        }
        EndSession();
    }

    private void EndSession()
    {
        if (Interlocked.Exchange(ref _busy, 0) == 0) return;
        _overlays.Clear();
        _infos.Clear();
        _tracker.Reset();
        LogMem("end session");
    }

    private static double GetDpiScale(Rectangle bounds)
    {
        try
        {
            var r = new RECT(bounds.X, bounds.Y, bounds.Right, bounds.Bottom);
            var hmon = MonitorFromRect(ref r, MONITOR_DEFAULTTONEAREST);
            if (hmon != IntPtr.Zero && GetDpiForMonitor(hmon, MDT_EFFECTIVE, out uint x, out _) == 0)
                return x / 96.0;
        }
        catch { }
        return 1.0;
    }

    private const uint MONITOR_DEFAULTTONEAREST = 2;
    private const int MDT_EFFECTIVE = 0;

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT(int left, int top, int right, int bottom)
    {
        public int Left = left;
        public int Top = top;
        public int Right = right;
        public int Bottom = bottom;
    }

    [DllImport("user32.dll")]
    private static extern IntPtr MonitorFromRect(ref RECT lprc, uint dwFlags);

    [DllImport("shcore.dll")]
    private static extern int GetDpiForMonitor(IntPtr hmonitor, int dpiType, out uint dpiX, out uint dpiY);

    [DllImport("dwmapi.dll")]
    private static extern void DwmFlush();

    private static void LogMem(string stage)
    {
        try
        {
            using var p = System.Diagnostics.Process.GetCurrentProcess();
            p.Refresh();
            Logger.Info($"[mem] {stage}: WS={p.WorkingSet64 / 1024 / 1024}MB PRIV={p.PrivateMemorySize64 / 1024 / 1024}MB PeakWS={p.PeakWorkingSet64 / 1024 / 1024}MB");
        }
        catch
        {
        }
    }
}
