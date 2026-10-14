<# Exercises the Settings dialog's real page lifecycle and persistence. #>
[CmdletBinding()]
param([string]$FbeExe, [int]$TimeoutSeconds = 90, [switch]$LayoutOnly)

$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-settings-dialog-' + [guid]::NewGuid().ToString('N'))
try {
    Copy-Item -LiteralPath (Split-Path $FbeExe -Parent) -Destination $root -Recurse -Force
    $exe = Join-Path $root 'FBE.exe'; $fixture = Join-Path $root 'settings.fb2'
    $report = Join-Path $root 'settings.txt'; $reopenReport = Join-Path $root 'settings-reopen.txt'; $legacyReport = Join-Path $root 'settings-legacy.txt'; $manualReport = Join-Path $root 'settings-manual.txt'; $verifyReport = Join-Path $root 'verify.txt'; $backupReport = Join-Path $root 'backup.txt'; $backupExistingReport = Join-Path $root 'backup-existing.txt'; $backupVerifyReport = Join-Path $root 'backup-verify.txt'
    [IO.File]::WriteAllText((Join-Path $root 'portable.ini'), "[Portable]`r`nDataPath=TestData`r`n", [Text.UTF8Encoding]::new($false))
    $settingsDirectory = Join-Path $root 'TestData\Settings'
    $settingsFile = Join-Path $settingsDirectory 'Settings.xml'
    New-Item -ItemType Directory -Path $settingsDirectory -Force | Out-Null
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>T</book-title><lang>en</lang></title-info><document-info><id>settings-dialog-test</id><version>1</version></document-info></description><body><section><p>BACKUP_RUNTIME_BEFORE</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $originalFb2Hash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    function Invoke-Scenario([string]$Scenario, [string]$Path) {
        $oldMode,$oldScenario=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO
        try {
            $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO=$Scenario
            $p=Start-Process -FilePath $exe -WorkingDirectory $root -ArgumentList @('--portable','-b',$Path,$fixture) -PassThru
            if(-not $p.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $p.Id -Force; throw "$Scenario timed out." }
            if($p.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $Path)) { $diagnostics=if(Test-Path -LiteralPath $Path){Get-Content -LiteralPath $Path -Raw}else{'<report missing>'}; throw "$Scenario failed: exit $($p.ExitCode). $diagnostics" }
            return Get-Content -LiteralPath $Path -Raw
        } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    }
    $first=Invoke-Scenario 'settings-dialog-runtime' $report
    if($LayoutOnly) {
        foreach($line in 'settings_dialog=1','layout=1','persisted=1','yo_usage=1','applied=1','caret_persisted=1','caret_applied=1','caret_initial_system=1','source_unchanged=1') {
            if($first -notmatch [regex]::Escape($line)) { throw "Settings layout smoke is missing ${line}: $first" }
        }
		$verification = Invoke-Scenario 'settings-dialog-runtime-verify' $verifyReport
		foreach($line in 'settings_dialog=0','yo_usage=1','caret_persisted=1','caret_applied=1','source_unchanged=1') {
			if($verification -notmatch [regex]::Escape($line)) { throw "Settings caret restart smoke is missing ${line}: $verification" }
		}
        if((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $originalFb2Hash) {
            throw 'Settings preview modified the opened FB2 file.'
        }
        Write-Host 'FBE Settings dialog short layout and Keyboard resize smoke passed.'
        return
    }
    $reopened=Invoke-Scenario 'settings-dialog-runtime' $reopenReport
    if (-not (Test-Path -LiteralPath $settingsFile)) { throw 'Portable TestData\Settings\Settings.xml was not saved.' }
    if (Test-Path -LiteralPath (Join-Path $root 'Settings.xml')) { throw 'Settings.xml was incorrectly saved next to FBE.exe.' }
    function Set-TestGeometry([bool]$Manual, [bool]$Legacy) {
        [xml]$document = Get-Content -LiteralPath $settingsFile -Raw
        $settings = $document.SelectSingleNode('/FBE/Settings[@ID="0"]')
        $geometry = $settings.SelectSingleNode('SettingsDialogGeometry')
        if (-not $geometry) { throw 'Saved Settings dialog geometry is missing.' }
        $parts = $geometry.InnerText.Split(';')
        if ($parts.Count -ne 5) { throw 'Saved Settings dialog geometry is invalid.' }
        $parts[2] = ([int]$parts[2] - 24).ToString()
        $parts[3] = ([int]$parts[3] - 24).ToString()
        $geometry.InnerText = $parts -join ';'
        $manualNode = $settings.SelectSingleNode('SettingsDialogManualSize')
        if ($Legacy) {
            if ($manualNode) { [void]$settings.RemoveChild($manualNode) }
        } else {
            if (-not $manualNode) { $manualNode = $document.CreateElement('SettingsDialogManualSize'); [void]$settings.AppendChild($manualNode) }
            $manualNode.InnerText = if ($Manual) { 'true' } else { 'false' }
        }
        $document.Save($settingsFile)
        return $geometry.InnerText
    }
    $legacySmall = Set-TestGeometry -Manual $false -Legacy $true
    $legacy = Invoke-Scenario 'settings-dialog-runtime' $legacyReport
    [xml]$afterLegacy = Get-Content -LiteralPath $settingsFile -Raw
    if ($afterLegacy.SelectSingleNode('/FBE/Settings[@ID="0"]/SettingsDialogGeometry').InnerText -eq $legacySmall -or
        $afterLegacy.SelectSingleNode('/FBE/Settings[@ID="0"]/SettingsDialogManualSize').InnerText -ne 'false') {
        throw 'Legacy saved geometry did not return to automatic sizing.'
    }
    $manualSmall = Set-TestGeometry -Manual $true -Legacy $false
    $manual = Invoke-Scenario 'settings-dialog-runtime' $manualReport
    [xml]$afterManual = Get-Content -LiteralPath $settingsFile -Raw
    if ($afterManual.SelectSingleNode('/FBE/Settings[@ID="0"]/SettingsDialogGeometry').InnerText -ne $manualSmall -or
        $afterManual.SelectSingleNode('/FBE/Settings[@ID="0"]/SettingsDialogManualSize').InnerText -ne 'true') {
        throw 'A manually chosen small Settings dialog size was not preserved.'
    }
    $second=Invoke-Scenario 'settings-dialog-runtime-verify' $verifyReport
    foreach($text in @($first,$reopened,$legacy,$manual,$second)) { foreach($line in 'persisted=1','applied=1') { if($text -notmatch [regex]::Escape($line)) { throw "Settings runtime report is missing ${line}: $text" } } }
    if($first -notmatch 'settings_dialog=1' -or $first -notmatch 'layout=1' -or
        $reopened -notmatch 'settings_dialog=1' -or $reopened -notmatch 'layout=1' -or
        $legacy -notmatch 'layout=1' -or $manual -notmatch 'layout=1' -or
        $second -notmatch 'settings_dialog=0') { throw 'Settings dialog layout/verify phases were not distinguished.' }
    $backupFirst=Invoke-Scenario 'backup-settings-save-runtime' $backupReport
    foreach($line in 'settings_dialog=1','backup_disabled=1','source_edited=1','source_applied=1','saved=1') { if($backupFirst -notmatch [regex]::Escape($line)) { throw "Backup runtime report is missing ${line}: $backupFirst" } }
    if((Get-Content -LiteralPath $fixture -Raw) -notmatch 'BACKUP_RUNTIME_AFTER') { throw 'Backup-disabled runtime save did not update the FB2.' }
    if(Test-Path -LiteralPath ($fixture + '.bak')) { throw 'Backup-disabled runtime save created a new .bak.' }
    [IO.File]::WriteAllText(($fixture + '.bak'), 'PREEXISTING_BACKUP', [Text.UTF8Encoding]::new($false))
    $backupExisting=Invoke-Scenario 'backup-settings-save-runtime' $backupExistingReport
    foreach($line in 'settings_dialog=1','backup_disabled=1','source_edited=1','source_applied=1','saved=1') { if($backupExisting -notmatch [regex]::Escape($line)) { throw "Existing-backup runtime report is missing ${line}: $backupExisting" } }
    if((Get-Content -LiteralPath $fixture -Raw) -notmatch 'BACKUP_RUNTIME_FINAL') { throw 'Backup-disabled runtime save did not update the FB2 with an existing .bak.' }
    if(-not (Test-Path -LiteralPath ($fixture + '.bak'))) { throw 'Backup-disabled runtime save removed the existing .bak.' }
    if([IO.File]::ReadAllText(($fixture + '.bak'), [Text.UTF8Encoding]::new($false)) -ne 'PREEXISTING_BACKUP') { throw 'Backup-disabled runtime save replaced the existing .bak.' }
    $backupSecond=Invoke-Scenario 'backup-settings-save-runtime-verify' $backupVerifyReport
    foreach($line in 'settings_dialog=0','backup_disabled=1') { if($backupSecond -notmatch [regex]::Escape($line)) { throw "Backup persistence report is missing ${line}: $backupSecond" } }
    if (Test-Path -LiteralPath (Join-Path $root 'Settings.xml')) { throw 'Settings.xml was incorrectly saved next to FBE.exe.' }
    Write-Host 'FBE Settings dialog auto/legacy/manual geometry, move/resize, Apply/OK, and backup-disabled save passed.'
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
