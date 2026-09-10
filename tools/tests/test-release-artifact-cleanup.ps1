<# Exercises the legacy release-output cleanup function against isolated directories. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildScript = Join-Path $repoRoot 'tools\build\build.ps1'

$tokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $buildScript,
    [ref]$tokens,
    [ref]$parseErrors)
if ($parseErrors.Count -ne 0) {
    throw "Cannot parse build script: $($parseErrors[0].Message)"
}
$cleanupFunction = $ast.Find(
    { param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Remove-ObsoleteReleaseArtifacts' },
    $true)
if ($null -eq $cleanupFunction) {
    throw 'Remove-ObsoleteReleaseArtifacts is missing from build.ps1.'
}
Invoke-Expression $cleanupFunction.Extent.Text

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fbe-release-artifact-cleanup-" + [guid]::NewGuid().ToString('N'))
try {
    $emptyOutput = Join-Path $testRoot 'empty-defaults'
    $emptyDefaults = Join-Path $emptyOutput 'defaults'
    New-Item -ItemType Directory -Path $emptyDefaults -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $emptyDefaults 'Words.xml') -Value 'legacy seed' -NoNewline

    Remove-ObsoleteReleaseArtifacts -OutputDirectory $emptyOutput
    if (Test-Path -LiteralPath (Join-Path $emptyDefaults 'Words.xml')) {
        throw 'Legacy defaults\Words.xml was not removed.'
    }
    if (Test-Path -LiteralPath $emptyDefaults -PathType Container) {
        throw 'An empty defaults directory was not removed after the legacy seed.'
    }

    $nonEmptyOutput = Join-Path $testRoot 'nonempty-defaults'
    $nonEmptyDefaults = Join-Path $nonEmptyOutput 'defaults'
    New-Item -ItemType Directory -Path $nonEmptyDefaults -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $nonEmptyDefaults 'Words.xml') -Value 'legacy seed' -NoNewline
    $preservedFile = Join-Path $nonEmptyDefaults 'operator-file.txt'
    Set-Content -LiteralPath $preservedFile -Value 'keep me' -NoNewline

    Remove-ObsoleteReleaseArtifacts -OutputDirectory $nonEmptyOutput
    if (Test-Path -LiteralPath (Join-Path $nonEmptyDefaults 'Words.xml')) {
        throw 'Legacy defaults\Words.xml was not removed from a non-empty directory.'
    }
    if (-not (Test-Path -LiteralPath $nonEmptyDefaults -PathType Container)) {
        throw 'A non-empty defaults directory was removed.'
    }
    if (-not (Test-Path -LiteralPath $preservedFile -PathType Leaf)) {
        throw 'A file in a non-empty defaults directory was modified or removed.'
    }
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

Write-Host 'Release artifact cleanup regression passed.'
