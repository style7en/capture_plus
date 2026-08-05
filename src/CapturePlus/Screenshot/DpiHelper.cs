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
