using System.Windows;
using System.Windows.Controls;
using CapturePlus.Core;
using CapturePlus.Features;
using CapturePlus.Logging;
using Windows.Media.Ocr;
using Button = System.Windows.Controls.Button;

namespace CapturePlus.Settings;

public partial class SettingsWindow : Window
{
    private readonly AppSettings _local;

    public SettingsWindow()
    {
        InitializeComponent();
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

        try
        {
            OcrLangBox.ItemsSource = OcrEngine.AvailableRecognizerLanguages
                .Select(l => l.LanguageTag).ToList();
        }
        catch (Exception ex) { Logger.Warn($"Could not list OCR languages: {ex.Message}"); }
        OcrLangBox.SelectedItem = _local.OcrLanguage;

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

    private async void OnTestConnection(object sender, RoutedEventArgs e)
    {
        CommitFieldsTo(_local);
        var btn = (Button)sender;
        btn.Content = "测试中…"; btn.IsEnabled = false;
        try
        {
            using var ai = new AiService();
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
        s.OcrLanguage = OcrLangBox.SelectedItem as string ?? "zh-CN";
        s.TranslateTargetLanguage = TranslateLangBox.Text;
        s.SaveDir = SaveDirBox.Text.Trim();
    }

    private void OnSave(object sender, RoutedEventArgs e)
    {
        CommitFieldsTo(_local);
        AppSettingsService.Save(_local);
        App.CurrentSettings = _local;
        System.Windows.MessageBox.Show("设置已保存。", "CapturePlus", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Information);
        Close();
    }

    private void OnCancel(object sender, RoutedEventArgs e) => Close();
}
