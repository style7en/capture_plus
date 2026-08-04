using System.Runtime.InteropServices;
using System.Windows.Interop;
using CapturePlus.Logging;

namespace CapturePlus.Hotkey;

public sealed class HotkeyManager : IDisposable
{
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    private const int HotkeyId = 0x9001;
    private const uint MOD_ALT = 0x0001;
    private const uint MOD_CONTROL = 0x0002;
    private const uint VK_A = 0x41;
    private const int WM_HOTKEY = 0x0312;

    private HwndSource? _source;
    private bool _registered;

    public event EventHandler? HotkeyPressed;

    public bool Register()
    {
        try
        {
            if (_source is null)
            {
                var p = new HwndSourceParameters("CapturePlusHotkey")
                {
                    Width = 0, Height = 0,
                    PositionX = 0, PositionY = 0,
                    WindowStyle = 0,
                };
                _source = new HwndSource(p);
                _source.AddHook(WndProc);
            }

            _registered = RegisterHotKey(_source.Handle, HotkeyId, MOD_CONTROL | MOD_ALT, VK_A);
            if (!_registered)
            {
                var err = Marshal.GetLastWin32Error();
                Logger.Warn($"RegisterHotKey failed (win32 error {err}). Hotkey may be in use.");
            }
            return _registered;
        }
        catch (Exception ex)
        {
            Logger.Error("HotkeyManager.Register failed", ex);
            return false;
        }
    }

    private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (msg == WM_HOTKEY && wParam.ToInt32() == HotkeyId)
        {
            HotkeyPressed?.Invoke(this, EventArgs.Empty);
            handled = true;
        }
        return IntPtr.Zero;
    }

    public void Dispose()
    {
        if (_registered && _source is not null)
        {
            UnregisterHotKey(_source.Handle, HotkeyId);
            _registered = false;
        }
        _source?.Dispose();
        _source = null;
    }
}
