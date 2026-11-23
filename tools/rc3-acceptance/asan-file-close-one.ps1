[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RepositoryRoot,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [Parameter(Mandatory)][ValidateSet('DISPOSABLE-WINDOWS')][string]$EnvironmentAcknowledgement
)

# Run only inside an already-provisioned disposable Windows GUI session.
# This script does not configure an operating-system crash collector.
$ErrorActionPreference = 'Stop'
$expectedHead = 'c4bcb76c3cd03d1dff88fa557aa75261b4ec0461'
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$output = Join-Path $root 'out/asan/Release'
$manifestPath = Join-Path $output 'fbe-sanitizer.json'
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne $expectedHead -or $manifest.gitCommit -ne $expectedHead) {
    throw 'Candidate SHA or ASan build provenance differs from the required SHA.'
}
& git -C $root -c diff.ignoreSubmodules=all diff --quiet HEAD --
if ($LASTEXITCODE -ne 0) { throw 'Tracked source files differ from HEAD.' }
if ($manifest.schemaVersion -ne 1 -or $manifest.sanitizer -ne 'Address' -or $manifest.publishable -ne $false -or
    $manifest.configuration -ne 'Release' -or $manifest.platform -ne 'Win32' -or $manifest.platformToolset -ne 'v143' -or
    $manifest.compiler -ne 'MSVC' -or $manifest.vcToolsVersion -notmatch '^14\.44\.' -or
    $manifest.compilerVersion -notmatch '^14\.44\.' -or $manifest.stlContainerAnnotations -ne $false -or
    @($manifest.translationUnits).Count -lt 200) {
    throw 'Expected a complete diagnostic Release Win32 v143/14.44 ASan build.'
}
$expectedModules = @('FBE.exe','FBV.exe','FBShell.dll','Plugins\ExportHTML.dll','Plugins\ExportDOCX.dll','Plugins\ExportEPUB.dll','Plugins\ImportEPUB.dll','Plugins\ImportEPUBLunaSVG.dll','ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe')
$names = @($manifest.modules | ForEach-Object { [string]$_.path })
if ($names.Count -ne $expectedModules.Count -or @($expectedModules | Where-Object { $_ -notin $names }).Count -or @($names | Select-Object -Unique).Count -ne $names.Count) {
    throw 'ASan manifest module list is incomplete or unexpected.'
}
foreach ($module in $manifest.modules) {
    if ($module.instrumented -ne $true -or $module.sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw "Invalid ASan module evidence: $($module.path)" }
    $path = Join-Path $output $module.path
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $module.sha256) {
        throw "ASan module hash mismatch: $($module.path)"
    }
}
if (Test-Path -LiteralPath $EvidenceDirectory) { throw 'EvidenceDirectory must not already exist.' }
New-Item -ItemType Directory -Path $EvidenceDirectory | Out-Null
$evidence = (Resolve-Path -LiteralPath $EvidenceDirectory).Path
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $evidence 'fbe-sanitizer.json')
$head | Set-Content -LiteralPath (Join-Path $evidence 'source-sha.txt') -Encoding utf8
Get-FileHash -LiteralPath (Join-Path $output 'FBE.exe'), (Join-Path $output 'FBE.pdb') -Algorithm SHA256 |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'exe-pdb-hashes.json') -Encoding utf8
& git -C $root submodule status --recursive *> (Join-Path $evidence 'submodules.log')
if ($LASTEXITCODE -ne 0) { throw 'Cannot verify submodule status; inspect submodules.log.' }
$submodules = Get-Content -LiteralPath (Join-Path $evidence 'submodules.log')
if (@($submodules | Where-Object { $_ -match '^[-+U]' }).Count) { throw 'Submodule checkout differs from gitlinks or is uninitialized.' }
& git -C $root submodule foreach --quiet --recursive 'git diff --quiet HEAD --'
if ($LASTEXITCODE -ne 0) { throw 'A submodule has tracked changes or could not be inspected.' }
$originalEnvironment = [Environment]::GetEnvironmentVariables('Process')
try {
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
. (Join-Path $root 'tools/tests/SanitizerHarness.ps1')
$caseFile = Join-Path $evidence 'case.json'
@{
    command = "./tools/tests/test-fbe-file-close-runtime.ps1 -FbeExe (Join-Path `$outputDir 'FBE.exe')"
    outputDirectory = $output
    sanitizer = 'Address'
    clangClPath = $null
    helperOptions = @()
} | ConvertTo-Json | Set-Content -LiteralPath $caseFile -Encoding utf8
$savedPath = $env:PATH
$savedCl = $env:_CL_
$savedSymbolizer = $env:ASAN_SYMBOLIZER_PATH
$savedTemp = $env:TEMP
$savedTmp = $env:TMP
try {
    $isolatedTemp = Join-Path $evidence 'temp'
    New-Item -ItemType Directory -Path $isolatedTemp | Out-Null
    $env:TEMP = $isolatedTemp
    $env:TMP = $isolatedTemp
    $env:PATH = $output + ';' + $savedPath
    $env:ASAN_SYMBOLIZER_PATH = Join-Path $output 'llvm-symbolizer.exe'
    $env:_CL_ = ($savedCl + ' /fsanitize=address /Zi /D_DISABLE_VECTOR_ANNOTATION /D_DISABLE_STRING_ANNOTATION /Fd"' + (Join-Path $evidence 'harness.pdb') + '"').Trim()
    $result = Invoke-SanitizerCase -Sanitizer Address -Name 'release.fbe-file-close-runtime.1' `
        -Worker (Join-Path $root 'tools/tests/SanitizerWorker.ps1') -WorkerArguments @('-CaseFile', $caseFile) `
        -Directory $evidence -Executable (Join-Path $output 'FBE.exe') -TimeoutSeconds 900 -ExpectedChildExitCodes @(0)
    if ($result.status -ne 'PASS') { throw 'ASan case failed; retain the evidence directory.' }
    Write-Host 'Single invocation passed. This does not resolve the earlier intermittent crash or constitute FAST.'
} finally {
    $env:PATH = $savedPath
    $env:_CL_ = $savedCl
    $env:ASAN_SYMBOLIZER_PATH = $savedSymbolizer
    $env:TEMP = $savedTemp
    $env:TMP = $savedTmp
}
} finally {
    $currentEnvironment = [Environment]::GetEnvironmentVariables('Process')
    foreach ($name in $currentEnvironment.Keys) {
        if (-not $originalEnvironment.Contains($name)) { Remove-Item -LiteralPath ('Env:' + $name) -ErrorAction Stop }
    }
    foreach ($name in $originalEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, [string]$originalEnvironment[$name], 'Process')
    }
}
