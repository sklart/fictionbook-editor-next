[CmdletBinding()]
param([string]$FbvExe)
$ErrorActionPreference = 'Stop'
if (-not $FbvExe) { $FbvExe = Join-Path $PSScriptRoot '..\..\out\Release\FBV.exe' }
$FbvExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbvExe)
if (-not (Test-Path -LiteralPath $FbvExe)) { throw "FBV not found: $FbvExe" }
$fixture = (Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FbvLayoutProbe
{
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    public delegate bool EnumProc(IntPtr hwnd, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr data);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    public static IntPtr Find(int pid)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr data) {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == (uint)pid && GetDlgItem(hwnd, 1000) != IntPtr.Zero) { found = hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@
$previousLocale = $env:FBE_NEXT_UI_LOCALE
$auditPath = Join-Path ([IO.Path]::GetTempPath()) ('fbe-fbv-layout-' + [guid]::NewGuid().ToString('N') + '.json')
$passed = $false
try {
    $env:FBE_NEXT_UI_LOCALE = 'ru-RU'
    $process = Start-Process -FilePath $FbvExe -ArgumentList ('"' + $fixture + '"') -PassThru
}
finally { $env:FBE_NEXT_UI_LOCALE = $previousLocale }
try {
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 30 -and $window -eq [IntPtr]::Zero; ++$attempt) {
        if ($process.HasExited) { throw "FBV exited before layout probe: $($process.ExitCode)" }
        Start-Sleep -Milliseconds 100
        $window = [FbvLayoutProbe]::Find($process.Id)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'FBV main dialog was not created.' }
    & (Join-Path $PSScriptRoot '..\ui\invoke-ui-layout-audit.ps1') -ProcessId $process.Id -Language ru-RU -OutputPath $auditPath
    $audit = Get-Content -Raw -Encoding UTF8 -LiteralPath $auditPath | ConvertFrom-Json
    $buttons = @($audit.controls | Where-Object { $_.Id -eq 1002 -or $_.Id -eq 2 })
    if ($buttons.Count -ne 2 -or @($buttons | Where-Object { $_.TextMayClip }).Count -ne 0) {
        throw 'FBV localized action button text is clipped.'
    }
    if (@($audit.issues | Where-Object { -not $_.ignored -and $_.kind -eq 'possible-overlap' }).Count -ne 0) {
        throw 'FBV controls overlap after fitting the localized buttons.'
    }
    $list = [FbvLayoutProbe]::GetDlgItem($window, 1000)
    $status = [FbvLayoutProbe]::GetDlgItem($window, 1004)
    $beforeWindow = New-Object FbvLayoutProbe+Rect
    $beforeList = New-Object FbvLayoutProbe+Rect
    $beforeStatus = New-Object FbvLayoutProbe+Rect
    [void][FbvLayoutProbe]::GetWindowRect($window, [ref]$beforeWindow)
    [void][FbvLayoutProbe]::GetWindowRect($list, [ref]$beforeList)
    [void][FbvLayoutProbe]::GetWindowRect($status, [ref]$beforeStatus)
    $beforeColumn = [FbvLayoutProbe]::SendMessage($list, 0x101D, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    $expandedWidth = $beforeWindow.Right - $beforeWindow.Left + 120
    if (-not [FbvLayoutProbe]::SetWindowPos($window, [IntPtr]::Zero, $beforeWindow.Left, $beforeWindow.Top,
        $expandedWidth, $beforeWindow.Bottom - $beforeWindow.Top, 0x14)) { throw 'FBV resize failed.' }
    Start-Sleep -Milliseconds 100
    $afterList = New-Object FbvLayoutProbe+Rect
    $afterStatus = New-Object FbvLayoutProbe+Rect
    [void][FbvLayoutProbe]::GetWindowRect($list, [ref]$afterList)
    [void][FbvLayoutProbe]::GetWindowRect($status, [ref]$afterStatus)
    $afterColumn = [FbvLayoutProbe]::SendMessage($list, 0x101D, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    if (($afterList.Right - $afterList.Left) -lt ($beforeList.Right - $beforeList.Left + 100) -or
        ($afterStatus.Right - $afterStatus.Left) -lt ($beforeStatus.Right - $beforeStatus.Left + 100) -or
        $afterColumn -lt ($beforeColumn + 100)) {
        throw "FBV resize regression: list $($beforeList.Right-$beforeList.Left)/$($afterList.Right-$afterList.Left), status $($beforeStatus.Right-$beforeStatus.Left)/$($afterStatus.Right-$afterStatus.Left), column $beforeColumn/$afterColumn"
    }
    Write-Host 'FBV list, filename column and status resize passed.'
    $passed = $true
}
finally {
    if ($window -ne [IntPtr]::Zero) { [void][FbvLayoutProbe]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
    if (-not $process.WaitForExit(1000)) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
    if ($passed) { Remove-Item -LiteralPath $auditPath -Force -ErrorAction SilentlyContinue }
    else { Write-Host "FBV layout failure artifact: $auditPath" }
}
