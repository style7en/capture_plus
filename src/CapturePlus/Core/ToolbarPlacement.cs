namespace CapturePlus.Core;

public readonly record struct ToolbarPos(double X, double Y);

public static class ToolbarPlacement
{
    public static ToolbarPos Place(
        double selX, double selY, double selW, double selH,
        double screenLeft, double screenTop, double screenW, double screenH,
        double toolbarW, double toolbarH, double margin)
    {
        double x = selX + (selW - toolbarW) / 2;
        double y = selY - margin - toolbarH;

        if (y < screenTop + margin)
            y = selY + selH + margin;

        if (x < screenLeft + margin) x = screenLeft + margin;
        if (x + toolbarW > screenLeft + screenW - margin)
            x = screenLeft + screenW - margin - toolbarW;

        return new ToolbarPos(x, y);
    }
}
