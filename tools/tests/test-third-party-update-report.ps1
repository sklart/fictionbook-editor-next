[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\ThirdPartyUpdateReport.ps1')
. (Join-Path $root 'tools\build\ThirdPartySources.ps1')
. (Join-Path $root 'tools\build\DictionarySources.ps1')

foreach ($entry in @((Get-DependencyCatalog)) + @((Get-DictionaryCatalog))) {
    if (-not (Get-ThirdPartyUpdatesPage -Item $entry)) { throw "Missing update page for $($entry.Name)." }
}

$catalogPaths = @(Get-DependencyCatalog | ForEach-Object { $_.RelativePath -replace '\\', '/' })
$submodules = Get-Content -Raw -LiteralPath (Join-Path $root '.gitmodules')
foreach ($match in [regex]::Matches($submodules, '(?m)^\s*path\s*=\s*(third_party/\S+)')) {
    if ($catalogPaths -notcontains $match.Groups[1].Value) { throw "Unmonitored submodule: $($match.Groups[1].Value)" }
}
$unmonitoredDirectories = @(Get-ChildItem -LiteralPath (Join-Path $root 'third_party') -Directory |
    Where-Object { $catalogPaths -notcontains ('third_party/' + $_.Name) } |
    Select-Object -ExpandProperty Name)
if ($unmonitoredDirectories.Count -ne 1 -or $unmonitoredDirectories[0] -ne 'uac') {
    throw "Unexpected unmonitored third_party directories: $($unmonitoredDirectories -join ', ')"
}

$pin = Get-DependencyEntry -Name 'fluent-pin'
$pinFiles = @(foreach ($name in $pin.AssetFiles) {
    [pscustomobject]@{ name=$name; sha=(Get-GitBlobSha1 -Bytes ([IO.File]::ReadAllBytes((Join-Path $pin.LocalPath $name)))) }
})
if ($pinFiles.Count -ne 10) { throw 'Fluent Pin catalog must cover all ten used SVGs.' }
$pinCurrent = Get-FluentPinUpdateInfo -Dependency $pin -RemoteFiles $pinFiles
if ($pinCurrent.Status -ne 'UpToDate') { throw 'Identical Fluent Pin SVGs must be current.' }
$pinFiles[0].sha = '0000000000000000000000000000000000000000'
$pinChanged = Get-FluentPinUpdateInfo -Dependency $pin -RemoteFiles $pinFiles
if ($pinChanged.Status -ne 'UpdateAvailable' -or $pinChanged.ChangedFiles.Count -ne 1 -or $pinChanged.ChangedFiles[0] -ne $pinFiles[0].name) {
    throw 'A changed Fluent Pin SVG must produce a precise update report.'
}
if ((Get-ThirdPartyUpdatesPage -Item $pinChanged) -ne 'https://github.com/microsoft/fluentui-system-icons/tree/main/assets/Pin/SVG') {
    throw 'Wrong Fluent Pin upstream link.'
}

$item = [pscustomobject]@{
    Name = 'libarchive'; DisplayName = 'libarchive'; Repository = 'https://github.com/libarchive/libarchive.git'
    LocalVersion = '3.8.9'; RemoteVersion = '3.8.10'; RemoteTag = 'v3.8.10'
}
$expected = 'libarchive: local 3.8.9, latest 3.8.10 (v3.8.10) https://github.com/libarchive/libarchive/releases'
if ((Get-ThirdPartyUpdatesPage -Item $item) -ne 'https://github.com/libarchive/libarchive/releases') { throw 'Wrong libarchive releases link.' }
foreach ($case in @(
    @{ Name='upx'; Repository='upx/upx'; Expected='https://github.com/upx/upx/releases' },
    @{ Name='dict-de_DE'; Repository='https://github.com/LibreOffice/dictionaries'; Expected='https://github.com/LibreOffice/dictionaries/commits/master/de' },
    @{ Name='aom'; Repository='https://aomedia.googlesource.com/aom'; Expected='https://aomedia.googlesource.com/aom/+refs' },
    @{ Name='scintilla'; Repository='https://www.scintilla.org/'; Expected='https://www.scintilla.org/ScintillaDownload.html' },
    @{ Name='wtl'; Repository='https://sourceforge.net/projects/wtl/'; Expected='https://sourceforge.net/projects/wtl/files/' }
)) {
    $entry = [pscustomobject]@{ Name=$case.Name; Repository=$case.Repository }
    if ((Get-ThirdPartyUpdatesPage -Item $entry) -ne $case.Expected) { throw "Wrong update link for $($case.Name)." }
}

$previousActions = $env:GITHUB_ACTIONS
try {
    $env:GITHUB_ACTIONS = 'true'
    $ciRecords = @(Write-ThirdPartyUpdateNotice -Item $item 6>&1 3>&1)
    if ($ciRecords.Count -ne 1 -or [string]$ciRecords[0] -ne ('::warning title=Third-party update available::' + $expected)) {
        throw 'CI must emit exactly one linked update annotation.'
    }
    $reviewRecords = @(Write-ThirdPartyUpdateNotice -Item $item -ManualReview 6>&1 3>&1)
    if ($reviewRecords.Count -ne 1 -or [string]$reviewRecords[0] -notmatch '^::warning title=Third-party manual review::libarchive: manual review required .* https://github.com/libarchive/libarchive/releases$') {
        throw 'CI must emit exactly one linked manual-review annotation.'
    }
    $pinRecords = @(Write-ThirdPartyUpdateNotice -Item $pinChanged 6>&1 3>&1)
    if ($pinRecords.Count -ne 1 -or [string]$pinRecords[0] -notmatch '::warning title=Third-party update available::Fluent Pin SVGs: upstream SVGs changed \(ic_fluent_pin_16_regular.svg\); pinned .* https://github.com/microsoft/fluentui-system-icons/tree/main/assets/Pin/SVG$') {
        throw 'CI must emit one linked Fluent Pin asset warning.'
    }

    $env:GITHUB_ACTIONS = 'false'
    $localRecords = @(Write-ThirdPartyUpdateNotice -Item $item 3>&1)
    if ($localRecords.Count -ne 1 -or [string]$localRecords[0] -ne $expected) {
        throw 'Local run must emit exactly one linked warning.'
    }
}
finally {
    $env:GITHUB_ACTIONS = $previousActions
}

Write-Host 'Third-party update report links and single-warning output passed.'
