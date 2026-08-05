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
