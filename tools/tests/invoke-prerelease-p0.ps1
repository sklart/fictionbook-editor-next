[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [switch]$Full, [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'out/tests/prerelease-p0' }
$run = Join-Path $OutputDirectory ([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run -Force | Out-Null
$summaryPath = Join-Path $run 'summary.json'
$summary = [ordered]@{ schemaVersion=1; sha=$null; executable=$FbeExe; executableSha256=$null; contour=if($Full){'FULL'}else{'FAST'}; status='RUNNING'; results=@() }
function Write-Summary { $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8 }
function Invoke-P0([string]$id, [string]$script, [string[]]$extra = @()) {
    $log = Join-Path $run ($id + '.log')
    $summary.results += [ordered]@{ id=$id; status='RUNNING'; log=$log }
    Write-Summary
    & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $PSScriptRoot $script) -FbeExe $exe @extra *> $log
    $code = $LASTEXITCODE
    if ($code -ne 0 -or -not (Test-Path -LiteralPath $log -PathType Leaf) -or (Get-Item -LiteralPath $log).Length -eq 0) {
        $summary.results[-1].status='FAIL'; Write-Summary
        throw "P0 $id failed or did not report (exit $code); see $log"
    }
    $summary.results[-1].status='PASS'; Write-Summary
    Write-Host "P0 $id PASS: $log"
}
Write-Summary
try {
    $sha = (& git -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $sha -notmatch '^[0-9a-f]{40}$') { throw 'Cannot identify P0 test source SHA.' }
    $summary.sha = $sha
    if ($env:GITHUB_SHA -and $sha -ne $env:GITHUB_SHA) { throw 'P0 source SHA does not match the CI commit.' }
    $exe = (Resolve-Path -LiteralPath $FbeExe).Path
    $summary.executable = $exe
    & git -C $root -c diff.ignoreSubmodules=all diff --quiet HEAD --
    if ($LASTEXITCODE -ne 0) { throw 'P0 source worktree has tracked changes; commit or use a clean checkout.' }
    $provenance = Join-Path (Split-Path (Split-Path $exe)) 'build-provenance/CommonCore-Release.json'
    if (-not (Test-Path -LiteralPath $provenance -PathType Leaf)) { throw 'P0 executable build provenance is missing.' }
    $build = Get-Content -LiteralPath $provenance -Raw | ConvertFrom-Json
    $exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    $summary.executableSha256 = $exeHash.ToLowerInvariant()
    if ($build.gitCommit -ne $sha -or $build.artifacts.'FBE.exe'.sha256 -ne $exeHash) { throw 'P0 executable does not match the source SHA and build provenance.' }
    if ($Full) { Invoke-P0 'issue72-full' 'test-fbe-issue72-runtime.ps1' }
    Invoke-P0 'issue72-history' 'test-fbe-issue72-runtime.ps1' @('-CaseName','subtitle-strong-p','-History')
    $saveCases = @(@('normal-save','valid'), @('normal-save','nested'), @('normal-save-as','nested'), @('save','valid'))
    if ($Full) { $saveCases += @(@('normal-save-as','valid'), @('save','nested'), @('save-as','valid'), @('save-as','nested')) }
    foreach ($entry in $saveCases) { Invoke-P0 "issue72-$($entry[0])-$($entry[1])" 'test-fbe-issue72-save-runtime.ps1' @('-Case',$entry[1],'-Operation',$entry[0]) }
    if ($Full) { foreach ($prior in @('source','validate')) { Invoke-P0 "issue72-after-$prior" 'test-fbe-issue72-save-runtime.ps1' @('-Case','nested','-Operation','normal-save','-After',$prior) } }
    foreach ($case in @('body','description')) { Invoke-P0 "save-timer-save-$case" 'test-fbe-save-timer-runtime.ps1' @('-Case',$case,'-Operation','save') }
    if ($Full) { foreach ($case in @('body','description')) { Invoke-P0 "save-timer-save-as-$case" 'test-fbe-save-timer-runtime.ps1' @('-Case',$case,'-Operation','save-as') } }
    if ($Full) { foreach ($case in @('body','description')) { Invoke-P0 "save-hook-$case" 'test-fbe-save-timer-runtime.ps1' @('-Case',$case,'-Hook') } }
    Invoke-P0 'recovery-history' 'test-fbe-recovery-history-runtime.ps1' @('-Case','links')
    if ($Full) {
        foreach ($case in @('binary','write-error','source-invalid')) { Invoke-P0 "recovery-$case" 'test-fbe-recovery-history-runtime.ps1' @('-Case',$case) }
        Invoke-P0 'binary-dirty' 'test-fbe-binary-dirty-runtime.ps1'
    } else {
        foreach ($case in @('save-error','reentrant-save')) { Invoke-P0 "binary-$case" 'test-fbe-binary-dirty-runtime.ps1' @('-Case',$case) }
    }
    Invoke-P0 'reentrant-close' 'test-fbe-reentrant-close-runtime.ps1'
    Invoke-P0 'description-script-save' 'test-fbe-script-form-dirty-runtime.ps1' @('-SaveFirst')
    $summary.status='PASS'; Write-Summary
} catch {
    $summary.status='FAIL'; $summary.error=$_.ToString(); Write-Summary
    throw
}
Write-Host "P0 summary: $summaryPath"
