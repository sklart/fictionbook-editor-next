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

$traceDirectory = Join-Path $env:LOCALAPPDATA "FBE Next\Diagnostics"
$traceFile = $null

function Get-TraceFileForProcess([int]$ProcessId) {
    if (-not (Test-Path -LiteralPath $traceDirectory -PathType Container)) {
        return $null
    }

    return Get-ChildItem -LiteralPath $traceDirectory -Filter ("fbe-trace-*-pid{0}*.log" -f $ProcessId) -File |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
}
$previousTraceSetting = $env:FBE_NEXT_TRACE
$traceRegistryPath = "HKCU:\Software\FBETeam\FictionBook Editor Next\Diagnostics"
$traceRegistryValue = "TraceNextLaunch"
$hadTraceRegistryValue = $false
$previousTraceRegistryValue = $null
if ($Trace) {
    $env:FBE_NEXT_TRACE = "1"
    if (Test-Path -LiteralPath $traceRegistryPath) {
        $existingTraceProperty = Get-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -ErrorAction SilentlyContinue
        if ($null -ne $existingTraceProperty) {
            $hadTraceRegistryValue = $true
            $previousTraceRegistryValue = [int]$existingTraceProperty.$traceRegistryValue
        }
    }
    New-Item -Path $traceRegistryPath -Force | Out-Null
    New-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -PropertyType DWord -Value 1 -Force | Out-Null
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
        if ($Trace) {
            $candidateTrace = Get-TraceFileForProcess $process.Id
            if ($candidateTrace) { $traceFile = $candidateTrace.FullName }
        }
        if ($Trace -and $traceFile -and (Test-Path -LiteralPath $traceFile -PathType Leaf)) {
            $traceCompleted = Select-String -LiteralPath $traceFile `
                -SimpleMatch "code=M160" -Quiet
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
        if (-not $traceFile -or -not (Test-Path -LiteralPath $traceFile -PathType Leaf)) {
            throw "Не создан диагностический журнал: $traceFile"
        }
        if (Select-String -LiteralPath $traceFile -SimpleMatch "code=-" -Quiet) { throw "В диагностическом журнале есть событие без явного code: $traceFile" }
        if (Select-String -LiteralPath $traceFile -Pattern "Р[А-Яа-я]" -Quiet) { throw "В диагностическом журнале обнаружен mojibake: $traceFile" }
        if (Select-String -LiteralPath $traceFile -Pattern "[A-Za-z]:[\\/]" -Quiet) { throw "В диагностическом журнале обнаружен полный путь: $traceFile" }
        if (Select-String -LiteralPath $traceFile -SimpleMatch "file:///" -Quiet) { throw "В диагностическом журнале обнаружен file URL: $traceFile" }
        $traceScriptLookups = @(Select-String -LiteralPath $traceFile -Pattern "code=XH120;.*method=TraceScript")
        if ($traceScriptLookups.Count -gt 1) { throw "TraceScript name-resolution повторяется $($traceScriptLookups.Count) раз: $traceFile" }
        if (-not (Select-String -LiteralPath $traceFile -SimpleMatch "diagnostic trace bridge=available" -Quiet)) {
            throw "В диагностическом журнале не подтверждён доступный TraceScript bridge: $traceFile"
        }
        foreach ($code in @('J100', 'J400', 'J500', 'J599', 'J299')) {
            if (-not (Select-String -LiteralPath $traceFile -SimpleMatch ("code=" + $code) -Quiet)) {
                throw "В диагностическом журнале нет обязательной JavaScript-стадии: $code"
            }
        }
        if (Select-String -LiteralPath $traceFile -SimpleMatch 'level=error' -Quiet) {
            throw "Успешная загрузка создала error-событие: $traceFile"
        }
        if (-not (Select-String -LiteralPath $traceFile -SimpleMatch "category=document;" -Quiet)) {
            throw "В диагностическом журнале нет событий документа: $traceFile"
        }
        if (-not (Select-String -LiteralPath $traceFile -SimpleMatch "external-typeinfo=" -Quiet)) {
            throw "В диагностическом журнале нет состояния window.external: $traceFile"
        }
        if (-not (Select-String -LiteralPath $traceFile -SimpleMatch "apiLoadFB2=" -Quiet)) {
            throw "В диагностическом журнале нет состояния JavaScript API: $traceFile"
        }
        if (-not (Select-String -LiteralPath $traceFile -SimpleMatch "user-agent=" -Quiet)) {
            throw "В диагностическом журнале нет navigator.userAgent: $traceFile"
        }
        if (-not (Select-String -LiteralPath $traceFile -SimpleMatch "app-version=" -Quiet)) {
            throw "В диагностическом журнале нет navigator.appVersion: $traceFile"
        }
        foreach ($code in @('WB111', 'WB112', 'WB200', 'WB210', 'WB220', 'WB230', 'WB240', 'WB250', 'WB270', 'WB295', 'WB299', 'WB199')) {
            if (-not (Select-String -LiteralPath $traceFile -SimpleMatch ("code=" + $code) -Quiet)) {
                throw "В диагностическом журнале нет обязательной стадии WebBrowser: $code"
            }
        }
        if (Select-String -LiteralPath $traceFile -SimpleMatch 'text="' -Quiet) {
            throw "Selection trace не должен содержать поле text: $traceFile"
        }

        Write-Host "Диагностический журнал:"
        Get-Content -LiteralPath $traceFile
    }
}
catch {
    if ($Trace -and $traceFile -and (Test-Path -LiteralPath $traceFile -PathType Leaf)) {
        Write-Warning "Частичный диагностический журнал:"
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
    $env:FBE_NEXT_TRACE = $previousTraceSetting
    if ($Trace) {
        if ($hadTraceRegistryValue) {
            Set-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -Value $previousTraceRegistryValue
        }
        else {
            Remove-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -ErrorAction SilentlyContinue
        }
    }
}
