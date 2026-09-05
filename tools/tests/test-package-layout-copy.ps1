<# Exercises layout-driven copies without release artifacts or user directories. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $repoRoot 'tools\build\PackageLayout.ps1')
$sandbox = Join-Path ([System.IO.Path]::GetTempPath()) ("fbe-package-layout-" + [Guid]::NewGuid().ToString('N'))
try {
    $sourceRoot = Join-Path $sandbox 'source'
    $stage = Join-Path $sandbox 'stage'
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceRoot 'tree\nested') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceRoot 'tree\nested\resource.txt') -Value 'resource' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $sourceRoot 'single.txt') -Value 'single' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $sourceRoot 'alias.txt') -Value 'alias' -Encoding ascii
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    $entries = @(
        [pscustomobject]@{ sourceRoot = 'fixture'; source = 'tree'; destination = ''; contents = $true; required = $true },
        [pscustomobject]@{ sourceRoot = 'fixture'; source = 'single.txt'; destination = 'nested/single.txt'; required = $true },
        [pscustomobject]@{ sourceRoot = 'fixture'; source = 'tree'; destination = 'copied-tree'; recursive = $true; required = $true },
        [pscustomobject]@{ sourceRoot = 'fixture'; source = 'alias.txt'; destination = 'alias.txt'; required = $true }
    )
    Copy-FbePackageLayoutEntries -Entries $entries -SourceRoots @{ fixture = $sourceRoot } -StageDirectory $stage
    Copy-FbePackageLayoutAliases -Aliases @([pscustomobject]@{ source = 'alias.txt'; destination = 'renamed-alias.txt'; required = $true }) -StageDirectory $stage

    foreach ($relativePath in @('nested\resource.txt', 'nested\single.txt', 'copied-tree\nested\resource.txt', 'renamed-alias.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $relativePath) -PathType Leaf)) {
            throw "Layout copy did not produce expected file: $relativePath"
        }
    }
    Write-Host 'Package layout copy behavior passed.'
}
finally {
    if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
}
