<# Short HTML Options scroll smoke at the current DPI and a synthetic constrained-DPI transition. #>
[CmdletBinding()]
param([string]$FbeExe)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'export-html-layout'
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class ExportHtmlProbeWin32 {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct MinMaxInfo {
        public Point Reserved, MaxSize, MaxPosition, MinTrackSize, MaxTrackSize;
    }
    [StructLayout(LayoutKind.Sequential)] public struct ScrollInfo {
        public uint cbSize, fMask; public int nMin, nMax; public uint nPage; public int nPos, nTrackPos;
    }
    public delegate bool EnumProc(IntPtr hwnd, IntPtr data);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int capacity);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);
    public static MinMaxInfo GetMinimumTrack(IntPtr hwnd) {
        MinMaxInfo empty = new MinMaxInfo();
        IntPtr buffer = Marshal.AllocHGlobal(Marshal.SizeOf(empty));
        try {
            Marshal.StructureToPtr(empty, buffer, false);
            SendMessage(hwnd, 0x24, IntPtr.Zero, buffer); // WM_GETMINMAXINFO
            return (MinMaxInfo)Marshal.PtrToStructure(buffer, typeof(MinMaxInfo));
        } finally { Marshal.FreeHGlobal(buffer); }
    }
    public static void ClickAsync(IntPtr hwnd) {
        var thread = new System.Threading.Thread(() => SendMessage(hwnd, 0x00F5, IntPtr.Zero, IntPtr.Zero));
        thread.IsBackground = true;
        thread.Start();
    }
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr hwnd, EnumProc proc, IntPtr data);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool GetScrollInfo(IntPtr hwnd, int bar, ref ScrollInfo info);
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr hwnd, IntPtr dc);
    [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr dc, int index);
    public static int WindowDpi(IntPtr hwnd) {
        try {
            uint windowDpi = GetDpiForWindow(hwnd);
            if (windowDpi > 0) return (int)windowDpi;
        } catch (EntryPointNotFoundException) { /* Windows 7 uses the display DC below. */ }
        IntPtr dc = GetDC(hwnd);
        if (dc == IntPtr.Zero) return 96;
        int dpi = GetDeviceCaps(dc, 88); // LOGPIXELSX, available on Windows 7.
        ReleaseDC(hwnd, dc);
        return dpi > 0 ? dpi : 96;
    }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int capacity);
    public static IntPtr FindFrame(uint pid) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hwnd,data) => {
            uint owner; GetWindowThreadProcessId(hwnd, out owner);
            var text = new StringBuilder(128); GetClassName(hwnd, text, text.Capacity);
            if (owner == pid && text.ToString() == "FictionBookEditorFrame") { found=hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static IntPtr FindChildWithText(IntPtr parent, string needle) {
        IntPtr found = IntPtr.Zero;
        EnumChildWindows(parent, (hwnd,data) => {
            var title = new StringBuilder(256); GetWindowText(hwnd, title, title.Capacity);
            if (title.ToString().Contains(needle)) { found = hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static IntPtr FindDescendantId(IntPtr parent, int id) {
        IntPtr found = IntPtr.Zero;
        EnumChildWindows(parent, (hwnd,data) => {
            if (GetDlgCtrlID(hwnd) == id) { found = hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static IntPtr FindTopLevelWithText(uint pid, string needle) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hwnd,data) => {
            uint owner; GetWindowThreadProcessId(hwnd, out owner);
            if (owner != pid) return true;
            var title = new StringBuilder(256); GetWindowText(hwnd, title, title.Capacity);
            if (title.ToString().Contains(needle)) { found = hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@
function Get-Scroll([IntPtr]$window, [int]$bar) {
    $info = [ExportHtmlProbeWin32+ScrollInfo]::new()
    $info.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($info)
    $info.fMask = 7
    if (-not [ExportHtmlProbeWin32]::GetScrollInfo($window, $bar, [ref]$info)) { throw 'GetScrollInfo failed.' }
    return $info
}
$process = $null
$passed = $false
$previousTrace = $env:FBE_NEXT_TRACE
try {
    $env:FBE_NEXT_TRACE = '1'
    $fixture = (Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
    $settingsDirectory = Join-Path $isolation.DataPath 'Settings'
    New-Item -ItemType Directory -Path $settingsDirectory -Force | Out-Null
    $template = [IO.File]::ReadAllText((Join-Path $isolation.Runtime 'Data\Settings\Settings.xml'))
    if ($template -notmatch '<IntefaceLangID>[0-9]+</IntefaceLangID>') { throw 'Interface language setting missing.' }
    $settings = [regex]::Replace($template, '<IntefaceLangID>[0-9]+</IntefaceLangID>',
        '<IntefaceLangID>16506882</IntefaceLangID>', 1)
    [IO.File]::WriteAllText((Join-Path $settingsDirectory 'Settings.xml'), $settings, [Text.UTF8Encoding]::new($false))
    $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', ('"' + $fixture + '"')) -PassThru
    $frame = [IntPtr]::Zero
    for ($i=0; $i -lt 100 -and $frame -eq [IntPtr]::Zero; ++$i) {
        Start-Sleep -Milliseconds 100
        $frame = [ExportHtmlProbeWin32]::FindFrame([uint32]$process.Id)
    }
    if ($frame -eq [IntPtr]::Zero) { throw 'Main frame not found.' }
    $trace = $null
    for ($i = 0; $i -lt 150; ++$i) {
        Start-Sleep -Milliseconds 200
        if (-not $trace) {
            $trace = Get-ChildItem -LiteralPath (Join-Path $isolation.DataPath 'Diagnostics') -File -Filter "*pid$($process.Id).log" -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending | Select-Object -First 1
        }
        if ($trace -and (Select-String -LiteralPath $trace.FullName -Pattern 'code=P150' -Quiet -ErrorAction SilentlyContinue)) { break }
    }
    if (-not $trace -or -not (Select-String -LiteralPath $trace.FullName -Pattern 'code=P150' -Quiet)) { throw 'Plugin initialization did not finish.' }
    $manifest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $isolation.Runtime 'Plugins\plugins.json') | ConvertFrom-Json
    $exports = @($manifest.plugins | Where-Object type -eq 'Export')
    $htmlIndex = -1
    for ($i = 0; $i -lt $exports.Count; ++$i) { if ($exports[$i].id -eq 'export-html') { $htmlIndex = $i; break } }
    $commandMatch = [regex]::Match((Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '..\..\src\fbe\resource.h')),
        '#define\s+ID_PLUGIN_EXPORT_FIRST\s+(\d+)')
    if ($htmlIndex -lt 0 -or -not $commandMatch.Success) { throw 'Export HTML command is missing.' }
    $htmlCommand = [int]$commandMatch.Groups[1].Value + $htmlIndex
    [void][ExportHtmlProbeWin32]::PostMessage($frame, 0x111, [IntPtr]$htmlCommand, [IntPtr]::Zero)
    $saveDialog = [IntPtr]::Zero
    for ($i=0; $i -lt 50 -and $saveDialog -eq [IntPtr]::Zero; ++$i) {
        Start-Sleep -Milliseconds 100
        $saveDialog = [ExportHtmlProbeWin32]::FindTopLevelWithText([uint32]$process.Id, 'Сохранение')
    }
    if ($saveDialog -eq [IntPtr]::Zero) { throw 'HTML Save dialog missing.' }
    $optionsButton = [ExportHtmlProbeWin32]::FindChildWithText($saveDialog, 'Параметры экспорта HTML')
    if ($optionsButton -eq [IntPtr]::Zero) { throw 'HTML options button missing.' }
    [void][ExportHtmlProbeWin32]::SetForegroundWindow($saveDialog)
    Start-Sleep -Milliseconds 300
    [ExportHtmlProbeWin32]::ClickAsync($optionsButton)
    $options = [IntPtr]::Zero
    for ($i=0; $i -lt 50 -and $options -eq [IntPtr]::Zero; ++$i) {
        Start-Sleep -Milliseconds 100
        $options = [ExportHtmlProbeWin32]::FindTopLevelWithText([uint32]$process.Id, 'HTML')
    }
    if ($options -eq [IntPtr]::Zero) { throw 'HTML Options dialog missing.' }
    $structure = [ExportHtmlProbeWin32]::FindDescendantId($options, 1057)
    if ($structure -eq [IntPtr]::Zero) { throw 'Document structure control missing.' }
    $page = [ExportHtmlProbeWin32]::GetParent($structure)
    $original = [ExportHtmlProbeWin32+Rect]::new()
    [void][ExportHtmlProbeWin32]::GetWindowRect($options, [ref]$original)
    $width = $original.Right - $original.Left
    $height = $original.Bottom - $original.Top
    if (-not [ExportHtmlProbeWin32]::SetWindowPos($options, [IntPtr]::Zero, $original.Left, $original.Top, ($width - 180), ($height - 100), 0x14)) {
        throw 'Cannot request a smaller HTML Options window.'
    }
    Start-Sleep -Milliseconds 300
    $actual = [ExportHtmlProbeWin32+Rect]::new()
    [void][ExportHtmlProbeWin32]::GetWindowRect($options, [ref]$actual)
    $horizontal = Get-Scroll $page 0
    $vertical = Get-Scroll $page 1
    if ($horizontal.nMax -ge $horizontal.nPage -or $vertical.nMax -ge $vertical.nPage) {
        throw 'The normal minimum size still shows a needless scrollbar.'
    }
    $minimumBefore = [ExportHtmlProbeWin32]::GetMinimumTrack($options)
    $initialDpi = [ExportHtmlProbeWin32]::WindowDpi($options)
    $probeDpi = [int]($initialDpi * 2)
    $suggested = [Runtime.InteropServices.Marshal]::AllocHGlobal(16)
    try {
        [Runtime.InteropServices.Marshal]::StructureToPtr($actual, $suggested, $false)
        $dpiWord = [IntPtr]([int]($probeDpi -bor ($probeDpi -shl 16)))
        [void][ExportHtmlProbeWin32]::SendMessage($options, 0x02E0, $dpiWord, $suggested)
    }
    finally { [Runtime.InteropServices.Marshal]::FreeHGlobal($suggested) }
    Start-Sleep -Milliseconds 300
    [void][ExportHtmlProbeWin32]::GetWindowRect($options, [ref]$actual)
    $minimumAfter = [ExportHtmlProbeWin32]::GetMinimumTrack($options)
    if ($minimumAfter.MinTrackSize.X -le $minimumBefore.MinTrackSize.X -or
        $minimumAfter.MinTrackSize.Y -le $minimumBefore.MinTrackSize.Y) {
        throw 'Minimum window size did not increase with the layout DPI.'
    }
    $scaledPage = [ExportHtmlProbeWin32+Rect]::new()
    [void][ExportHtmlProbeWin32]::GetWindowRect($page, [ref]$scaledPage)
    if (-not [ExportHtmlProbeWin32]::SetWindowPos($page, [IntPtr]::Zero, 0, 0,
        [Math]::Max(80, $scaledPage.Right - $scaledPage.Left - 250),
        [Math]::Max(80, $scaledPage.Bottom - $scaledPage.Top - 140), 0x16)) {
        throw 'Cannot constrain the HTML Options page viewport.'
    }
    [void][ExportHtmlProbeWin32]::SendMessage($page, 0x114, [IntPtr]6, [IntPtr]::Zero)
    [void][ExportHtmlProbeWin32]::SendMessage($page, 0x115, [IntPtr]6, [IntPtr]::Zero)
    $horizontal = Get-Scroll $page 0
    $vertical = Get-Scroll $page 1
    if ($horizontal.nMax -lt $horizontal.nPage -or $vertical.nMax -lt $vertical.nPage) {
        throw 'Constrained DPI layout did not enable both scroll directions.'
    }
    $before = [ExportHtmlProbeWin32+Rect]::new()
    [void][ExportHtmlProbeWin32]::GetWindowRect($structure, [ref]$before)
    [void][ExportHtmlProbeWin32]::PostMessage($page, 0x114, [IntPtr]7, [IntPtr]::Zero)
    [void][ExportHtmlProbeWin32]::PostMessage($page, 0x115, [IntPtr]7, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 300
    $after = [ExportHtmlProbeWin32+Rect]::new()
    [void][ExportHtmlProbeWin32]::GetWindowRect($structure, [ref]$after)
    $horizontal = Get-Scroll $page 0
    $vertical = Get-Scroll $page 1
    foreach ($id in 1, 2) {
        $button = [ExportHtmlProbeWin32]::GetDlgItem($options, $id)
        $rect = [ExportHtmlProbeWin32+Rect]::new()
        [void][ExportHtmlProbeWin32]::GetWindowRect($button, [ref]$rect)
        if ($rect.Left -lt $actual.Left -or $rect.Top -lt $actual.Top -or $rect.Right -gt $actual.Right -or $rect.Bottom -gt $actual.Bottom) {
            throw "Button $id escaped the options window."
        }
    }
    if ($horizontal.nPos -le 0 -or $vertical.nPos -le 0 -or $after.Left -ge $before.Left -or $after.Top -ge $before.Top) {
        throw 'Options page did not scroll its content in both directions.'
    }
    Write-Host "HTML Options scroll smoke passed: current DPI $initialDpi, constrained layout $probeDpi, both scroll axes and footer buttons."
    $passed = $true
}
finally {
    $env:FBE_NEXT_TRACE = $previousTrace
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
