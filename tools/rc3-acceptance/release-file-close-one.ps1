[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RepositoryRoot,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [Parameter(Mandatory)][ValidateSet('DISPOSABLE-WINDOWS')][string]$EnvironmentAcknowledgement
)

# Run only in an independently verified disposable Windows GUI session.
# Configure and verify a crash collector in that environment before running.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$expected = 'c4bcb76c3cd03d1dff88fa557aa75261b4ec0461'
$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne $expected) { throw 'Unexpected source SHA.' }
& git -C $root -c diff.ignoreSubmodules=all diff --quiet HEAD --
if ($LASTEXITCODE -ne 0) { throw 'Tracked source files differ from HEAD.' }
$output = Join-Path $root 'out/Release'
$provenance = Join-Path $root 'out/build-provenance'
foreach ($name in @('CommonCore-Release.json','Runtime-Release.json')) {
    $m = Get-Content -LiteralPath (Join-Path $provenance $name) -Raw | ConvertFrom-Json
    $kind = $name.Split('-')[0]
    if ($m.schemaVersion -ne 1 -or $m.gitCommit -ne $expected -or $m.sanitizer -ne 'None' -or $m.kind -ne $kind -or
        $m.configuration -ne 'Release' -or $m.platform -ne 'Win32' -or $m.platformToolset -ne 'v143' -or
        $m.compiler -ne 'MSVC' -or $m.compilerVersion -notmatch '^14\.44\.') { throw "Unexpected provenance: $name" }
    $required = if ($kind -eq 'CommonCore') { @('FBE.exe','FBV.exe','ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll','html.xsl') } else { @('Scintilla.dll','Lexilla.dll','ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe') }
    $names = @($m.artifacts.PSObject.Properties.Name)
    if ($names.Count -ne $required.Count -or @($required | Where-Object { $_ -notin $names }).Count) { throw "Incomplete provenance artifacts: $name" }
}
& (Join-Path $root 'tools/build/build-provenance.ps1') -Action Validate -Kind CommonCore -Configuration Release -CommonDirectory $output
& (Join-Path $root 'tools/build/build-provenance.ps1') -Action Validate -Kind Runtime -Configuration Release -ProfileDirectory (Join-Path $root 'out/editor-runtime') -BatchDirectory $output
foreach ($name in @('Scintilla.dll','Lexilla.dll')) {
    $built = Join-Path $root "out/editor-runtime/$name"
    $staged = Join-Path $output $name
    if ((Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash) {
        throw "Staged runtime differs from validated provenance: $name"
    }
}
if (Test-Path -LiteralPath $EvidenceDirectory) { throw 'EvidenceDirectory must not already exist.' }
New-Item -ItemType Directory -Path $EvidenceDirectory | Out-Null
$evidence = (Resolve-Path -LiteralPath $EvidenceDirectory).Path
Copy-Item -LiteralPath (Join-Path $provenance 'CommonCore-Release.json'), (Join-Path $provenance 'Runtime-Release.json') -Destination $evidence
$head | Set-Content -LiteralPath (Join-Path $evidence 'source-sha.txt') -Encoding utf8
Get-FileHash -LiteralPath (Join-Path $output 'FBE.exe'), (Join-Path $output 'FBE.pdb') -Algorithm SHA256 |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'exe-pdb-hashes.json') -Encoding utf8
& git -C $root submodule status --recursive *> (Join-Path $evidence 'submodules.log')
if ($LASTEXITCODE -ne 0) { throw 'Cannot verify submodule status; inspect submodules.log.' }
if (@(Get-Content -LiteralPath (Join-Path $evidence 'submodules.log') | Where-Object { $_ -match '^[-+U]' }).Count) {
    throw 'Submodule checkout differs from gitlinks or is uninitialized.'
}
& git -C $root submodule foreach --quiet --recursive 'git diff --quiet HEAD --'
if ($LASTEXITCODE -ne 0) { throw 'A submodule has tracked changes or could not be inspected.' }
$savedTemp = $env:TEMP
$savedTmp = $env:TMP
try {
    $env:TEMP = Join-Path $evidence 'temp'
    New-Item -ItemType Directory -Path $env:TEMP | Out-Null
    $env:TMP = $env:TEMP
    $global:LASTEXITCODE = 0
    try {
        & (Join-Path $root 'tools/tests/test-fbe-file-close-runtime.ps1') -FbeExe (Join-Path $output 'FBE.exe') *> (Join-Path $evidence 'test.log')
        if ($LASTEXITCODE -ne 0) { throw "Native exit: $LASTEXITCODE" }
        @{status='PASS';source=$head;scenario='release.fbe-file-close-runtime.1'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'result.json')
    } catch {
        @{status='FAIL';source=$head;scenario='release.fbe-file-close-runtime.1';error=$_.ToString()} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'result.json')
        throw
    }
} finally {
    $env:TEMP = $savedTemp
    $env:TMP = $savedTmp
}
