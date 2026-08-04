namespace CapturePlus.Core;

public readonly record struct NormRect(double X, double Y, double Width, double Height);

public static class SelectionNormalizer
{
    public static NormRect Normalize(double x, double y, double w, double h)
    {
        if (w < 0) { x += w; w = -w; }
        if (h < 0) { y += h; h = -h; }
        return new NormRect(x, y, w, h);
    }

    public static bool IsValid(double x, double y, double w, double h)
        => w >= 8 && h >= 8;
}
