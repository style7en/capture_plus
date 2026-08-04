using System;

namespace CapturePlus.Core;

public static class SaveFileNameGenerator
{
    public static string Generate(DateTime dt)
        => $"CapturePlus_{dt:yyyyMMdd_HHmmss}";

    public static string WithExtension(DateTime dt, string ext)
        => $"{Generate(dt)}.{ext}";
}
