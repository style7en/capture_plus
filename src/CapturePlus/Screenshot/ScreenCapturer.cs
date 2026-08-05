using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Screenshot;

public static class ScreenCapturer
{
    [DllImport("gdi32.dll")]
    private static extern bool BitBlt(IntPtr hdcDest, int nXDest, int nYDest, int nWidth, int nHeight,
        IntPtr hdcSrc, int nXSrc, int nYSrc, int dwRop);

    [DllImport("user32.dll")]
    private static extern IntPtr GetDC(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);

    private const int SRCCOPY = 0x00CC0020;

    public static Bitmap CaptureVirtualScreen()
    {
        var bounds = SystemInformation.VirtualScreen;
        Logger.Info($"CaptureVirtualScreen: bounds={bounds}");
        try
        {
            var bmp = new Bitmap(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb);
            using var g = Graphics.FromImage(bmp);
            var hdcDest = g.GetHdc();
            var hdcSrc = GetDC(IntPtr.Zero);
            try
            {
                BitBlt(hdcDest, 0, 0, bounds.Width, bounds.Height,
                    hdcSrc, bounds.X, bounds.Y, SRCCOPY);
            }
            finally
            {
                ReleaseDC(IntPtr.Zero, hdcSrc);
                g.ReleaseHdc(hdcDest);
            }

            Logger.Info($"CaptureVirtualScreen: bitmap={bmp.Width}x{bmp.Height}");
            return bmp;
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
