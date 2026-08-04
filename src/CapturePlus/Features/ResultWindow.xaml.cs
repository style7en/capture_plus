using System.Drawing;
using System.Windows;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Features;

public partial class ResultWindow : Window
{
    private static readonly AiService Ai = new();
    private CancellationTokenSource? _cts;

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
        _ = w.RunOcrAsync(bmp, useSystem: true);
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

    private async Task RunOcrAsync(Bitmap bmp, bool useSystem)
    {
        _cts = new CancellationTokenSource();
        SetLoading("正在识别文字…");
        try
        {
            string text;
            if (useSystem)
            {
                text = await OcrService.RecognizeAsync(bmp, App.CurrentSettings.OcrLanguage);
                if (string.IsNullOrWhiteSpace(text))
                {
                    SetError("系统 OCR 未识别到文字。可点“用 AI 重新提取”。");
                    RetryBtn.Content = "用 AI 重新提取";
                    RetryBtn.Visibility = Visibility.Visible;
                    RetryBtn.Click -= OnRetry;
                    RetryBtn.Click += async (_, _) => await RunOcrAsync(bmp, useSystem: false);
                    return;
                }
            }
            else
            {
                text = await Ai.AiOcrAsync(bmp, App.CurrentSettings, _cts.Token);
                if (string.IsNullOrWhiteSpace(text)) text = "AI 未提取到文字。";
            }
            SetResult(text);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("OCR result window failed", ex);
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
            SetResult(string.IsNullOrWhiteSpace(text) ? "（AI 未返回内容）" : text);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("AI analysis failed", ex);
            SetError($"AI 分析失败：{ex.Message}");
        }
    }

    private async Task RunTranslateAsync(Bitmap bmp)
    {
        _cts = new CancellationTokenSource();
        SetLoading("正在识别并翻译…");
        try
        {
            var ocrText = await OcrService.RecognizeAsync(bmp, App.CurrentSettings.OcrLanguage);
            if (string.IsNullOrWhiteSpace(ocrText))
            {
                SetError("未识别到可翻译文字。");
                return;
            }
            var translated = await Ai.TranslateAsync(ocrText, App.CurrentSettings, _cts.Token);
            SetResult(translated);
        }
        catch (OperationCanceledException) { Close(); }
        catch (Exception ex)
        {
            Logger.Error("Translate failed", ex);
            SetError($"翻译失败：{ex.Message}");
        }
    }

    private void SetLoading(string msg)
    {
        ContentBox.Foreground = System.Windows.Media.Brushes.Gray;
        ContentBox.Text = msg;
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
        // Default retry: re-run AI analysis. OCR has its own retry handler swap.
    }

    private void OnClose(object sender, RoutedEventArgs e) => Close();
}
