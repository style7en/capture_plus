namespace CapturePlus.Core;

public static class HotkeyParser
{
    public const uint ModAlt = 0x0001;
    public const uint ModControl = 0x0002;
    public const uint ModShift = 0x0004;
    public const uint ModWin = 0x0008;

    private static readonly Dictionary<string, uint> Modifiers = new(StringComparer.OrdinalIgnoreCase)
    {
        ["Ctrl"] = ModControl,
        ["Alt"] = ModAlt,
        ["Shift"] = ModShift,
        ["Win"] = ModWin,
    };

    private static readonly Dictionary<string, uint> Keys = new(StringComparer.OrdinalIgnoreCase)
    {
        ["A"] = 0x41, ["B"] = 0x42, ["C"] = 0x43, ["D"] = 0x44, ["E"] = 0x45,
        ["F"] = 0x46, ["G"] = 0x47, ["H"] = 0x48, ["I"] = 0x49, ["J"] = 0x4A,
        ["K"] = 0x4B, ["L"] = 0x4C, ["M"] = 0x4D, ["N"] = 0x4E, ["O"] = 0x4F,
        ["P"] = 0x50, ["Q"] = 0x51, ["R"] = 0x52, ["S"] = 0x53, ["T"] = 0x54,
        ["U"] = 0x55, ["V"] = 0x56, ["W"] = 0x57, ["X"] = 0x58, ["Y"] = 0x59,
        ["Z"] = 0x5A,
        ["0"] = 0x30, ["1"] = 0x31, ["2"] = 0x32, ["3"] = 0x33, ["4"] = 0x34,
        ["5"] = 0x35, ["6"] = 0x36, ["7"] = 0x37, ["8"] = 0x38, ["9"] = 0x39,
        ["F1"] = 0x70, ["F2"] = 0x71, ["F3"] = 0x72, ["F4"] = 0x73,
        ["F5"] = 0x74, ["F6"] = 0x75, ["F7"] = 0x76, ["F8"] = 0x77,
        ["F9"] = 0x78, ["F10"] = 0x79, ["F11"] = 0x7A, ["F12"] = 0x7B,
        ["Space"] = 0x20, ["Enter"] = 0x0D, ["Esc"] = 0x1B, ["Tab"] = 0x09,
        ["Backspace"] = 0x08, ["Delete"] = 0x2E, ["Home"] = 0x24, ["End"] = 0x23,
        ["PageUp"] = 0x21, ["PageDown"] = 0x22, ["Left"] = 0x25, ["Right"] = 0x27,
        ["Up"] = 0x26, ["Down"] = 0x28,
    };

    public static bool TryParse(string? text, out uint modifiers, out uint vk)
    {
        modifiers = 0;
        vk = 0;
        if (string.IsNullOrWhiteSpace(text)) return false;

        var parts = text.Split('+', StringSplitOptions.TrimEntries);
        if (parts.Length < 2) return false;

        for (int i = 0; i < parts.Length - 1; i++)
        {
            if (!Modifiers.TryGetValue(parts[i], out var mod)) return false;
            modifiers |= mod;
        }
        if (modifiers == 0) return false;
        return Keys.TryGetValue(parts[^1], out vk);
    }

    public static bool IsSupported(uint vk) => Keys.Values.Contains(vk);

    public static string Format(uint modifiers, uint vk)
    {
        var mods = new List<string>();
        if ((modifiers & ModControl) != 0) mods.Add("Ctrl");
        if ((modifiers & ModAlt) != 0) mods.Add("Alt");
        if ((modifiers & ModShift) != 0) mods.Add("Shift");
        if ((modifiers & ModWin) != 0) mods.Add("Win");
        string key = Keys.FirstOrDefault(kv => kv.Value == vk).Key;
        return string.Join("+", mods.Concat(new[] { key }));
    }
}
