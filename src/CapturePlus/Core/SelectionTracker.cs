using System;

namespace CapturePlus.Core;

public sealed class SelectionTracker
{
    private double _startX, _startY;

    public NormRect Rect { get; private set; }
    public bool IsDragging { get; private set; }
    public bool IsLocked { get; private set; }

    public void Begin(double x, double y)
    {
        if (IsLocked) return;
        _startX = x;
        _startY = y;
        Rect = new NormRect(x, y, 0, 0);
        IsDragging = true;
    }

    public void Update(double x, double y)
    {
        if (!IsDragging || IsLocked) return;
        Rect = SelectionNormalizer.Normalize(_startX, _startY, x - _startX, y - _startY);
    }

    public bool End()
    {
        if (!IsDragging || IsLocked) return false;
        IsDragging = false;
        if (SelectionNormalizer.IsValid(Rect.X, Rect.Y, Rect.Width, Rect.Height))
        {
            IsLocked = true;
            return true;
        }
        Reset();
        return false;
    }

    public void Reset()
    {
        IsDragging = false;
        IsLocked = false;
        Rect = default;
    }
}
