using System.Drawing;
using System.Windows;
using System.Windows.Interop;
using CapturePlus.Logging;

namespace CapturePlus.Features;

public static class CopyImageService
{
    public static void Copy(Bitmap bmp)
    {
        for (int attempt = 0; attempt < 3; attempt++)
        {
            try
            {
                var src = System.Windows.Interop.Imaging.CreateBitmapSourceFromHBitmap(
                    bmp.GetHbitmap(), IntPtr.Zero, System.Windows.Int32Rect.Empty,
                    System.Windows.Media.Imaging.BitmapSizeOptions.FromEmptyOptions());
                src.Freeze();
                var data = new System.Windows.DataObject();
                data.SetData(System.Windows.DataFormats.Bitmap, src);
                System.Windows.Clipboard.SetDataObject(data, copy: true);
                TrayIconAdapter.ShowBalloon("已复制到剪贴板", 500);
                return;
            }
            catch (Exception ex)
            {
                Logger.Warn($"Clipboard attempt {attempt + 1} failed: {ex.Message}");
                Thread.Sleep(100);
            }
        }
        System.Windows.MessageBox.Show("复制到剪贴板失败，请重试。", "CapturePlus",
            System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Warning);
    }
}
