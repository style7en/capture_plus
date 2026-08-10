using System.Windows;

namespace CapturePlus.Screenshot;

public partial class ToolbarWindow : Window
{
    public event Action<ScreenshotAction>? ActionRequested;
    public event Action? Cancelled;

    public ToolbarWindow()
    {
        InitializeComponent();
    }

    private void OnCopy(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.CopyImage);
    private void OnSave(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.SaveImage);
    private void OnOcr(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.Ocr);
    private void OnAi(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.AiAnalysis);
    private void OnTranslate(object sender, RoutedEventArgs e) => Fire(ScreenshotAction.Translate);
    private void OnCancel(object sender, RoutedEventArgs e) => Cancelled?.Invoke();

    private void Fire(ScreenshotAction action)
    {
        ActionRequested?.Invoke(action);
    }
}

public enum ScreenshotAction { CopyImage, SaveImage, Ocr, AiAnalysis, Translate }
