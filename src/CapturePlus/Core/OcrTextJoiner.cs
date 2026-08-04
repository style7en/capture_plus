namespace CapturePlus.Core;

public static class OcrTextJoiner
{
    public static string Join(string[]? lines)
    {
        if (lines is null || lines.Length == 0) return "";
        var trimmed = lines
            .Select(l => l.Trim())
            .Where(l => l.Length > 0)
            .ToArray();
        return string.Join("\n", trimmed);
    }
}
