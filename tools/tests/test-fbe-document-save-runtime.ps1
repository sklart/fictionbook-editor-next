[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$FbeExe, [ValidateRange(30, 300)][int]$TimeoutSeconds = 180)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe not found: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-save-runtime-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
try {
    $source = Join-Path $root 'original.fb2'; $report = Join-Path $root 'report.txt'; $destination = Join-Path $root 'missing-parent\copy.fb2'
    [IO.File]::WriteAllText($source, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Test</last-name></author><book-title>Save</book-title><lang>en</lang></title-info><document-info><author><nickname>FBE</nickname></author><program-used>FBE</program-used><date value="2026-01-01">1 January 2026</date><id>11111111-1111-1111-1111-111111111111</id><version>1.0</version></document-info></description><body><section><p>save</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $oldMode,$oldScenario,$oldPath=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_SAVE_PATH
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='save-as-failure-runtime'; $env:FBE_NEXT_TEST_SAVE_PATH=$destination
        $process=Start-Process -FilePath $FbeExe -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$source) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Save As failure runtime timed out.' }
        $state=Get-Content -LiteralPath $report -Raw
        foreach($line in @('failed=1','filename=1','namevalid=1','type=1','encoding=1','session=1')) { if($process.ExitCode -ne 0 -or $state -notmatch [regex]::Escape($line)) { throw "Save As rollback regression: $state" } }
		$env:FBE_NEXT_TEST_SCENARIO='save-as-cancel-runtime'; $process=Start-Process -FilePath $FbeExe -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$source) -WorkingDirectory (Split-Path $FbeExe) -PassThru
		if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Save As cancel runtime timed out.' }
		$state=Get-Content -LiteralPath $report -Raw
		foreach($line in @('cancelled=1','filename=1','namevalid=1','type=1','encoding=1','session=1')) { if($process.ExitCode -ne 0 -or $state -notmatch [regex]::Escape($line)) { throw "Save As cancel regression: $state" } }
    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_SAVE_PATH=$oldMode,$oldScenario,$oldPath }
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host 'Document Save As failure runtime regression passed.'
