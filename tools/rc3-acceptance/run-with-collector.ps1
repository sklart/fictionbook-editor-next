[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Address','Release')][string]$Variant,
    [Parameter(Mandatory)][string]$RepositoryRoot,
    [Parameter(Mandatory)][string]$EvidenceRoot
)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'This entry point requires a GitHub-hosted Windows runner.'
}
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
$evidence = (Resolve-Path -LiteralPath $EvidenceRoot).Path
$archive = Join-Path $evidence 'procdump.zip'
Invoke-WebRequest -Uri 'https://download.sysinternals.com/files/Procdump.zip' -OutFile $archive
Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $evidence 'procdump')
$collector = Join-Path $evidence 'procdump/procdump64.exe'
$signature = Get-AuthenticodeSignature -LiteralPath $collector
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'CN=Microsoft Corporation') { throw 'ProcDump signature is not a valid Microsoft signature.' }
@{sha256=(Get-FileHash -LiteralPath $collector -Algorithm SHA256).Hash; signer=$signature.SignerCertificate.Subject; source='https://download.sysinternals.com/files/Procdump.zip'} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'collector.json') -Encoding utf8

# Exercise the same wait-by-name attachment used for the real case. The probe
# waits for a debugger before triggering native x86 fast-fail.
$probe = Join-Path $evidence 'probe'
New-Item -ItemType Directory -Path $probe | Out-Null
$probeSource = Join-Path $probe 'crash.cpp'
$probeExe = Join-Path $probe 'FBE.exe'
@'
#include <windows.h>
#include <intrin.h>
int main() {
    for (int i = 0; i < 200; ++i) {
        if (IsDebuggerPresent()) { __fastfail(7); }
        Sleep(100);
    }
    return 42;
}
'@ | Set-Content -LiteralPath $probeSource -Encoding ascii
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x86' -or $env:VCToolsVersion -notlike '14.44*') { throw 'Probe requires the VC 14.44 x86 environment.' }
& cl.exe /nologo /Zi /Od ("/Fo$(Join-Path $probe 'crash.obj')") ("/Fd$(Join-Path $probe 'compile.pdb')") ("/Fe$probeExe") $probeSource /link /DEBUG ("/PDB:$(Join-Path $probe 'crash.pdb')") *> (Join-Path $probe 'compile.log')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $probeExe -PathType Leaf)) { throw 'Native fast-fail probe compilation failed.' }
$probeOut = Join-Path $probe 'collector-stdout.log'
$probeErr = Join-Path $probe 'collector-stderr.log'
if (Get-Process -Name FBE -ErrorAction SilentlyContinue) { throw 'Unexpected FBE.exe exists before collector probe.' }
$probeProcess = Start-Process -FilePath $collector -ArgumentList @('-accepteula','-ma','-e','-w','FBE.exe',('"'+$probe+'"')) -RedirectStandardOutput $probeOut -RedirectStandardError $probeErr -PassThru
$probeChild = $null
try {
    Start-Sleep -Seconds 2
    if ($probeProcess.HasExited) { throw 'Collector probe exited before the target started.' }
    $probeChild = Start-Process -FilePath $probeExe -WorkingDirectory $probe -PassThru
    if (-not $probeChild.WaitForExit(30000)) { throw 'Native fast-fail probe timed out.' }
    $probeChild.Refresh()
    if ($probeChild.ExitCode -ne -1073740791) { throw "Unexpected native probe exit: $($probeChild.ExitCode)." }
    if (-not $probeProcess.WaitForExit(30000)) { throw 'Collector probe timed out.' }
} finally {
    if ($probeChild -and -not $probeChild.HasExited) { Stop-Process -Id $probeChild.Id -Force -ErrorAction SilentlyContinue }
    if (-not $probeProcess.HasExited) { Stop-Process -Id $probeProcess.Id -Force -ErrorAction SilentlyContinue }
}
$probeLog = [string](Get-Content -Raw -LiteralPath $probeOut)
$probePid = [regex]::Escape([string]$probeChild.Id)
if ($probeLog -notmatch ("(?im)^\s*Process:\s*FBE\.exe\s*\(${probePid}\)")) { throw 'Probe collector did not confirm the expected process.' }
$probeDumps = @(Get-ChildItem -LiteralPath $probe -Filter '*.dmp' -File)
if ($probeProcess.ExitCode -ne 0 -or $probeDumps.Count -ne 1 -or $probeDumps[0].Length -lt 32) { throw 'Collector probe did not produce a fast-fail dump.' }
. (Join-Path $PSScriptRoot 'ProbeMinidump.ps1')
$dumpStream = [IO.File]::OpenRead($probeDumps[0].FullName)
try { $probeDumpSummary = Assert-Rc3FastFailMinidump -Stream $dumpStream }
finally { $dumpStream.Dispose() }
$probeDumpSummary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $probe 'dump-structure.json') -Encoding utf8

$caseDir = Join-Path $evidence 'case'
$dumpDir = Join-Path $evidence 'dumps'
New-Item -ItemType Directory -Path $dumpDir | Out-Null
$stdout = Join-Path $evidence 'collector-stdout.log'
$stderr = Join-Path $evidence 'collector-stderr.log'
. (Join-Path $PSScriptRoot 'CollectorObservation.ps1')
if (Get-Process -Name FBE -ErrorAction SilentlyContinue) { throw 'Unexpected FBE.exe exists before the diagnostic case.' }
$watcher = Start-Process -FilePath $collector -ArgumentList @('-accepteula','-ma','-e','-w','FBE.exe',('"'+$dumpDir+'"')) -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
$caseError = $null
$forcedStop = $false
try {
    Start-Sleep -Seconds 2
    if ($watcher.HasExited) { throw 'Collector exited before the case started.' }
    $scenario = if ($Variant -eq 'Address') { 'asan-file-close-one.ps1' } else { 'release-file-close-one.ps1' }
    & (Join-Path $PSScriptRoot $scenario) -RepositoryRoot $root -EvidenceDirectory $caseDir -EnvironmentAcknowledgement DISPOSABLE-WINDOWS
} catch {
    $caseError = $_
} finally {
    if (-not $watcher.WaitForExit(30000)) { $forcedStop = $true; Stop-Process -Id $watcher.Id -Force; $watcher.WaitForExit() }
}
$watcher.Refresh()
$log = if (Test-Path -LiteralPath $stdout) { [string](Get-Content -Raw -LiteralPath $stdout) } else { '' }
$dumps = @(Get-ChildItem -LiteralPath $dumpDir -Filter '*.dmp' -File)
$observation = Get-Rc3CollectorObservation -ForcedStop $forcedStop -ExitCode $watcher.ExitCode -Log $log -DumpCount $dumps.Count -ExpectedTempRoot (Join-Path $caseDir 'temp')
@{status=$observation.status;reasons=$observation.reasons;forcedStop=$forcedStop;exitCode=$watcher.ExitCode;dumps=@($dumps.FullName);caseError=$(if($caseError){$caseError.ToString()}else{$null})} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidence 'collector-result.json') -Encoding utf8
# Failure to attach can miss an early crash. Neither a missing dump nor a
# passing case overrides a failed or forcibly stopped collector.
if ($caseError) { throw $caseError }
if ($observation.status -ne 'PASS') { throw "Collector observation failed: $($observation.reasons -join '; ')" }
