using System;

namespace CapturePlus.Core;

public static class RectMath
{
    public static bool Contains(NormRect r, double x, double y)
        => x >= r.X && y >= r.Y && x < r.X + r.Width && y < r.Y + r.Height;

    public static bool Intersects(NormRect a, NormRect b)
        => a.X < b.X + b.Width && b.X < a.X + a.Width
           && a.Y < b.Y + b.Height && b.Y < a.Y + a.Height;

    public static NormRect Intersect(NormRect a, NormRect b)
    {
        double x1 = Math.Max(a.X, b.X);
        double y1 = Math.Max(a.Y, b.Y);
        double x2 = Math.Min(a.X + a.Width, b.X + b.Width);
        double y2 = Math.Min(a.Y + a.Height, b.Y + b.Height);
        return new NormRect(x1, y1, Math.Max(0, x2 - x1), Math.Max(0, y2 - y1));
    }
}
