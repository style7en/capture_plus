using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Interop;
using CapturePlus.Logging;

namespace CapturePlus.Features;

public static class CopyImageService
{
    [DllImport("gdi32.dll")]
    private static extern bool DeleteObject(IntPtr hObject);

    public static void Copy(Bitmap bmp)
    {
        for (int attempt = 0; attempt < 3; attempt++)
        {
            IntPtr hBitmap = IntPtr.Zero;
            try
            {
                hBitmap = bmp.GetHbitmap();
                var src = System.Windows.Interop.Imaging.CreateBitmapSourceFromHBitmap(
                    hBitmap, IntPtr.Zero, System.Windows.Int32Rect.Empty,
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
            finally
            {
                if (hBitmap != IntPtr.Zero) DeleteObject(hBitmap);
            }
        }
        System.Windows.MessageBox.Show("复制到剪贴板失败，请重试。", "CapturePlus",
            System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Warning);
    }
}
