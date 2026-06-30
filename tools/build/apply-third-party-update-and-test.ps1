<#
.SYNOPSIS
Раскладывает обновление выбранной зависимости и запускает связанный build/test-контур.
#>

[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = "High")]
param(
    [Parameter(Mandatory)]
    [ValidateSet("scintilla", "lexilla", "pcre2", "hunspell", "wtl")]
    [string]$Dependency,

    [string]$SourcePath,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [switch]$Force,

    [switch]$SkipApply
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "ThirdPartySources.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$script:IsWhatIfMode = [bool]$WhatIfPreference

function Invoke-ScriptStep {
    param(
        [Parameter(Mandatory)]
        [string]$Label,

        [Parameter(Mandatory)]
        [scriptblock]$Action
    )

    Write-Host ("== {0} ==" -f $Label)
    & $Action
}

function Invoke-ApplyStep {
    if ($SkipApply) {
        return
    }

    $applyArguments = @{
        Dependency = $Dependency
        Force = $Force
        Confirm = $false
    }

    if ($SourcePath) {
        $applyArguments["SourcePath"] = $SourcePath
    }

    if ($script:IsWhatIfMode) {
        $applyArguments["WhatIf"] = $true
    }

    & (Join-Path $PSScriptRoot "apply-third-party-update.ps1") @applyArguments
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

function Invoke-ScintillaPipeline {
    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "0KDQsNC30LLRkdGA0YLRi9Cy0LDQvdC40LUg0L7QsdC90L7QstC70LXQvdC40Y8=") -Action { Invoke-ApplyStep }

    if ($script:IsWhatIfMode) {
        Write-Host (Get-ThirdPartyText -Base64 "V2hhdElmINCw0LrRgtC40LLQtdC9OiDRiNCw0LPQuCDRgdCx0L7RgNC60Lgg0Lgg0YLQtdGB0YLQvtCyINC/0YDQvtC/0YPRidC10L3Riy4=")
        return
    }

    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "0J/QtdGA0LXRgdCx0L7RgNC60LAgU2NpbnRpbGxhINC4IExleGlsbGE=") -Action {
        & (Join-Path $PSScriptRoot "build-scintilla.ps1")
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "U21va2Ut0YLQtdGB0YIgU2NpbnRpbGxh") -Action {
        & (Join-Path $repoRoot "tools\tests\test-scintilla.ps1")
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

function Invoke-Pcre2Pipeline {
    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "0KDQsNC30LLRkdGA0YLRi9Cy0LDQvdC40LUg0L7QsdC90L7QstC70LXQvdC40Y8=") -Action { Invoke-ApplyStep }

    if ($script:IsWhatIfMode) {
        Write-Host (Get-ThirdPartyText -Base64 "V2hhdElmINCw0LrRgtC40LLQtdC9OiDRiNCw0LPQuCDRgdCx0L7RgNC60Lgg0Lgg0YLQtdGB0YLQvtCyINC/0YDQvtC/0YPRidC10L3Riy4=")
        return
    }

    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "0J/QtdGA0LXRgdCx0L7RgNC60LAgUENSRTI=") -Action {
        & (Join-Path $PSScriptRoot "build-pcre2.ps1") -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "U21va2Ut0YLQtdGB0YIgcnVudGltZSBQQ1JFMg==") -Action {
        & (Join-Path $repoRoot "tools\tests\test-pcre2.ps1") -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "U21va2Ut0YLQtdGB0YIgd3JhcHBlciBQQ1JFMg==") -Action {
        & (Join-Path $repoRoot "tools\tests\test-pcre2-wrapper.ps1") -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "U21va2Ut0YLQtdGB0YIgcmVwbGFjZSBQQ1JFMg==") -Action {
        & (Join-Path $repoRoot "tools\tests\test-pcre2-replace.ps1") -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

function Invoke-HunspellPipeline {
    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "0KDQsNC30LLRkdGA0YLRi9Cy0LDQvdC40LUg0L7QsdC90L7QstC70LXQvdC40Y8=") -Action { Invoke-ApplyStep }

    if ($script:IsWhatIfMode) {
        Write-Host (Get-ThirdPartyText -Base64 "V2hhdElmINCw0LrRgtC40LLQtdC9OiDRiNCw0LPQuCDRgdCx0L7RgNC60Lgg0Lgg0YLQtdGB0YLQvtCyINC/0YDQvtC/0YPRidC10L3Riy4=")
        return
    }

    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "0J/QtdGA0LXRgdCx0L7RgNC60LAgSHVuc3BlbGw=") -Action {
        & (Join-Path $PSScriptRoot "build-hunspell.ps1") -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "0J/RgNC+0LLQtdGA0LrQsCDRgdC70L7QstCw0YDQtdC5INC+0YDRhNC+0LPRgNCw0YTQuNC4") -Action {
        & (Join-Path $repoRoot "tools\tests\test-spellcheck-dictionaries.ps1") -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

function Invoke-WtlPipeline {
    Invoke-ScriptStep -Label (Get-ThirdPartyText -Base64 "0KDQsNC30LLRkdGA0YLRi9Cy0LDQvdC40LUg0L7QsdC90L7QstC70LXQvdC40Y8=") -Action { Invoke-ApplyStep }

    if ($script:IsWhatIfMode) {
        Write-Host (Get-ThirdPartyText -Base64 "V2hhdElmINCw0LrRgtC40LLQtdC9OiDRiNCw0LPQuCDRgdCx0L7RgNC60Lgg0Lgg0YLQtdGB0YLQvtCyINC/0YDQvtC/0YPRidC10L3Riy4=")
        return
    }

    Invoke-ScriptStep -Label "Сборка после обновления WTL" -Action {
        & (Join-Path $PSScriptRoot "build.ps1") -Configuration $Configuration -Platform Win32 -SkipUpx
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    Invoke-ScriptStep -Label "Release-gate после обновления WTL" -Action {
        & (Join-Path $PSScriptRoot "verify-release.ps1") -Configuration $Configuration
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

switch ($Dependency) {
    "scintilla" { Invoke-ScintillaPipeline }
    "lexilla" { Invoke-ScintillaPipeline }
    "pcre2" { Invoke-Pcre2Pipeline }
    "hunspell" { Invoke-HunspellPipeline }
    "wtl" { Invoke-WtlPipeline }
    default { throw (Format-ThirdPartyText "0J3QtdC40LfQstC10YHRgtC90YvQuSBvcmNoZXN0cmF0b3Ig0LTQu9GPINC30LDQstC40YHQuNC80L7RgdGC0LggezB9" $Dependency) }
}

if (-not $script:IsWhatIfMode) {
    Write-Host ""
    Write-Host (Format-ThirdPartyText "0JPQvtGC0L7QstC+OiB7MH0g0L7QsdC90L7QstC70LXQvdC+LCDQv9C10YDQtdGB0L7QsdGA0LDQvdC+INC4INC/0YDQvtCy0LXRgNC10L3Qvi4=" $Dependency)
}
