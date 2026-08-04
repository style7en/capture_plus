using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using CapturePlus.Core;
using CapturePlus.Logging;
using Microsoft.Win32;

namespace CapturePlus.Features;

public static class SaveImageService
{
    public static void Save(Bitmap bmp)
    {
        string dir = ((App)App.Current).SaveDir;
        if (string.IsNullOrWhiteSpace(dir) || !Directory.Exists(dir))
        {
            dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.MyPictures),
                "Screenshots");
            if (!Directory.Exists(dir))
                dir = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
        }

        var dlg = new Microsoft.Win32.SaveFileDialog
        {
            FileName = SaveFileNameGenerator.WithExtension(DateTime.Now, "png"),
            InitialDirectory = dir,
            Filter = "PNG (*.png)|*.png|JPEG (*.jpg)|*.jpg|BMP (*.bmp)|*.bmp",
            DefaultExt = "png",
        };

        if (dlg.ShowDialog() != true) return;

        try
        {
            var lower = dlg.FileName.ToLowerInvariant();
            var fmt = lower.EndsWith(".jpg") ? ImageFormat.Jpeg
                    : lower.EndsWith(".bmp") ? ImageFormat.Bmp
                    : ImageFormat.Png;
            bmp.Save(dlg.FileName, fmt);
            TrayIconAdapter.ShowBalloon($"已保存到 {dlg.FileName}", 1500);
        }
        catch (Exception ex)
        {
            Logger.Error("Save image failed", ex);
            System.Windows.MessageBox.Show($"保存失败：\n{ex.Message}", "CapturePlus",
                System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Error);
        }
    }
}
