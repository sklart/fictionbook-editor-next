using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

// Read-only Win32 probe. GetDpiForWindow is resolved at runtime for Windows 7.
public static class UiLayoutAudit
{
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct MonitorInfo
    {
        public int Size;
        public Rect Monitor, Work;
        public int Flags;
    }
    public sealed class Row
    {
        public string Hwnd { get; set; }
        public string Parent { get; set; }
        public string Dialog { get; set; }
        public int Id { get; set; }
        public string Class { get; set; }
        public string Text { get; set; }
        public int Dpi { get; set; }
        public Rect Bounds { get; set; }
        public Rect WorkArea { get; set; }
        public bool Enabled { get; set; }
        public bool Decorative { get; set; }
        public bool OutsideWorkArea { get; set; }
        public bool TextMayClip { get; set; }
        public int MeasuredTextWidth { get; set; }
        public int MeasuredTextHeight { get; set; }
        public string FontHandle { get; set; }
    }

    private delegate bool EnumProc(IntPtr hwnd, IntPtr data);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] private delegate uint GetDpiProc(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc callback, IntPtr data);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowEnabled(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] private static extern IntPtr GetParent(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder name, int count);
    [DllImport("user32.dll")] private static extern int GetWindowLong(IntPtr hwnd, int index);
    [DllImport("user32.dll")] private static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern bool GetMonitorInfo(IntPtr monitor, ref MonitorInfo info);
    [DllImport("user32.dll")] private static extern IntPtr GetDC(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern int ReleaseDC(IntPtr hwnd, IntPtr dc);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int DrawText(IntPtr dc, string text, int count, ref Rect rect, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint message, IntPtr wParam, StringBuilder text, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll")] private static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam, uint flags, uint timeout, out IntPtr result);
    [DllImport("gdi32.dll")] private static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);
    [DllImport("gdi32.dll")] private static extern int GetDeviceCaps(IntPtr dc, int index);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi)] private static extern IntPtr GetModuleHandle(string name);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi)] private static extern IntPtr GetProcAddress(IntPtr module, string name);

    private static string Handle(IntPtr value) { return "0x" + value.ToInt64().ToString("X"); }
    private static string ClassName(IntPtr hwnd)
    {
        var buffer = new StringBuilder(256);
        GetClassName(hwnd, buffer, buffer.Capacity);
        return buffer.ToString();
    }
    private static string WindowText(IntPtr hwnd)
    {
        IntPtr length;
        if (SendMessageTimeout(hwnd, 0x000E, IntPtr.Zero, IntPtr.Zero, 0x0002, 250, out length) == IntPtr.Zero)
            return "";
        var buffer = new StringBuilder(Math.Min(65536, Math.Max(1, length.ToInt32()) + 1));
        IntPtr result;
        if (SendMessageTimeout(hwnd, 0x000D, (IntPtr)buffer.Capacity, buffer, 0x0002, 250, out result) == IntPtr.Zero)
            return "";
        return buffer.ToString();
    }
    private static int WindowDpi(IntPtr hwnd)
    {
        IntPtr module = GetModuleHandle("user32.dll");
        IntPtr function = module == IntPtr.Zero ? IntPtr.Zero : GetProcAddress(module, "GetDpiForWindow");
        if (function != IntPtr.Zero)
        {
            uint dpi = ((GetDpiProc)Marshal.GetDelegateForFunctionPointer(function, typeof(GetDpiProc)))(hwnd);
            if (dpi != 0) return (int)dpi;
        }
        IntPtr dc = GetDC(hwnd);
        if (dc == IntPtr.Zero) return 96;
        int fallback = GetDeviceCaps(dc, 88);
        ReleaseDC(hwnd, dc);
        return fallback > 0 ? fallback : 96;
    }
    private static Row CaptureRow(IntPtr hwnd, IntPtr parent, string dialog, Rect work)
    {
        Rect bounds;
        if (!GetWindowRect(hwnd, out bounds)) return null;
        string kind = ClassName(hwnd);
        int style = GetWindowLong(hwnd, -16);
        // Password characters are not visible text and must not enter a report.
        string text = kind.Equals("Edit", StringComparison.OrdinalIgnoreCase) && (style & 0x20) != 0 ? "" : WindowText(hwnd);
        var row = new Row {
            Hwnd = Handle(hwnd), Parent = Handle(parent), Dialog = dialog,
            Id = parent == IntPtr.Zero ? 0 : GetDlgCtrlID(hwnd), Class = kind, Text = text,
            Dpi = WindowDpi(hwnd), Bounds = bounds, WorkArea = work,
            Enabled = IsWindowEnabled(hwnd),
            OutsideWorkArea = bounds.Left < work.Left || bounds.Top < work.Top || bounds.Right > work.Right || bounds.Bottom > work.Bottom
        };
        // Lists, edits, combos, icons and custom controls have their own scrolling/drawing rules.
        bool textControl = kind.Equals("Static", StringComparison.OrdinalIgnoreCase) || kind.Equals("Button", StringComparison.OrdinalIgnoreCase);
        if (!textControl || text.Length == 0 || bounds.Right <= bounds.Left || bounds.Bottom <= bounds.Top) return row;
        row.Decorative = kind.Equals("Static", StringComparison.OrdinalIgnoreCase) && ((style & 0x1F) == 3 || (style & 0x1F) == 14) ||
                         kind.Equals("Button", StringComparison.OrdinalIgnoreCase) && (style & 0xF) == 7;
        if (row.Decorative) return row;
        IntPtr font;
        SendMessageTimeout(hwnd, 0x0031, IntPtr.Zero, IntPtr.Zero, 0x0002, 250, out font);
        row.FontHandle = Handle(font);
        IntPtr dc = GetDC(hwnd);
        if (dc == IntPtr.Zero) return row;
        IntPtr old = font == IntPtr.Zero ? IntPtr.Zero : SelectObject(dc, font);
        int inset = Math.Max(2, row.Dpi / 48);
        int available = Math.Max(1, bounds.Right - bounds.Left - 2 * inset);
        if (kind.Equals("Button", StringComparison.OrdinalIgnoreCase) && ((style & 0xF) == 2 || (style & 0xF) == 3))
            available = Math.Max(1, available - row.Dpi / 5); // checkbox/radio glyph
        Rect measured = new Rect { Left = 0, Top = 0, Right = available, Bottom = 0 };
        bool multiline = kind.Equals("Static", StringComparison.OrdinalIgnoreCase) ||
            (kind.Equals("Button", StringComparison.OrdinalIgnoreCase) && (style & 0x00002000) != 0); // BS_MULTILINE
        uint flags = multiline ? 0x00000410U : 0x00000420U; // CALCRECT with WORDBREAK/SINGLELINE
        // Buttons and ordinary static labels render '&' as a mnemonic marker.
        // Edit text and SS_NOPREFIX labels render it literally.
        if ((!kind.Equals("Button", StringComparison.OrdinalIgnoreCase) &&
             !kind.Equals("Static", StringComparison.OrdinalIgnoreCase)) ||
            (kind.Equals("Static", StringComparison.OrdinalIgnoreCase) && (style & 0x80) != 0))
            flags |= 0x00000800U; // NOPREFIX
        DrawText(dc, text, text.Length, ref measured, flags);
        if (old != IntPtr.Zero) SelectObject(dc, old);
        ReleaseDC(hwnd, dc);
        row.MeasuredTextWidth = measured.Right - measured.Left;
        row.MeasuredTextHeight = measured.Bottom - measured.Top;
        row.TextMayClip = row.MeasuredTextWidth > available || row.MeasuredTextHeight > bounds.Bottom - bounds.Top - 2 * inset;
        return row;
    }
    public static Row[] Capture(int processId)
    {
        var rows = new List<Row>();
        EnumWindows(delegate(IntPtr top, IntPtr unused) {
            uint owner;
            GetWindowThreadProcessId(top, out owner);
            if (owner != (uint)processId || !IsWindowVisible(top)) return true;
            var info = new MonitorInfo { Size = Marshal.SizeOf(typeof(MonitorInfo)) };
            if (!GetMonitorInfo(MonitorFromWindow(top, 2), ref info)) return true;
            string dialog = WindowText(top);
            Row root = CaptureRow(top, IntPtr.Zero, dialog, info.Work);
            if (root != null) rows.Add(root);
            EnumChildWindows(top, delegate(IntPtr child, IntPtr childData) {
                if (!IsWindowVisible(child)) return true;
                Row row = CaptureRow(child, GetParent(child), dialog, info.Work);
                if (row != null) rows.Add(row);
                return true;
            }, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
        return rows.ToArray();
    }
}
