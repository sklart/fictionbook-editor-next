<# Runs the Integration staging entry point against isolated, non-release inputs. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$configuration = 'PackageLayoutFixture'
$fixtureRoot = Join-Path $repoRoot 'out\tests\package-layout-integration-fixture'
$shellRoot = Join-Path $repoRoot 'out\package\shell-build'
$shellWin32 = Join-Path $shellRoot "Win32\$configuration"
$shellX64 = Join-Path $shellRoot "x64\$configuration"
$stage = Join-Path $fixtureRoot 'stage'
try {
    foreach ($path in @($shellWin32, $shellX64, (Join-Path $fixtureRoot 'build\Lang\Shell'))) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
    }
    Set-Content -LiteralPath (Join-Path $shellWin32 'FBShell.dll') -Value 'win32 fixture' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $shellX64 'FBShell.dll') -Value 'x64 fixture' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $fixtureRoot 'build\Lang\Shell\FBVVerbResources.dll') -Value 'mui fixture' -Encoding ascii

    & (Join-Path $repoRoot 'tools\build\stage-integration.ps1') -Configuration $configuration -OutputDirectory $stage -BuildOutputDirectory (Join-Path $fixtureRoot 'build')
    foreach ($relativePath in @('FBShell.dll', 'FBShell64.dll', 'FBE.Sequence.propdesc', 'InstallerTools\register-modern-property-handler.ps1', 'Lang\Shell\FBVVerbResources.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $relativePath) -PathType Leaf)) {
            throw "Layout-driven Integration stage omitted: $relativePath"
        }
    }
    Write-Host 'Package layout Integration staging passed.'
}
finally {
    foreach ($path in @($fixtureRoot, $shellWin32, $shellX64)) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
    }
}
