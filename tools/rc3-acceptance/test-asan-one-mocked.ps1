$ErrorActionPreference = 'Stop'
$runner = Join-Path $PSScriptRoot 'asan-file-close-one.ps1'
$base = Join-Path $PSScriptRoot ('../../out/rc3-isolated-runner/mock-' + [guid]::NewGuid().ToString('N'))
$root = Join-Path $base 'repo'
$output = Join-Path $root 'out/asan/Release'
New-Item -ItemType Directory -Path $output, (Join-Path $root 'tools/build'), (Join-Path $root 'tools/tests') -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $output 'FBE.exe'), 'mock executable')
[IO.File]::WriteAllText((Join-Path $output 'FBE.pdb'), 'mock symbols')
$moduleNames = @('FBE.exe','FBV.exe','FBShell.dll','Plugins\ExportHTML.dll','Plugins\ExportDOCX.dll','Plugins\ExportEPUB.dll','Plugins\ImportEPUB.dll','Plugins\ImportEPUBLunaSVG.dll','ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe')
$modules = @(foreach ($name in $moduleNames) {
    $path = Join-Path $output $name
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    if ($name -ne 'FBE.exe') { [IO.File]::WriteAllText($path, 'mock module') }
    @{path=$name;sha256=(Get-FileHash -LiteralPath $path).Hash;instrumented=$true}
})
@{schemaVersion=1;gitCommit='c4bcb76c3cd03d1dff88fa557aa75261b4ec0461';sanitizer='Address';publishable=$false;configuration='Release';platform='Win32';platformToolset='v143';compiler='MSVC';compilerVersion='14.44.35207';vcToolsVersion='14.44.35207';stlContainerAnnotations=$false;translationUnits=@(1..200);modules=$modules} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'fbe-sanitizer.json')
@'
param($Arch,$HostArch,$PlatformToolset,$VcVarsVersion)
if ($Arch -ne 'x86' -or $VcVarsVersion -ne '14.44') { throw 'Wrong toolchain arguments' }
$env:Path = $env:Path + ';mock-toolchain'
$env:FBE_RC3_MOCK_IMPORT = 'set'
if ($global:mockImportFails) { throw 'Injected toolchain import failure' }
'@ | Set-Content -LiteralPath (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1')
@'
function Invoke-SanitizerCase {
    param($Sanitizer,$Name,$Worker,$WorkerArguments,$Directory,$Executable,$TimeoutSeconds,$ExpectedChildExitCodes)
    if ($global:mockHarnessFails) { throw 'Injected harness failure' }
    $case = Get-Content -Raw -LiteralPath $WorkerArguments[1] | ConvertFrom-Json
    if ($Name -ne 'release.fbe-file-close-runtime.1' -or $Sanitizer -ne 'Address' -or
        $case.command -ne "./tools/tests/test-fbe-file-close-runtime.ps1 -FbeExe (Join-Path `$outputDir 'FBE.exe')" -or
        $WorkerArguments[0] -ne '-CaseFile' -or $TimeoutSeconds -ne 900 -or $ExpectedChildExitCodes[0] -ne 0 -or
        $env:TEMP -ne (Join-Path $Directory 'temp') -or $env:TMP -ne $env:TEMP) { throw 'Incorrect harness call' }
    @{name=$Name;status='PASS'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Directory 'mock-result.json')
    [pscustomobject]@{status='PASS'}
}
'@ | Set-Content -LiteralPath (Join-Path $root 'tools/tests/SanitizerHarness.ps1')
function global:git {
    $global:LASTEXITCODE = 0
    if ($args -contains 'foreach') { if ($global:mockDirtySubmodule) { $global:LASTEXITCODE = 1 } }
    elseif ($args -contains 'rev-parse') { 'c4bcb76c3cd03d1dff88fa557aa75261b4ec0461' }
    elseif ($args -contains 'submodule') { ' b141d9c45db236335351c8a4b2a14b3ade27c937 third_party/aom (mock)' }
}
$beforeTemp = $env:TEMP
$beforeTmp = $env:TMP
$beforePath = $env:PATH
$beforeCl = $env:_CL_
$beforeSymbolizer = $env:ASAN_SYMBOLIZER_PATH
$beforeImportMarker = $env:FBE_RC3_MOCK_IMPORT
$global:mockDirtySubmodule = $false
$global:mockHarnessFails = $false
$global:mockImportFails = $false
try {
    $destination = Join-Path $base 'evidence'
    & $runner -RepositoryRoot $root -EvidenceDirectory $destination -EnvironmentAcknowledgement DISPOSABLE-WINDOWS
    if (-not (Test-Path -LiteralPath (Join-Path $destination 'mock-result.json'))) { throw 'Mock harness not invoked' }
    if ($env:TEMP -ne $beforeTemp -or $env:TMP -ne $beforeTmp -or $env:PATH -ne $beforePath -or $env:FBE_RC3_MOCK_IMPORT -ne $beforeImportMarker) { throw "Environment was not restored after toolchain import (TEMP=$($env:TEMP -eq $beforeTemp); TMP=$($env:TMP -eq $beforeTmp); PATH=$($env:PATH -eq $beforePath); marker=$($env:FBE_RC3_MOCK_IMPORT -eq $beforeImportMarker))" }
    $manifestPath = Join-Path $output 'fbe-sanitizer.json'
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $manifest.vcToolsVersion = '14.43.1'
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'wrong-toolchain') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'v143/14.44' }
    if (-not $blocked) { throw 'Wrong ASan toolchain was accepted' }
    $manifest.vcToolsVersion = '14.44.35207'
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath
    $global:mockImportFails = $true
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'import-failure') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'Injected toolchain import failure' }
    $global:mockImportFails = $false
    if (-not $blocked -or $env:PATH -ne $beforePath -or $env:FBE_RC3_MOCK_IMPORT -ne $beforeImportMarker) { throw 'Environment was not restored after toolchain import failure' }
    $global:mockHarnessFails = $true
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'harness-failure') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'Injected harness failure' }
    $global:mockHarnessFails = $false
    if (-not $blocked) { throw 'Injected harness failure was not propagated' }
    if ($env:TEMP -ne $beforeTemp -or $env:TMP -ne $beforeTmp -or $env:PATH -ne $beforePath -or $env:_CL_ -ne $beforeCl -or $env:ASAN_SYMBOLIZER_PATH -ne $beforeSymbolizer -or $env:FBE_RC3_MOCK_IMPORT -ne $beforeImportMarker) { throw 'Environment was not restored after failure' }
    $global:mockDirtySubmodule = $true
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'dirty-submodule') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'submodule has tracked changes' }
    $global:mockDirtySubmodule = $false
    if (-not $blocked) { throw 'Dirty submodule was accepted' }
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory $destination -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $true }
    if (-not $blocked) { throw 'Existing evidence directory was accepted' }
    [IO.File]::WriteAllText((Join-Path $output 'FBE.exe'), 'changed mock executable')
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'bad-hash') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'hash mismatch' }
    if (-not $blocked) { throw 'Changed module was accepted' }
    $manifestPath = Join-Path $output 'fbe-sanitizer.json'
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $manifest.modules = @($manifest.modules | Where-Object path -ne 'FBE.exe')
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'missing-module') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'module list' }
    if (-not $blocked) { throw 'Incomplete module list was accepted' }
    Write-Host "Mock verification PASS; artifacts retained: $base"
} finally {
    Remove-Item Function:\global:git -ErrorAction SilentlyContinue
    Remove-Variable mockDirtySubmodule -Scope Global -ErrorAction SilentlyContinue
    Remove-Variable mockHarnessFails -Scope Global -ErrorAction SilentlyContinue
    Remove-Variable mockImportFails -Scope Global -ErrorAction SilentlyContinue
}
