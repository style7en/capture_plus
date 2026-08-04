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

        foreach (var sc in screens)
        {
            var local = new Rectangle(
                sc.Bounds.X - virtualBounds.X,
                sc.Bounds.Y - virtualBounds.Y,
                sc.Bounds.Width,
                sc.Bounds.Height);
            Bitmap slice = ScreenCapturer.Crop(full, new NormRect(local.X, local.Y, local.Width, local.Height));

            var overlay = new OverlayWindow(
                slice, sc.Bounds.X, sc.Bounds.Y, sc.Bounds.Width, sc.Bounds.Height);

            overlay.ActionRequested += (crop, action) => OnAction(crop, action);
            overlay.Closed += (_, _) => Decrement();

            overlay.Show();
        }

        full.Dispose();
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
