using System.Drawing;
using System.Windows;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Features;

public partial class ResultWindow : Window
{
    private static readonly AiService Ai = new();
    private CancellationTokenSource? _cts;
    private Func<Task>? _retryAction;

    private ResultWindow(string title)
    {
        InitializeComponent();
        TitleText.Text = title;
        Closed += (_, _) => _cts?.Cancel();
    }

    public static ResultWindow ShowOcrAsync(Bitmap bmp)
    {
        var w = new ResultWindow("提取文字");
        w.Show();
        w.RetryBtn.Visibility = Visibility.Collapsed;
        _ = w.RunOcrAsync(bmp);
        return w;
    }

    public static ResultWindow ShowAiAnalysisAsync(Bitmap bmp)
    {
        var w = new ResultWindow("AI 分析");
        w.Show();
        _ = w.RunAiAsync(bmp);
        return w;
    }

    public static ResultWindow ShowTranslateAsync(Bitmap bmp)
    {
        var w = new ResultWindow($"翻译 → {App.CurrentSettings.TranslateTargetLanguage}");
        w.Show();
        _ = w.RunTranslateAsync(bmp);
        return w;
    }

    private async Task RunOcrAsync(Bitmap bmp)
    {
        _cts = new CancellationTokenSource();
        SetLoading("正在识别文字…");
        try
        {
            var text = await Ai.AiOcrAsync(bmp, App.CurrentSettings, _cts.Token);
            if (string.IsNullOrWhiteSpace(text)) text = "AI 未提取到文字。";
            _retryAction = () => RunOcrAsync(bmp);
            SetResult(text);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("OCR result window failed", ex);
            _retryAction = () => RunOcrAsync(bmp);
            SetError($"识别失败：{ex.Message}");
        }
    }

    private async Task RunAiAsync(Bitmap bmp)
    {
        _cts = new CancellationTokenSource();
        SetLoading("AI 正在分析…");
        try
        {
            var text = await Ai.AnalyzeAsync(bmp, App.CurrentSettings, _cts.Token);
            _retryAction = () => RunAiAsync(bmp);
            SetResult(string.IsNullOrWhiteSpace(text) ? "（AI 未返回内容）" : text);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("AI analysis failed", ex);
            _retryAction = () => RunAiAsync(bmp);
            SetError($"AI 分析失败：{ex.Message}");
        }
    }

    private async Task RunTranslateAsync(Bitmap bmp)
    {
        _cts = new CancellationTokenSource();
        SetLoading("正在识别并翻译…");
        try
        {
            var ocrText = await Ai.AiOcrAsync(bmp, App.CurrentSettings, _cts.Token);
            if (string.IsNullOrWhiteSpace(ocrText))
            {
                SetError("未识别到文字，无法翻译。");
                return;
            }
            var translated = await Ai.TranslateAsync(ocrText, App.CurrentSettings, _cts.Token);
            _retryAction = () => RunTranslateAsync(bmp);
            SetResult(translated);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("Translate failed", ex);
            _retryAction = () => RunTranslateAsync(bmp);
            SetError($"翻译失败：{ex.Message}");
        }
    }

    private void SetLoading(string msg)
    {
        ContentBox.Foreground = System.Windows.Media.Brushes.Gray;
        ContentBox.Text = msg;
        _retryAction = null;
        RetryBtn.Visibility = Visibility.Collapsed;
    }

    private void SetResult(string text)
    {
        ContentBox.Foreground = System.Windows.Media.Brushes.Black;
        ContentBox.Text = text;
        RetryBtn.Visibility = Visibility.Visible;
    }

    private void SetError(string msg)
    {
        ContentBox.Foreground = System.Windows.Media.Brushes.Red;
        ContentBox.Text = msg;
        RetryBtn.Visibility = Visibility.Visible;
    }

    private void OnCopy(object sender, RoutedEventArgs e)
    {
        try { System.Windows.Clipboard.SetText(ContentBox.Text); }
        catch (Exception ex) { Logger.Warn($"Copy result failed: {ex.Message}"); }
    }

    private void OnRetry(object sender, RoutedEventArgs e)
    {
        if (_retryAction is not null) _ = _retryAction();
    }

    private void OnClose(object sender, RoutedEventArgs e) => Close();
}
