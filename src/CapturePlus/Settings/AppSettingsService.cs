using System.IO;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Settings;

public static class AppSettingsService
{
    private static readonly string Dir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "CapturePlus");

    private static readonly string UserFilePath = Path.Combine(Dir, "settings.json");
    private static readonly string ProgramFilePath = Path.Combine(AppContext.BaseDirectory, "settings.json");

    public static AppSettings Load()
    {
        try
        {
            string? baseJson = ReadText(ProgramFilePath);
            string? userJson = ReadText(UserFilePath);

            string? mergedJson = SettingsOverlay.MergeJson(baseJson, userJson);
            if (mergedJson is null)
            {
                Directory.CreateDirectory(Dir);
                Save(AppSettings.Default);
                return AppSettings.Default;
            }

            var effective = AppSettings.FromJson(mergedJson);

            if (userJson is null)
            {
                Directory.CreateDirectory(Dir);
                WriteFile(UserFilePath, AppSettings.ToJson(effective));
            }
            return effective;
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
            WriteFile(UserFilePath, AppSettings.ToJson(settings));
        }
        catch (Exception ex)
        {
            Logger.Error($"Settings save failed: {ex.Message}");
        }
    }

    private static string? ReadText(string path)
    {
        try
        {
            if (path is not null && File.Exists(path)) return File.ReadAllText(path);
            return null;
        }
        catch (Exception ex)
        {
            Logger.Warn($"Settings read failed ({path}): {ex.Message}");
            return null;
        }
    }

    private static void WriteFile(string path, string json)
    {
        var tmp = path + ".tmp";
        File.WriteAllText(tmp, json);
        File.Copy(tmp, path, overwrite: true);
        File.Delete(tmp);
    }
}