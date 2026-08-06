using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using CapturePlus.Core;
using CapturePlus.Features;
using CapturePlus.Hotkey;
using CapturePlus.Logging;
using Button = System.Windows.Controls.Button;
using KeyEventArgs = System.Windows.Input.KeyEventArgs;

namespace CapturePlus.Settings;

public partial class SettingsWindow : Window
{
    private readonly AppSettings _local;
    private readonly HotkeyManager _hotkeyManager;
    private string _hotkey;
    private bool _capturing;

    public SettingsWindow(HotkeyManager hotkeyManager)
    {
        InitializeComponent();
        _hotkeyManager = hotkeyManager;
        _hotkey = App.CurrentSettings.Hotkey;
        _local = Clone(App.CurrentSettings);
        LoadIntoUi();
    }

    private static AppSettings Clone(AppSettings s) => AppSettings.FromJson(AppSettings.ToJson(s));

    private void LoadIntoUi()
    {
        BaseUrlBox.Text = _local.Api.BaseUrl;
        ApiKeyPwd.Password = _local.Api.ApiKey;
        ApiKeyBox.Text = _local.Api.ApiKey;
        VisionModelBox.Text = _local.Api.VisionModel;
        TextModelBox.Text = _local.Api.TextModel;
        SaveDirBox.Text = _local.SaveDir;
        HotkeyBox.Text = _hotkey;

        TranslateLangBox.ItemsSource = new[]
        {
            "中文（简体）", "中文（繁體）", "English", "日本語", "한국어", "Français", "Deutsch", "Español"
        };
        TranslateLangBox.Text = _local.TranslateTargetLanguage;
    }

    private void OnApiKeyPwdChanged(object sender, RoutedEventArgs e) => _local.Api.ApiKey = ApiKeyPwd.Password;
    private void OnApiKeyChanged(object sender, TextChangedEventArgs e) => _local.Api.ApiKey = ApiKeyBox.Text;
    private void OnToggleKey(object sender, RoutedEventArgs e)
    {
        bool show = ApiKeyBox.Visibility == Visibility.Visible;
        ApiKeyBox.Visibility = show ? Visibility.Collapsed : Visibility.Visible;
        ApiKeyPwd.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
        ToggleKeyBtn.Content = show ? "👁" : "🙈";
        if (show) ApiKeyPwd.Password = ApiKeyBox.Text;
        else ApiKeyBox.Text = ApiKeyPwd.Password;
    }

    private void OnBrowse(object sender, RoutedEventArgs e)
    {
        using var dlg = new System.Windows.Forms.FolderBrowserDialog();
        if (dlg.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            SaveDirBox.Text = dlg.SelectedPath;
    }

    private void OnHotkeyBoxMouseDown(object sender, MouseButtonEventArgs e)
    {
        _capturing = true;
        HotkeyBox.Text = "请按下新快捷键…";
        HotkeyBox.CaretIndex = 0;
        HotkeyBox.Focus();
        e.Handled = true;
    }

    private void OnHotkeyBoxKeyDown(object sender, KeyEventArgs e)
    {
        if (!_capturing) return;
        e.Handled = true;

        var key = e.Key == Key.System ? e.SystemKey : e.Key;
        if (key == Key.Escape)
        {
            _capturing = false;
            HotkeyBox.Text = _hotkey;
            return;
        }

        if (IsModifierKey(key)) return;

        if (TryGetCombo(key, Keyboard.Modifiers, out string combo))
        {
            _capturing = false;
            _hotkey = combo;
            HotkeyBox.Text = combo;
        }
    }

    private static bool IsModifierKey(Key key) => key is
        Key.LeftCtrl or Key.RightCtrl or Key.LeftShift or Key.RightShift
        or Key.LeftAlt or Key.RightAlt or Key.LWin or Key.RWin;

    private static bool TryGetCombo(Key key, ModifierKeys modifiers, out string combo)
    {
        combo = "";
        uint mods = 0;
        if (modifiers.HasFlag(ModifierKeys.Control)) mods |= HotkeyParser.ModControl;
        if (modifiers.HasFlag(ModifierKeys.Alt)) mods |= HotkeyParser.ModAlt;
        if (modifiers.HasFlag(ModifierKeys.Shift)) mods |= HotkeyParser.ModShift;
        if (modifiers.HasFlag(ModifierKeys.Windows)) mods |= HotkeyParser.ModWin;
        if (mods == 0) return false;

        uint vk = ToVirtualKey(key);
        if (vk == 0 || !HotkeyParser.IsSupported(vk)) return false;
        combo = HotkeyParser.Format(mods, vk);
        return true;
    }

    private static uint ToVirtualKey(Key key)
    {
        if (key is >= Key.A and <= Key.Z) return 0x41 + (uint)(key - Key.A);
        if (key is >= Key.D0 and <= Key.D9) return 0x30 + (uint)(key - Key.D0);
        if (key is >= Key.F1 and <= Key.F12) return 0x70 + (uint)(key - Key.F1);
        return key switch
        {
            Key.Space => 0x20,
            Key.Enter => 0x0D,
            Key.Escape => 0x1B,
            Key.Tab => 0x09,
            Key.Back => 0x08,
            Key.Delete => 0x2E,
            Key.Home => 0x24,
            Key.End => 0x23,
            Key.PageUp => 0x21,
            Key.PageDown => 0x22,
            Key.Left => 0x25,
            Key.Right => 0x27,
            Key.Up => 0x26,
            Key.Down => 0x28,
            _ => 0,
        };
    }

    private async void OnTestConnection(object sender, RoutedEventArgs e)
    {
        CommitFieldsTo(_local);
        var btn = (Button)sender;
        btn.Content = "测试中…"; btn.IsEnabled = false;
        try
        {
            var ai = new AiService();
            var cts = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            var result = await ai.TranslateAsync("hello", _local, cts.Token);
            System.Windows.MessageBox.Show($"连接成功。模型返回示例：\n{result}", "测试连接",
                System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
            Logger.Warn($"Test connection failed: {ex.Message}");
            System.Windows.MessageBox.Show($"连接失败：\n{ex.Message}", "测试连接",
                System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Warning);
        }
        finally { btn.Content = "测试连接"; btn.IsEnabled = true; }
    }

    private void CommitFieldsTo(AppSettings s)
    {
        s.Api.BaseUrl = BaseUrlBox.Text.Trim();
        s.Api.ApiKey = string.IsNullOrEmpty(ApiKeyPwd.Password) ? ApiKeyBox.Text : ApiKeyPwd.Password;
        s.Api.VisionModel = VisionModelBox.Text.Trim();
        s.Api.TextModel = TextModelBox.Text.Trim();
        s.TranslateTargetLanguage = TranslateLangBox.Text;
        s.SaveDir = SaveDirBox.Text.Trim();
        s.Hotkey = _hotkey;
    }

    private void OnSave(object sender, RoutedEventArgs e)
    {
        _capturing = false;
        CommitFieldsTo(_local);

        if (!string.Equals(_local.Hotkey, App.CurrentSettings.Hotkey, StringComparison.Ordinal))
        {
            if (!_hotkeyManager.ReRegister(_local.Hotkey))
            {
                _local.Hotkey = App.CurrentSettings.Hotkey;
                _hotkey = _local.Hotkey;
                HotkeyBox.Text = _local.Hotkey;
                System.Windows.MessageBox.Show("新快捷键无法注册，可能已被其他程序占用。已保留原快捷键。",
                    "CapturePlus", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Warning);
            }
        }

        AppSettingsService.Save(_local);
        App.CurrentSettings = _local;
        System.Windows.MessageBox.Show("设置已保存。", "CapturePlus", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Information);
        Close();
    }

    private void OnCancel(object sender, RoutedEventArgs e) => Close();
}
