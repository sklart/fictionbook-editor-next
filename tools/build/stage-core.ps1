[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('Modern', 'Win7')][string]$CompatibilityTarget = 'Modern',
    [string]$EditorRuntimeDirectory = '',
    [string]$BatchOutputDirectory = '',
    [string]$ArchHandlerOutputDirectory = '',
    [string]$CommonCoreDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildOutput = Join-Path $repoRoot "out\$Configuration"
$editorRuntime = if ($EditorRuntimeDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EditorRuntimeDirectory) } else { Join-Path $repoRoot 'runtime' }
$batchOutput = if ($BatchOutputDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BatchOutputDirectory) } else { $buildOutput }
$archOutput = if ($ArchHandlerOutputDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArchHandlerOutputDirectory) } else { Join-Path $repoRoot "out\archhandler\Win32\$Configuration" }
$commonCore = if ($CommonCoreDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($CommonCoreDirectory) } else { '' }
$stage = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)

foreach ($path in @($buildOutput, $editorRuntime, $batchOutput, $archOutput)) {
    if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "Не найден подготовленный input Core: $path" }
}
if ($commonCore -and -not (Test-Path -LiteralPath $commonCore -PathType Container)) { throw "Не найден общий Core payload: $commonCore" }
$commonSource = if ($commonCore) { $commonCore } else { $buildOutput }
& (Join-Path $PSScriptRoot 'build-provenance.ps1') -Action Validate -Kind CommonCore `
    -Configuration $Configuration -CommonDirectory $commonSource
& (Join-Path $PSScriptRoot 'build-provenance.ps1') -Action Validate -Kind $CompatibilityTarget `
    -Configuration $Configuration -ProfileDirectory $editorRuntime -BatchDirectory $batchOutput -ArchHandlerDirectory $archOutput
foreach ($name in @('FBE.exe','FBV.exe','ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $buildOutput $name) -PathType Leaf)) { throw "Не найден Core artifact: $name" }
}
foreach ($name in @('ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $batchOutput $name) -PathType Leaf)) { throw "Не найден Core batch artifact: $name" }
}
foreach ($name in @('Scintilla.dll','Lexilla.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $editorRuntime $name) -PathType Leaf)) { throw "Не найден Core runtime artifact: $name" }
}
foreach ($name in @('ZipHandler.exe','RarHandler.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $archOutput $name) -PathType Leaf)) { throw "Не найден Core ArchHandler artifact: $name" }
}

if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
@(
    '[Deployment]',
    "CompatibilityTarget=$CompatibilityTarget",
    'Architecture=Win32'
) | Set-Content -LiteralPath (Join-Path $stage 'deployment.ini') -Encoding ASCII
Copy-Item -Path (Join-Path $repoRoot 'runtime\*') -Destination $stage -Recurse -Force
# Current Librusec catalog names are shipped alongside the historical *_L
# aliases.  The executable accepts both while older manual installations keep
# working, but every newly staged portable payload must expose the current
# names directly.
foreach ($entry in @{ 'genres.txt_L' = 'genres.librusec.txt'; 'genres.rus.txt_L' = 'genres.rus.librusec.txt' }.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $stage $entry.Key) -Destination (Join-Path $stage $entry.Value) -Force
}
foreach ($name in @('FBShell.dll','FBShell64.dll','FBE.Sequence.propdesc')) { Remove-Item -LiteralPath (Join-Path $stage $name) -Force -ErrorAction SilentlyContinue }
foreach ($name in @('Scintilla.dll','Lexilla.dll')) { Copy-Item -LiteralPath (Join-Path $editorRuntime $name) -Destination $stage -Force }
foreach ($name in @('FBE.exe','FBV.exe','ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll')) { Copy-Item -LiteralPath (Join-Path $buildOutput $name) -Destination $stage -Force }
if ($commonCore) {
    # Win7-профиль меняет только editor runtime и batch-конвертеры. Общие
    # приложения, плагины и локализованные Win32-ресурсы берём из Modern.
    foreach ($name in @('FBE.exe','FBV.exe','ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll')) {
        Copy-Item -LiteralPath (Join-Path $commonCore $name) -Destination $stage -Force
    }
}
foreach ($name in @('ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe')) { Copy-Item -LiteralPath (Join-Path $batchOutput $name) -Destination $stage -Force }
foreach ($entry in @{ 'ru-RU' = 'res_rus.dll'; 'uk-UA' = 'res_ukr.dll' }.GetEnumerator()) {
    $source = Join-Path $buildOutput "Lang\\$($entry.Key)\\$($entry.Value)"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Не найден Core localization artifact: $source" }
    $destination = Join-Path $stage "Lang\\$($entry.Key)"; New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}
if ($commonCore) {
    foreach ($entry in @{ 'ru-RU' = 'res_rus.dll'; 'uk-UA' = 'res_ukr.dll' }.GetEnumerator()) {
        Copy-Item -LiteralPath (Join-Path $commonCore "Lang\\$($entry.Key)\\$($entry.Value)") -Destination (Join-Path $stage "Lang\\$($entry.Key)") -Force
    }
}
$archDestination = Join-Path $stage 'Utilities\ArchHandler'; New-Item -ItemType Directory -Path $archDestination -Force | Out-Null
foreach ($name in @('ZipHandler.exe','RarHandler.exe')) { Copy-Item -LiteralPath (Join-Path $archOutput $name) -Destination $archDestination -Force }
foreach ($name in @('custom.dic','Hotkeys.xml','languages.txt','root_genres.xml','Words.xml')) { Copy-Item -LiteralPath (Join-Path $repoRoot $name) -Destination $stage -Force }
& (Join-Path $repoRoot 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $repoRoot -OutputDirectory (Join-Path $stage 'Lang') -Clean
foreach ($entry in @{ 'ru-RU' = 'res_rus.dll'; 'uk-UA' = 'res_ukr.dll' }.GetEnumerator()) {
    $source = Join-Path $buildOutput "Lang\\$($entry.Key)\\$($entry.Value)"
    $destination = Join-Path $stage "Lang\\$($entry.Key)"; New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}
if ($commonCore) {
    foreach ($entry in @{ 'ru-RU' = 'res_rus.dll'; 'uk-UA' = 'res_ukr.dll' }.GetEnumerator()) {
        Copy-Item -LiteralPath (Join-Path $commonCore "Lang\\$($entry.Key)\\$($entry.Value)") -Destination (Join-Path $stage "Lang\\$($entry.Key)") -Force
    }
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'runtime\gpl-3.0.txt') -Destination (Join-Path $stage 'LICENSE') -Force
Remove-Item -LiteralPath (Join-Path $stage 'gpl-3.0.txt') -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $repoRoot 'NOTICE') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'THIRD-PARTY-NOTICES.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'THIRD-PARTY-LICENSES') -Destination $stage -Recurse -Force
$licenseDestination = Join-Path $stage 'THIRD-PARTY-LICENSES'
$licenses = @{ 'Scintilla-Lexilla.txt'='third_party\scintilla\License.txt'; 'PCRE2.txt'='third_party\pcre2\LICENCE.md'; 'Hunspell.txt'='third_party\hunspell\license.hunspell'; 'Hunspell-MySpell.txt'='third_party\hunspell\license.myspell'; 'libwebp.txt'='third_party\libwebp\COPYING'; 'OpenJPEG.txt'='third_party\openjpeg\LICENSE'; 'libheif.txt'='third_party\libheif\COPYING'; 'libde265.txt'='third_party\libde265\COPYING'; 'libaom.txt'='third_party\aom\LICENSE'; 'libaom-PATENTS.txt'='third_party\aom\PATENTS'; 'LunaSVG.txt'='src\import-epub\thirdparty\lunasvg\LICENSE'; 'PlutoVG.txt'='src\import-epub\thirdparty\lunasvg\plutovg\LICENSE'; 'UAC.txt'='third_party\uac\License.txt' }
foreach ($entry in $licenses.GetEnumerator()) { Copy-Item -LiteralPath (Join-Path $repoRoot $entry.Value) -Destination (Join-Path $licenseDestination $entry.Key) -Force }
Remove-Item -LiteralPath (Join-Path $stage 'Themes\licenses') -Recurse -Force -ErrorAction SilentlyContinue
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Core -StageDirectory $stage
Write-Host "Core stage prepared: $stage"
