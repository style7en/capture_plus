using System.IO;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Settings;

public static class AppSettingsService
{
    private static readonly string Dir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "CapturePlus");

    private static readonly string FilePath = Path.Combine(Dir, "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (!File.Exists(FilePath))
            {
                Directory.CreateDirectory(Dir);
                Save(AppSettings.Default);
                return AppSettings.Default;
            }
            var json = File.ReadAllText(FilePath);
            return AppSettings.FromJson(json);
        }
        catch (Exception ex)
        {
            Logger.Warn($"Settings load failed, using defaults: {ex.Message}");
            return AppSettings.Default;
        }
    }

    public static void Save(AppSettings settings)
    {
        try
        {
            Directory.CreateDirectory(Dir);
            var json = AppSettings.ToJson(settings);
            var tmp = FilePath + ".tmp";
            File.WriteAllText(tmp, json);
            File.Copy(tmp, FilePath, overwrite: true);
            File.Delete(tmp);
        }
        catch (Exception ex)
        {
            Logger.Error($"Settings save failed: {ex.Message}");
        }
    }
}
