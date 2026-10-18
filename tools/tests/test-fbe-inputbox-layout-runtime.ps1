[CmdletBinding()]
param([string]$FbeExe)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name inputbox-layout
$passed = $false
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class InputBoxProbeWindows {
  public delegate bool Callback(IntPtr hwnd, IntPtr data);
  [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct MonitorInfo {
    public int Size;
    public Rect Monitor, Work;
    public int Flags;
  }
  [StructLayout(LayoutKind.Sequential)] public struct ScrollInfo {
    public uint Size, Mask;
    public int Min, Max;
    public uint Page;
    public int Position, TrackPosition;
  }
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool EnumWindows(Callback callback, IntPtr data);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint process);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int size);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int size);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int command);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
  [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint flags);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool GetMonitorInfo(IntPtr monitor, ref MonitorInfo info);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr hwnd, int index);
  [DllImport("user32.dll")] public static extern bool GetScrollInfo(IntPtr hwnd, int bar, ref ScrollInfo info);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint message, IntPtr wp, IntPtr lp, uint flags, uint timeout, out IntPtr result);
  public static IntPtr Find(int processId, bool dialog) {
    IntPtr found=IntPtr.Zero;
    EnumWindows((h,d)=>{
      uint owner; GetWindowThreadProcessId(h,out owner);
      if(owner!=(uint)processId) return true;
      var name=new StringBuilder(128); GetClassName(h,name,name.Capacity);
      if(dialog) {
        if(name.ToString()!="#32770" || GetDlgItem(h,1001)==IntPtr.Zero || !IsWindowVisible(h)) return true;
      } else if(name.ToString()!="FictionBookEditorFrame") return true;
      found=h; return false;
    },IntPtr.Zero);
    return found;
  }
  public static string Class(IntPtr hwnd) {
    var name=new StringBuilder(128); GetClassName(hwnd,name,name.Capacity); return name.ToString();
  }
  public static string[] List(int processId) {
    var result=new System.Collections.Generic.List<string>();
    EnumWindows((h,d)=>{
      uint owner; GetWindowThreadProcessId(h,out owner);
      if(owner==(uint)processId && IsWindowVisible(h)) {
        var name=new StringBuilder(128); var text=new StringBuilder(256);
        GetClassName(h,name,name.Capacity); GetWindowText(h,text,text.Capacity);
        result.Add(name + " | " + text);
      }
      return true;
    },IntPtr.Zero);
    return result.ToArray();
  }
}
'@
[void][InputBoxProbeWindows]::SetProcessDPIAware()
$process = $null
try {
    $scriptFolder = Join-Path $isolation.Runtime 'InputBoxProbe'
    New-Item -ItemType Directory -Path $scriptFolder | Out-Null
    [IO.File]::WriteAllText((Join-Path $scriptFolder 'probe.js'),
        'function Run() { window.external.InputBox(new Array(180).join("Long prompt line for layout verification.\n"), "InputBox layout probe", ""); }',
        [Text.UTF8Encoding]::new($false))
    $uid = '11111111-1111-1111-1111-111111111111'
    $encodedUid = -join ($uid.ToCharArray() | ForEach-Object { '{0:X4}' -f [int][char]$_ })
    $settingsDirectory = Join-Path $isolation.DataPath 'Settings'
    New-Item -ItemType Directory -Path $settingsDirectory -Force | Out-Null
    # Build explicit isolated settings: a fresh build may omit empty fields or
    # serialize them as self-closing elements, which regex replacement missed.
    $settingsFile = New-IsolatedFbeSettingsSeed -Isolation $isolation
    [xml]$settings = [IO.File]::ReadAllText($settingsFile)
    $settingsNode = $settings.SelectSingleNode('/FBE/Settings[@ID="0"]')
    foreach ($entry in @(@('ScriptsFolder', 'InputBoxProbe\'), @('ScriptCommandIds', "1:$encodedUid"))) {
        $node = $settings.CreateElement($entry[0]); $node.InnerText = $entry[1]
        $null = $settingsNode.AppendChild($node)
    }
    [IO.File]::WriteAllText($settingsFile, $settings.OuterXml, [Text.UTF8Encoding]::new($false))
    $registry = '<Scripts version="1">' + "`r`n" +
        '  <Script uid="' + $uid + '" path="probe.js" fingerprint="0000000000000000" />' + "`r`n" +
        '</Scripts>' + "`r`n"
    [IO.File]::WriteAllText((Join-Path $settingsDirectory 'ScriptRegistry.xml'), $registry, [Text.Encoding]::Unicode)
    $fixture = (Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
    $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', ('"' + $fixture + '"')) -PassThru
    $frame = [IntPtr]::Zero
    for ($i=0; $i -lt 80 -and $frame -eq [IntPtr]::Zero; ++$i) {
        Start-Sleep -Milliseconds 100
        if ($process.HasExited) { throw 'FBE exited before opening a document.' }
        $frame = [InputBoxProbeWindows]::Find($process.Id, $false)
    }
    if ($frame -eq [IntPtr]::Zero) { throw 'FBE frame missing.' }
    [void][InputBoxProbeWindows]::ShowWindow($frame, 5)
    Start-Sleep -Seconds 3
    [void][InputBoxProbeWindows]::PostMessage($frame, 0x0111, [IntPtr]9001, [IntPtr]::Zero)
    $dialog = [IntPtr]::Zero
    for ($i=0; $i -lt 80 -and $dialog -eq [IntPtr]::Zero; ++$i) {
        Start-Sleep -Milliseconds 100
        $dialog = [InputBoxProbeWindows]::Find($process.Id, $true)
    }
    if ($dialog -eq [IntPtr]::Zero) { [InputBoxProbeWindows]::List($process.Id); throw 'InputBox did not open.' }
    $prompt = [InputBoxProbeWindows]::GetDlgItem($dialog, 1001)
    if ([InputBoxProbeWindows]::Class($prompt) -ne 'Edit') { throw 'Long prompt did not become a scrollable Edit control.' }
    if (([InputBoxProbeWindows]::GetWindowLong($prompt, -16) -band 0x00200000) -eq 0) { throw 'Long prompt has no vertical scrollbar.' }
    $textLength = [IntPtr]::Zero
    if ([InputBoxProbeWindows]::SendMessageTimeout($prompt, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero, 2, 1000, [ref]$textLength) -eq [IntPtr]::Zero -or
        $textLength.ToInt64() -lt 5000) { throw 'Long prompt text was truncated.' }
    $scroll = New-Object InputBoxProbeWindows+ScrollInfo
    $scroll.Size = [Runtime.InteropServices.Marshal]::SizeOf($scroll)
    $scroll.Mask = 0x3 # SIF_RANGE | SIF_PAGE
    if (-not [InputBoxProbeWindows]::GetScrollInfo($prompt, 1, [ref]$scroll) -or $scroll.Max - $scroll.Min + 1 -le $scroll.Page) {
        throw 'Long prompt has no usable vertical scroll range.'
    }
    $dialogRect = New-Object InputBoxProbeWindows+Rect
    [void][InputBoxProbeWindows]::GetWindowRect($dialog, [ref]$dialogRect)
    $monitor = New-Object InputBoxProbeWindows+MonitorInfo
    $monitor.Size = [Runtime.InteropServices.Marshal]::SizeOf($monitor)
    if (-not [InputBoxProbeWindows]::GetMonitorInfo([InputBoxProbeWindows]::MonitorFromWindow($dialog, 2), [ref]$monitor) -or
        $dialogRect.Left -lt $monitor.Work.Left -or $dialogRect.Top -lt $monitor.Work.Top -or
        $dialogRect.Right -gt $monitor.Work.Right -or $dialogRect.Bottom -gt $monitor.Work.Bottom) {
        throw 'Expanded InputBox extends beyond the monitor work area.'
    }
    foreach ($id in 1001,1000,6,7,2) {
        $child = [InputBoxProbeWindows]::GetDlgItem($dialog, $id)
        if ($child -eq [IntPtr]::Zero) { throw "InputBox control $id missing." }
        $bounds = New-Object InputBoxProbeWindows+Rect
        [void][InputBoxProbeWindows]::GetWindowRect($child, [ref]$bounds)
        if ($bounds.Left -lt $dialogRect.Left -or $bounds.Top -lt $dialogRect.Top -or
            $bounds.Right -gt $dialogRect.Right -or $bounds.Bottom -gt $dialogRect.Bottom) {
            throw "InputBox control $id extends beyond the dialog."
        }
    }
    Write-Host "InputBox long prompt scrolls; bounds $($dialogRect.Left),$($dialogRect.Top)..$($dialogRect.Right),$($dialogRect.Bottom)."
    [void][InputBoxProbeWindows]::PostMessage($dialog, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    $passed = $true
}
finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    for ($attempt=0; $attempt -lt 8; ++$attempt) {
        try { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed; break }
        catch { if ($attempt -eq 7) { throw }; Start-Sleep -Milliseconds 500 }
    }
}
