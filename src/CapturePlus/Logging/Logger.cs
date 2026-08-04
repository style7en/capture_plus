using System.IO;
using System.Text;

namespace CapturePlus.Logging;

public enum LogLevel { Info, Warn, Error }

public static class Logger
{
    private static readonly string LogDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "CapturePlus", "logs");

    private static readonly object Gate = new();

    public static void Info(string msg) => Write(LogLevel.Info, msg);
    public static void Warn(string msg) => Write(LogLevel.Warn, msg);
    public static void Error(string msg, Exception? ex = null) =>
        Write(LogLevel.Error, ex is null ? msg : $"{msg}{Environment.NewLine}{ex}");

    private static void Write(LogLevel level, string msg)
    {
        try
        {
            Directory.CreateDirectory(LogDir);
            var path = Path.Combine(LogDir, $"app-{DateTime.Now:yyyy-MM-dd}.log");
            var line = $"[{DateTime.Now:HH:mm:ss.fff}] [{level}] {msg}{Environment.NewLine}";
            lock (Gate) File.AppendAllText(path, line, Encoding.UTF8);
            CleanupOldLogs();
        }
        catch { /* logging must never throw */ }
    }

    private static void CleanupOldLogs()
    {
        try
        {
            var cutoff = DateTime.Now.AddDays(-7);
            foreach (var f in Directory.EnumerateFiles(LogDir, "app-*.log"))
            {
                if (File.GetLastWriteTime(f) < cutoff)
                    File.Delete(f);
            }
        }
        catch { }
    }
}
