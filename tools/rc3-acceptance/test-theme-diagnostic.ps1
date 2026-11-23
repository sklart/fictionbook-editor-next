[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe)
$ErrorActionPreference='Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'Theme diagnostics require a disposable GitHub-hosted runner.' }
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $root 'out/tests/rc3-theme-diagnostic'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$old=$env:FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR
$results=[Collections.Generic.List[object]]::new()
try {
    $env:FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR=$null
    foreach($mode in @('ordinary','capture-uia')) {
        $status='PASS';$details=''
        try {
            if($mode -eq 'ordinary') { & (Join-Path $root 'tools/tests/test-fbe-theme-application-runtime.ps1') -FbeExe $FbeExe }
            else { & (Join-Path $root 'tools/rc3-acceptance/test-theme-capture.ps1') -FbeExe $FbeExe }
        } catch { $status='FAIL';$details=$_.ToString();Write-Warning "Theme ${mode}: $details" }
        $results.Add([pscustomobject]@{mode=$mode;status=$status;details=$details})
    }
} finally { $env:FBE_NEXT_TEST_APPEARANCE_ARTIFACT_DIR=$old }
@{scope='partial diagnostic, not release gate';results=@($results)} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'summary.json') -Encoding utf8
if(@($results | Where-Object status -ne 'PASS').Count){throw 'One or more theme diagnostics failed; see summary.json and runtime evidence.'}
Write-Host 'Ordinary and capture-mode theme diagnostics PASS.'
