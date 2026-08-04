namespace CapturePlus.Features;

public static class TrayIconAdapter
{
    public static System.Action<string, int>? OnShowBalloon { get; set; }

    public static void ShowBalloon(string message, int durationMs = 1500)
    {
        OnShowBalloon?.Invoke(message, durationMs);
    }
}
