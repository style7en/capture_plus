using System.Runtime.InteropServices;
using System.Windows.Interop;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Hotkey;

public sealed class HotkeyManager : IDisposable
{
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    private const int HotkeyId = 0x9001;
    private const int WM_HOTKEY = 0x0312;

    private HwndSource? _source;
    private bool _registered;
    private uint _modifiers;
    private uint _vk;

    public event EventHandler? HotkeyPressed;

    public bool Register(string hotkey)
    {
        if (!HotkeyParser.TryParse(hotkey, out uint mods, out uint vk))
        {
            Logger.Warn($"Invalid hotkey '{hotkey}'. Skipped.");
            return false;
        }
        return Register(mods, vk);
    }

    public bool ReRegister(string hotkey)
    {
        if (!HotkeyParser.TryParse(hotkey, out uint mods, out uint vk))
        {
            Logger.Warn($"Invalid hotkey '{hotkey}'.");
            return false;
        }

        EnsureSource();
        if (RegisterHotKey(_source!.Handle, HotkeyId, mods, vk))
        {
            _modifiers = mods;
            _vk = vk;
            _registered = true;
            return true;
        }

        Logger.Warn($"RegisterHotKey failed (win32 error {Marshal.GetLastWin32Error()}). Hotkey may be in use.");
        if (_registered) RegisterHotKey(_source.Handle, HotkeyId, _modifiers, _vk);
        return false;
    }

    public void Unregister()
    {
        if (_registered && _source is not null)
        {
            UnregisterHotKey(_source.Handle, HotkeyId);
            _registered = false;
        }
    }

    private bool Register(uint mods, uint vk)
    {
        try
        {
            EnsureSource();
            _registered = RegisterHotKey(_source!.Handle, HotkeyId, mods, vk);
            if (_registered)
            {
                _modifiers = mods;
                _vk = vk;
            }
            else
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

    private void EnsureSource()
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
        Unregister();
        _source?.Dispose();
        _source = null;
    }
}
