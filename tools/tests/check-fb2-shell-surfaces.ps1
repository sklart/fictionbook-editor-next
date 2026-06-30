[CmdletBinding()]
param(
    [string]$FilePath = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$canonicalNames = @(
    "FBE.Genre",
    "FBE.Sequence",
    "FBE.DocumentVersion",
    "FBE.DocumentDate",
    "FBE.Keywords",
    "FBE.DocumentId"
)

$expectedFb2ShellStrings = [ordered]@{
    InfoTip = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;System.Size"
    TileInfo = "prop:System.Author;System.Title"
    Details = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
    PreviewDetails = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
}

$fb2ProgId = "FictionBook.2"
$fb2ProgIdKey = "HKCU:\Software\Classes\$fb2ProgId"
$fb2ExtensionKey = "HKCU:\Software\Classes\.fb2"
$propertyHandlerKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2"
$thumbnailProviderKey = "HKLM:\SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
$thumbnailProviderProgIdKey = "HKLM:\SOFTWARE\Classes\$fb2ProgId\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
$thumbnailProviderClsid = "{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}"
$thumbnailProviderClsidKey = "HKLM:\SOFTWARE\Classes\CLSID\$thumbnailProviderClsid\InprocServer32"
$installDirRegistryKey = "HKCU:\SOFTWARE\FBE Team\FictionBook Editor"
$uninstallRegistryKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\FictionBook Editor"

function Ensure-PropSysProbeType {
    if (-not ("FbeShellSurfaceProbe" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class FbeShellSurfaceProbe {
    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSGetPropertyDescriptionByName(string canonicalName, ref Guid riid, out IntPtr ppv);
}
"@
    }
}

function Get-RegistryDefaultValue {
    param(
        [Parameter(Mandatory)]
        [string]$RegistryPath
    )

    try {
        return (Get-ItemProperty -Path $RegistryPath -ErrorAction Stop)."(default)"
    }
    catch {
        return $null
    }
}

function Get-PropertyProbe {
    param(
        [Parameter(Mandatory)]
        [string]$CanonicalName
    )

    Ensure-PropSysProbeType

    $iid = [Guid]'6F79D558-3E96-4549-A1D1-7D75D2288814'
    $ptr = [IntPtr]::Zero
    $hr = [FbeShellSurfaceProbe]::PSGetPropertyDescriptionByName(
        $CanonicalName,
        [ref]$iid,
        [ref]$ptr
    )

    if ($ptr -ne [IntPtr]::Zero) {
        [Runtime.InteropServices.Marshal]::Release($ptr) | Out-Null
    }

    [pscustomobject]@{
        CanonicalName = $CanonicalName
        HResult = ('0x{0:X8}' -f ($hr -band 0xffffffff))
        Registered = ($hr -ge 0)
    }
}

function Get-Fb2ShellStringState {
    $progIdRegistryPath = "Software\Classes\$fb2ProgId"
    $progIdRegistryKey = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($progIdRegistryPath)

    $result = foreach ($valueName in $expectedFb2ShellStrings.Keys) {
        $actualValue = if ($progIdRegistryKey) { $progIdRegistryKey.GetValue($valueName, $null) } else { $null }

        [pscustomobject]@{
            Name = $valueName
            Present = -not [string]::IsNullOrWhiteSpace($actualValue)
            MatchesExpected = ($actualValue -eq $expectedFb2ShellStrings[$valueName])
            ActualValue = $actualValue
            ExpectedValue = $expectedFb2ShellStrings[$valueName]
        }
    }

    if ($progIdRegistryKey) {
        $progIdRegistryKey.Dispose()
    }

    return @($result)
}

function Resolve-DiagnosticFilePath {
    param([string]$RequestedFilePath)

    if (-not [string]::IsNullOrWhiteSpace($RequestedFilePath)) {
        return (Resolve-Path -LiteralPath $RequestedFilePath -ErrorAction SilentlyContinue)?.Path ?? $RequestedFilePath
    }

    $defaultFilePath = Join-Path $repoRoot "tools\tests\fb2-metadata-smoke.fb2"
    if (Test-Path -LiteralPath $defaultFilePath -PathType Leaf) {
        return (Resolve-Path -LiteralPath $defaultFilePath).Path
    }

    return ""
}

function Get-InstallDirectoryState {
    $installDir = (Get-ItemProperty -Path $installDirRegistryKey -Name "InstallDir" -ErrorAction SilentlyContinue).InstallDir
    $installLocation = (Get-ItemProperty -Path $uninstallRegistryKey -Name "InstallLocation" -ErrorAction SilentlyContinue).InstallLocation
    $resolvedInstallDirectory = $null

    foreach ($candidate in @($installDir, $installLocation)) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate -PathType Container)) {
            $resolvedInstallDirectory = (Resolve-Path -LiteralPath $candidate).Path
            break
        }
    }

    if (-not $resolvedInstallDirectory) {
        $stageDirectory = Join-Path $repoRoot "out\package\FictionBookEditor"
        if (Test-Path -LiteralPath $stageDirectory -PathType Container) {
            $resolvedInstallDirectory = (Resolve-Path -LiteralPath $stageDirectory).Path
        }
    }

    $fbeExePath = if ($resolvedInstallDirectory) { Join-Path $resolvedInstallDirectory "FBE.exe" } else { $null }
    $fbvExePath = if ($resolvedInstallDirectory) { Join-Path $resolvedInstallDirectory "FBV.exe" } else { $null }

    [pscustomobject]@{
        InstallDirFromFbeKey = $installDir
        InstallLocationFromUninstall = $installLocation
        EffectiveInstallDirectory = $resolvedInstallDirectory
        FbeExeExists = [bool]($fbeExePath -and (Test-Path -LiteralPath $fbeExePath -PathType Leaf))
        FbvExeExists = [bool]($fbvExePath -and (Test-Path -LiteralPath $fbvExePath -PathType Leaf))
        FbeExePath = $fbeExePath
        FbvExePath = $fbvExePath
    }
}

function Get-SurfaceAssessment {
    param(
        [Parameter(Mandatory)]
        [object[]]$PropertyStates,
        [Parameter(Mandatory)]
        [object[]]$ShellStringStates,
        [Parameter(Mandatory)]
        [bool]$PropertyHandlerRegistered,
        [Parameter(Mandatory)]
        [bool]$ThumbnailProviderRegistered,
        [Parameter(Mandatory)]
        [bool]$ThumbnailProviderProgIdRegistered,
        [Parameter(Mandatory)]
        [bool]$ThumbnailProviderClsidRegistered,
        [Parameter(Mandatory)]
        [bool]$ProgIdRegistered,
        [AllowEmptyString()]
        [string]$ExtensionProgId = ""
    )

    $allPublishedPropertiesRegistered = @($PropertyStates | Where-Object { -not $_.Registered }).Count -eq 0
    $infoTipReady = @($ShellStringStates | Where-Object { $_.Name -eq "InfoTip" -and $_.MatchesExpected }).Count -eq 1
    $detailsReady = @($ShellStringStates | Where-Object { $_.Name -eq "PreviewDetails" -and $_.MatchesExpected }).Count -eq 1

    $columnsLikelyReady =
        $PropertyHandlerRegistered -and
        $ProgIdRegistered -and
        ($ExtensionProgId -eq $fb2ProgId) -and
        $allPublishedPropertiesRegistered

    $tooltipConfigured = $ProgIdRegistered -and ($ExtensionProgId -eq $fb2ProgId) -and $infoTipReady
    $detailsPaneConfigured = $ProgIdRegistered -and ($ExtensionProgId -eq $fb2ProgId) -and $detailsReady

    return @(
        [pscustomobject]@{
            Поверхность = "Колонки Проводника"
            Настроено = $columnsLikelyReady
            Вывод = if ($columnsLikelyReady) { "Базовая конфигурация готова." } else { "Не хватает shell/property-регистрации для стабильной работы колонок." }
        }
        [pscustomobject]@{
            Поверхность = "Tooltip"
            Настроено = $tooltipConfigured
            Вывод = if ($tooltipConfigured) { "Shell-строка InfoTip прописана: tooltip должен показывать данные из .fb2 после обновления Проводника." } else { "InfoTip/ProgID настроены не полностью, сначала нужно починить shell-регистрацию." }
        }
        [pscustomobject]@{
            Поверхность = "Правая панель сведений"
            Настроено = $detailsPaneConfigured
            Вывод = if ($detailsPaneConfigured) { "Shell-строка PreviewDetails прописана: правая панель сведений должна показывать данные из .fb2 после обновления Проводника." } else { "PreviewDetails/ProgID настроены не полностью, сначала нужно починить shell-регистрацию." }
        }
        [pscustomobject]@{
            Поверхность = "Миниатюры"
            Настроено = ($ThumbnailProviderRegistered -and $ThumbnailProviderProgIdRegistered -and $ThumbnailProviderClsidRegistered)
            Вывод = if ($ThumbnailProviderRegistered -and $ThumbnailProviderProgIdRegistered -and $ThumbnailProviderClsidRegistered) { "Thumbnail provider зарегистрирован в ShellEx и как COM-класс: можно переходить к ручной проверке вида значков." } elseif ($ThumbnailProviderRegistered -and $ThumbnailProviderProgIdRegistered) { "ShellEx прописан, но COM-класс thumbnail provider не зарегистрирован полностью." } elseif ($ThumbnailProviderRegistered) { "Thumbnail provider зарегистрирован только на .fb2; для надёжности стоит добавить и на FictionBook.2." } else { "ShellEx thumbnail handler для .fb2 не зарегистрирован." }
        }
    )
}

$resolvedFilePath = Resolve-DiagnosticFilePath -RequestedFilePath $FilePath
$propertyStates = @(
    foreach ($canonicalName in $canonicalNames) {
        Get-PropertyProbe -CanonicalName $canonicalName
    }
)
$shellStringStates = Get-Fb2ShellStringState
$installState = Get-InstallDirectoryState
$propertyHandlerRegistered = Test-Path $propertyHandlerKey
$thumbnailProviderRegistered = Test-Path $thumbnailProviderKey
$thumbnailProviderProgIdRegistered = Test-Path $thumbnailProviderProgIdKey
$thumbnailProviderClsidRegistered = Test-Path $thumbnailProviderClsidKey
$thumbnailProviderValue = Get-RegistryDefaultValue -RegistryPath $thumbnailProviderKey
$thumbnailProviderProgIdValue = Get-RegistryDefaultValue -RegistryPath $thumbnailProviderProgIdKey
$progIdRegistered = Test-Path $fb2ProgIdKey
$extensionRegistryKey = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey("Software\Classes\.fb2")
$extensionProgId = if ($extensionRegistryKey) { $extensionRegistryKey.GetValue("", "") } else { "" }
if ($extensionRegistryKey) {
    $extensionRegistryKey.Dispose()
}

$surfaceAssessment = Get-SurfaceAssessment `
    -PropertyStates $propertyStates `
    -ShellStringStates $shellStringStates `
    -PropertyHandlerRegistered $propertyHandlerRegistered `
    -ThumbnailProviderRegistered $thumbnailProviderRegistered `
    -ThumbnailProviderProgIdRegistered $thumbnailProviderProgIdRegistered `
    -ThumbnailProviderClsidRegistered $thumbnailProviderClsidRegistered `
    -ProgIdRegistered $progIdRegistered `
    -ExtensionProgId $extensionProgId

Write-Host "Диагностика поверхностей shell-интеграции .fb2"
Write-Host "  PropertyHandlers\\.fb2 зарегистрирован: $propertyHandlerRegistered"
Write-Host "  Thumbnail ShellEx зарегистрирован: $thumbnailProviderRegistered"
Write-Host "  Thumbnail CLSID: $(if ($thumbnailProviderValue) { $thumbnailProviderValue } else { '<не задан>' })"
Write-Host "  Thumbnail FictionBook.2 ShellEx зарегистрирован: $thumbnailProviderProgIdRegistered"
Write-Host "  Thumbnail FictionBook.2 CLSID: $(if ($thumbnailProviderProgIdValue) { $thumbnailProviderProgIdValue } else { '<не задан>' })"
Write-Host "  Thumbnail COM InprocServer32 зарегистрирован: $thumbnailProviderClsidRegistered"
Write-Host "  Ожидаемый thumbnail CLSID: $thumbnailProviderClsid"
Write-Host "  ProgID FictionBook.2 зарегистрирован: $progIdRegistered"
Write-Host "  Ассоциация .fb2 -> $extensionProgId"
Write-Host "  Тестовый файл: $(if ($resolvedFilePath) { $resolvedFilePath } else { '<не найден>' })"
Write-Host ""

Write-Host "Каталог установки"
$installState |
    Select-Object `
        @{ Name = "InstallDir (FBE)"; Expression = { $_.InstallDirFromFbeKey } }, `
        @{ Name = "InstallLocation"; Expression = { $_.InstallLocationFromUninstall } }, `
        @{ Name = "Эффективный каталог"; Expression = { $_.EffectiveInstallDirectory } }, `
        @{ Name = "FBE.exe"; Expression = { $_.FbeExeExists } }, `
        @{ Name = "FBV.exe"; Expression = { $_.FbvExeExists } } |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Published FBE-specific свойства"
$propertyStates |
    Select-Object `
        @{ Name = "Каноническое имя"; Expression = { $_.CanonicalName } }, `
        @{ Name = "HRESULT"; Expression = { $_.HResult } }, `
        @{ Name = "Зарегистрировано"; Expression = { $_.Registered } } |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Shell-строки FictionBook.2"
$shellStringStates |
    Select-Object `
        @{ Name = "Имя"; Expression = { $_.Name } }, `
        @{ Name = "Присутствует"; Expression = { $_.Present } }, `
        @{ Name = "Совпадает с ожидаемым"; Expression = { $_.MatchesExpected } }, `
        @{ Name = "Фактическое значение"; Expression = { $_.ActualValue } } |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Оценка готовности поверхностей"
$surfaceAssessment | Format-Table -AutoSize

Write-Host ""
Write-Host "Примечание"
Write-Host "  Для миниатюр одного PropertySystem недостаточно: отдельно нужен ShellEx thumbnail handler."
Write-Host "  Для tooltip и правой панели сведений сначала проверьте InfoTip и PreviewDetails."
Write-Host "  Для миниатюр после регистрации thumbnail handler переключите Проводник в крупные значки"
Write-Host "  и проверяйте файл с валидной встроенной обложкой, например tools\\tests\\fb2-cover-smoke.fb2."
