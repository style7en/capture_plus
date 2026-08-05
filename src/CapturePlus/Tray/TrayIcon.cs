using System.Drawing;
using System.Windows.Forms;
using CapturePlus.Logging;

namespace CapturePlus.Tray;

public sealed class TrayIcon : IDisposable
{
    private readonly NotifyIcon _notify;
    private bool _disposed;

    public event EventHandler? ScreenshotRequested;
    public event EventHandler? SettingsRequested;
    public event EventHandler? ExitRequested;

    public TrayIcon()
    {
        _notify = new NotifyIcon
        {
            Text = "CapturePlus",
            Visible = true,
        };
        try
        {
            var info = System.Windows.Application.GetResourceStream(
                new Uri("pack://application:,,,/Assets/app.ico"));
            _notify.Icon = new Icon(info.Stream);
        }
        catch (Exception ex)
        {
            Logger.Warn($"Tray icon load failed: {ex.Message}");
            _notify.Icon = SystemIcons.Application;
        }

        var menu = new ContextMenuStrip();
        menu.Items.Add("截图 (Ctrl+Alt+A)", null, (_, _) => ScreenshotRequested?.Invoke(this, EventArgs.Empty));
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("设置", null, (_, _) => SettingsRequested?.Invoke(this, EventArgs.Empty));
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("关于", null, (_, _) => OnAbout());
        menu.Items.Add("退出", null, (_, _) => ExitRequested?.Invoke(this, EventArgs.Empty));
        _notify.ContextMenuStrip = menu;

        _notify.DoubleClick += (_, _) => ScreenshotRequested?.Invoke(this, EventArgs.Empty);
    }

    public void ShowBalloon(string message, int durationMs = 1500)
    {
        _notify.ShowBalloonTip(durationMs, "CapturePlus", message, ToolTipIcon.None);
    }

    private void OnAbout()
    {
        System.Windows.MessageBox.Show(
            "CapturePlus v" + System.Reflection.Assembly.GetExecutingAssembly().GetName().Version,
            "关于", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Information);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _notify.Visible = false;
        _notify.Dispose();
        _disposed = true;
    }
}
