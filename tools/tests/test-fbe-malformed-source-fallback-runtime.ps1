[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$FbeExe, [ValidateRange(30, 300)][int]$TimeoutSeconds = 180)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe not found: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-malformed-source-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
try {
    $valid = Join-Path $root 'original.fb2'; $malformed = Join-Path $root 'malformed.fb2'; $report = Join-Path $root 'report.txt'
    [IO.File]::WriteAllText($valid, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Test</last-name></author><book-title>Original</book-title><lang>en</lang></title-info><document-info><author><nickname>FBE Next</nickname></author><program-used>FBE Next</program-used><date value="2026-09-11">11 September 2026</date><id>11111111-1111-1111-1111-111111111111</id><version>1.0</version></document-info></description><body><section><p>original</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($malformed, '<?xml version="1.0"?><FictionBook><body><section><p>broken</p></section></body>', [Text.UTF8Encoding]::new($false))
    $oldMode,$oldScenario,$oldPath=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_MALFORMED_SOURCE_PATH
    try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='malformed-source-fallback-runtime'; $env:FBE_NEXT_TEST_MALFORMED_SOURCE_PATH=$malformed
        $process=Start-Process -FilePath $FbeExe -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$valid) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'malformed source fallback runtime timed out' }
        $state=Get-Content -LiteralPath $report -Raw
        if($process.ExitCode -ne 0 -or $state -notmatch 'fallback=1' -or $state -notmatch 'identity=1' -or $state -notmatch 'active=1' -or $state -notmatch 'session=1' -or $state -notmatch 'source=1') { throw "malformed source fallback regression: $state" }
    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_MALFORMED_SOURCE_PATH=$oldMode,$oldScenario,$oldPath }
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host 'Malformed source fallback runtime regression passed.'
