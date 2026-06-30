[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [ValidateSet("Win32", "x64")]
    [string]$Platform = "x64",

    [string]$ShellDllPath = "",

    [switch]$SkipBuild,

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
$propertyHandlerClsid = "{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}"
$thumbnailProviderClsid = "{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}"
$fb2ProgId = "FictionBook.2"
$propertyHandlerRegistryKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2"
$thumbnailProviderRegistryKey = "HKLM:\SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
$thumbnailProviderProgIdRegistryKey = "HKLM:\SOFTWARE\Classes\$fb2ProgId\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
$thumbnailProviderClsidRegistryKey = "HKLM:\SOFTWARE\Classes\CLSID\$thumbnailProviderClsid"
$thumbnailProviderInprocRegistryKey = Join-Path $thumbnailProviderClsidRegistryKey "InprocServer32"
$thumbnailProviderProgrammableRegistryKey = Join-Path $thumbnailProviderClsidRegistryKey "Programmable"
$repairShellRegistrationScript = Join-Path $repoRoot "tools\build\repair-fb2-shell-registration.ps1"

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

function Ensure-RegistryDefaultValue {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$Value
    )

    New-Item -Path $Path -Force | Out-Null
    Set-ItemProperty -Path $Path -Name "(default)" -Value $Value
}

if (-not (Test-IsAdministrator)) {
    throw "Для регистрации экспериментального shell-контура нужны права администратора."
}

if (-not $SkipBuild) {
    & (Join-Path $repoRoot "tools\build\build-experimental-property-handler.ps1") `
        -Configuration $Configuration `
        -Platform $Platform
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path -LiteralPath $fbshellPath -PathType Leaf)) {
    throw "Не найден FBShell.dll: $fbshellPath"
}

Write-Host "Регистрация экспериментального modern shell-контура через regsvr32:"
Write-Host "  $fbshellPath"

& (Get-RegSvr32Path -Platform $Platform) /s $fbshellPath
if ($LASTEXITCODE -ne 0) {
    throw "regsvr32 завершился с кодом $LASTEXITCODE."
}

New-Item -Path $propertyHandlerRegistryKey -Force | Out-Null
Set-ItemProperty -Path $propertyHandlerRegistryKey -Name "(default)" -Value $propertyHandlerClsid
Write-Host "Прописан PropertySystem\PropertyHandlers\.fb2 -> $propertyHandlerClsid"

New-Item -Path $thumbnailProviderRegistryKey -Force | Out-Null
Set-ItemProperty -Path $thumbnailProviderRegistryKey -Name "(default)" -Value $thumbnailProviderClsid
Write-Host "Прописан thumbnail handler .fb2 -> $thumbnailProviderClsid"

New-Item -Path $thumbnailProviderProgIdRegistryKey -Force | Out-Null
Set-ItemProperty -Path $thumbnailProviderProgIdRegistryKey -Name "(default)" -Value $thumbnailProviderClsid
Write-Host "Прописан thumbnail handler $fb2ProgId -> $thumbnailProviderClsid"

if (-not (Test-Path -LiteralPath $thumbnailProviderClsidRegistryKey)) {
    Write-Host "regsvr32 не создал CLSID thumbnail provider автоматически; достраиваем регистрацию вручную."
}

Ensure-RegistryDefaultValue `
    -Path $thumbnailProviderClsidRegistryKey `
    -Value "FictionBook Modern Thumbnail Provider (Experimental)"
Ensure-RegistryDefaultValue `
    -Path $thumbnailProviderInprocRegistryKey `
    -Value $fbshellPath
Set-ItemProperty -Path $thumbnailProviderInprocRegistryKey -Name "ThreadingModel" -Value "Apartment"
New-Item -Path $thumbnailProviderProgrammableRegistryKey -Force | Out-Null
Write-Host "Проверена COM-регистрация CLSID thumbnail provider."

if (Test-Path -LiteralPath $repairShellRegistrationScript -PathType Leaf) {
    Write-Host "Синхронизируем per-user shell-регистрацию .fb2 (ProgID, иконки, InfoTip, PreviewDetails)..."
    & $repairShellRegistrationScript -NoExplorerRefresh -Confirm:$false
    if ($LASTEXITCODE -ne 0) {
        throw "repair-fb2-shell-registration.ps1 завершился с кодом $LASTEXITCODE."
    }
}
else {
    Write-Warning "Не найден repair-fb2-shell-registration.ps1; пропускаем синхронизацию per-user shell-регистрации."
}

if (-not $NoRestartExplorer) {
    Write-Host "Перезапуск explorer.exe для применения изменений..."
    Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    Start-Process explorer.exe
}

Write-Host "Экспериментальный modern shell-контур зарегистрирован."
Write-Host "Для ручной GUI-проверки свойств используйте docs/experimental-property-handler-manual-test.md."
Write-Host "Для ручной GUI-проверки миниатюр используйте docs/experimental-thumbnail-provider-manual-test.md."
