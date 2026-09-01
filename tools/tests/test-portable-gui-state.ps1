<# Full-only, deterministic GUI E2E for a materialised portable package. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageDirectory)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$package = (Resolve-Path -LiteralPath $PackageDirectory).Path
$testRoot = Join-Path $root 'out\tests\portable-gui-state'

function Get-FileTreeSnapshot([string]$Directory) {
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) { return '<absent>' }
    return (Get-ChildItem -LiteralPath $Directory -Recurse -File | Sort-Object FullName | ForEach-Object {
        "$($_.FullName)|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)|$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
    }) -join "`n"
}
function Get-RegistrySnapshot([string]$Key) {
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & reg.exe query $Key /s 2>&1
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($LASTEXITCODE -eq 1) { return '<absent>' }
    if ($LASTEXITCODE -ne 0) { throw "reg.exe query failed for ${Key}: $output" }
    return (($output | ForEach-Object { [string]$_ }) -join "`n").Trim()
}
function Invoke-PortableStateScenario([string]$Scenario) {
    $oldMode = $env:FBE_NEXT_TEST_MODE; $oldScenario = $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = $Scenario
        $process = Start-Process -FilePath (Join-Path $testRoot 'FBE.exe') -ArgumentList @('--portable', $fixture) -WorkingDirectory $testRoot -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Portable GUI state scenario $Scenario exited with $($process.ExitCode)." }
    } finally {
        $env:FBE_NEXT_TEST_MODE = $oldMode; $env:FBE_NEXT_TEST_SCENARIO = $oldScenario
    }
}

$registryKeys = @(
    'HKCU\Software\FBETeam\FictionBook Editor Next',
    'HKCU\Software\Classes\FictionBook.2', 'HKCU\Software\Classes\.fb2',
    'HKCU\Software\Classes\TypeLib\{37B16C7D-4400-4D7D-AA35-14C74E265EA4}',
    'HKCU\Software\Classes\CLSID\{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}',
    'HKCU\Software\Classes\CLSID\{C3098839-EF69-4DE5-B27D-1E80051CA843}',
    'HKCU\Software\Classes\CLSID\{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}',
    'HKCU\Software\Classes\CLSID\{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}'
)
$installedData = Join-Path $env:LOCALAPPDATA 'FBE Next'
$installedBefore = Get-FileTreeSnapshot $installedData
$registryBefore = @{}; foreach ($key in $registryKeys) { $registryBefore[$key] = Get-RegistrySnapshot $key }

Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath $package -Destination $testRoot -Recurse -Force
$fixture = Join-Path $testRoot 'portable-state-sentinel.fb2'
Copy-Item -LiteralPath (Join-Path $root 'tools\tests\fb2-metadata-cyrillic-smoke.fb2') -Destination $fixture

# The real GUI creates and reloads these values through a test-only,
# self-closing scenario; no SendKeys, timing sleeps, or user profile writes.
Invoke-PortableStateScenario 'portable-state-write'
$data = Join-Path $testRoot 'Data'
$settings = Join-Path $data 'Settings'
foreach ($path in @(
    (Join-Path $settings 'Settings.xml'), (Join-Path $settings 'Hotkeys.xml'),
    (Join-Path $settings 'Words.xml'), (Join-Path $settings 'MRU.xml'), (Join-Path $settings 'Toolbars.xml'),
    (Join-Path $data 'Diagnostics\portable-state-sentinel.txt'),
    (Join-Path $data 'Recovery\Recovery.fb2')
)) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Portable GUI did not persist $path." } }
if ((Get-Content -Raw -LiteralPath (Join-Path $settings 'Words.xml')) -notmatch 'portable-state-sentinel') { throw 'Words sentinel was not persisted.' }
if ((Get-Content -Raw -Encoding Unicode -LiteralPath (Join-Path $settings 'MRU.xml')) -notmatch 'portable-state-sentinel\.fb2') { throw 'MRU sentinel was not persisted.' }
$hotkeysXml = Get-Content -Raw -LiteralPath (Join-Path $settings 'Hotkeys.xml')
if ($hotkeysXml -notmatch '<Name>Words</Name>\s*<Accel>13;135</Accel>') { throw 'Hotkeys exact sentinel Ctrl+Shift+F24 was not persisted.' }
$settingsXml = Get-Content -Raw -LiteralPath (Join-Path $settings 'Settings.xml')
if ($settingsXml -notmatch '<Toolbars>[^<]*60160,\d+,731;') { throw 'Toolbar exact sentinel width for the first rebar band was not persisted.' }
$portableToolbarsXml = Get-Content -Raw -Encoding Unicode -LiteralPath (Join-Path $settings 'Toolbars.xml')
if ($portableToolbarsXml -notmatch '<Toolbars version="1">' -or $portableToolbarsXml -notmatch '<Toolbar name="Command">' -or $portableToolbarsXml -notmatch '<Toolbar name="Scripts">') { throw 'Portable toolbar layout was not persisted as versioned structured data.' }
$persistedSnapshot = Get-FileTreeSnapshot $data

Invoke-PortableStateScenario 'portable-state-read'
$report = Get-Content -Raw -LiteralPath (Join-Path $data 'Diagnostics\portable-state-report.txt')
foreach ($state in @('settings','hotkeys','words','locale','mru','toolbar','scripts','diagnostics','recovery')) {
    if ($report -notmatch "(?m)^$state=1$") { throw "Portable state was not read after restart: $state." }
}
if ($report -notmatch '(?m)^result=pass$') { throw "Portable GUI state readback failed:`n$report" }

# Simulate a portable copy which was moved from E: to the current package.
# The old entry must be migrated by its relative script suffix; a missing peer
# must not prevent that migration.
$scriptTemplate = Get-ChildItem -LiteralPath (Join-Path $root 'runtime\Scripts') -Recurse -Filter '*.js' |
    Where-Object { (Get-Content -Raw -LiteralPath $_.FullName) -match 'function\s+Run\s*\(' } |
    Select-Object -First 1
if ($null -eq $scriptTemplate) { throw 'Could not locate a valid script fixture with Run().' }
Copy-Item -LiteralPath $scriptTemplate.FullName -Destination (Join-Path $data 'Scripts\foo.js') -Force
$legacyHotkeys = Get-Content -Raw -LiteralPath (Join-Path $settings 'Hotkeys.xml')
$legacyScriptEntries = @"
<Hotkeys>
    <Hotkey><Name>E:\BeforeMove\Scripts\foo.js</Name><Accel>9;120</Accel></Hotkey>
    <Hotkey><Name>E:\BeforeMove\Scripts\missing.js</Name><Accel>9;121</Accel></Hotkey>
</Hotkeys>
"@
$scriptsGroupMatch = [regex]::Match($legacyHotkeys, '(?s)(<GroupName>Scripts</GroupName>\s*)<Hotkeys\s*/>')
if (-not $scriptsGroupMatch.Success) { throw 'Could not prepare legacy Scripts hotkey fixture.' }
$legacyHotkeys = $legacyHotkeys.Substring(0, $scriptsGroupMatch.Index) + $scriptsGroupMatch.Groups[1].Value + $legacyScriptEntries + $legacyHotkeys.Substring($scriptsGroupMatch.Index + $scriptsGroupMatch.Length)
Set-Content -LiteralPath (Join-Path $settings 'Hotkeys.xml') -Value $legacyHotkeys -Encoding utf8
Invoke-PortableStateScenario 'portable-legacy-hotkey-read'
$legacyReport = Get-Content -Raw -LiteralPath (Join-Path $data 'Diagnostics\portable-state-report.txt')
if ($legacyReport -notmatch '(?m)^legacy-hotkey=1$' -or $legacyReport -notmatch '(?m)^result=pass$') { throw "Moved portable legacy hotkey was not migrated:`n$legacyReport" }
$migratedHotkeys = Get-Content -Raw -LiteralPath (Join-Path $settings 'Hotkeys.xml')
if ($migratedHotkeys -notmatch '<Name>foo\.js</Name>' -or $migratedHotkeys -match 'BeforeMove|missing\.js') { throw 'Legacy Hotkeys.xml was not rewritten with only the valid relative script key.' }

# An explicitly empty toolbar is a valid customization, not a missing/corrupt
# settings file.  Verify it survives a second real application start.
Invoke-PortableStateScenario 'portable-toolbar-empty-write'
$emptyToolbarXml = Get-Content -Raw -Encoding Unicode -LiteralPath (Join-Path $settings 'Toolbars.xml')
if ($emptyToolbarXml -notmatch '(?s)<Toolbar name="Command">\s*</Toolbar>' -or $emptyToolbarXml -notmatch '(?s)<Toolbar name="Scripts">\s*</Toolbar>') { throw 'Empty portable toolbars were not serialized explicitly.' }
Invoke-PortableStateScenario 'portable-toolbar-empty-read'
$emptyReport = Get-Content -Raw -LiteralPath (Join-Path $data 'Diagnostics\portable-state-report.txt')
if ($emptyReport -notmatch '(?m)^empty-toolbar=1$' -or $emptyReport -notmatch '(?m)^result=pass$') { throw "Empty portable toolbar was replaced by default after restart:`n$emptyReport" }

# A removed script recorded in a saved toolbar must be ignored; it must not
# resurrect the default scripts toolbar.
@"
<Toolbars version="1">
  <Toolbar name="Command">
  </Toolbar>
  <Toolbar name="Scripts">
    <Script path="deleted-script.js" />
  </Toolbar>
</Toolbars>
"@ | Set-Content -LiteralPath (Join-Path $settings 'Toolbars.xml') -Encoding Unicode
Invoke-PortableStateScenario 'portable-toolbar-empty-read'
$deletedScriptReport = Get-Content -Raw -LiteralPath (Join-Path $data 'Diagnostics\portable-state-report.txt')
if ($deletedScriptReport -notmatch '(?m)^empty-toolbar=1$') { throw 'Deleted saved script reset a toolbar to its defaults.' }
$rewrittenToolbars = Get-Content -Raw -Encoding Unicode -LiteralPath (Join-Path $settings 'Toolbars.xml')
if ($rewrittenToolbars -match 'deleted-script\.js') { throw 'Deleted script remained in the saved toolbar after restart.' }

Invoke-PortableStateScenario 'portable-scripts-reload'
$reloadReport = Get-Content -Raw -LiteralPath (Join-Path $data 'Diagnostics\portable-state-report.txt')
if ($reloadReport -notmatch '(?m)^gdi-stable=1$' -or $reloadReport -notmatch '(?m)^result=pass$') { throw "Repeated script reload leaked GDI objects:`n$reloadReport" }
if ((Get-FileTreeSnapshot $data) -eq '<absent>' -or $persistedSnapshot -eq '<absent>') { throw 'Portable Data disappeared after restart.' }
if ((Get-FileTreeSnapshot $installedData) -cne $installedBefore) { throw '%LOCALAPPDATA%\FBE Next changed during portable GUI state test.' }
foreach ($key in $registryKeys) {
    if ((Get-RegistrySnapshot $key) -cne $registryBefore[$key]) { throw "Portable GUI changed FBE-owned registry state: $key" }
}
Write-Host 'Portable full-state GUI persistence and isolation passed.'
