<#
.SYNOPSIS
Runs the opt-in diagnostic fault points against the real Win32 FBE executable.
#>
[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [int]$TimeoutSeconds = 25
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputDir = Join-Path $repoRoot "out\$Configuration"
$executable = Join-Path $outputDir 'FBE.exe'
if(-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "FBE executable was not found: $executable" }
if(Get-Process FBE -ErrorAction SilentlyContinue) { throw 'Close all FBE instances before running diagnostic fault tests.' }

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class FbeDiagnosticFaultWindow {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr window, System.Text.StringBuilder className, int maxCount);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    public static void DismissVisibleDialogs(int targetProcessId) {
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            System.Text.StringBuilder className = new System.Text.StringBuilder(32);
            GetClassName(window, className, className.Capacity);
            if(processId == (uint)targetProcessId && IsWindowVisible(window) && className.ToString() == "#32770")
                PostMessage(window, 0x0111, (IntPtr)1, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
    }
}
"@

$traceDirectories = @(
    (Join-Path $env:LOCALAPPDATA 'FBE Next\Diagnostics'),
    (Join-Path $env:TEMP 'FBE Next Diagnostics')
)
$traceRegistryPath = 'HKCU:\Software\FBETeam\FictionBook Editor Next\Diagnostics'
$traceRegistryValue = 'TraceNextLaunch'
$hadTraceRegistryValue = $false
$previousTraceRegistryValue = $null
if(Test-Path -LiteralPath $traceRegistryPath) {
    $existingTraceProperty = Get-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -ErrorAction SilentlyContinue
    if($null -ne $existingTraceProperty) {
        $hadTraceRegistryValue = $true
        $previousTraceRegistryValue = [int]$existingTraceProperty.$traceRegistryValue
        Remove-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue
    }
}

function Get-TraceForProcess([int]$ProcessId, [DateTime]$NotBefore) {
    $threshold = $NotBefore.ToUniversalTime().AddSeconds(-2)
    $files = foreach($directory in $traceDirectories) {
        if(Test-Path -LiteralPath $directory -PathType Container) {
            Get-ChildItem -LiteralPath $directory -Filter ("fbe-trace-*-pid{0}*.log" -f $ProcessId) -File |
                Where-Object { $_.LastWriteTimeUtc -ge $threshold }
        }
    }
    return $files | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
}

function Assert-TraceCode([string]$Trace, [string]$Code) {
    if(-not (Select-String -LiteralPath $Trace -SimpleMatch ("code=" + $Code) -Quiet)) {
        throw "Trace does not contain expected ${Code}: $Trace"
    }
}

function Assert-TraceDoesNotContain([string]$Trace, [string]$Text) {
    if(Select-String -LiteralPath $Trace -SimpleMatch $Text -Quiet) {
        throw "Trace unexpectedly contains '$Text': $Trace"
    }
}

function Invoke-DiagnosticFault([string]$Fault, [string[]]$ExpectedCodes) {
    $started = Get-Date
    $process = $null
    $trace = $null
    try {
        $env:FBE_NEXT_TRACE = '1'
        $env:FBE_NEXT_TRACE_VERBOSE = '1'
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_FAULT_INJECT = $Fault
        $process = Start-Process -FilePath $executable -WorkingDirectory $outputDir -PassThru
        $deadline = $started.AddSeconds($TimeoutSeconds)
        do {
            Start-Sleep -Milliseconds 200
            [FbeDiagnosticFaultWindow]::DismissVisibleDialogs($process.Id)
            $traceFile = Get-TraceForProcess $process.Id $started
            if($traceFile) {
                $trace = $traceFile.FullName
                $allPresent = $true
                foreach($code in $ExpectedCodes) {
                    if(-not (Select-String -LiteralPath $trace -SimpleMatch ("code=" + $code) -Quiet)) { $allPresent = $false; break }
                }
                if($allPresent) { break }
            }
            $process.Refresh()
        } while((Get-Date) -lt $deadline -and -not $process.HasExited)

        if(-not $trace) { throw "No diagnostic trace was created for fault '$Fault'." }
        foreach($code in $ExpectedCodes) { Assert-TraceCode $trace $code }
        Assert-TraceDoesNotContain $trace 'code=J299'
        Assert-TraceDoesNotContain $trace 'code=D113'
        Write-Host "Diagnostic fault '$Fault' passed: $trace"
    }
    finally {
        if($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit(10000) | Out-Null
        }
        Remove-Item Env:FBE_NEXT_TRACE,Env:FBE_NEXT_TRACE_VERBOSE,Env:FBE_NEXT_TEST_MODE,Env:FBE_NEXT_FAULT_INJECT -ErrorAction SilentlyContinue
    }
}

try {
    Invoke-DiagnosticFault 'get-extended-style' @('FI000', 'FI820', 'J820', 'XH140', 'J900', 'D115')
    Invoke-DiagnosticFault 'inflate-paragraphs' @('FI000', 'FI595', 'J595', 'XH140', 'J900', 'D115')
    Invoke-DiagnosticFault 'api-load-exception' @('FI000', 'J105', 'J900', 'D115')
    Invoke-DiagnosticFault 'api-load-return-false' @('FI000', 'J106', 'D116', 'D112')
    Invoke-DiagnosticFault 'css-restore-failure' @('FI000', 'J210', 'J212', 'D116', 'D112')
    Write-Host 'Diagnostic fault-injection tests passed.'
}
finally {
    if($hadTraceRegistryValue) {
        New-Item -Path $traceRegistryPath -Force | Out-Null
        New-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -PropertyType DWord -Value $previousTraceRegistryValue -Force | Out-Null
    }
}
