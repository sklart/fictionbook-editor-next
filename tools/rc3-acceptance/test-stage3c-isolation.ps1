[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe)
$ErrorActionPreference='Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'Stage3C isolation requires a disposable GitHub-hosted runner.' }
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $root 'out/tests/rc3-stage3c-isolation'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$results=[Collections.Generic.List[object]]::new()
foreach($withHistory in @($false,$true)) {
    $label=if($withHistory){'nonempty-history'}else{'empty-history'}
    $directory=Join-Path $evidence $label
    $invocationError=''
    try {
        & (Join-Path $root 'tools/tests/test-fbe-scripting-stage3c.ps1') -FbeExe $FbeExe -Part dialogs -Mode portable -PrepareHistory:$withHistory -EvidenceDirectory $directory
    } catch { $invocationError=$_.ToString() }
    $reportPath=Join-Path $directory 'api3c.txt'
    $report=if(Test-Path -LiteralPath $reportPath){Get-Content -LiteralPath $reportPath -Raw}else{''}
    $setup=[regex]::Match($report,'setup-dirty=([01]),([01]),([01])')
    $setupDirtyStatus='INCOMPLETE'
    if($setup.Success -and $setup.Groups[1].Value -eq '0') {
        if($setup.Groups[2].Value -eq '0' -and $setup.Groups[3].Value -eq '0' -and $report -match '(?m)^script-setup-dirty=PASS\r?$') { $setupDirtyStatus='PASS' }
        elseif($setup.Groups[2].Value -eq '1' -and $setup.Groups[3].Value -eq '1' -and $report -match '(?m)^script-setup-dirty=KNOWN FAILURE\r?$') { $setupDirtyStatus='KNOWN FAILURE' }
        else { $setupDirtyStatus='FAIL' }
    }
    $pattern='(?m)^dialog=(\d+) isolation xml=([01]) live=([01]) binary=([01]) savepoints=([01]) dirty=([01]),([01]) undo=([01]) redo=([01])\r?$'
    $rows=@([regex]::Matches($report,$pattern))
    $seen=@{};$rowsValid=$true
    foreach($row in $rows) {
        $index=[int]$row.Groups[1].Value
        if($index -lt 0 -or $index -gt 7 -or $seen.ContainsKey($index)){$rowsValid=$false}
        $seen[$index]=$true
        if($row.Groups[2].Value -ne '1' -or $row.Groups[3].Value -ne '1' -or $row.Groups[4].Value -ne '1' -or $row.Groups[5].Value -ne '1' -or $row.Groups[6].Value -ne $row.Groups[7].Value -or $row.Groups[8].Value -ne '1' -or $row.Groups[9].Value -ne '1'){$rowsValid=$false}
    }
    $history=[regex]::Match($report,'history-prepared undo=(\d+) redo=(\d+)')
    $historyVerified=-not $withHistory -or ($history.Success -and [int]$history.Groups[1].Value -gt 0 -and [int]$history.Groups[2].Value -gt 0)
    $isolation=if($rows.Count -ne 8 -or -not $historyVerified){'INCOMPLETE'}elseif($rowsValid){'PASS'}else{'FAIL'}
    $scenarioStatus=if(-not $report){'INCOMPLETE'}elseif($isolation -eq 'FAIL' -or $setupDirtyStatus -eq 'FAIL' -or $invocationError){'FAIL'}elseif($setupDirtyStatus -in @('PASS','KNOWN FAILURE') -and $isolation -eq 'PASS' -and $report -match '(?m)^result=PASS\r?$'){'PASS'}else{'INCOMPLETE'}
    $results.Add([pscustomobject]@{case=$label;setupDirtyStatus=$setupDirtyStatus;dialogIsolation=$isolation;historyVerified=$historyVerified;dialogRows=$rows.Count;scenarioStatus=$scenarioStatus;invocationError=$invocationError})
}
$summary=@{scope='dialog isolation; standalone no-op result is recorded separately';results=@($results)}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidence 'summary.json') -Encoding utf8
$results | Format-Table -AutoSize | Out-String | Write-Host
if(@($results | Where-Object { $_.scenarioStatus -ne 'PASS' }).Count){throw 'Stage3C dialog isolation did not fully pass; see summary.json.'}
Write-Host 'Dialog isolation passed; script setup dirty is reported separately.'
