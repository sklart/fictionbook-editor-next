[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$InstallDirectory = "",
    [switch]$NoExplorerRefresh
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$fb2ProgId = "FictionBook.2"
$productVendor = "FBE Team"
$productName = "FictionBook Editor"
$installDirRegistryKey = "HKCU:\SOFTWARE\$productVendor\$productName"
$uninstallRegistryKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$productName"
$fb2ExtensionKey = "HKCU:\Software\Classes\.fb2"
$fb2ProgIdKey = "HKCU:\Software\Classes\$fb2ProgId"

$shellStrings = [ordered]@{
    InfoTip = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;System.Size"
    TileInfo = "prop:System.Author;System.Title"
    Details = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
    PreviewDetails = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
}

function Resolve-InstallDirectory {
    param([string]$RequestedDirectory)

    if (-not [string]::IsNullOrWhiteSpace($RequestedDirectory)) {
        return (Resolve-Path -LiteralPath $RequestedDirectory -ErrorAction SilentlyContinue)?.Path ?? $RequestedDirectory
    }

    $registryCandidates = @(
        (Get-ItemProperty -Path $installDirRegistryKey -Name "InstallDir" -ErrorAction SilentlyContinue).InstallDir,
        (Get-ItemProperty -Path $uninstallRegistryKey -Name "InstallLocation" -ErrorAction SilentlyContinue).InstallLocation
    )

    foreach ($candidate in $registryCandidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate -PathType Container)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $stageDirectory = Join-Path $repoRoot "out\package\FictionBookEditor"
    if (Test-Path -LiteralPath $stageDirectory -PathType Container) {
        return (Resolve-Path -LiteralPath $stageDirectory).Path
    }

    throw "Не удалось определить каталог установки FBE. Укажите его явно через -InstallDirectory."
}

function Assert-FileExists {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Не найден ${Description}: $Path"
    }
}

function Set-RegistryStringValue {
    param(
        [Parameter(Mandatory)]
        [string]$KeyPath,
        [Parameter(Mandatory)]
        [string]$ValueName,
        [Parameter(Mandatory)]
        [string]$Value
    )

    if ($PSCmdlet.ShouldProcess("$KeyPath [$ValueName]", "Записать строковое значение реестра")) {
        $normalizedPath = $KeyPath
        $hive = $null

        if ($normalizedPath.StartsWith("HKCU:\", [StringComparison]::OrdinalIgnoreCase)) {
            $hive = [Microsoft.Win32.Registry]::CurrentUser
            $normalizedPath = $normalizedPath.Substring(6)
        }
        else {
            throw "Скрипт умеет записывать только per-user ветки HKCU: $KeyPath"
        }

        $registryKey = $hive.CreateSubKey($normalizedPath)
        if ($null -eq $registryKey) {
            throw "Не удалось создать или открыть ключ реестра: $KeyPath"
        }

        try {
            $registryValueName = if ($ValueName -eq "(default)") { "" } else { $ValueName }
            $registryKey.SetValue($registryValueName, $Value, [Microsoft.Win32.RegistryValueKind]::String)
        }
        finally {
            $registryKey.Dispose()
        }
    }
}

function Remove-RegistryValueIfExists {
    param(
        [Parameter(Mandatory)]
        [string]$KeyPath,
        [Parameter(Mandatory)]
        [string]$ValueName
    )

    $normalizedPath = $KeyPath
    $hive = $null

    if ($normalizedPath.StartsWith("HKCU:\", [StringComparison]::OrdinalIgnoreCase)) {
        $hive = [Microsoft.Win32.Registry]::CurrentUser
        $normalizedPath = $normalizedPath.Substring(6)
    }
    else {
        throw "Скрипт умеет работать только с per-user ветками HKCU: $KeyPath"
    }

    $registryKey = $hive.OpenSubKey($normalizedPath, $true)
    if ($null -eq $registryKey) {
        return
    }

    try {
        $registryValueName = if ($ValueName -eq "(default)") { "" } else { $ValueName }
        if ($null -ne $registryKey.GetValue($registryValueName, $null)) {
            $registryKey.DeleteValue($registryValueName, $false)
        }
    }
    finally {
        $registryKey.Dispose()
    }
}

function Ensure-ShellNotifyType {
    if (-not ("FbeShellNotifyNative" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class FbeShellNotifyNative {
    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(uint wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@
    }
}

function Invoke-ShellAssociationRefresh {
    Ensure-ShellNotifyType
    [FbeShellNotifyNative]::SHChangeNotify(0x08000000u, 0x0000u, [IntPtr]::Zero, [IntPtr]::Zero)
}

$InstallDirectory = Resolve-InstallDirectory -RequestedDirectory $InstallDirectory
$fbeExePath = Join-Path $InstallDirectory "FBE.exe"
$fbvExePath = Join-Path $InstallDirectory "FBV.exe"
$fbvVerbResourceModulePath = Join-Path $InstallDirectory "FBVVerbResources.dll"

Assert-FileExists -Path $fbeExePath -Description "FBE.exe"
Assert-FileExists -Path $fbvExePath -Description "FBV.exe"
Assert-FileExists -Path $fbvVerbResourceModulePath -Description "FBVVerbResources.dll"

$defaultIconValue = "$fbeExePath,0"
$editCommandValue = '"' + $fbeExePath + '" "%1"'
$validateIconValue = '"' + $fbvExePath + '",0'
$validateCommandValue = '"' + $fbvExePath + '" "%1"'
$validateMUIVerbValue = '@' + $fbvVerbResourceModulePath + ',-109;v2'

Write-Host "Восстановление per-user shell-регистрации .fb2"
Write-Host "  Каталог установки: $InstallDirectory"
Write-Host "  FBE.exe: $fbeExePath"
Write-Host "  FBV.exe: $fbvExePath"

Set-RegistryStringValue -KeyPath $fb2ProgIdKey -ValueName "(default)" -Value "FictionBook"
Set-RegistryStringValue -KeyPath (Join-Path $fb2ProgIdKey "CurVer") -ValueName "(default)" -Value $fb2ProgId

Set-RegistryStringValue -KeyPath $fb2ExtensionKey -ValueName "(default)" -Value $fb2ProgId
Set-RegistryStringValue -KeyPath $fb2ExtensionKey -ValueName "PerceivedType" -Value "Text"
Set-RegistryStringValue -KeyPath $fb2ExtensionKey -ValueName "Content Type" -Value "text/xml"
Remove-RegistryValueIfExists -KeyPath $fb2ExtensionKey -ValueName "DefaultIcon"
Set-RegistryStringValue -KeyPath (Join-Path $fb2ExtensionKey "DefaultIcon") -ValueName "(default)" -Value $defaultIconValue

Set-RegistryStringValue -KeyPath (Join-Path $fb2ProgIdKey "DefaultIcon") -ValueName "(default)" -Value $defaultIconValue

foreach ($entry in $shellStrings.GetEnumerator()) {
    Set-RegistryStringValue -KeyPath $fb2ProgIdKey -ValueName $entry.Key -Value $entry.Value
}

Set-RegistryStringValue -KeyPath (Join-Path $fb2ProgIdKey "shell\Edit\Command") -ValueName "(default)" -Value $editCommandValue
Set-RegistryStringValue -KeyPath (Join-Path $fb2ProgIdKey "shell\Validate") -ValueName "(default)" -Value "Validate"
Set-RegistryStringValue -KeyPath (Join-Path $fb2ProgIdKey "shell\Validate") -ValueName "MUIVerb" -Value $validateMUIVerbValue
Set-RegistryStringValue -KeyPath (Join-Path $fb2ProgIdKey "shell\Validate") -ValueName "Icon" -Value $validateIconValue
Set-RegistryStringValue -KeyPath (Join-Path $fb2ProgIdKey "shell\Validate\Command") -ValueName "(default)" -Value $validateCommandValue

Set-RegistryStringValue -KeyPath $installDirRegistryKey -ValueName "InstallDir" -Value $InstallDirectory

if (-not $NoExplorerRefresh) {
    if ($PSCmdlet.ShouldProcess("shell32.dll", "Обновить shell-ассоциации через SHChangeNotify")) {
        Invoke-ShellAssociationRefresh
        Write-Host "Проводник уведомлён об обновлении shell-ассоциаций."
    }
}

Write-Host "Восстановление shell-регистрации .fb2 завершено."
Write-Host "  ProgID: $fb2ProgId"
Write-Host "  DefaultIcon: $defaultIconValue"
Write-Host "  Edit: $editCommandValue"
Write-Host "  Validate MUIVerb: $validateMUIVerbValue"
Write-Host "  Validate: $validateCommandValue"

