<# Exercises the Settings dialog's real page lifecycle and persistence. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-settings-dialog-' + [guid]::NewGuid().ToString('N'))
try {
    Copy-Item -LiteralPath (Split-Path $FbeExe -Parent) -Destination $root -Recurse -Force
    $exe = Join-Path $root 'FBE.exe'; $fixture = Join-Path $root 'settings.fb2'
    $report = Join-Path $root 'settings.txt'; $verifyReport = Join-Path $root 'verify.txt'; $backupReport = Join-Path $root 'backup.txt'; $backupExistingReport = Join-Path $root 'backup-existing.txt'; $backupVerifyReport = Join-Path $root 'backup-verify.txt'
    [IO.File]::WriteAllText((Join-Path $root 'portable.ini'), "[Portable]`r`nDataPath=TestData`r`n", [Text.UTF8Encoding]::new($false))
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>T</book-title><lang>en</lang></title-info><document-info><id>settings-dialog-test</id><version>1</version></document-info></description><body><section><p>BACKUP_RUNTIME_BEFORE</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
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
    $second=Invoke-Scenario 'settings-dialog-runtime-verify' $verifyReport
    foreach($text in @($first,$second)) { foreach($line in 'persisted=1','applied=1') { if($text -notmatch [regex]::Escape($line)) { throw "Settings runtime report is missing ${line}: $text" } } }
    if($first -notmatch 'settings_dialog=1' -or $second -notmatch 'settings_dialog=0') { throw 'Settings dialog/verify phases were not distinguished.' }
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
    Write-Host 'FBE Settings dialog Apply/OK, backup-disabled save, and portable restart persistence passed.'
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
