<#
.SYNOPSIS
Verifies that table serialization corruption aborts Save without replacing the
original FB2 and makes every later Save fail closed.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90,
    [ValidateSet('drop-row-after-normalize', 'change-colspan-after-normalize')]
    [string]$Fault = 'drop-row-after-normalize'
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$directory = Join-Path $repositoryRoot ('out\tests\fbe table failure ' + [guid]::NewGuid().ToString('N'))
$runtime = Join-Path $directory 'runtime'
$fixture = Join-Path $directory 'table.fb2'
$report = Join-Path $directory 'report.tsv'
$portableIni = Join-Path $runtime 'portable.ini'
$traceDirectories = @(Join-Path $runtime 'Data\Diagnostics')

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class TableSafetyDialogCloser {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr window, System.Text.StringBuilder className, int maxCount);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    public static void Dismiss(int targetProcessId) {
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            uint processId; GetWindowThreadProcessId(window, out processId);
            System.Text.StringBuilder className = new System.Text.StringBuilder(32);
            GetClassName(window, className, className.Capacity);
            if(processId == (uint)targetProcessId && IsWindowVisible(window) && className.ToString() == "#32770")
                PostMessage(window, 0x0111, (IntPtr)1, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
    }
}
"@

function Get-TraceForProcess([int]$ProcessId, [datetime]$NotBefore) {
    $threshold = $NotBefore.ToUniversalTime().AddSeconds(-2)
    foreach($path in $traceDirectories) {
        if(Test-Path -LiteralPath $path) {
            $trace = Get-ChildItem -LiteralPath $path -Filter ("fbe-trace-*-pid{0}*.log" -f $ProcessId) -File |
                Where-Object { $_.LastWriteTimeUtc -ge $threshold } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
            if($trace) { return $trace.FullName }
        }
    }
}

try {
    # Never use installed mode here: --installed would involve the developer's
    # per-user settings and registry.  A complete copy gives FBE a private
    # executable/Data root and also exercises paths containing spaces.
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Copy-Item -LiteralPath (Split-Path $FbeExe -Parent) -Destination $runtime -Recurse -Force
    "[Portable]`r`nDataPath=Data`r`n`r`n[Diagnostics]`r`nTraceNextLaunch=1`r`n" |
        Set-Content -LiteralPath $portableIni -Encoding utf8NoBOM
@'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>fault</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>fault-table-test</id><version>1.0</version></document-info></description><body><section><table id="fault-table"><tr><td>one</td><td>two</td></tr><tr><td>three</td><td>four</td></tr></table></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $originalHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    $started = Get-Date
    $previousTrace = $env:FBE_NEXT_TRACE
    $env:FBE_NEXT_TRACE = '1'; $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_FAULT_INJECT = $Fault
    $process = Start-Process -FilePath (Join-Path $runtime 'FBE.exe') -WorkingDirectory $runtime `
        -ArgumentList @('-s', '-b', ('"{0}"' -f $report), ('"{0}"' -f $fixture)) -PassThru
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do { Start-Sleep -Milliseconds 200; [TableSafetyDialogCloser]::Dismiss($process.Id); $process.Refresh() } while(-not $process.HasExited -and (Get-Date) -lt $deadline)
    if(-not $process.HasExited) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил test-only table safety pipeline.' }
    if($process.ExitCode -eq 0) { throw 'FBE не вернул код ошибки для отклонённого Save.' }
    if((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $originalHash) { throw 'Инъекция перезаписала исходный FB2.' }
    if(-not (Test-Path -LiteralPath $report)) { throw 'FBE не записал report безопасного отказа.' }
    $rejection = Import-Csv -LiteralPath $report -Delimiter "`t" | Where-Object { $_.phase -like 'table-save-rejected:*' }
    if(@($rejection).Count -ne 1 -or $rejection.phase -notmatch 'second-save-rejected=1') { throw 'После table serialization failure повторный Save не был запрещён.' }
    $trace = Get-TraceForProcess $process.Id $started
    if(-not $trace) { throw 'Не найден диагностический trace для table safety test.' }
    $faultCode = if($Fault -eq 'change-colspan-after-normalize') { 'FI041' } else { 'FI040' }
    foreach($code in @($faultCode, 'D224', 'D223', 'D226')) {
        if(-not (Select-String -LiteralPath $trace -SimpleMatch ("code=" + $code) -Quiet)) { throw "Trace не содержит ${code}: $trace" }
    }
    Write-Host 'Table serialization failure safety passed.'
}
finally {
    Remove-Item Env:FBE_NEXT_TEST_MODE,Env:FBE_NEXT_FAULT_INJECT -ErrorAction SilentlyContinue
    if ($null -eq $previousTrace) { Remove-Item Env:FBE_NEXT_TRACE -ErrorAction SilentlyContinue } else { $env:FBE_NEXT_TRACE = $previousTrace }
    # Keep this isolated directory for failed-run diagnostics; it never holds
    # user state and is covered by /out/ in .gitignore.
}
