using System.Threading;
using System.Windows;
using System.Windows.Threading;
using CapturePlus.Hotkey;
using CapturePlus.Logging;
using CapturePlus.Settings;
using CapturePlus.Screenshot;
using CapturePlus.Tray;
using Application = System.Windows.Application;
using AppSettings = CapturePlus.Core.AppSettings;

namespace CapturePlus;

public partial class App : Application
{
    private static Mutex? _singleMutex;
    private static bool _ownsMutex;
    private TrayIcon? _tray;
    private HotkeyManager? _hotkey;
    private ScreenshotSession? _session;

    public static AppSettings CurrentSettings { get; set; } = new();

    public string SaveDir => CurrentSettings.SaveDir;

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        _singleMutex = new Mutex(true, "Global\\CapturePlus_SingleInstance", out bool isNew);
        _ownsMutex = isNew;
        if (!isNew)
        {
            System.Windows.MessageBox.Show("CapturePlus 已在后台运行。", "CapturePlus",
                System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Information);
            Shutdown();
            return;
        }

        DispatcherUnhandledException += OnUnhandled;

        CurrentSettings = AppSettingsService.Load();

        _tray = new TrayIcon();
        Features.TrayIconAdapter.OnShowBalloon = (msg, ms) => _tray.ShowBalloon(msg, ms);
        _tray.ScreenshotRequested += (_, _) => StartScreenshot();
        _tray.SettingsRequested += (_, _) => OpenSettings();
        _tray.ExitRequested += (_, _) => ShutdownGracefully();
        _tray.ShowBalloon("CapturePlus 已启动，按 Ctrl+Alt+A 截图", 1500);

        _hotkey = new HotkeyManager();
        _hotkey.HotkeyPressed += (_, _) => StartScreenshot();
        bool ok = _hotkey.Register();
        if (!ok) _tray.ShowBalloon("Ctrl+Alt+A 被占用，请关闭冲突软件（可用托盘菜单截图）", 2500);

        _session = new ScreenshotSession();
    }

    private void StartScreenshot()
    {
        _session?.StartAsync().ContinueWith(t =>
        {
            if (t.IsFaulted) Logger.Error("Screenshot session faulted", t.Exception);
        });
    }

    private void OpenSettings()
    {
        var w = new SettingsWindow();
        w.Show();
        w.Activate();
    }

    private void ShutdownGracefully()
    {
        _hotkey?.Dispose();
        _tray?.Dispose();
        Shutdown();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _hotkey?.Dispose();
        _tray?.Dispose();
        if (_ownsMutex) _singleMutex?.ReleaseMutex();
        _singleMutex?.Dispose();
        base.OnExit(e);
    }

    private void OnUnhandled(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        Logger.Error("Unhandled UI exception", e.Exception);
        System.Windows.MessageBox.Show($"发生未处理异常：\n{e.Exception.Message}", "CapturePlus",
            System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Error);
        e.Handled = true;
    }
}
