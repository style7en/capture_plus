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

    public static double GetScaleForRect(int x, int y, int w, int h)
    {
        try
        {
            var rect = new RECT { Left = x, Top = y, Right = x + w, Bottom = y + h };
            var hmon = MonitorFromRect(ref rect, 2);
            if (hmon == IntPtr.Zero) return 1.0;
            GetDpiForMonitor(hmon, 0, out uint dpiX, out _);
            return dpiX / 96.0;
        }
        catch { return 1.0; }
    }
}
