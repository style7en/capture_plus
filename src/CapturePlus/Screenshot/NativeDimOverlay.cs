using System.Drawing;
using System.Runtime.InteropServices;
using CapturePlus.Core;

namespace CapturePlus.Screenshot;

public enum InputPhase { Begin, Move, End }

internal sealed class NativeDimOverlay : IDisposable
{
    private const int WS_POPUP = unchecked((int)0x80000000);
    private const int WS_EX_LAYERED = 0x00080000;
    private const int WS_EX_TOOLWINDOW = 0x00000080;
    private const int WS_EX_TOPMOST = 0x00000008;

    private const int WM_DESTROY = 0x0002;
    private const int WM_PAINT = 0x000F;
    private const int WM_ERASEBKGND = 0x0014;
    private const int WM_LBUTTONDOWN = 0x0201;
    private const int WM_MOUSEMOVE = 0x0200;
    private const int WM_LBUTTONUP = 0x0202;
    private const int WM_KEYDOWN = 0x0100;
    private const int WM_SETCURSOR = 0x0020;
    private const int VK_ESCAPE = 0x1B;
    private const int HTCLIENT = 1;

    private const uint LWA_COLORKEY = 0x00000001;
    private const uint LWA_ALPHA = 0x00000002;
    private const byte DimAlpha = 130;

    private const int SW_SHOWNOACTIVATE = 4;
    private const int SW_HIDE = 0;
    private const uint SWP_NOACTIVATE = 0x0010;
    private const uint SWP_NOOWNERZORDER = 0x0200;
    private const uint SWP_NOSIZE = 0x0001;
    private const uint SWP_NOMOVE = 0x0002;
    private static readonly IntPtr HWND_TOPMOST = new(-1);

    private const string ClassName = "CapturePlus_DimOverlay_3F2A";
    private const int ColorKeyWin32 = 0x00FF00FF; // magenta (BGR)
    private const int WhiteWin32 = 0x00FFFFFF;
    private const int BlackWin32 = 0x00000000;

    private static readonly IntPtr s_hModule = GetModuleHandle(null)!;
    private static readonly WndProcDelegate s_wndProc = new WndProcDelegate(StaticWndProc);
    private static readonly Dictionary<IntPtr, NativeDimOverlay> s_byHwnd = new();
    private static readonly IntPtr s_blackBrush = GetStockObject(BLACK_BRUSH);
    private static readonly IntPtr s_magentaBrush = CreateSolidBrush(ColorKeyWin32);
    private static readonly IntPtr s_whitePen = CreatePen(PS_SOLID, 2, WhiteWin32);
    private static readonly IntPtr s_nullBrush = GetStockObject(NULL_BRUSH);
    private static bool s_classRegistered;
    private static readonly IntPtr s_crossCursor = LoadCursor(IntPtr.Zero, IDC_CROSS);

    private const int BLACK_BRUSH = 4;
    private const int NULL_BRUSH = 5;
    private const int IDC_CROSS = 32515;
    private const int PS_SOLID = 0;

    private IntPtr _hwnd;
    private readonly Rectangle _bounds;
    private NormRect? _selection;
    private bool _disposed;

    public event Action<InputPhase, double, double>? SelectionInput;
    public event Action? Cancelled;

    public NativeDimOverlay(Rectangle bounds)
    {
        _bounds = bounds;
        RegisterClass();
        _hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            ClassName, null, WS_POPUP,
            bounds.X, bounds.Y, bounds.Width, bounds.Height,
            IntPtr.Zero, IntPtr.Zero, s_hModule, IntPtr.Zero);
        if (_hwnd == IntPtr.Zero)
            throw new InvalidOperationException("CreateWindowEx failed");
        s_byHwnd[_hwnd] = this;
        SetLayeredWindowAttributes(_hwnd, ColorKeyWin32, DimAlpha, LWA_COLORKEY | LWA_ALPHA);
    }

    public IntPtr Handle => _hwnd;

    public void Show()
    {
        SetWindowPos(_hwnd, HWND_TOPMOST, _bounds.X, _bounds.Y, _bounds.Width, _bounds.Height,
            SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    }

    public void Activate() => SetForegroundWindow(_hwnd);

    public void Hide() => ShowWindow(_hwnd, SW_HIDE);

    public void RenderSelection(NormRect? selection)
    {
        var old = _selection;
        _selection = selection;
        var r = selection ?? old;
        if (r.HasValue)
        {
            var local = ToLocal(r.Value);
            var dirty = new RECT(
                Math.Max(0, (int)Math.Floor(local.X) - 3),
                Math.Max(0, (int)Math.Floor(local.Y) - 3),
                Math.Min(_bounds.Width, (int)Math.Ceiling(local.X + local.Width) + 3),
                Math.Min(_bounds.Height, (int)Math.Ceiling(local.Y + local.Height) + 3));
            InvalidateRect(_hwnd, ref dirty, false);
        }
        else
        {
            InvalidateRect(_hwnd, IntPtr.Zero, true);
        }
    }

    private NormRect ToLocal(NormRect sel)
    {
        return new NormRect(sel.X - _bounds.X, sel.Y - _bounds.Y, sel.Width, sel.Height);
    }

    private static void RegisterClass()
    {
        if (s_classRegistered) return;
        var wc = new WNDCLASSEXW
        {
            cbSize = Marshal.SizeOf<WNDCLASSEXW>(),
            lpfnWndProc = s_wndProc,
            hInstance = s_hModule,
            hCursor = s_crossCursor,
            hbrBackground = IntPtr.Zero,
            lpszClassName = ClassName,
        };
        RegisterClassExW(ref wc);
        s_classRegistered = true;
    }

    private static IntPtr StaticWndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam)
    {
        if (s_byHwnd.TryGetValue(hwnd, out var inst))
            return inst.WndProc(hwnd, msg, wParam, lParam);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam)
    {
        switch (msg)
        {
            case WM_ERASEBKGND:
                return (IntPtr)1;
            case WM_PAINT:
                OnPaint(hwnd);
                return IntPtr.Zero;
            case WM_LBUTTONDOWN:
                SetCapture(hwnd);
                Fire(InputPhase.Begin, lParam);
                return IntPtr.Zero;
            case WM_MOUSEMOVE:
                if ((wParam & 0x0001) != 0)
                    Fire(InputPhase.Move, lParam);
                return IntPtr.Zero;
            case WM_LBUTTONUP:
                ReleaseCapture();
                Fire(InputPhase.End, lParam);
                return IntPtr.Zero;
            case WM_KEYDOWN:
                if ((int)wParam == VK_ESCAPE)
                    Cancelled?.Invoke();
                return IntPtr.Zero;
            case WM_SETCURSOR:
                if ((int)(lParam & 0xFFFF) == HTCLIENT)
                {
                    SetCursor(s_crossCursor);
                    return (IntPtr)1;
                }
                break;
            case WM_DESTROY:
                s_byHwnd.Remove(hwnd);
                _hwnd = IntPtr.Zero;
                break;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    private void Fire(InputPhase phase, IntPtr lParam)
    {
        int x = (short)(lParam & 0xFFFF);
        int y = (short)((lParam >> 16) & 0xFFFF);
        SelectionInput?.Invoke(phase, _bounds.X + x, _bounds.Y + y);
    }

    private void OnPaint(IntPtr hwnd)
    {
        var ps = new PAINTSTRUCT();
        IntPtr hdc = BeginPaint(hwnd, ref ps);
        try
        {
            var client = new RECT(0, 0, _bounds.Width, _bounds.Height);
            FillRect(hdc, ref client, s_blackBrush);

            if (_selection.HasValue)
            {
                var local = ToLocal(_selection.Value);
                var hole = new RECT(
                    (int)Math.Round(local.X),
                    (int)Math.Round(local.Y),
                    (int)Math.Round(local.X + local.Width),
                    (int)Math.Round(local.Y + local.Height));
                if (hole.Right > hole.Left && hole.Bottom > hole.Top)
                {
                    FillRect(hdc, ref hole, s_magentaBrush);
                    var oldPen = SelectObject(hdc, s_whitePen);
                    var oldBrush = SelectObject(hdc, s_nullBrush);
                    Rectangle(hdc, hole.Left - 1, hole.Top - 1, hole.Right + 1, hole.Bottom + 1);
                    SelectObject(hdc, oldPen);
                    SelectObject(hdc, oldBrush);
                }
            }
        }
        finally
        {
            EndPaint(hwnd, ref ps);
        }
    }

    public void Close()
    {
        if (_disposed) return;
        _disposed = true;
        if (_hwnd != IntPtr.Zero)
        {
            s_byHwnd.Remove(_hwnd);
            DestroyWindow(_hwnd);
            _hwnd = IntPtr.Zero;
        }
    }

    public void Dispose() => Close();

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT
    {
        public int Left, Top, Right, Bottom;
        public RECT(int l, int t, int r, int b) { Left = l; Top = t; Right = r; Bottom = b; }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PAINTSTRUCT
    {
        public IntPtr hdc;
        public bool fErase;
        public RECT rcPaint;
        public bool fRestore;
        public bool fIncUpdate;
        public byte Reserved0;
        public byte Reserved1;
        public byte Reserved2;
        public byte Reserved3;
        public byte Reserved4;
        public byte Reserved5;
        public byte Reserved6;
        public byte Reserved7;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct WNDCLASSEXW
    {
        public int cbSize;
        public int style;
        public WndProcDelegate lpfnWndProc;
        public int cbClsExtra;
        public int cbWndExtra;
        public IntPtr hInstance;
        public IntPtr hIcon;
        public IntPtr hCursor;
        public IntPtr hbrBackground;
        public string? lpszMenuName;
        public string lpszClassName;
        public IntPtr hIconSm;
    }

    private delegate IntPtr WndProcDelegate(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandle(string? lpModuleName);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern ushort RegisterClassExW(ref WNDCLASSEXW lpwcx);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateWindowExW(int dwExStyle, string lpClassName, string? lpWindowName,
        int dwStyle, int x, int y, int nWidth, int nHeight,
        IntPtr hWndParent, IntPtr hMenu, IntPtr hInstance, IntPtr lpParam);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter,
        int x, int y, int cx, int cy, uint uFlags);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool SetLayeredWindowAttributes(IntPtr hwnd, int crKey, byte alpha, uint dwFlags);

    [DllImport("user32.dll")]
    private static extern bool InvalidateRect(IntPtr hWnd, ref RECT lpRect, bool bErase);

    [DllImport("user32.dll")]
    private static extern bool InvalidateRect(IntPtr hWnd, IntPtr lpRect, bool bErase);

    [DllImport("user32.dll")]
    private static extern IntPtr BeginPaint(IntPtr hwnd, ref PAINTSTRUCT lpPaint);

    [DllImport("user32.dll")]
    private static extern bool EndPaint(IntPtr hwnd, ref PAINTSTRUCT lpPaint);

    [DllImport("user32.dll")]
    private static extern int FillRect(IntPtr hdc, ref RECT lprc, IntPtr hbr);

    [DllImport("gdi32.dll")]
    private static extern IntPtr CreateSolidBrush(int color);

    [DllImport("gdi32.dll")]
    private static extern IntPtr CreatePen(int style, int width, int color);

    [DllImport("gdi32.dll")]
    private static extern IntPtr SelectObject(IntPtr hdc, IntPtr hgdiobj);

    [DllImport("gdi32.dll")]
    private static extern bool Rectangle(IntPtr hdc, int left, int top, int right, int bottom);

    [DllImport("gdi32.dll")]
    private static extern IntPtr GetStockObject(int fnObject);

    [DllImport("user32.dll")]
    private static extern IntPtr LoadCursor(IntPtr hInstance, int lpCursorName);

    [DllImport("user32.dll")]
    private static extern IntPtr SetCursor(IntPtr hCursor);

    [DllImport("user32.dll")]
    private static extern IntPtr SetCapture(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool ReleaseCapture();

    [DllImport("user32.dll")]
    private static extern bool DestroyWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern IntPtr DefWindowProcW(IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam);
}
