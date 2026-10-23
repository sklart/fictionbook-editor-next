[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$validator=Join-Path $PSScriptRoot 'Assert-CiOleRuntimeEvidence.ps1'
$receipt=@{schemaVersion=1;result='PASS';suite='clipboard-ole';head=('a'*40);commandSha256=('b'*64);repository='sklart/fictionbook-editor-next';runId='123';executableSha256=('c'*64)}
$run=@{id=123;head_sha=('a'*40);status='completed';conclusion='success';path='.github/workflows/build.yml'}
& $validator -Receipt $receipt -Run $run -Head ('a'*40) -CommandSha256 ('b'*64)
foreach($case in @(
    @{Target='Receipt';Key='head';Value=('d'*40)},
    @{Target='Receipt';Key='commandSha256';Value=('d'*64)},
    @{Target='Receipt';Key='result';Value='NOT RUN'},
    @{Target='Receipt';Key='executableSha256';Value='missing'},
    @{Target='Receipt';Key='repository';Value='another/repo'},
    @{Target='Run';Key='id';Value=124},
    @{Target='Run';Key='head_sha';Value=('d'*40)},
    @{Target='Run';Key='status';Value='in_progress'},
    @{Target='Run';Key='conclusion';Value='failure'},
    @{Target='Run';Key='conclusion';Value='skipped'},
    @{Target='Run';Key='path';Value='.github/workflows/other.yml'}
)) {
    $candidateReceipt=$receipt.Clone();$candidateRun=$run.Clone()
    if($case.Target -eq 'Receipt'){$candidateReceipt[$case.Key]=$case.Value}else{$candidateRun[$case.Key]=$case.Value}
    $rejected=$false
    try { & $validator -Receipt $candidateReceipt -Run $candidateRun -Head ('a'*40) -CommandSha256 ('b'*64) } catch {$rejected=$true}
    if(!$rejected){throw "Invalid external evidence accepted: $($case.Target)/$($case.Key)"}
}
Write-Host 'CI OLE evidence validation: PASS (valid fixture and eleven rejection cases; no runtime PASS implied).'
