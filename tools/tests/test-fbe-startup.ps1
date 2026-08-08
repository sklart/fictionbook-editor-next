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

$traceDirectories = @(
    (Join-Path $env:LOCALAPPDATA "FBE Next\Diagnostics"),
    (Join-Path $env:TEMP "FBE Next Diagnostics")
)
$traceFile = $null

function Get-TraceFileForProcess([int]$ProcessId, [DateTime]$NotBefore) {
    $threshold = $NotBefore.ToUniversalTime().AddSeconds(-5)
    $files = foreach($directory in $traceDirectories) { if (Test-Path -LiteralPath $directory -PathType Container) { Get-ChildItem -LiteralPath $directory -Filter ("fbe-trace-*-pid{0}*.log" -f $ProcessId) -File | Where-Object { $_.LastWriteTimeUtc -ge $threshold } } }
    return $files | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
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
            Remove-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue
        }
    }
}

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class FbeStartupWindow {
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr window);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr window, System.Text.StringBuilder className, int maxCount);

    public static IntPtr FindVisibleTopLevelWindow(int targetProcessId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId == (uint)targetProcessId && IsWindowVisible(window) && GetWindowTextLength(window) > 0) {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static IntPtr FindVisibleDialog(int targetProcessId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            System.Text.StringBuilder className = new System.Text.StringBuilder(256);
            GetClassName(window, className, className.Capacity);
            if (processId == (uint)targetProcessId && IsWindowVisible(window) && className.ToString() == "#32770") {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
"@

$started = Get-Date
$process = Start-Process -FilePath $executable -WorkingDirectory $outputDir -PassThru
$deadline = $started.AddSeconds($TimeoutSeconds)
$traceCompleted = -not $Trace
$mainWindowHandle = [IntPtr]::Zero
try {
    do {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        $mainWindowHandle = [FbeStartupWindow]::FindVisibleTopLevelWindow($process.Id)
        if ($Trace) {
            $candidateTrace = Get-TraceFileForProcess $process.Id $started
            if ($candidateTrace) { $traceFile = $candidateTrace.FullName }
        }
        if ($Trace -and $traceFile -and (Test-Path -LiteralPath $traceFile -PathType Leaf)) {
            $traceCompleted = Select-String -LiteralPath $traceFile `
                -SimpleMatch "code=S230" -Quiet
        }
    }
    while (-not $process.HasExited -and
        (-not $process.Responding -or $mainWindowHandle -eq [IntPtr]::Zero -or
            -not $traceCompleted) -and
        (Get-Date) -lt $deadline)

    if ($process.HasExited) {
        throw "FBE завершился во время запуска с кодом $($process.ExitCode)."
    }
    if (-not $process.Responding -or $mainWindowHandle -eq [IntPtr]::Zero) {
        throw "FBE не успел создать отзывчивое главное окно за $TimeoutSeconds секунд."
    }
    if (-not $traceCompleted) {
        throw "FBE не завершил инициализацию главного окна за $TimeoutSeconds секунд."
    }
    if (-not [FbeStartupWindow]::IsWindowVisible($mainWindowHandle)) {
        throw "FBE создал главное окно, но оно скрыто."
    }

    if (-not [FbeStartupWindow]::PostMessage($mainWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) { throw "Не удалось отправить WM_CLOSE FBE." }
    $closeDeadline = (Get-Date).AddSeconds(13)
    do {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        if (-not $process.HasExited) {
            $discardDialog = [FbeStartupWindow]::FindVisibleDialog($process.Id)
            if ($discardDialog -ne [IntPtr]::Zero) {
                [void][FbeStartupWindow]::PostMessage($discardDialog, 0x0111, [IntPtr]7, [IntPtr]::Zero)
            }
        }
    }
    while (-not $process.HasExited -and (Get-Date) -lt $closeDeadline)
    if (-not $process.HasExited) {
        if (-not [FbeStartupWindow]::PostMessage($mainWindowHandle, 0x0111, [IntPtr]0xE141, [IntPtr]::Zero)) { throw "Не удалось отправить команду Exit FBE." }
        $closeDeadline = (Get-Date).AddSeconds(10)
        do {
            Start-Sleep -Milliseconds 250
            $process.Refresh()
            if (-not $process.HasExited) {
                $discardDialog = [FbeStartupWindow]::FindVisibleDialog($process.Id)
                if ($discardDialog -ne [IntPtr]::Zero) {
                    [void][FbeStartupWindow]::PostMessage($discardDialog, 0x0111, [IntPtr]7, [IntPtr]::Zero)
                }
            }
        }
        while (-not $process.HasExited -and (Get-Date) -lt $closeDeadline)
    }
    if (-not $process.HasExited) { throw "FBE не завершился штатно после WM_CLOSE и команды Exit." }
    if ($Trace) { foreach($code in @("S900","S999")) { if (-not (Select-String -LiteralPath $traceFile -SimpleMatch ("code=" + $code) -Quiet)) { throw "После WM_CLOSE в журнале нет ${code}: $traceFile" } }; $bytes=[IO.File]::ReadAllBytes($traceFile); try { [void]([Text.UTF8Encoding]::new($false,$true)).GetString($bytes) } catch { throw "Диагностический журнал не является корректным UTF-8: $traceFile" }; if ($bytes.Length -eq 0 -or $bytes[$bytes.Length - 1] -ne 10) { throw "Последняя строка диагностического журнала обрезана: $traceFile" } }
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
    if ($Trace -and $hadTraceRegistryValue) {
        New-Item -Path $traceRegistryPath -Force | Out-Null
        New-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -PropertyType DWord -Value $previousTraceRegistryValue -Force | Out-Null
    }
}
