[CmdletBinding()]
param(
    [switch]$NoRestartExplorer
)

# .SYNOPSIS
# Снимает с регистрации FBE-specific property schema и обновляет Property
# System. Повторный запуск допускается: отсутствие зарегистрированной схемы
# трактуется как идемпотентный результат, а не как фатальная ошибка.

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$schemaPath = Join-Path $repoRoot "packaging\property-schema\FBE.Sequence.propdesc"

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsAdministrator)) {
    throw "Для отмены регистрации схемы свойств нужны права администратора."
}

if (-not (Test-Path -LiteralPath $schemaPath -PathType Leaf)) {
    throw "Файл схемы свойств не найден: $schemaPath"
}

if (-not ("FbePropertySchemaUnregisterNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class FbePropertySchemaUnregisterNative {
    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSUnregisterPropertySchema(string path);

    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSRefreshPropertySchema();
}
"@
}

function Test-HResultFailed {
    param([int]$HResult)

    return $HResult -lt 0
}

function Test-IsIgnorableUnregisterHResult {
    param([int]$HResult)

    return (($HResult -band 0xffffffff) -eq 0x80070002)
}

Write-Host "Отмена регистрации схемы свойств:"
Write-Host "  $schemaPath"

$hr = [FbePropertySchemaUnregisterNative]::PSUnregisterPropertySchema($schemaPath)
if ((Test-HResultFailed $hr) -and (-not (Test-IsIgnorableUnregisterHResult $hr))) {
    throw ("Ошибка PSUnregisterPropertySchema: 0x{0:X8}" -f ($hr -band 0xffffffff))
}
if ((Test-IsIgnorableUnregisterHResult $hr)) {
    Write-Host ("PSUnregisterPropertySchema сообщил, что схема уже отсутствует или не найдена в активной регистрации: 0x{0:X8}" -f ($hr -band 0xffffffff))
}
elseif ($hr -ne 0) {
    Write-Host ("PSUnregisterPropertySchema вернул нефатальный статус: 0x{0:X8}" -f ($hr -band 0xffffffff))
}

$refreshHr = [FbePropertySchemaUnregisterNative]::PSRefreshPropertySchema()
if (Test-HResultFailed $refreshHr) {
    throw ("Ошибка PSRefreshPropertySchema: 0x{0:X8}" -f ($refreshHr -band 0xffffffff))
}
if ($refreshHr -ne 0) {
    Write-Host ("PSRefreshPropertySchema вернул нефатальный статус: 0x{0:X8}" -f ($refreshHr -band 0xffffffff))
}

if (-not $NoRestartExplorer) {
    Write-Host "Перезапуск explorer.exe для применения изменений..."
    Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    Start-Process explorer.exe
}

Write-Host "Схема свойств FBE-specific снята с регистрации."
