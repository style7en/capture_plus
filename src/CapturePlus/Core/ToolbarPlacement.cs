namespace CapturePlus.Core;

public readonly record struct ToolbarPos(double X, double Y);

public static class ToolbarPlacement
{
    public static ToolbarPos Place(
        double selX, double selY, double selW, double selH,
        double screenLeft, double screenTop, double screenW, double screenH,
        double toolbarW, double toolbarH, double margin)
    {
        double x = selX + selW + margin;
        double y = selY + selH + margin;

        if (x + toolbarW > screenLeft + screenW)
            x = selX - margin - toolbarW;

        if (y + toolbarH > screenTop + screenH)
            y = selY - margin - toolbarH;

        if (x < screenLeft) x = screenLeft + margin;
        if (y < screenTop) y = screenTop + margin;

        return new ToolbarPos(x, y);
    }
}
