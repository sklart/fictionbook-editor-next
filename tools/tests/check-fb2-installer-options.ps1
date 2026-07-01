<#
Проверяет результат ручной установки FBE Next с разными опциями .fb2-интеграции.
Скрипт ничего не устанавливает и не меняет: он только читает реестр и файлы,
чтобы подтвердить выбранный профиль установщика.
#>
[CmdletBinding()]
param(
    [ValidateSet(
        "Diagnose",
        "NoFb2Integration",
        "AssociationOnly",
        "ValidateOnly",
        "ExplorerOnly",
        "Full"
    )]
    [string]$Profile = "Diagnose",

    [string]$InstallDirectory = ""
)

$ErrorActionPreference = "Stop"

$expectedFb2ShellStrings = [ordered]@{
    InfoTip = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;System.Size"
    TileInfo = "prop:System.Author;System.Title"
    Details = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
    PreviewDetails = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
}

function Get-DefaultRegistryValue {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    return (Get-ItemProperty -Path $Path -ErrorAction SilentlyContinue).'(default)'
}

function Get-RegistryValue {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$Name
    )

    return (Get-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue).$Name
}

function Resolve-InstallDirectory {
    param(
        [string]$ExplicitInstallDirectory
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitInstallDirectory)) {
        $resolvedPath = Resolve-Path -LiteralPath $ExplicitInstallDirectory -ErrorAction SilentlyContinue
        if ($null -ne $resolvedPath) {
            return $resolvedPath.Path
        }
        return $ExplicitInstallDirectory
    }

    $candidateKeys = @(
        "HKCU:\SOFTWARE\FBE Team\FictionBook Editor Next",
        "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\FictionBook Editor Next"
    )

    foreach ($keyPath in $candidateKeys) {
        if (-not (Test-Path $keyPath)) {
            continue
        }

        $installDir = Get-RegistryValue -Path $keyPath -Name "InstallDir"
        if ([string]::IsNullOrWhiteSpace($installDir)) {
            $installDir = Get-RegistryValue -Path $keyPath -Name "InstallLocation"
        }
        if (-not [string]::IsNullOrWhiteSpace($installDir)) {
            return $installDir
        }
    }

    return (Join-Path $env:LOCALAPPDATA "Programs\FictionBook Editor Next")
}

function Get-InstallerOptionState {
    param(
        [Parameter(Mandatory)]
        [string]$ResolvedInstallDirectory
    )

    $fbeExePath = Join-Path $ResolvedInstallDirectory "FBE.exe"
    $fbvExePath = Join-Path $ResolvedInstallDirectory "FBV.exe"
    $fbvVerbResourcesPath = Join-Path $ResolvedInstallDirectory "FBVVerbResources.dll"

    $fb2Key = "HKCU:\Software\Classes\.fb2"
    $fb2DefaultIconKey = "HKCU:\Software\Classes\.fb2\DefaultIcon"
    $progIdKey = "HKCU:\Software\Classes\FictionBook.2"
    $progIdDefaultIconKey = "HKCU:\Software\Classes\FictionBook.2\DefaultIcon"
    $systemValidateKey = "HKCU:\Software\Classes\SystemFileAssociations\.fb2\shell\Validate"
    $systemValidateCommandKey = "$systemValidateKey\Command"
    $progIdValidateKey = "$progIdKey\shell\Validate"
    $progIdValidateCommandKey = "$progIdValidateKey\Command"

    $sharedShellDir = Join-Path $env:ProgramData "FictionBook Editor\Shell"
    $sharedSchemaPath = Join-Path $sharedShellDir "FBE.Sequence.propdesc"
    $sharedShell32Path = Join-Path $sharedShellDir "FBShell.dll"
    $sharedShell64Path = Join-Path $sharedShellDir "FBShell64.dll"

    $expectedFbeIcon = "$fbeExePath,0"
    $expectedFbvIcon = '"' + $fbvExePath + '",0'
    $expectedFbvCommand = '"' + $fbvExePath + '" "%1"'
    $expectedFbvMUIVerb = '@' + $fbvVerbResourcesPath + ',-109;v2'

    $shellStrings = [ordered]@{}
    foreach ($name in $expectedFb2ShellStrings.Keys) {
        $shellStrings[$name] = Get-RegistryValue -Path $progIdKey -Name $name
    }

    [pscustomobject]@{
        InstallDirectory = $ResolvedInstallDirectory
        FbeExeExists = Test-Path -LiteralPath $fbeExePath -PathType Leaf
        FbvExeExists = Test-Path -LiteralPath $fbvExePath -PathType Leaf

        FileAssociationKeyExists = Test-Path $fb2Key
        Fb2DefaultProgId = Get-DefaultRegistryValue -Path $fb2Key
        Fb2PerceivedType = Get-RegistryValue -Path $fb2Key -Name "PerceivedType"
        Fb2ContentType = Get-RegistryValue -Path $fb2Key -Name "Content Type"
        Fb2DefaultIcon = Get-DefaultRegistryValue -Path $fb2DefaultIconKey
        ProgIdDefaultIcon = Get-DefaultRegistryValue -Path $progIdDefaultIconKey
        ExpectedFbeIcon = $expectedFbeIcon

        SystemValidateKeyExists = Test-Path $systemValidateKey
        SystemValidateCommandKeyExists = Test-Path $systemValidateCommandKey
        SystemValidateLabel = Get-DefaultRegistryValue -Path $systemValidateKey
        SystemValidateMUIVerb = Get-RegistryValue -Path $systemValidateKey -Name "MUIVerb"
        SystemValidateIcon = Get-RegistryValue -Path $systemValidateKey -Name "Icon"
        SystemValidateCommand = Get-DefaultRegistryValue -Path $systemValidateCommandKey

        ProgIdValidateKeyExists = Test-Path $progIdValidateKey
        ProgIdValidateCommandKeyExists = Test-Path $progIdValidateCommandKey
        ProgIdValidateMUIVerb = Get-RegistryValue -Path $progIdValidateKey -Name "MUIVerb"
        ProgIdValidateIcon = Get-RegistryValue -Path $progIdValidateKey -Name "Icon"
        ProgIdValidateCommand = Get-DefaultRegistryValue -Path $progIdValidateCommandKey
        ExpectedFbvMUIVerb = $expectedFbvMUIVerb
        ExpectedFbvIcon = $expectedFbvIcon
        ExpectedFbvCommand = $expectedFbvCommand

        PropertyHandlerRegistered = Test-Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2"
        ThumbnailExtensionRegistered = Test-Path "HKLM:\SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
        ThumbnailProgIdRegistered = Test-Path "HKLM:\SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"
        ThumbnailClsidRegistered = Test-Path "HKLM:\SOFTWARE\Classes\CLSID\{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}\InprocServer32"
        SharedSchemaExists = Test-Path -LiteralPath $sharedSchemaPath -PathType Leaf
        SharedShell32Exists = Test-Path -LiteralPath $sharedShell32Path -PathType Leaf
        SharedShell64Exists = Test-Path -LiteralPath $sharedShell64Path -PathType Leaf
        ShellStrings = [pscustomobject]$shellStrings
    }
}

function Assert-Condition {
    param(
        [Parameter(Mandatory)]
        [bool]$Condition,
        [Parameter(Mandatory)]
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-FileAssociation {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [bool]$Expected
    )

    if ($Expected) {
        Assert-Condition -Condition $State.FileAssociationKeyExists -Message "Ожидалась ассоциация .fb2, но ключ .fb2 отсутствует."
        Assert-Condition -Condition ($State.Fb2DefaultProgId -eq "FictionBook.2") -Message "Ожидалось .fb2 -> FictionBook.2, фактически: $($State.Fb2DefaultProgId)"
        Assert-Condition -Condition ($State.Fb2DefaultIcon -eq $State.ExpectedFbeIcon) -Message "Ожидалась иконка .fb2 '$($State.ExpectedFbeIcon)', фактически: $($State.Fb2DefaultIcon)"
        Assert-Condition -Condition ($State.ProgIdDefaultIcon -eq $State.ExpectedFbeIcon) -Message "Ожидалась иконка FictionBook.2 '$($State.ExpectedFbeIcon)', фактически: $($State.ProgIdDefaultIcon)"
        return
    }

    Assert-Condition -Condition ($State.Fb2DefaultProgId -ne "FictionBook.2") -Message "Ассоциация .fb2 неожиданно назначена на FictionBook.2."
    Assert-Condition -Condition ($State.Fb2DefaultIcon -ne $State.ExpectedFbeIcon) -Message "Иконка .fb2 неожиданно указывает на FBE Next."
}

function Assert-ValidateCommand {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [bool]$Expected
    )

    if ($Expected) {
        Assert-Condition -Condition $State.SystemValidateKeyExists -Message "Ожидалась команда Validate в SystemFileAssociations\.fb2, но ключ отсутствует."
        Assert-Condition -Condition $State.SystemValidateCommandKeyExists -Message "Ожидался ключ SystemFileAssociations\.fb2\shell\Validate\Command, но он отсутствует."
        Assert-Condition -Condition ($State.SystemValidateMUIVerb -eq $State.ExpectedFbvMUIVerb) -Message "Неожиданный MUIVerb SystemFileAssociations Validate.`nОжидалось: $($State.ExpectedFbvMUIVerb)`nФактически: $($State.SystemValidateMUIVerb)"
        Assert-Condition -Condition ($State.SystemValidateIcon -eq $State.ExpectedFbvIcon) -Message "Неожиданная иконка SystemFileAssociations Validate.`nОжидалось: $($State.ExpectedFbvIcon)`nФактически: $($State.SystemValidateIcon)"
        Assert-Condition -Condition ($State.SystemValidateCommand -eq $State.ExpectedFbvCommand) -Message "Неожиданная команда SystemFileAssociations Validate.`nОжидалось: $($State.ExpectedFbvCommand)`nФактически: $($State.SystemValidateCommand)"
        return
    }

    Assert-Condition -Condition (-not $State.SystemValidateKeyExists) -Message "Команда Validate неожиданно присутствует в SystemFileAssociations\.fb2."
    Assert-Condition -Condition (-not $State.ProgIdValidateKeyExists) -Message "Команда Validate неожиданно присутствует в FictionBook.2."
}

function Assert-ExplorerProperties {
    param(
        [Parameter(Mandatory)]
        [object]$State,
        [Parameter(Mandatory)]
        [bool]$Expected
    )

    if ($Expected) {
        Assert-Condition -Condition $State.PropertyHandlerRegistered -Message "Ожидалась регистрация PropertyHandlers\.fb2, но она отсутствует."
        Assert-Condition -Condition $State.ThumbnailExtensionRegistered -Message "Ожидалась ShellEx-регистрация thumbnail provider для .fb2, но она отсутствует."
        Assert-Condition -Condition $State.ThumbnailProgIdRegistered -Message "Ожидалась ShellEx-регистрация thumbnail provider для FictionBook.2, но она отсутствует."
        Assert-Condition -Condition $State.ThumbnailClsidRegistered -Message "Ожидалась COM-регистрация thumbnail provider, но она отсутствует."
        Assert-Condition -Condition $State.SharedSchemaExists -Message "Ожидался FBE.Sequence.propdesc в shared shell dir, но файл отсутствует."
        Assert-Condition -Condition ($State.SharedShell32Exists -or $State.SharedShell64Exists) -Message "Ожидался FBShell.dll/FBShell64.dll в shared shell dir, но файлы отсутствуют."

        foreach ($entry in $expectedFb2ShellStrings.GetEnumerator()) {
            $actualValue = $State.ShellStrings.($entry.Key)
            Assert-Condition -Condition ($actualValue -eq $entry.Value) -Message "Неожиданное shell-значение FictionBook.2/$($entry.Key).`nОжидалось: $($entry.Value)`nФактически: $actualValue"
        }
        return
    }

    Assert-Condition -Condition (-not $State.PropertyHandlerRegistered) -Message "PropertyHandlers\.fb2 неожиданно зарегистрирован."
    Assert-Condition -Condition (-not $State.ThumbnailExtensionRegistered) -Message "Thumbnail ShellEx для .fb2 неожиданно зарегистрирован."
    Assert-Condition -Condition (-not $State.ThumbnailClsidRegistered) -Message "Thumbnail COM CLSID неожиданно зарегистрирован."
}

function Get-ExpectedProfile {
    param(
        [Parameter(Mandatory)]
        [string]$SelectedProfile
    )

    switch ($SelectedProfile) {
        "NoFb2Integration" {
            return [pscustomobject]@{ Association = $false; Validate = $false; Explorer = $false }
        }
        "AssociationOnly" {
            return [pscustomobject]@{ Association = $true; Validate = $false; Explorer = $false }
        }
        "ValidateOnly" {
            return [pscustomobject]@{ Association = $false; Validate = $true; Explorer = $false }
        }
        "ExplorerOnly" {
            return [pscustomobject]@{ Association = $false; Validate = $false; Explorer = $true }
        }
        "Full" {
            return [pscustomobject]@{ Association = $true; Validate = $true; Explorer = $true }
        }
        default {
            return $null
        }
    }
}

$resolvedInstallDirectory = Resolve-InstallDirectory -ExplicitInstallDirectory $InstallDirectory
$state = Get-InstallerOptionState -ResolvedInstallDirectory $resolvedInstallDirectory

Write-Host "Проверка опций .fb2-интеграции установщика FBE Next"
Write-Host "  Профиль: $Profile"
Write-Host "  Каталог установки: $($state.InstallDirectory)"
Write-Host "  FBE.exe: $($state.FbeExeExists)"
Write-Host "  FBV.exe: $($state.FbvExeExists)"
Write-Host ""
Write-Host "Ассоциация .fb2"
Write-Host "  .fb2 default: $($state.Fb2DefaultProgId)"
Write-Host "  .fb2 icon:    $($state.Fb2DefaultIcon)"
Write-Host ""
Write-Host "Команда проверки"
Write-Host "  SystemFileAssociations Validate: $($state.SystemValidateKeyExists)"
Write-Host "  MUIVerb: $($state.SystemValidateMUIVerb)"
Write-Host "  Command: $($state.SystemValidateCommand)"
Write-Host "  FictionBook.2 Validate: $($state.ProgIdValidateKeyExists)"
Write-Host ""
Write-Host "Свойства и миниатюры"
Write-Host "  PropertyHandlers\\.fb2: $($state.PropertyHandlerRegistered)"
Write-Host "  Thumbnail .fb2:        $($state.ThumbnailExtensionRegistered)"
Write-Host "  Thumbnail FictionBook.2: $($state.ThumbnailProgIdRegistered)"
Write-Host "  Thumbnail COM CLSID:   $($state.ThumbnailClsidRegistered)"
Write-Host "  Shared schema:         $($state.SharedSchemaExists)"
Write-Host "  Shared FBShell.dll:    $($state.SharedShell32Exists)"
Write-Host "  Shared FBShell64.dll:  $($state.SharedShell64Exists)"

$expectedProfile = Get-ExpectedProfile -SelectedProfile $Profile
if ($null -eq $expectedProfile) {
    Write-Host ""
    Write-Host "Режим Diagnose: строгие ожидания не проверялись."
    return
}

Assert-FileAssociation -State $state -Expected $expectedProfile.Association
Assert-ValidateCommand -State $state -Expected $expectedProfile.Validate
Assert-ExplorerProperties -State $state -Expected $expectedProfile.Explorer

Write-Host ""
Write-Host "Профиль '$Profile' соответствует текущему состоянию установки."
