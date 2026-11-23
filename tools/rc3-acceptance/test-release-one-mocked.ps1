$ErrorActionPreference = 'Stop'
$base = Join-Path $PSScriptRoot ('../../out/rc3-isolated-runner/mock-release-' + [guid]::NewGuid().ToString('N'))
$root = Join-Path $base 'repo'
$output = Join-Path $root 'out/Release'
$profile = Join-Path $root 'out/editor-runtime'
$provenance = Join-Path $root 'out/build-provenance'
New-Item -ItemType Directory -Path $output, $profile, $provenance, (Join-Path $root 'tools/build'), (Join-Path $root 'tools/tests') -Force | Out-Null
foreach ($name in @('FBE.exe','FBE.pdb','Scintilla.dll','Lexilla.dll')) { [IO.File]::WriteAllText((Join-Path $output $name), "mock $name") }
foreach ($name in @('Scintilla.dll','Lexilla.dll')) { Copy-Item -LiteralPath (Join-Path $output $name) -Destination $profile }
$common = @('FBE.exe','FBV.exe','ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll','html.xsl')
$runtime = @('Scintilla.dll','Lexilla.dll','ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe')
foreach ($kind in @('CommonCore','Runtime')) {
    $items = if ($kind -eq 'CommonCore') { $common } else { $runtime }
    $artifacts = @{}; foreach ($name in $items) { $artifacts[$name] = @{sha256='mock'} }
    @{schemaVersion=1;gitCommit='c4bcb76c3cd03d1dff88fa557aa75261b4ec0461'; sanitizer='None'; kind=$kind; configuration='Release'; platform='Win32'; platformToolset='v143'; compiler='MSVC'; compilerVersion='14.44.35207'; artifacts=$artifacts} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $provenance "$kind-Release.json")
}
@'
param($Action,$Kind,$Configuration,$CommonDirectory,$ProfileDirectory,$BatchDirectory)
if ($Action -ne 'Validate' -or $Configuration -ne 'Release' -or $Kind -notin @('CommonCore','Runtime')) { throw 'Wrong provenance arguments' }
'@ | Set-Content -LiteralPath (Join-Path $root 'tools/build/build-provenance.ps1')
@'
param($FbeExe)
if ($FbeExe -ne (Join-Path $PSScriptRoot '../../out/Release/FBE.exe') -and -not $FbeExe.EndsWith('out\Release\FBE.exe')) { throw 'Wrong executable' }
if ($env:TEMP -ne $env:TMP) { throw 'Incorrect isolated TEMP' }
if ($global:mockCaseFails) { throw 'Injected case failure' }
'@ | Set-Content -LiteralPath (Join-Path $root 'tools/tests/test-fbe-file-close-runtime.ps1')
function global:git {
    $global:LASTEXITCODE = 0
    if ($args -contains 'foreach') { if ($global:mockDirtySubmodule) { $global:LASTEXITCODE = 1 } }
    elseif ($args -contains 'rev-parse') { 'c4bcb76c3cd03d1dff88fa557aa75261b4ec0461' }
    elseif ($args -contains 'submodule') { ' b141d9c45db236335351c8a4b2a14b3ade27c937 third_party/aom (mock)' }
}
$global:mockDirtySubmodule = $false
$global:mockCaseFails = $false
try {
    $runner = Join-Path $PSScriptRoot 'release-file-close-one.ps1'
    $oldTemp = $env:TEMP; $oldTmp = $env:TMP
    & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'ok') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS
    if ((Get-Content -Raw (Join-Path $base 'ok/result.json') | ConvertFrom-Json).status -ne 'PASS') { throw 'Mock case did not pass.' }
    if ($env:TEMP -ne $oldTemp -or $env:TMP -ne $oldTmp) { throw 'TEMP was not restored.' }
    $toolchainPath = Join-Path $provenance 'CommonCore-Release.json'
    $toolchainManifest = Get-Content -Raw $toolchainPath | ConvertFrom-Json
    $toolchainManifest.compilerVersion = '14.43.1'
    $toolchainManifest | ConvertTo-Json -Depth 5 | Set-Content $toolchainPath
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'wrong-toolchain') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'Unexpected provenance' }
    if (-not $blocked) { throw 'Wrong Release toolchain was accepted.' }
    $toolchainManifest.compilerVersion = '14.44.35207'
    $toolchainManifest | ConvertTo-Json -Depth 5 | Set-Content $toolchainPath
    $global:mockCaseFails = $true
    $blocked = $false
    $failurePath = Join-Path $base 'case-failure'
    try { & $runner -RepositoryRoot $root -EvidenceDirectory $failurePath -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'Injected case failure' }
    $global:mockCaseFails = $false
    if (-not $blocked -or (Get-Content -Raw (Join-Path $failurePath 'result.json') | ConvertFrom-Json).status -ne 'FAIL') { throw 'Injected case failure was not reported.' }
    if ($env:TEMP -ne $oldTemp -or $env:TMP -ne $oldTmp) { throw 'TEMP was not restored after failure.' }
    $global:mockDirtySubmodule = $true
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'dirty-submodule') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'submodule has tracked changes' }
    $global:mockDirtySubmodule = $false
    if (-not $blocked) { throw 'Dirty submodule was accepted.' }
    [IO.File]::WriteAllText((Join-Path $output 'Scintilla.dll'), 'changed')
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'bad-runtime') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'Staged runtime differs' }
    if (-not $blocked) { throw 'Changed staged runtime was accepted.' }
    $manifestPath = Join-Path $provenance 'CommonCore-Release.json'
    $manifest = Get-Content -Raw $manifestPath | ConvertFrom-Json
    $manifest.artifacts.PSObject.Properties.Remove('FBE.exe')
    $manifest | ConvertTo-Json -Depth 5 | Set-Content $manifestPath
    $blocked = $false
    try { & $runner -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'missing-artifact') -EnvironmentAcknowledgement DISPOSABLE-WINDOWS } catch { $blocked = $_.ToString() -match 'Incomplete provenance artifacts' }
    if (-not $blocked) { throw 'Incomplete provenance was accepted.' }
    Write-Host "Release mock verification PASS; artifacts retained: $base"
} finally { Remove-Item Function:\global:git -ErrorAction SilentlyContinue; Remove-Variable mockDirtySubmodule,mockCaseFails -Scope Global -ErrorAction SilentlyContinue }
