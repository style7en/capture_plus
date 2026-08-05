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
