<#
.SYNOPSIS
Exercises an installed upgrade from the pre-Plugins flat plug-in layout.

.DESCRIPTION
The test deliberately uses the setup artifact, not the staging scripts: it
seeds real legacy plug-in DLLs in an isolated install directory, runs a silent
upgrade, then verifies both the new layout and uninstaller cleanup.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InstallerPath,
    [Parameter(Mandatory)][string]$PluginSourceDirectory,
    [string]$TestDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$pluginSource = (Resolve-Path -LiteralPath $PluginSourceDirectory).Path
if ([string]::IsNullOrWhiteSpace($TestDirectory)) {
    $TestDirectory = Join-Path $repoRoot 'out\tests\bundled-plugin-upgrade-uninstall'
}
$testRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($TestDirectory)
$installDirectory = Join-Path $testRoot 'install'
$legacyDlls = @('ExportHTML.dll', 'ExportDOCX.dll', 'ExportEPUB.dll', 'ImportEPUB.dll', 'ImportEPUBLunaSVG.dll')
$installedPlugins = @('ExportHTML.dll', 'ExportDOCX.dll', 'ExportEPUB.dll', 'ImportEPUB.dll')

function Invoke-Silent([string]$Executable, [string[]]$Arguments, [string]$Description) {
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0) { throw "$Description завершился с кодом $($process.ExitCode)." }
}

if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
New-Item -ItemType Directory -Path $installDirectory -Force | Out-Null
try {
    foreach ($name in $legacyDlls) {
        $source = Join-Path $pluginSource $name
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Не найдена DLL для synthetic upgrade: $source" }
        Copy-Item -LiteralPath $source -Destination (Join-Path $installDirectory $name) -Force
    }

    # NSIS requires /D to be the final argument.  The isolated path contains
    # no spaces, so it is safe for the setup command-line parser.
    Invoke-Silent $installer @('/S', "/D=$installDirectory") 'Synthetic plugin upgrade installer'

    foreach ($name in $legacyDlls) {
        if (Test-Path -LiteralPath (Join-Path $installDirectory $name) -PathType Leaf) {
            throw "Upgrade оставил legacy DLL в корне: $name"
        }
    }
    $pluginsDirectory = Join-Path $installDirectory 'Plugins'
    if (-not (Test-Path -LiteralPath (Join-Path $pluginsDirectory 'plugins.json') -PathType Leaf)) {
        throw 'Upgrade не установил Plugins\plugins.json.'
    }
    foreach ($name in $installedPlugins) {
        if (-not (Test-Path -LiteralPath (Join-Path $pluginsDirectory $name) -PathType Leaf)) {
            throw "Upgrade не установил bundled DLL в Plugins: $name"
        }
    }

    $uninstaller = Join-Path $installDirectory 'uninst.exe'
    if (-not (Test-Path -LiteralPath $uninstaller -PathType Leaf)) { throw 'Upgrade не создал uninst.exe.' }
    Invoke-Silent $uninstaller @('/S') 'Synthetic plugin upgrade uninstaller'

    if (Test-Path -LiteralPath (Join-Path $pluginsDirectory 'plugins.json')) { throw 'Uninstall оставил Plugins\plugins.json.' }
    if (Test-Path -LiteralPath $pluginsDirectory) { throw 'Uninstall оставил каталог Plugins.' }
    if (Test-Path -LiteralPath $installDirectory) { throw 'Uninstall оставил файлы synthetic installation.' }
    Write-Host 'Bundled plugin upgrade/uninstall smoke passed.'
}
finally {
    # Leave no test installation behind even if an assertion failed.  The
    # directory is always the dedicated out\tests fixture created above.
    $uninstaller = Join-Path $installDirectory 'uninst.exe'
    if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
        Start-Process -FilePath $uninstaller -ArgumentList @('/S') -Wait -WindowStyle Hidden -ErrorAction SilentlyContinue | Out-Null
    }
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
