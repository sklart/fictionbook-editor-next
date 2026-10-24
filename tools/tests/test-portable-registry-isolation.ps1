<#
Exercises both the portable command-line and GUI startup paths while comparing
all FBE-owned HKCU locations before and after them. Neither path may initialise
the registry profile, register the embedded typelib or bundled plugins, or touch
file-association state.
#>
[CmdletBinding()]
param([string]$FbeExecutable, [switch]$SkipPluginSettingsRuntime)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FbeExecutable) { $FbeExecutable = Join-Path $root 'out\Release\FBE.exe' }
$FbeExecutable = (Resolve-Path -LiteralPath $FbeExecutable).Path
$testRoot = Join-Path $root 'out\tests\portable-registry-isolation'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null

function Get-RegistrySnapshot([string]$Key) {
    $output = & reg.exe query $Key /s 2>&1
    if ($LASTEXITCODE -eq 1) { $global:LASTEXITCODE = 0; return '<absent>' }
    if ($LASTEXITCODE -ne 0) { throw "reg.exe query failed for ${Key}: $output" }
    return (($output | ForEach-Object { [string]$_ }) -join "`n").Trim()
}

$keys = @(
    'HKCU\Software\FBETeam\FictionBook Editor Next',
    'HKCU\Software\Classes\FictionBook.2',
    'HKCU\Software\Classes\.fb2',
    'HKCU\Software\Classes\TypeLib\{37B16C7D-4400-4D7D-AA35-14C74E265EA4}',
    'HKCU\Software\Classes\CLSID\{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}',
    'HKCU\Software\Classes\CLSID\{C3098839-EF69-4DE5-B27D-1E80051CA843}',
    'HKCU\Software\Classes\CLSID\{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}',
    'HKCU\Software\Classes\CLSID\{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}'
)
$before = @{}
foreach ($key in $keys) { $before[$key] = Get-RegistrySnapshot $key }

. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExecutable -Name 'portable-registry'
$FbeExecutable = $isolation.Exe
$passed = $false
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class PortableRegistryProbeWindow {
    public delegate bool Callback(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr data);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr window, Callback callback, IntPtr data);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder text, int size);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr window, StringBuilder text, int size);
    [DllImport("user32.dll")] static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window, uint message, IntPtr wp, IntPtr lp);
    public static bool Close(int processId) {
        bool posted=false;
        EnumWindows((window,data)=> {
            uint owner; GetWindowThreadProcessId(window,out owner);
            var name=new StringBuilder(128); GetClassName(window,name,name.Capacity);
            if(owner!=(uint)processId || name.ToString()!="FictionBookEditorFrame") return true;
            posted=PostMessage(window,0x0010,IntPtr.Zero,IntPtr.Zero);
            return false;
        },IntPtr.Zero);
        return posted;
    }
    public static bool DiscardUntitled(int processId) {
        bool posted=false;
        EnumWindows((window,data)=> {
            uint owner; GetWindowThreadProcessId(window,out owner);
            if(owner!=(uint)processId || GetDlgItem(window,6)==IntPtr.Zero ||
                GetDlgItem(window,7)==IntPtr.Zero || GetDlgItem(window,2)==IntPtr.Zero) return true;
            bool untitled=false;
            EnumChildWindows(window,(child,unused)=> {
                var text=new StringBuilder(512); GetWindowText(child,text,text.Capacity);
                if(text.ToString().IndexOf("Untitled.fb2",StringComparison.OrdinalIgnoreCase)>=0) untitled=true;
                return true;
            },IntPtr.Zero);
            if(!untitled) return true;
            posted=PostMessage(window,0x0111,(IntPtr)7,IntPtr.Zero);
            return !posted;
        },IntPtr.Zero);
        return posted;
    }
}
'@
try {
$startupScripts = Join-Path $isolation.Root 'startup-scripts'
$null = New-IsolatedFbeSettingsSeed -Isolation $isolation -StartupScriptsDirectory $startupScripts
[IO.File]::WriteAllText((Join-Path $startupScripts 'registry-probe.js'), 'function Run() {}', [Text.UTF8Encoding]::new($false))
$stdout = Join-Path $testRoot 'portable.stdout'
$stderr = Join-Path $testRoot 'portable.stderr'
Remove-Item -LiteralPath $stdout,$stderr -Force -ErrorAction SilentlyContinue
$process = Start-Process -FilePath $FbeExecutable -ArgumentList @('--portable', '--print-runtime-paths') -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdout -RedirectStandardError $stderr
if ($process.ExitCode -ne 0) { throw "Portable runtime CLI exited with $($process.ExitCode): $(Get-Content -Raw -LiteralPath $stderr)" }
$paths = Get-Content -Raw -LiteralPath $stdout | ConvertFrom-Json
if ($paths.mode -ne 'Portable' -or $paths.registryPersistenceAllowed) { throw 'Portable CLI did not report registry persistence as disabled.' }
if ([IO.Path]::GetFullPath($paths.settingsDirectory).TrimEnd('\') -ne [IO.Path]::GetFullPath((Join-Path $isolation.DataPath 'Settings')).TrimEnd('\')) { throw 'Portable CLI did not use the isolated settings seed.' }

foreach ($key in $keys) {
    $after = Get-RegistrySnapshot $key
    if ($after -cne $before[$key]) { throw "Portable runtime CLI changed FBE-owned registry state: $key" }
}

$previousTrace = $env:FBE_NEXT_TRACE
$guiProcess = $null
try {
    # Observe normal startup; enabling the production trace does not enable
    # test-mode or bypass any of the registry persistence paths under test.
    $env:FBE_NEXT_TRACE = '1'
    $guiProcess = Start-Process -FilePath $FbeExecutable -ArgumentList @('--portable') -WorkingDirectory (Split-Path -Parent $FbeExecutable) -WindowStyle Hidden -PassThru
    $ready = $false
    $startupDeadline = [DateTime]::UtcNow.AddSeconds(90)
    do {
        $guiProcess.Refresh()
        if ($guiProcess.HasExited) { throw "Portable GUI exited during startup with $($guiProcess.ExitCode)." }
        if (Test-Path -LiteralPath $paths.diagnosticsDirectory) {
            $trace = Get-ChildItem -LiteralPath $paths.diagnosticsDirectory -Filter ("*pid{0}*.log" -f $guiProcess.Id) -File -Recurse |
                Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
            if ($trace -and (Select-String -LiteralPath $trace.FullName -SimpleMatch 'code=S230' -Quiet)) { $ready = $true; break }
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $startupDeadline)
    if (-not $ready) { throw "Portable GUI did not reach main-frame-ready S230; diagnostics: $($paths.diagnosticsDirectory)" }
    Start-Sleep -Seconds 5
    $guiProcess.Refresh()
    if ($guiProcess.HasExited) { throw "Portable GUI exited during startup with $($guiProcess.ExitCode)." }
    if (-not [PortableRegistryProbeWindow]::Close($guiProcess.Id)) { throw 'Portable GUI frame was not found for WM_CLOSE.' }
    $closeDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $discardedUntitled = $false
    while (-not $guiProcess.WaitForExit(100)) {
        if ([PortableRegistryProbeWindow]::DiscardUntitled($guiProcess.Id)) { $discardedUntitled = $true }
        if ([DateTime]::UtcNow -gt $closeDeadline) { throw 'Portable GUI did not close normally after WM_CLOSE.' }
    }
    $guiProcess.WaitForExit(); $guiProcess.Refresh()
    if ($guiProcess.ExitCode -ne 0) { throw "Portable GUI exited with $($guiProcess.ExitCode)." }
    Write-Host 'Portable ordinary startup reached S230 and closed normally with exit 0.'
    if ($discardedUntitled) { Write-Host 'Answered No only to the owned Untitled.fb2 Yes/No/Cancel save prompt.' }
}
finally {
    $env:FBE_NEXT_TRACE = $previousTrace
    if ($guiProcess) {
        $guiProcess.Refresh()
        if (-not $guiProcess.HasExited) {
            Stop-Process -Id $guiProcess.Id -Force
            $guiProcess.WaitForExit()
        }
    }
}

foreach ($key in $keys) {
    $after = Get-RegistrySnapshot $key
    if ($after -cne $before[$key]) { throw "Portable GUI startup changed FBE-owned registry state: $key" }
}

# Load all four production DLLs and exercise their actual settings dialogs,
# restart, relocation and the Installed/Portable persistence boundaries.
if (-not $SkipPluginSettingsRuntime) {
    & (Join-Path $PSScriptRoot 'test-plugin-settings-runtime.ps1') -FbeExe $FbeExecutable
}
Write-Host 'Portable CLI, GUI and bundled plug-in registry isolation behavior passed.'

$passed = $true
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
