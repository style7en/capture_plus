using System.Drawing;
using System.Threading;
using System.Windows.Forms;
using CapturePlus.Core;
using CapturePlus.Features;
using CapturePlus.Logging;

namespace CapturePlus.Screenshot;

public sealed class ScreenshotSession
{
    private int _activeOverlays;
    private readonly List<OverlayWindow> _overlays = new();

#pragma warning disable CS1998 // async method lacks await
    public async Task StartAsync()
#pragma warning restore CS1998
    {
        if (_activeOverlays > 0) return; // already capturing

        Bitmap full;
        try { full = ScreenCapturer.CaptureVirtualScreen(); }
        catch (Exception ex)
        {
            Logger.Error("Capture failed at session start", ex);
            return;
        }

        var virtualBounds = SystemInformation.VirtualScreen;
        var screens = Screen.AllScreens;
        _activeOverlays = screens.Length;
        _overlays.Clear();

        double primaryScale = DpiHelper.SystemScale;
        Logger.Info($"ScreenshotSession: primaryScale={primaryScale}, screens={screens.Length}, virtualScreen={virtualBounds}");

        foreach (var sc in screens)
        {
            double monitorScale = DpiHelper.GetScaleForRect(sc.Bounds.X, sc.Bounds.Y, sc.Bounds.Width, sc.Bounds.Height);

            var local = new Rectangle(
                sc.Bounds.X - virtualBounds.X,
                sc.Bounds.Y - virtualBounds.Y,
                sc.Bounds.Width,
                sc.Bounds.Height);
            Bitmap slice = ScreenCapturer.Crop(full, new NormRect(local.X, local.Y, local.Width, local.Height));

            double dipX = sc.Bounds.X / primaryScale;
            double dipY = sc.Bounds.Y / primaryScale;
            double dipW = sc.Bounds.Width / monitorScale;
            double dipH = sc.Bounds.Height / monitorScale;

            Logger.Info($"  screen: phys=({sc.Bounds.X},{sc.Bounds.Y},{sc.Bounds.Width},{sc.Bounds.Height}) monitorScale={monitorScale} dip=({dipX},{dipY},{dipW},{dipH})");

            var overlay = new OverlayWindow(slice, dipX, dipY, dipW, dipH, monitorScale);

            overlay.ActionRequested += (crop, action) =>
            {
                OnAction(crop, action);
                CloseAllOverlays();
            };
            overlay.Cancelled += () => CloseAllOverlays();
            overlay.Closed += (_, _) => Decrement();

            _overlays.Add(overlay);
            overlay.Show();
        }

        full.Dispose();
    }

    private void CloseAllOverlays()
    {
        var toClose = _overlays.ToList();
        _overlays.Clear();
        foreach (var o in toClose)
        {
            try { o.Close(); } catch { }
        }
    }

    private void Decrement()
    {
        if (Interlocked.Decrement(ref _activeOverlays) < 0) _activeOverlays = 0;
    }

    private void OnAction(Bitmap crop, ScreenshotAction action)
    {
        try
        {
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
    }
}
