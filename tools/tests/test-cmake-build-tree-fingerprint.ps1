<# Regression: a restored/generated CMake tree may only be reused with the
same VS instance and minor MSVC toolset. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$helper = Join-Path $repoRoot 'tools\build\CmakeBuildTree.ps1'
$testRoot = Join-Path $repoRoot 'build\tests\cmake-build-tree-fingerprint'
$buildDirectory = Join-Path $testRoot 'tree'
$installDirectory = Join-Path $testRoot 'install'

try {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
    # The test only exercises the generated-tree contract.  Use a registered
    # VS2022 instance for its disposable configure even if a developer also
    # has an unregistered portable IDE active in the parent environment.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $instance = @(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -version '[17.0,18.0)' -format json | ConvertFrom-Json)[0]
    $toolset = @(Get-ChildItem -LiteralPath (Join-Path $instance.installationPath 'VC\Tools\MSVC') -Directory | Select-Object -ExpandProperty Name | Sort-Object -Descending | Where-Object { $_ -match '^14\.44\.' })[0]
    $cmake = Get-ChildItem -LiteralPath (Join-Path $instance.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin') -Filter cmake.exe | Select-Object -First 1 -ExpandProperty FullName
    $vs = [PSCustomObject]@{ CMake = $cmake; CMakeVersion = ((& $cmake --version | Select-Object -First 1) -replace '^cmake version\s+', ''); Generator = 'Visual Studio 17 2022'; GeneratorToolset = "v143,version=$toolset"; InstallationPath = $instance.installationPath; InstallationVersion = $instance.installationVersion; VCToolsVersion = $toolset; GeneratorInstance = $instance.installationPath }
    $arguments = @{ DependencyName = 'fingerprint-fixture'; BuildDirectory = $buildDirectory; InstallDirectory = $installDirectory; Vs = $vs; PlatformToolset = 'v143' }

    # A stale restored tree must be discarded along with its matching install
    # prefix, not patched in place.
    New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
    New-Item -ItemType Directory -Path $installDirectory -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $buildDirectory 'stale.marker') -Value 'generated'
    Set-Content -LiteralPath (Join-Path $installDirectory 'stale.lib') -Value 'old library'
    @{ schemaVersion = 1; dependency = 'fingerprint-fixture'; generator = 'Visual Studio 17 2022'; installationPath = 'c:\BuildTools2022'; installationVersion = '17.0.0'; platform = 'Win32'; platformToolset = 'v143'; vcToolsVersion = '14.40.0'; generatorToolset = 'v143,version=14.40.0'; cmakeVersion = 'old' } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $buildDirectory '.fbe-cmake-fingerprint.json') -Encoding utf8NoBOM
    $prepared = & $helper -Action Prepare @arguments
    if (-not $prepared.Recreated -or (Test-Path -LiteralPath $buildDirectory) -or (Test-Path -LiteralPath $installDirectory)) {
        throw 'Stale CMake tree or its install prefix was not removed.'
    }

    # This is the fresh configure boundary: the selected instance and explicit
    # v143 minor toolset are passed exactly as dependency scripts pass them.
    & $vs.CMake -S (Join-Path $repoRoot 'tools\tests\fixtures\cmake-build-tree') -B $buildDirectory -G $vs.Generator -A Win32 -T $vs.GeneratorToolset "-DCMAKE_GENERATOR_INSTANCE=$($vs.GeneratorInstance)" "-DCMAKE_INSTALL_PREFIX=$installDirectory"
    if ($LASTEXITCODE -ne 0) { throw "Fresh CMake configure failed: $LASTEXITCODE" }
    & $helper -Action Write @arguments | Out-Null
    $reused = & $helper -Action Prepare @arguments
    if ($reused.Recreated -or -not (Test-Path -LiteralPath (Join-Path $buildDirectory 'CMakeCache.txt'))) {
        throw 'Current CMake tree was unexpectedly discarded instead of reused.'
    }
}
finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}

Write-Host 'CMake build-tree fingerprint regression passed.'
