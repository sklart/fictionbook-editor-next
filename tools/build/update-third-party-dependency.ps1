<#
.SYNOPSIS
Выполняет полный цикл check -> download -> apply -> build -> test для выбранной зависимости.
#>

[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = "High")]
param(
    [Parameter(Mandatory)]
    [ValidateSet("scintilla", "lexilla", "pcre2", "hunspell", "wtl")]
    [string]$Dependency,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$DownloadRoot,

    [switch]$ForceDownload,
    [switch]$AllowCurrentVersion,
    [switch]$ForceApply
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "ThirdPartySources.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$script:IsWhatIfMode = [bool]$WhatIfPreference
$dependencyEntry = Get-DependencyEntry -Name $Dependency

if ([string]::IsNullOrWhiteSpace($DownloadRoot)) {
    $DownloadRoot = Join-Path $repoRoot "tmp\third-party-updates"
}

function Invoke-Step {
    param(
        [Parameter(Mandatory)]
        [string]$Label,

        [Parameter(Mandatory)]
        [scriptblock]$Action
    )

    Write-Host ("== {0} ==" -f $Label)
    & $Action
}

function Get-DownloadDirectory {
    param(
        [Parameter(Mandatory)]
        [string]$RootPath,

        [Parameter(Mandatory)]
        [string]$DependencyName,

        [Parameter(Mandatory)]
        [string]$Version
    )

    return (Join-Path $RootPath ("{0}-{1}" -f $DependencyName, $Version))
}

function Invoke-CheckStep {
    $info = Get-DependencyUpdateInfo -Dependency $dependencyEntry

    Write-Host ((Get-ThirdPartyText -Base64 "0JvQvtC60LDQu9GM0L3QsNGPINCy0LXRgNGB0LjRjw==") + (" : {0}" -f $info.LocalVersion))
    Write-Host ((Get-ThirdPartyText -Base64 "0KPQtNCw0LvRkdC90L3QsNGPINCy0LXRgNGB0LjRjw==") + (" : {0}" -f $info.RemoteVersion))
    Write-Host ((Get-ThirdPartyText -Base64 "0KHRgtCw0YLRg9GB") + ("           : {0}" -f (Get-DependencyStatusDisplay -Status $info.Status)))
    if ($info.RemoteTag) {
        Write-Host ((Get-ThirdPartyText -Base64 "0KPQtNCw0LvRkdC90L3Ri9C5INGC0LXQsw==") + ("     : {0}" -f $info.RemoteTag))
    }
    if ($info.RemoteSource) {
        Write-Host ((Get-ThirdPartyText -Base64 "0JjRgdGC0L7Rh9C90LjQuiB1cHN0cmVhbS3QstC10YDRgdC40Lg=") + (" : {0}" -f $info.RemoteSource))
    }

    return $info
}

function Invoke-DownloadStep {
    param(
        [Parameter(Mandatory)]
        [pscustomobject]$UpdateInfo
    )

    $downloadArguments = @{
        Dependency = $Dependency
        DestinationRoot = $DownloadRoot
    }

    if ($ForceDownload) {
        $downloadArguments["Force"] = $true
    }

    if ($AllowCurrentVersion) {
        $downloadArguments["AllowCurrentVersion"] = $true
    }

    if ($script:IsWhatIfMode) {
        Write-Host (Format-ThirdPartyText "V2hhdElmINCw0LrRgtC40LLQtdC9OiDRgdC60LDRh9C40LLQsNC90LjQtSDQsdGD0LTQtdGCINC/0YDQvtC/0YPRidC10L3Qviwg0L7QttC40LTQsNC10LzRi9C5INC60LDRgtCw0LvQvtCzOiB7MH0=" (Get-DownloadDirectory -RootPath $DownloadRoot -DependencyName $Dependency -Version $UpdateInfo.RemoteVersion))
        return (Get-DownloadDirectory -RootPath $DownloadRoot -DependencyName $Dependency -Version $UpdateInfo.RemoteVersion)
    }

    & (Join-Path $PSScriptRoot "download-third-party-updates.ps1") @downloadArguments
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    return (Get-DownloadDirectory -RootPath $DownloadRoot -DependencyName $Dependency -Version $UpdateInfo.RemoteVersion)
}

function Invoke-ApplyAndTestStep {
    param(
        [string]$SourcePath,

        [switch]$SkipApply
    )

    $arguments = @{
        Dependency = $Dependency
        Configuration = $Configuration
        Confirm = $false
    }

    if ($SourcePath) {
        $arguments["SourcePath"] = $SourcePath
    }

    if ($ForceApply) {
        $arguments["Force"] = $true
    }

    if ($SkipApply) {
        $arguments["SkipApply"] = $true
    }

    if ($script:IsWhatIfMode) {
        $arguments["WhatIf"] = $true
    }

    & (Join-Path $PSScriptRoot "apply-third-party-update-and-test.ps1") @arguments
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$updateInfo = Invoke-Step -Label (Get-ThirdPartyText -Base64 "0J/RgNC+0LLQtdGA0LrQsCDQtNC+0YHRgtGD0L/QvdC+0YHRgtC4IHVwc3RyZWFtLdC+0LHQvdC+0LLQu9C10L3QuNGP") -Action { Invoke-CheckStep }

if (($updateInfo.Status -eq "UpToDate") -and (-not $AllowCurrentVersion) -and (-not $ForceDownload)) {
    Write-Host ""
    Write-Host (Format-ThirdPartyText "0JTQu9GPIHswfSDQvdC+0LLQvtCz0L4gdXBzdHJlYW0t0L7QsdC90L7QstC70LXQvdC40Y8g0L3QtdGCLiDQlNC70Y8g0L/QvtCy0YLQvtGA0L3QvtCz0L4g0YbQuNC60LvQsCDQuNGB0L/QvtC70YzQt9GD0LnRgtC1IC1BbGxvd0N1cnJlbnRWZXJzaW9uINC40LvQuCAtRm9yY2VEb3dubG9hZC4=" $Dependency)
    exit 0
}

$sourceDirectory = Invoke-Step -Label (Get-ThirdPartyText -Base64 "0KHQutCw0YfQuNCy0LDQvdC40LUg0Lgg0YDQsNGB0L/QsNC60L7QstC60LAg0LjRgdGF0L7QtNC90LjQutC+0LI=") -Action { Invoke-DownloadStep -UpdateInfo $updateInfo }

if ($script:IsWhatIfMode) {
    Write-Host ""
    Write-Host (Format-ThirdPartyText "V2hhdElmINC30LDQstC10YDRiNGR0L06INC00LDQu9GM0YjQtSDQsdGL0Lsg0LHRiyDQuNGB0L/QvtC70YzQt9C+0LLQsNC9INC60LDRgtCw0LvQvtCzIHswfSDQuCDQt9Cw0L/Rg9GJ0LXQvSBidWlsZC90ZXN0LdC60L7QvdGC0YPRgCDQtNC70Y8gezF9Lg==" $sourceDirectory, $Dependency)
    exit 0
}

if (-not (Test-Path -LiteralPath $sourceDirectory)) {
    throw (Format-ThirdPartyText "0J3QtSDQvdCw0LnQtNC10L0g0LrQsNGC0LDQu9C+0LMg0YHQutCw0YfQsNC90L3QvtCz0L4g0L7QsdC90L7QstC70LXQvdC40Y86IHswfQ==" $sourceDirectory)
}

$skipApply = ($updateInfo.Status -eq "UpToDate" -and $AllowCurrentVersion -and -not $ForceApply)
Invoke-Step -Label (Get-ThirdPartyText -Base64 "0KDQsNGB0LrQu9Cw0LTQutCwINC+0LHQvdC+0LLQu9C10L3QuNGPLCDQv9C10YDQtdGB0LHQvtGA0LrQsCDQuCDRgtC10YHRgtGL") -Action { Invoke-ApplyAndTestStep -SourcePath $sourceDirectory -SkipApply:$skipApply }

if (-not $script:IsWhatIfMode) {
    Write-Host ""
    Write-Host (Format-ThirdPartyText "0J/QvtC70L3Ri9C5INGG0LjQutC7INC+0LHQvdC+0LLQu9C10L3QuNGPINC30LDQstC40YHQuNC80L7RgdGC0Lgg0LfQsNCy0LXRgNGI0ZHQvTogezB9IC0+IHsxfQ==" $Dependency, $updateInfo.RemoteVersion)
}
