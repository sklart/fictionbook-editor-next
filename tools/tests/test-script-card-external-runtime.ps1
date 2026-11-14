#Requires -Version 7.0
<# Nonvisual, isolated checks of card lookup and settings storage. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$sample = Get-ChildItem (Join-Path $repo 'runtime\ScriptCards') -Filter '*.json' -File -Recurse | Select-Object -First 1
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'card-external'
$firstIso = $iso
$otherIso = $null
$passed = $false
$saved = @{}
foreach($name in @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_CARD_SCRIPT','FBE_NEXT_TEST_CARD_RESOURCE_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    $emptyScripts = Join-Path $iso.Root 'fixture-empty'
    New-Item -ItemType Directory -Force $emptyScripts | Out-Null
    $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory $emptyScripts
    $fixture = Join-Path $iso.Root 'fixture.fb2'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>T</nickname></author><book-title>Test</book-title><lang>en</lang></title-info><document-info><author><nickname>T</nickname></author><date>2026-01-01</date><id>test</id><version>1</version></document-info></description><body><section><p>test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $script = Join-Path $iso.Root 'Scripts\07_Проверка\01_Тест.js'
    $default = Join-Path $iso.Root 'ScriptCards\07_Проверка\01_Тест.json'
    $userCard = Join-Path $iso.DataPath 'Settings\ScriptCards\07_Проверка\01_Тест.json'
    New-Item -ItemType Directory -Force (Split-Path $script), (Split-Path $default) | Out-Null
    $code = [Text.Encoding]::UTF8.GetBytes("throw 'must not execute';`r`n")
    [IO.File]::WriteAllBytes($script, $code)
    $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($code)).ToLowerInvariant()
    $card = Get-Content -LiteralPath $sample.FullName -Raw -Encoding utf8 | ConvertFrom-Json -AsHashtable
    $card.summary = 'External initial'; $card.sourceRevision.algorithm = 'sha256-raw-js-v1'; $card.sourceRevision.sha256 = $hash
    $dependency = Join-Path $iso.Root 'dep.bin'; [IO.File]::WriteAllText($dependency, 'old', [Text.UTF8Encoding]::new($false))
    $card.dependencies = @(@{path='dep.bin'; sha256=(Get-FileHash -LiteralPath $dependency).Hash.ToLowerInvariant(); optional=$false}); $card.dependenciesComplete = $true
    function Write-Card($path, $value) { [IO.File]::WriteAllText($path, ($value | ConvertTo-Json -Depth 100), [Text.UTF8Encoding]::new($false)) }
    Write-Card $default $card
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'script-card-external-runtime'; $env:FBE_NEXT_TEST_CARD_SCRIPT = $script; $env:FBE_NEXT_TEST_CARD_RESOURCE_ROOT = $iso.Root
    $report = Join-Path $iso.Root 'card-probe.txt'
    function Run-Probe([string]$mode = '--portable') {
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @($mode, '-b', $report, $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
        if($exitCode -ne 0) { throw "Probe exited $exitCode" }
        return Get-Content -LiteralPath $report -Raw -Encoding utf8
    }
    function Require([string]$text, [string]$expression) { if($text -notmatch $expression) { throw "Expected $expression in: $text" } }
    $first = Run-Probe; Require $first '(?m)^state=3\r?$'; Require $first '(?m)^dependency-state=1\r?$'; Require $first '(?m)^preview=External initial\r?$'
    [IO.File]::WriteAllText($dependency, 'new', [Text.UTF8Encoding]::new($false))
    $dependencyChanged = Run-Probe; Require $dependencyChanged '(?m)^state=3\r?$'; Require $dependencyChanged '(?m)^dependency-state=2\r?$'
    [IO.File]::WriteAllText($dependency, 'old', [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_CARD_SCRIPT = $script.Replace('\','/')
    $mixedSeparators = Run-Probe; Require $mixedSeparators '(?m)^state=3\r?$'; Require $mixedSeparators '(?m)^preview=External initial\r?$'
    $env:FBE_NEXT_TEST_CARD_SCRIPT = $script
    if(-not (Test-Path -LiteralPath $userCard)) { throw 'Portable card was not seeded in DataPath settings.' }
    $bundledRoot = Join-Path $iso.Root 'ScriptCards'; $settingsCardsRoot = Join-Path $iso.DataPath 'Settings\ScriptCards'
    $bundledFiles = @(Get-ChildItem -LiteralPath $bundledRoot -File -Recurse -Filter '*.json')
    foreach($file in $bundledFiles) {
        $relative = [IO.Path]::GetRelativePath($bundledRoot, $file.FullName)
        if(-not (Test-Path -LiteralPath (Join-Path $settingsCardsRoot $relative) -PathType Leaf)) { throw "Missing seeded card: $relative" }
    }
    $manifest = Join-Path $settingsCardsRoot '.fbe-defaults.ini'
    if(-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { throw 'Bundled card manifest was not created.' }
    $manifestEntries = @((Get-Content -LiteralPath $manifest) | Where-Object { $_ -match '^[0-9a-f]{64}=[0-9a-f]{64}$' })
    if($manifestEntries.Count -ne $bundledFiles.Count) { throw "Manifest has $($manifestEntries.Count) entries for $($bundledFiles.Count) bundled cards." }
    $uneditedHash = (Get-FileHash $userCard).Hash
    $card.summary = 'Unedited default update'; Write-Card $default $card
    $update = Run-Probe; Require $update '(?m)^state=3\r?$'; Require $update '(?m)^preview=External initial\r?$'
    if((Get-FileHash $userCard).Hash -ne $uneditedHash -or
       -not (Get-ChildItem (Split-Path $userCard) -Filter '*.fbe-new-*') -or
       (Get-Content -LiteralPath $manifest -Raw) -notmatch '(?m)^\[updates\]\r?$') { throw 'Unedited default update was not preserved for review.' }
    $user = Get-Content $userCard -Raw -Encoding utf8 | ConvertFrom-Json -AsHashtable
    $user.summary = 'User changed'; Write-Card $userCard $user; $userHash = (Get-FileHash $userCard).Hash
    $card.summary = 'New bundled revision'; Write-Card $default $card
    $second = Run-Probe; Require $second '(?m)^state=3\r?$'; Require $second '(?m)^preview=User changed\r?$'
    if((Get-FileHash $userCard).Hash -ne $userHash -or -not (Get-ChildItem (Split-Path $userCard) -Filter '*.fbe-new-*')) { throw 'User card was overwritten or updated default was not preserved.' }
    $firstIso = $iso; $firstFixture = $fixture; $firstScript = $script; $firstReport = $report
    $otherIso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'card-external-copy'
    $otherEmpty = Join-Path $otherIso.Root 'fixture-empty'; New-Item -ItemType Directory -Force $otherEmpty | Out-Null
    $null = New-IsolatedFbeSettingsSeed -Isolation $otherIso -StartupScriptsDirectory $otherEmpty
    $otherScript = Join-Path $otherIso.Root 'Scripts\07_Проверка\01_Тест.js'
    $otherDefault = Join-Path $otherIso.Root 'ScriptCards\07_Проверка\01_Тест.json'
    New-Item -ItemType Directory -Force (Split-Path $otherScript), (Split-Path $otherDefault) | Out-Null
    [IO.File]::WriteAllBytes($otherScript, $code); Write-Card $otherDefault $card
    [IO.File]::WriteAllText((Join-Path $otherIso.Root 'dep.bin'), 'old', [Text.UTF8Encoding]::new($false))
    $iso = $otherIso; $fixture = Join-Path $iso.Root 'fixture.fb2'; Copy-Item -LiteralPath $firstFixture -Destination $fixture
    $script = $otherScript; $report = Join-Path $iso.Root 'card-probe.txt'; $env:FBE_NEXT_TEST_CARD_SCRIPT = $script; $env:FBE_NEXT_TEST_CARD_RESOURCE_ROOT = $iso.Root
    $otherCopy = Run-Probe; Require $otherCopy '(?m)^state=3\r?$'; Require $otherCopy '(?m)^dependency-state=1\r?$'; Require $otherCopy '(?m)^preview=New bundled revision\r?$'
    if((Get-FileHash $userCard).Hash -ne $userHash) { throw 'A second Portable copy changed the first copy.' }
    $iso = $firstIso; $fixture = $firstFixture; $script = $firstScript; $report = $firstReport; $env:FBE_NEXT_TEST_CARD_SCRIPT = $script; $env:FBE_NEXT_TEST_CARD_RESOURCE_ROOT = $iso.Root
    $secondData = Join-Path $iso.Root 'Данные'; New-Item -ItemType Directory -Force (Join-Path $secondData 'Settings') | Out-Null
    Copy-Item -LiteralPath (Join-Path $iso.DataPath 'Settings\Settings.xml') -Destination (Join-Path $secondData 'Settings\Settings.xml')
    [IO.File]::WriteAllText((Join-Path $iso.Root 'portable.ini'), "[Portable]`r`nDataPath=Данные`r`n", [Text.UnicodeEncoding]::new($false, $true))
    $other = Run-Probe; Require $other '(?m)^state=3\r?$'; Require $other '(?m)^preview=New bundled revision\r?$'
    if((Get-FileHash $userCard).Hash -ne $userHash) { throw 'A second Portable profile changed the first profile.' }
    [IO.File]::WriteAllText((Join-Path $iso.Root 'portable.ini'), "[Portable]`r`nDataPath=$([IO.Path]::GetFileName($iso.DataPath))`r`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($userCard, '{invalid', [Text.UTF8Encoding]::new($false))
    $invalid = Run-Probe; Require $invalid '(?m)^state=1\r?$'; Require $invalid '(?m)^preview=\r?$'
    $uncheckedCard = $user.Clone(); $uncheckedCard.sourceRevision = $user.sourceRevision.Clone(); $uncheckedCard.sourceRevision.sha256 = $null
    Write-Card $userCard $uncheckedCard
    $unchecked = Run-Probe; Require $unchecked '(?m)^state=2\r?$'
    Write-Card $userCard $user
    [IO.File]::AppendAllText($script, '// changed', [Text.UTF8Encoding]::new($false))
    $changed = Run-Probe; Require $changed '(?m)^state=4\r?$'
    [IO.File]::WriteAllBytes($script, $code)
    $installedSettings = Join-Path $iso.Root 'installed-settings'
    New-Item -ItemType Directory -Force $installedSettings | Out-Null
    Copy-Item -LiteralPath (Join-Path $iso.DataPath 'Settings\Settings.xml') -Destination (Join-Path $installedSettings 'Settings.xml')
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $installedSettings
    $installed = Run-Probe '--installed'; Require $installed '(?m)^state=3\r?$'
    if(-not (Test-Path -LiteralPath (Join-Path $installedSettings 'ScriptCards\07_Проверка\01_Тест.json'))) { throw 'Installed-mode user settings were not used.' }
    if((Get-FileHash $userCard).Hash -ne $userHash) { throw 'Portable settings changed during installed-mode run.' }
    Remove-Item Env:FBE_NEXT_TEST_SETTINGS_DIRECTORY
    Remove-Item -LiteralPath $default,$userCard -Force
    $legacy = $card.Clone(); $legacy.sourceRevision = $card.sourceRevision.Clone()
    $legacy.summary = 'Embedded legacy'; $legacy.sourceRevision.algorithm = 'sha256-exclude-fbe-card-v1'; $legacy.sourceRevision.sha256 = $hash
    $body = $legacy | ConvertTo-Json -Depth 100
    foreach($marker in @('FBE-SCRIPT-CARD','FBE-NEXT-CARD')) {
        $prefix = "/*<$marker version=`"1`">`r`n$body`r`n</$marker>*/`r`n"
        [IO.File]::WriteAllBytes($script, [Text.Encoding]::UTF8.GetBytes($prefix) + $code)
        $embedded = Run-Probe; Require $embedded '(?m)^state=3\r?$'; Require $embedded '(?m)^preview=Embedded legacy\r?$'
    }
    [IO.File]::WriteAllText($userCard, '{invalid', [Text.UTF8Encoding]::new($false))
    $invalidOverride = Run-Probe; Require $invalidOverride '(?m)^state=1\r?$'; Require $invalidOverride '(?m)^preview=\r?$'
    $override = $card.Clone(); $override.sourceRevision = $card.sourceRevision.Clone()
    $override.summary = 'External overrides embedded'
    $override.sourceRevision.sha256 = (Get-FileHash -LiteralPath $script -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Card $userCard $override
    $overridden = Run-Probe; Require $overridden '(?m)^state=3\r?$'; Require $overridden '(?m)^preview=External overrides embedded\r?$'
    Remove-Item -LiteralPath $userCard -Force
    [IO.File]::WriteAllBytes($script, $code)
    $missing = Run-Probe; Require $missing '(?m)^state=0\r?$'
    $outsideA = Join-Path $iso.Root 'user-one\same.js'; $outsideB = Join-Path $iso.Root 'user-two\same.js'
    New-Item -ItemType Directory -Force (Split-Path $outsideA), (Split-Path $outsideB) | Out-Null
    [IO.File]::WriteAllBytes($outsideA, $code); [IO.File]::WriteAllBytes($outsideB, $code)
    $env:FBE_NEXT_TEST_CARD_SCRIPT = $outsideA; $probeA = Run-Probe; Require $probeA '(?m)^state=0\r?$'
    $pathA = [regex]::Match($probeA, '(?m)^external=(.+)\r?$').Groups[1].Value.Trim()
    $env:FBE_NEXT_TEST_CARD_SCRIPT = $outsideB; $probeB = Run-Probe; Require $probeB '(?m)^state=0\r?$'
    $pathB = [regex]::Match($probeB, '(?m)^external=(.+)\r?$').Groups[1].Value.Trim()
    if(!$pathA -or !$pathB -or $pathA -eq $pathB -or $pathA -notmatch '\\ScriptCards\\_external\\[0-9a-f]{64}\.json\.fbe-external$') { throw 'External script card mapping is not distinct and confined.' }
    $outsideCard = $card.Clone(); $outsideCard.sourceRevision = $card.sourceRevision.Clone()
    $outsideCard.summary = 'Outside script'; $outsideCard.sourceRevision.sha256 = $hash
    Write-Card $pathA $outsideCard
    $env:FBE_NEXT_TEST_CARD_SCRIPT = $outsideA; $outsideResult = Run-Probe
    Require $outsideResult '(?m)^state=3\r?$'; Require $outsideResult '(?m)^preview=Outside script\r?$'
    $trapTarget = Join-Path $iso.Root 'outside-target'; $trapLink = Join-Path $settingsCardsRoot 'escape'
    $trapScript = Join-Path $iso.Root 'Scripts\escape\trap.js'
    New-Item -ItemType Directory -Force $trapTarget, (Split-Path $trapScript) | Out-Null
    [IO.File]::WriteAllBytes($trapScript, $code)
    New-Item -ItemType Junction -Path $trapLink -Target $trapTarget | Out-Null
    try {
        $env:FBE_NEXT_TEST_CARD_SCRIPT = $trapScript; $trapResult = Run-Probe
        Require $trapResult '(?m)^external=\r?$'
        if(Get-ChildItem -LiteralPath $trapTarget -Force) { throw 'Card lookup escaped the settings directory through a junction.' }
    } finally { Remove-Item -LiteralPath $trapLink -Force }
    $passed = $true
    Write-Host 'External cards: two Portable profiles, DataPath, seeded/default update, preservation, invalid JSON, changed JS, installed mode, legacy markers and missing card passed.'
} finally {
    foreach($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    if($otherIso) { Complete-IsolatedFbeRuntime -Isolation $otherIso -Passed $passed }
    Complete-IsolatedFbeRuntime -Isolation $firstIso -Passed $passed
}
