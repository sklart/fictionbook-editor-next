[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [int]$TimeoutSeconds = 90,
    [switch]$Trace
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDir = Join-Path $repoRoot "out\$Configuration"
$executable = Join-Path $outputDir "FBE.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Не найден исполняемый файл FBE: $executable"
}

$traceFile = Join-Path $env:LOCALAPPDATA "FBE\startup-trace.log"
$previousTraceSetting = $env:FBE_STARTUP_TRACE
if ($Trace) {
    $env:FBE_STARTUP_TRACE = "1"
}

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class FbeStartupWindow {
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
}
"@

$process = Start-Process -FilePath $executable -WorkingDirectory $outputDir -PassThru
$started = Get-Date
$deadline = $started.AddSeconds($TimeoutSeconds)
$traceCompleted = -not $Trace
try {
    do {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        if ($Trace -and (Test-Path -LiteralPath $traceFile -PathType Leaf)) {
            $traceCompleted = Select-String -LiteralPath $traceFile `
                -SimpleMatch "main frame OnCreate completed" -Quiet
        }
    }
    while (-not $process.HasExited -and
        (-not $process.Responding -or $process.MainWindowHandle -eq 0 -or
            -not $traceCompleted) -and
        (Get-Date) -lt $deadline)

    if ($process.HasExited) {
        throw "FBE завершился во время запуска с кодом $($process.ExitCode)."
    }
    if (-not $process.Responding -or $process.MainWindowHandle -eq 0) {
        throw "FBE не успел создать отзывчивое главное окно за $TimeoutSeconds секунд."
    }
    if (-not $traceCompleted) {
        throw "FBE не завершил инициализацию главного окна за $TimeoutSeconds секунд."
    }
    if (-not [FbeStartupWindow]::IsWindowVisible([IntPtr]$process.MainWindowHandle)) {
        throw "FBE создал главное окно, но оно скрыто."
    }

    $elapsed = [int]((Get-Date) - $started).TotalSeconds
    Write-Host "Проверка видимого запуска FBE прошла успешно за $elapsed секунд."
    if ($Trace) {
        if (-not (Test-Path -LiteralPath $traceFile -PathType Leaf)) {
            throw "Не создан startup trace: $traceFile"
        }

        Write-Host "Startup trace:"
        Get-Content -LiteralPath $traceFile
    }
}
catch {
    if ($Trace -and (Test-Path -LiteralPath $traceFile -PathType Leaf)) {
        Write-Warning "Частичный startup trace:"
        Get-Content -LiteralPath $traceFile | Write-Warning
    }
    throw
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        [void]$process.WaitForExit(10000)
        if (-not $process.HasExited) {
            Write-Warning "Тестовый процесс FBE не завершился в течение 10 секунд."
        }
    }
    $env:FBE_STARTUP_TRACE = $previousTraceSetting
}
