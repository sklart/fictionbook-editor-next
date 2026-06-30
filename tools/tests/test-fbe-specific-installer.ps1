[CmdletBinding()]
param(
    [string]$InstallerPath = "",
    [string]$InstallDirectory = "",
    [switch]$UnregisterRepoSchemaBeforeTest,
    [switch]$KeepInstallDirectory
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$fbeSpecificCanonicalNames = @(
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

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Ensure-Admin {
    if (-not (Test-IsAdministrator)) {
        throw "Для smoke-проверки FBE-specific схемы свойств нужны права администратора."
    }
}

function Ensure-PropSysProbeType {
    if (-not ("FbeSpecificInstallerPropSysProbe" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class FbeSpecificInstallerPropSysProbe {
    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSGetPropertyDescriptionByName(string canonicalName, ref Guid riid, out IntPtr ppv);
}
"@
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
    $hr = [FbeSpecificInstallerPropSysProbe]::PSGetPropertyDescriptionByName(
        $CanonicalName,
        [ref]$iid,
        [ref]$ptr
    )
    if ($ptr -ne [IntPtr]::Zero) {
        [Runtime.InteropServices.Marshal]::Release($ptr) | Out-Null
    }

    [pscustomobject]@{
        CanonicalName = $CanonicalName
        HResult = $hr
        HResultHex = ('0x{0:X8}' -f ($hr -band 0xffffffff))
        IsRegistered = ($hr -ge 0)
    }
}

function Get-AllPropertyProbes {
    return @(
        foreach ($canonicalName in $fbeSpecificCanonicalNames) {
            Get-PropertyProbe -CanonicalName $canonicalName
        }
    )
}

function Get-PropertyHandlerRegistryState {
    return Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2'
}

function Get-ThumbnailProviderRegistryStates {
    return [pscustomobject]@{
        ExtensionShellEx = (Test-Path 'HKLM:\SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}')
        ProgIdShellEx = (Test-Path 'HKLM:\SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}')
        ClsidInproc = (Test-Path 'HKLM:\SOFTWARE\Classes\CLSID\{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}\InprocServer32')
    }
}

function Assert-ThumbnailProviderRegistered {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    if (-not $State.ExtensionShellEx) {
        throw "${StageLabel}: после установки не появилась регистрация thumbnail provider для .fb2."
    }
    if (-not $State.ProgIdShellEx) {
        throw "${StageLabel}: после установки не появилась регистрация thumbnail provider для FictionBook.2."
    }
    if (-not $State.ClsidInproc) {
        throw "${StageLabel}: после установки не появилась COM-регистрация thumbnail provider."
    }
}

function Assert-ThumbnailProviderUnregistered {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    if ($State.ExtensionShellEx) {
        throw "${StageLabel}: после деинсталляции thumbnail provider для .fb2 всё ещё зарегистрирован."
    }
    if ($State.ProgIdShellEx) {
        throw "${StageLabel}: после деинсталляции thumbnail provider для FictionBook.2 всё ещё зарегистрирован."
    }
    if ($State.ClsidInproc) {
        throw "${StageLabel}: после деинсталляции COM-регистрация thumbnail provider всё ещё присутствует."
    }
}

function Get-ValidateVerbState {
    $validateKeyPath = 'HKCU:\Software\Classes\FictionBook.2\shell\Validate'
    $validateCommandKeyPath = 'HKCU:\Software\Classes\FictionBook.2\shell\Validate\Command'

    [pscustomobject]@{
        ValidateKeyExists = (Test-Path $validateKeyPath)
        CommandKeyExists = (Test-Path $validateCommandKeyPath)
        Label = (Get-ItemProperty -Path $validateKeyPath -ErrorAction SilentlyContinue).'(default)'
        MUIVerb = (Get-ItemProperty -Path $validateKeyPath -ErrorAction SilentlyContinue).MUIVerb
        Icon = (Get-ItemProperty -Path $validateKeyPath -ErrorAction SilentlyContinue).Icon
        Command = (Get-ItemProperty -Path $validateCommandKeyPath -ErrorAction SilentlyContinue).'(default)'
    }
}

function Get-UninstallRegistryState {
    $uninstallKeyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\FictionBook Editor'

    [pscustomobject]@{
        KeyExists = (Test-Path $uninstallKeyPath)
        InstallLocation = (Get-ItemProperty -Path $uninstallKeyPath -ErrorAction SilentlyContinue).InstallLocation
        UninstallString = (Get-ItemProperty -Path $uninstallKeyPath -ErrorAction SilentlyContinue).UninstallString
        SystemIntegrationInstalled = (Get-ItemProperty -Path $uninstallKeyPath -ErrorAction SilentlyContinue).SystemIntegrationInstalled
    }
}

function Assert-UninstallRegistryStatePresent {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [string]$ExpectedInstallDirectory,
        [Parameter(Mandatory)]
        [string]$ExpectedUninstallerPath,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    if (-not $State.KeyExists) {
        throw "${StageLabel}: отсутствует uninstall-ключ FictionBook Editor."
    }
    if ($State.InstallLocation -ne $ExpectedInstallDirectory) {
        throw "${StageLabel}: uninstall-ключ содержит неожиданный InstallLocation.`nОжидалось: $ExpectedInstallDirectory`nФактически: $($State.InstallLocation)"
    }
    if ($State.UninstallString -ne $ExpectedUninstallerPath) {
        throw "${StageLabel}: uninstall-ключ содержит неожиданный UninstallString.`nОжидалось: $ExpectedUninstallerPath`nФактически: $($State.UninstallString)"
    }
    if ($State.SystemIntegrationInstalled -ne 1) {
        throw "${StageLabel}: uninstall-ключ не содержит маркер SystemIntegrationInstalled=1."
    }
}

function Assert-UninstallRegistryStateAbsent {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    if ($State.KeyExists) {
        throw "${StageLabel}: uninstall-ключ FictionBook Editor всё ещё присутствует."
    }
}

function Assert-ValidateVerbRegistered {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [string]$ExpectedFbvExePath,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    $expectedMuiModulePath = Join-Path (Split-Path -Path $ExpectedFbvExePath -Parent) "FBVVerbResources.dll"
    $expectedMUIVerb = '@' + $expectedMuiModulePath + ',-109;v2'
    $expectedIcon = '"' + $ExpectedFbvExePath + '",0'
    $expectedCommand = '"' + $ExpectedFbvExePath + '" "%1"'

    if (-not $State.ValidateKeyExists) {
        throw "${StageLabel}: shell-verb Validate для FictionBook.2 не зарегистрирован."
    }
    if (-not $State.CommandKeyExists) {
        throw "${StageLabel}: отсутствует ключ Validate\\Command."
    }
    if ($State.Label -ne 'Validate') {
        throw "${StageLabel}: ключ Validate имеет неожиданное значение (default): $($State.Label)"
    }
    if ($State.MUIVerb -ne $expectedMUIVerb) {
        throw "${StageLabel}: ключ Validate имеет неожиданный MUIVerb.`nОжидалось: $expectedMUIVerb`nФактически: $($State.MUIVerb)"
    }
    if ($State.Icon -ne $expectedIcon) {
        throw "${StageLabel}: ключ Validate имеет неожиданный Icon.`nОжидалось: $expectedIcon`nФактически: $($State.Icon)"
    }
    if ($State.Command -ne $expectedCommand) {
        throw "${StageLabel}: ключ Validate\\Command имеет неожиданную команду.`nОжидалось: $expectedCommand`nФактически: $($State.Command)"
    }
}

function Assert-ValidateVerbUnregistered {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    if ($State.ValidateKeyExists -or $State.CommandKeyExists) {
        throw "${StageLabel}: shell-verb Validate для FictionBook.2 всё ещё присутствует."
    }
}

function Get-Fb2ProgIdShellStrings {
    $progIdKey = 'HKCU:\Software\Classes\FictionBook.2'
    $result = [ordered]@{}

    foreach ($valueName in $expectedFb2ShellStrings.Keys) {
        $value = $null
        if (Test-Path $progIdKey) {
            $value = (Get-ItemProperty -Path $progIdKey -Name $valueName -ErrorAction SilentlyContinue).$valueName
        }
        $result[$valueName] = $value
    }

    return [pscustomobject]$result
}

function Assert-Fb2ShellStringsPresent {
    param(
        [Parameter(Mandatory)]
        [object]$ActualValues,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    foreach ($entry in $expectedFb2ShellStrings.GetEnumerator()) {
        $actualValue = $ActualValues.($entry.Key)
        if ([string]::IsNullOrWhiteSpace($actualValue)) {
            throw "${StageLabel}: не заполнено shell-значение FictionBook.2/$($entry.Key)."
        }
        if ($actualValue -ne $entry.Value) {
            throw "${StageLabel}: FictionBook.2/$($entry.Key) имеет неожиданное значение.`nОжидалось: $($entry.Value)`nФактически: $actualValue"
        }
    }
}

function Assert-Fb2ShellStringsAbsent {
    param(
        [Parameter(Mandatory)]
        [object]$ActualValues,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    foreach ($entry in $expectedFb2ShellStrings.GetEnumerator()) {
        $actualValue = $ActualValues.($entry.Key)
        if (-not [string]::IsNullOrWhiteSpace($actualValue)) {
            throw "${StageLabel}: FictionBook.2/$($entry.Key) всё ещё присутствует: $actualValue"
        }
    }
}

function Resolve-DefaultInstallerPath {
    $artifactsDir = Join-Path $repoRoot "out\artifacts"
    $candidates = @(
        Get-ChildItem -LiteralPath $artifactsDir -Filter "FictionBookEditor-*-setup.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -notlike "*sequence-smoke*" } |
            Sort-Object LastWriteTime -Descending
    )

    if ($candidates.Count -gt 0) {
        return $candidates[0].FullName
    }

    $fallbackCandidates = @(
        Get-ChildItem -LiteralPath $artifactsDir -Filter "*setup.exe" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending
    )

    if ($fallbackCandidates.Count -gt 0) {
        return $fallbackCandidates[0].FullName
    }

    throw "Не удалось найти setup.exe для smoke-проверки в out\artifacts."
}

function Assert-AllRegistered {
    param(
        [Parameter(Mandatory)]
        [object[]]$Probes,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    foreach ($probe in $Probes) {
        if (-not $probe.IsRegistered) {
            throw "${StageLabel}: свойство $($probe.CanonicalName) не зарегистрировано ($($probe.HResultHex))."
        }
    }
}

function Assert-AllUnregistered {
    param(
        [Parameter(Mandatory)]
        [object[]]$Probes,
        [Parameter(Mandatory)]
        [string]$StageLabel
    )

    foreach ($probe in $Probes) {
        if ($probe.IsRegistered) {
            throw "${StageLabel}: свойство $($probe.CanonicalName) всё ещё зарегистрировано ($($probe.HResultHex))."
        }
    }
}

function Test-ResidualPublishedPropertyBaseline {
    param(
        [Parameter(Mandatory)]
        [bool]$PropertyHandlerRegistered,
        [Parameter(Mandatory)]
        [object]$ThumbnailProviderState,
        [Parameter(Mandatory)]
        [object[]]$PropertyStates
    )

    $anyPublishedProperty = @($PropertyStates | Where-Object { $_.IsRegistered }).Count -gt 0
    if (-not $anyPublishedProperty) {
        return $false
    }

    if ($PropertyHandlerRegistered) {
        return $false
    }

    if ($ThumbnailProviderState.ExtensionShellEx -or $ThumbnailProviderState.ProgIdShellEx -or $ThumbnailProviderState.ClsidInproc) {
        return $false
    }

    return $true
}

Ensure-Admin

if ([string]::IsNullOrWhiteSpace($InstallerPath)) {
    $InstallerPath = Resolve-DefaultInstallerPath
} else {
    $InstallerPath = (Resolve-Path -LiteralPath $InstallerPath).Path
}

if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
    $InstallDirectory = Join-Path $repoRoot "out\installer-smoke-app"
} else {
    $InstallDirectory = (Resolve-Path -LiteralPath $InstallDirectory -ErrorAction SilentlyContinue)?.Path ?? $InstallDirectory
}

$shellSharedDir = Join-Path $env:ProgramData "FictionBook Editor\Shell"
$schemaPathInSharedShellDir = Join-Path $shellSharedDir "FBE.Sequence.propdesc"
$uninstallerPath = Join-Path $InstallDirectory "uninst.exe"
$fbvExePath = Join-Path $InstallDirectory "FBV.exe"
$repoSchemaScript = Join-Path $repoRoot "tools\build\unregister-sequence-property-schema.ps1"
$propertyStatesBefore = Get-AllPropertyProbes

if ($UnregisterRepoSchemaBeforeTest) {
    & $repoSchemaScript -NoRestartExplorer
    $propertyStatesBefore = Get-AllPropertyProbes
}

if (Test-Path -LiteralPath $InstallDirectory) {
    Remove-Item -Recurse -Force -LiteralPath $InstallDirectory
}

Write-Host "Smoke-проверка setup.exe для FBE-specific схемы свойств:"
Write-Host "  Установщик: $InstallerPath"
Write-Host "  Папка установки: $InstallDirectory"
Write-Host "  Shared shell dir: $shellSharedDir"
foreach ($probe in $propertyStatesBefore) {
    Write-Host "  До установки $($probe.CanonicalName): $($probe.HResultHex)"
}

& $InstallerPath /S /D=$InstallDirectory
Start-Sleep -Seconds 5

if (-not (Test-Path -LiteralPath $uninstallerPath -PathType Leaf)) {
    throw "Тихая установка не создала uninst.exe: $uninstallerPath"
}
if (-not (Test-Path -LiteralPath $schemaPathInSharedShellDir -PathType Leaf)) {
    throw "Тихая установка не положила FBE.Sequence.propdesc в shared shell dir: $schemaPathInSharedShellDir"
}
if (-not (Get-PropertyHandlerRegistryState)) {
    throw "После установки не появилась регистрация PropertyHandlers\.fb2."
}
$thumbnailProviderStateAfterInstall = Get-ThumbnailProviderRegistryStates
Assert-ThumbnailProviderRegistered -State $thumbnailProviderStateAfterInstall -StageLabel "После установки"

$propertyStatesAfterInstall = Get-AllPropertyProbes
Assert-AllRegistered -Probes $propertyStatesAfterInstall -StageLabel "После установки"
$fb2ShellStringsAfterInstall = Get-Fb2ProgIdShellStrings
Assert-Fb2ShellStringsPresent -ActualValues $fb2ShellStringsAfterInstall -StageLabel "После установки"
$validateVerbStateAfterInstall = Get-ValidateVerbState
Assert-ValidateVerbRegistered -State $validateVerbStateAfterInstall -ExpectedFbvExePath $fbvExePath -StageLabel "После установки"
$uninstallRegistryStateAfterInstall = Get-UninstallRegistryState
Assert-UninstallRegistryStatePresent -State $uninstallRegistryStateAfterInstall -ExpectedInstallDirectory $InstallDirectory -ExpectedUninstallerPath $uninstallerPath -StageLabel "После установки"

Write-Host "После установки:"
Write-Host "  PropertyHandlers\\.fb2: зарегистрирован"
Write-Host "  Thumbnail provider .fb2: $($thumbnailProviderStateAfterInstall.ExtensionShellEx)"
Write-Host "  Thumbnail provider FictionBook.2: $($thumbnailProviderStateAfterInstall.ProgIdShellEx)"
Write-Host "  Thumbnail provider COM CLSID: $($thumbnailProviderStateAfterInstall.ClsidInproc)"
Write-Host "  Validate MUIVerb: $($validateVerbStateAfterInstall.MUIVerb)"
Write-Host "  Validate verb: $($validateVerbStateAfterInstall.Command)"
Write-Host "  Uninstall/SystemIntegrationInstalled: $($uninstallRegistryStateAfterInstall.SystemIntegrationInstalled)"
foreach ($probe in $propertyStatesAfterInstall) {
    Write-Host "  $($probe.CanonicalName): $($probe.HResultHex)"
}
foreach ($entry in $expectedFb2ShellStrings.GetEnumerator()) {
    Write-Host "  FictionBook.2/$($entry.Key): $($fb2ShellStringsAfterInstall.($entry.Key))"
}

& $uninstallerPath /S
Start-Sleep -Seconds 5

$handlerStateAfterUninstall = Get-PropertyHandlerRegistryState
$thumbnailProviderStateAfterUninstall = Get-ThumbnailProviderRegistryStates
$propertyStatesAfterUninstall = Get-AllPropertyProbes
$fb2ShellStringsAfterUninstall = Get-Fb2ProgIdShellStrings
$validateVerbStateAfterUninstall = Get-ValidateVerbState
$uninstallRegistryStateAfterUninstall = Get-UninstallRegistryState

if ($handlerStateAfterUninstall) {
    throw "После деинсталляции PropertyHandlers\.fb2 всё ещё присутствует."
}
Assert-ThumbnailProviderUnregistered -State $thumbnailProviderStateAfterUninstall -StageLabel "После деинсталляции"
Assert-Fb2ShellStringsAbsent -ActualValues $fb2ShellStringsAfterUninstall -StageLabel "После деинсталляции"
Assert-ValidateVerbUnregistered -State $validateVerbStateAfterUninstall -StageLabel "После деинсталляции"
Assert-UninstallRegistryStateAbsent -State $uninstallRegistryStateAfterUninstall -StageLabel "После деинсталляции"

$residualPublishedBaselineAfterUninstall = Test-ResidualPublishedPropertyBaseline `
    -PropertyHandlerRegistered:$handlerStateAfterUninstall `
    -ThumbnailProviderState $thumbnailProviderStateAfterUninstall `
    -PropertyStates $propertyStatesAfterUninstall

$anyRegisteredBefore = @($propertyStatesBefore | Where-Object { $_.IsRegistered }).Count -gt 0
if ($anyRegisteredBefore) {
    $stateChangedComparedToBefore = $false
    foreach ($probeBefore in $propertyStatesBefore) {
        $probeAfter = $propertyStatesAfterUninstall | Where-Object { $_.CanonicalName -eq $probeBefore.CanonicalName } | Select-Object -First 1
        if ($null -eq $probeAfter) {
            throw "Не удалось найти итоговое состояние свойства $($probeBefore.CanonicalName) после деинсталляции."
        }
        if ($probeBefore.IsRegistered -ne $probeAfter.IsRegistered) {
            $stateChangedComparedToBefore = $true
        }
    }

Write-Warning "До smoke-проверки часть FBE-specific свойств уже была зарегистрирована."
    if ($residualPublishedBaselineAfterUninstall) {
        Write-Warning "После деинсталляции published-состояние FBE-specific свойств сохраняется при уже снятых PropertyHandler/ShellEx/CLSID."
        Write-Warning "Это похоже на остаточный machine baseline Property System, а не на регрессию текущего setup/uninstall."
    }
    if ($stateChangedComparedToBefore) {
        Write-Warning "После деинсталляции итоговое опубликованное состояние отличается от исходного."
        Write-Warning "Это не обязательно означает ошибку инсталлятора: тест стартовал не с чистого исходного состояния."
        Write-Warning "Для строгой проверки цикла установки и деинсталляции лучше повторить smoke с ключом -UnregisterRepoSchemaBeforeTest."
    }
    foreach ($probeAfter in $propertyStatesAfterUninstall) {
        Write-Warning ("После деинсталляции {0}: {1}" -f $probeAfter.CanonicalName, $probeAfter.HResultHex)
    }
}
else {
    if ($residualPublishedBaselineAfterUninstall) {
        Write-Warning "После деинсталляции published-состояние FBE-specific свойств сохраняется при уже снятых PropertyHandler/ShellEx/CLSID."
        Write-Warning "Это похоже на остаточный machine baseline Property System, а не на регрессию текущего setup/uninstall."
        foreach ($probeAfter in $propertyStatesAfterUninstall) {
            Write-Warning ("После деинсталляции {0}: {1}" -f $probeAfter.CanonicalName, $probeAfter.HResultHex)
        }
    }
    else {
        Assert-AllUnregistered -Probes $propertyStatesAfterUninstall -StageLabel "После деинсталляции"
    }
}

if (-not $KeepInstallDirectory -and (Test-Path -LiteralPath $InstallDirectory)) {
    $remainingItems = @(Get-ChildItem -Force -LiteralPath $InstallDirectory -ErrorAction SilentlyContinue)
    if ($remainingItems.Count -eq 0) {
        Remove-Item -Force -LiteralPath $InstallDirectory
    }
}

Write-Host "Smoke-проверка установщика FBE-specific схемы свойств прошла успешно."
foreach ($probe in $propertyStatesAfterUninstall) {
    Write-Host "  После деинсталляции $($probe.CanonicalName): $($probe.HResultHex)"
}





