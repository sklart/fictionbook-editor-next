[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $repoRoot 'tools\build\editor-runtime-helpers.ps1')

function Assert-Result {
    param([bool]$Actual, [bool]$Expected, [string]$Scenario)
    if ($Actual -ne $Expected) { throw "Fingerprint scenario '$Scenario' expected $Expected, got $Actual." }
}

function New-Fingerprint {
    param(
        [string]$PlatformToolset = 'v143',
        [string]$VCToolsVersion = '14.44.35207',
        [string]$ScintillaVersion = '5.6.6',
        [string]$LexillaVersion = '5.5.3'
    )
    return [pscustomobject]@{
        platformToolset = $PlatformToolset
        vcToolsVersion = $VCToolsVersion
        scintillaVersion = $ScintillaVersion
        lexillaVersion = $LexillaVersion
    }
}

$expected = @{ PlatformToolset = 'v143'; VCToolsVersion = '14.44.35207'; ScintillaVersion = '5.6.6'; LexillaVersion = '5.5.3' }
$valid = New-Fingerprint
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint $valid @expected) $true 'valid'
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint (New-Fingerprint -ScintillaVersion '5.6.4') @expected) $false 'stale Scintilla'
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint (New-Fingerprint -LexillaVersion '5.5.1') @expected) $false 'stale Lexilla'
$missingScintilla = New-Fingerprint; $missingScintilla.PSObject.Properties.Remove('scintillaVersion')
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint $missingScintilla @expected) $false 'missing Scintilla version'
$missingLexilla = New-Fingerprint; $missingLexilla.PSObject.Properties.Remove('lexillaVersion')
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint $missingLexilla @expected) $false 'missing Lexilla version'
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint (New-Fingerprint -PlatformToolset 'v142') @expected) $false 'wrong toolset'
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint (New-Fingerprint -VCToolsVersion '14.45.10000') @expected) $false 'wrong VCToolsVersion'
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint (ConvertFrom-EditorRuntimeFingerprintJson -Json '{broken') @expected) $false 'corrupt JSON'

$wrongSeries = New-Fingerprint -VCToolsVersion '14.45.10000'
$wrongSeriesExpected = @{ PlatformToolset = 'v143'; VCToolsVersion = '14.45.10000'; ScintillaVersion = '5.6.6'; LexillaVersion = '5.5.3' }
Assert-Result (Test-EditorRuntimeFingerprint -Fingerprint $wrongSeries @wrongSeriesExpected) $false 'universal runtime requires VC Tools 14.44'

Assert-Result (Test-SubmoduleCommitMatch -ExpectedCommit 'abc' -ActualCommit 'abc') $true 'matching submodule commit'
Assert-Result (Test-SubmoduleCommitMatch -ExpectedCommit 'abc' -ActualCommit 'def') $false 'stale submodule commit'
Assert-Result (Test-SubmoduleCommitMatch -ExpectedCommit 'abc' -ActualCommit '') $false 'missing submodule commit'

$archiveRoot = Join-Path ([System.IO.Path]::GetTempPath()) "fbe-editor-runtime-archive-$PID"
$versionPath = Join-Path ([System.IO.Path]::GetTempPath()) "fbe-editor-runtime-version-$PID.txt"
try {
    New-Item -ItemType Directory -Path $archiveRoot | Out-Null
    Assert-LexillaSubmoduleCheckout -RepositoryRoot $archiveRoot

    Set-Content -LiteralPath $versionPath -Value '5100' -NoNewline
    if ((Get-EditorDependencyVersion -Path $versionPath -Name 'test') -ne '5.10.0') {
        throw 'Four-digit editor-runtime version parsing failed.'
    }
}
finally {
    Remove-Item -LiteralPath $archiveRoot -Force -Recurse -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $versionPath -Force -ErrorAction SilentlyContinue
}

Write-Host 'Editor-runtime fingerprint behavior passed.'
