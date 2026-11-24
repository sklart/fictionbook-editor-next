$ErrorActionPreference='Stop'
$base=Join-Path (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path ('out/rc3-stage3c-mock-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force (Join-Path $base 'tools/rc3-acceptance'),(Join-Path $base 'tools/tests') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'test-stage3c-isolation.ps1') -Destination (Join-Path $base 'tools/rc3-acceptance/test-stage3c-isolation.ps1')
$stub=@'
param($FbeExe,$Part,$Mode,[switch]$PrepareHistory,$EvidenceDirectory)
New-Item -ItemType Directory -Force $EvidenceDirectory | Out-Null
$rows=@('setup-dirty=0,1,1 revision=100,100')
if($PrepareHistory){$rows+='history-prepared undo=1 redo=1'}
for($i=0;$i -lt 8;$i++){
 if($env:RC3_MOCK_BAD_DIALOG -eq 'missing' -and $i -eq 7){continue}
 $xml=if($env:RC3_MOCK_BAD_DIALOG -eq '1' -and $i -eq 4){0}else{1}
 $live=if($env:RC3_MOCK_BAD_DIALOG -eq 'live' -and $i -eq 4){0}else{1}
 $binary=if($env:RC3_MOCK_BAD_DIALOG -eq 'binary' -and $i -eq 4){0}else{1}
 $savepoints=if($env:RC3_MOCK_BAD_DIALOG -eq 'savepoints' -and $i -eq 4){0}else{1}
 $rows+="dialog=$i isolation xml=$xml live=$live binary=$binary savepoints=$savepoints dirty=1,1 undo=1 redo=1"
}
$rows+=@('result=FAIL','details=dialog changed document: dirty=0->1 xml_equal=1')
$rows | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'api3c.txt') -Encoding utf8
throw 'Expected no-op failure'
'@
[IO.File]::WriteAllText((Join-Path $base 'tools/tests/test-fbe-scripting-stage3c.ps1'),$stub,[Text.UTF8Encoding]::new($false))
$oldAction=$env:GITHUB_ACTIONS;$oldRunner=$env:RUNNER_ENVIRONMENT;$oldBad=$env:RC3_MOCK_BAD_DIALOG
try {
    $env:GITHUB_ACTIONS='true';$env:RUNNER_ENVIRONMENT='github-hosted'
    foreach($bad in @('0','1','live','binary','savepoints','missing')) {
        $env:RC3_MOCK_BAD_DIALOG=$bad;$rejected=$false
        try { & (Join-Path $base 'tools/rc3-acceptance/test-stage3c-isolation.ps1') -FbeExe 'fixture' } catch { $rejected=$_.ToString() -match 'did not fully pass' }
        $summary=Get-Content -Raw (Join-Path $base 'out/tests/rc3-stage3c-isolation/summary.json') | ConvertFrom-Json
        if(-not $rejected -or @($summary.results).Count -ne 2 -or @($summary.results | Where-Object noOpStatus -ne 'FAIL').Count) { throw 'No-op failure was hidden.' }
        $expected=if($bad -eq 'missing'){'INCOMPLETE'}elseif($bad -eq '0'){'PASS'}else{'FAIL'}
        if(@($summary.results | Where-Object dialogIsolation -ne $expected).Count -or -not $summary.results[1].historyVerified) { throw 'Dialog isolation or history was classified incorrectly.' }
    }
    Write-Host 'Stage3C isolation parser preserves no-op and dialog failures: PASS'
} finally { $env:GITHUB_ACTIONS=$oldAction;$env:RUNNER_ENVIRONMENT=$oldRunner;$env:RC3_MOCK_BAD_DIALOG=$oldBad }
