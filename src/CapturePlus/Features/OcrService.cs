using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Graphics.Imaging;
using Windows.Media.Ocr;
using CapturePlus.Core;
using CapturePlus.Logging;

namespace CapturePlus.Features;

public static class OcrService
{
    public static async Task<string> RecognizeAsync(Bitmap bmp, string languageTag)
    {
        try
        {
            var langs = OcrEngine.AvailableRecognizerLanguages;
            var lang = langs.FirstOrDefault(l => l.LanguageTag == languageTag)
                    ?? langs.FirstOrDefault(l => l.LanguageTag == "zh-CN")
                    ?? langs.FirstOrDefault(l => l.LanguageTag == "en-US")
                    ?? langs.FirstOrDefault();
            if (lang is null)
            {
                Logger.Warn("No OCR recognizer available on this system.");
                return "";
            }

            var engine = OcrEngine.TryCreateFromLanguage(lang);
            if (engine is null)
            {
                Logger.Warn($"OCR engine could not be created for language {lang.LanguageTag}.");
                return "";
            }
            var softwareBitmap = await ToSoftwareBitmapAsync(bmp);
            var result = await engine.RecognizeAsync(softwareBitmap);
            var lines = result.Lines.Select(l => l.Text).ToArray();
            return OcrTextJoiner.Join(lines);
        }
        catch (Exception ex)
        {
            Logger.Error("OCR failed", ex);
            return "";
        }
    }

    private static Task<SoftwareBitmap> ToSoftwareBitmapAsync(Bitmap bmp)
    {
        var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
        var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try
        {
            var buffer = new byte[data.Stride * bmp.Height];
            Marshal.Copy(data.Scan0, buffer, 0, buffer.Length);
            var sb = new SoftwareBitmap(BitmapPixelFormat.Bgra8, bmp.Width, bmp.Height, BitmapAlphaMode.Premultiplied);
            sb.CopyFromBuffer(buffer.AsBuffer());
            return Task.FromResult(sb);
        }
        finally
        {
            bmp.UnlockBits(data);
        }
    }
}
