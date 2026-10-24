[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateSet(0, 1, 2)][int]$Theme = 0)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'navigation-startup-mode'
$passed = $false
$process = $null
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class NavigationStartupWindows {
    public delegate bool Callback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, Callback callback, IntPtr data);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    static string ClassName(IntPtr window) { var name = new StringBuilder(128); GetClassName(window, name, name.Capacity); return name.ToString(); }
    public static IntPtr FindMain(int processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, data) => { uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner == (uint)processId && ClassName(window) == "FictionBookEditorFrame") { found = window; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static string ChildClasses(IntPtr parent) {
        var names = new StringBuilder();
        EnumChildWindows(parent, (child, data) => { names.Append(ClassName(child)).Append(','); return true; }, IntPtr.Zero);
        return names.ToString();
    }
    public static IntPtr FindChild(IntPtr parent, string className, int id) {
        IntPtr found = IntPtr.Zero;
        EnumChildWindows(parent, (child, data) => {
            if (ClassName(child) == className && (id < 0 || GetDlgCtrlID(child) == id)) { found = child; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@
$savedTestMode,$savedTestScenario=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO
try {
    # This probe owns a disposable fixture and does not test save prompts.
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='navigation-layout-probe'
    $source = New-IsolatedFbeSettingsSeed -Isolation $isolation
    $settings = [IO.File]::ReadAllText($source)
    if ($settings -notmatch '<DocumentTreeScripts>false</DocumentTreeScripts>') { throw 'Settings fixture has no disabled Scripts mode.' }
    $settings = $settings.Replace('<DocumentTreeScripts>false</DocumentTreeScripts>', '<DocumentTreeScripts>true</DocumentTreeScripts>')
    if ($settings -notmatch '<InterfaceTheme>0</InterfaceTheme>') { throw 'Settings fixture has no automatic theme.' }
    $settings = $settings.Replace('<InterfaceTheme>0</InterfaceTheme>', "<InterfaceTheme>$Theme</InterfaceTheme>")
    # This test covers startup navigation visibility, not catalog-scan throughput.
    # Keep a real script, but avoid making its 15-second visibility deadline
    # depend on the hundreds of unrelated bundled scripts on a developer host.
    $scripts = Join-Path $isolation.Root 'NavigationProbeScripts'
    New-Item -ItemType Directory -Path $scripts | Out-Null
    [IO.File]::WriteAllText((Join-Path $scripts 'Probe.js'), 'function Run() {}', [Text.UTF8Encoding]::new($false))
    $settings = $settings.Replace('</Settings>', ('<ScriptsFolder>' + [Security.SecurityElement]::Escape($scripts) + '</ScriptsFolder></Settings>'))
    [IO.File]::WriteAllText($source, $settings, [Text.UTF8Encoding]::new($false))
    $fixture = (Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
    $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', ('"' + $fixture + '"')) -PassThru
    $main = [IntPtr]::Zero
    $tree = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 150; ++$attempt) {
        if ($process.HasExited) { throw "FBE exited during startup: $($process.ExitCode)" }
        Start-Sleep -Milliseconds 100
        $main = [NavigationStartupWindows]::FindMain($process.Id)
        if ($main -ne [IntPtr]::Zero) { $tree = [NavigationStartupWindows]::FindChild($main, 'DocumentTreeFrame', -1) }
        if ($tree -ne [IntPtr]::Zero -and [NavigationStartupWindows]::IsWindowVisible($tree)) { break }
    }
    if ($main -eq [IntPtr]::Zero) { throw 'Main window with DocumentTreeFrame did not appear.' }
    if ($tree -eq [IntPtr]::Zero) { throw "DocumentTreeFrame missing; child classes: $([NavigationStartupWindows]::ChildClasses($main))" }
    if (-not [NavigationStartupWindows]::IsWindowVisible($tree)) { throw 'DocumentTreeFrame stayed hidden during startup.' }
    $search = [NavigationStartupWindows]::FindChild($tree, 'Edit', 0x7e10)
    $rebar = [NavigationStartupWindows]::FindChild($tree, 'ReBarWindow32', -1)
    $viewBar = [NavigationStartupWindows]::FindChild($tree, 'WTL_CommandBar', -1)
    if ($search -eq [IntPtr]::Zero -or $rebar -eq [IntPtr]::Zero -or $viewBar -eq [IntPtr]::Zero) { throw 'Navigation mode controls are missing.' }
    $searchVisible = [NavigationStartupWindows]::IsWindowVisible($search)
    $rebarVisible = [NavigationStartupWindows]::IsWindowVisible($rebar)
    Write-Host "Startup Scripts search visible: $searchVisible; structure toolbar visible: $rebarVisible"
    if (-not $searchVisible -or $rebarVisible -or [NavigationStartupWindows]::IsWindowVisible($viewBar)) {
        throw 'Startup Scripts mode displays structure controls.'
    }
    # Repeat the theme refresh that used to restore WS_VISIBLE from the rebar's creation style.
    [void][NavigationStartupWindows]::SendMessage($tree, 0x8000 + 0x146, [IntPtr]::Zero, [IntPtr]::Zero)
    if ([NavigationStartupWindows]::IsWindowVisible($rebar)) { throw 'Theme refresh exposed Structure commands in Scripts mode.' }
    [void][NavigationStartupWindows]::SendMessage($tree, 0x0111, [IntPtr]57600, [IntPtr]::Zero)
    if ([NavigationStartupWindows]::IsWindowVisible($search) -or
        -not [NavigationStartupWindows]::IsWindowVisible($rebar) -or
        -not [NavigationStartupWindows]::IsWindowVisible($viewBar)) { throw 'Structure mode did not restore its controls.' }
    [void][NavigationStartupWindows]::SendMessage($tree, 0x0111, [IntPtr]57601, [IntPtr]::Zero)
    if (-not [NavigationStartupWindows]::IsWindowVisible($search) -or
        [NavigationStartupWindows]::IsWindowVisible($rebar) -or
        [NavigationStartupWindows]::IsWindowVisible($viewBar)) { throw 'Returning to Scripts mode exposed Structure controls.' }
    [void][NavigationStartupWindows]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    if (-not $process.WaitForExit(30000)) { throw 'Navigation startup probe did not shut down normally.' }
    if ($process.ExitCode -ne 0) { throw "Navigation startup probe exited with $($process.ExitCode)." }
    $passed = $true
}
finally {
    $env:FBE_NEXT_TEST_MODE=$savedTestMode; $env:FBE_NEXT_TEST_SCENARIO=$savedTestScenario
    if ($process) {
        if (-not $process.HasExited) { [void][NavigationStartupWindows]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
        if (-not $process.WaitForExit(5000)) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $process.WaitForExit() }
    }
    for ($attempt = 0; $attempt -lt 60; ++$attempt) {
        try { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed -ErrorAction Stop; break }
        catch {
            if ($_.Exception.Message -like 'Disposable runtime leaked FBE.exe process*' -or $attempt -eq 59) { throw }
            Start-Sleep -Milliseconds 500
        }
    }
}
