[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",

    [string]$ShellDllPath = "",

    [switch]$NoRestartExplorer
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$defaultFbshellPath = if ($Platform -eq "x64") {
    Join-Path $repoRoot "out\package\shell-build\x64\$Configuration\FBShell.dll"
} else {
    Join-Path $repoRoot "out\package\shell-build\Win32\$Configuration\FBShell.dll"
}
$fbshellPath = if ([string]::IsNullOrWhiteSpace($ShellDllPath)) {
    $defaultFbshellPath
} else {
    $ShellDllPath
}
$fb2ProgId = "FictionBook.2"
$propertyHandlerRegistryKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2"
$thumbnailProviderRegistryKey = "HKLM:\SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
$thumbnailProviderProgIdRegistryKey = "HKLM:\SOFTWARE\Classes\$fb2ProgId\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
$thumbnailProviderClsid = "{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}"
$thumbnailProviderClsidRegistryKey = "HKLM:\SOFTWARE\Classes\CLSID\$thumbnailProviderClsid"

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-RegSvr32Path {
    param([string]$Platform)

    if ($Platform -eq "x64") {
        return "$env:SystemRoot\System32\regsvr32.exe"
    }

    return "$env:SystemRoot\SysWOW64\regsvr32.exe"
}

if (-not (Test-IsAdministrator)) {
    throw "Для снятия регистрации экспериментального shell-контура нужны права администратора."
}

if (Test-Path -LiteralPath $fbshellPath -PathType Leaf) {
    Write-Host "Снятие регистрации экспериментального modern shell-контура через regsvr32:"
    Write-Host "  $fbshellPath"

    & (Get-RegSvr32Path -Platform $Platform) /u /s $fbshellPath
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "regsvr32 /u завершился с кодом $LASTEXITCODE. Продолжаем ручную очистку shell-регистрации."
    }
}
else {
    Write-Warning "Не найден FBShell.dll для regsvr32 /u: $fbshellPath. Продолжаем ручную очистку shell-регистрации."
}

if (Test-Path -LiteralPath $propertyHandlerRegistryKey) {
    Remove-Item -LiteralPath $propertyHandlerRegistryKey -Force
    Write-Host "Удалена ассоциация PropertySystem\PropertyHandlers\.fb2"
}

if (Test-Path -LiteralPath $thumbnailProviderRegistryKey) {
    Remove-Item -LiteralPath $thumbnailProviderRegistryKey -Force
    Write-Host "Удалена ассоциация thumbnail handler для .fb2"
}

if (Test-Path -LiteralPath $thumbnailProviderProgIdRegistryKey) {
    Remove-Item -LiteralPath $thumbnailProviderProgIdRegistryKey -Force
    Write-Host "Удалена ассоциация thumbnail handler для $fb2ProgId"
}

if (Test-Path -LiteralPath $thumbnailProviderClsidRegistryKey) {
    Remove-Item -LiteralPath $thumbnailProviderClsidRegistryKey -Recurse -Force
    Write-Host "Удалён CLSID experimental thumbnail provider."
}

if (-not $NoRestartExplorer) {
    Write-Host "Перезапуск explorer.exe для применения изменений..."
    Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    Start-Process explorer.exe
}

Write-Host "Экспериментальный modern shell-контур снят с регистрации."
