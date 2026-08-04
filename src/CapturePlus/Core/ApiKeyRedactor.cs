namespace CapturePlus.Core;

public static class ApiKeyRedactor
{
    public static string Redact(string key)
    {
        if (string.IsNullOrEmpty(key)) return "";
        return "sk-***";
    }
}
