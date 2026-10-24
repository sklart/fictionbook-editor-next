[CmdletBinding()]
param([string]$SanitizerDirectory)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(-not $SanitizerDirectory) { $SanitizerDirectory=Join-Path $root 'out/asan/Release' }
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$msbuild=Join-Path $env:VSINSTALLDIR 'MSBuild/Current/Bin/MSBuild.exe'
$checks=0
foreach($project in Get-ChildItem (Join-Path $root 'src') -Recurse -Filter '*.vcxproj') {
    $xml=Get-Content -Raw -LiteralPath $project.FullName
    if($xml -notmatch 'FBE.Common.props') { continue }
    if($xml -match 'ProjectConfiguration Include="ASan') { throw 'Sanitizer must remain a mode of Release, not a configuration.' }
    foreach($mode in @('None','Address')) {
        $json=& $msbuild $project.FullName /nologo /p:Configuration=Release /p:Platform=Win32 "/p:FbeSanitizer=$mode" '/getProperty:OutDir;IntDir;LinkIncremental;PlatformToolset;VCToolsVersion;FbeLanguageStandard;FbeSanitizerCompileOptions' /getItem:ClCompile
        if($LASTEXITCODE -ne 0) { throw "Policy evaluation failed: $($project.Name)/$mode" }
        $data=($json -join "`n") | ConvertFrom-Json
        if($data.Properties.PlatformToolset -ne 'v143' -or $data.Properties.VCToolsVersion -notlike '14.44*' -or $data.Properties.FbeLanguageStandard -ne 'stdcpp17') { throw 'Sanitizer changed the supported toolchain.' }
        $asan=$mode -eq 'Address'
        if($asan) {
            if(-not $data.Properties.OutDir.StartsWith((Join-Path $root 'out\asan\'),[StringComparison]::OrdinalIgnoreCase) -or -not $data.Properties.IntDir.StartsWith((Join-Path $root 'build\asan\'),[StringComparison]::OrdinalIgnoreCase)) { throw 'ASan and normal build directories overlap.' }
            if($data.Properties.LinkIncremental -ne 'false') { throw 'ASan incremental linking must be disabled.' }
        } elseif($data.Properties.OutDir -match '[/\\]asan[/\\]' -or $data.Properties.IntDir -match '[/\\]asan[/\\]') { throw 'Ordinary Release points into sanitizer output.' }
        foreach($item in @($data.Items.ClCompile)) {
            if($project.BaseName -in @('ImportEPUB','ImportEPUBBatch','ImportEPUBLunaSVG') -and $item.TreatWChar_tAsBuiltInType -ne 'false') { throw 'Importer/adapter function pointer wchar_t types must agree.' }
            $instrumented=$item.AdditionalOptions -match '(^|\s)/fsanitize=address(\s|$)'
            if($instrumented -ne $asan) { throw "Wrong instrumentation policy: $($project.Name): $($item.Identity)" }
            if($asan -and ($item.DebugInformationFormat -ne 'ProgramDatabase' -or $item.BasicRuntimeChecks -ne 'Default')) { throw 'ASan debug/runtime-check options are incompatible.' }
            ++$checks
        }
    }
}
$diagnostic=Get-Content -Raw (Join-Path $SanitizerDirectory 'fbe-sanitizer.json') | ConvertFrom-Json
if(-not $diagnostic.translationUnits.Count -or @($diagnostic.translationUnits | Where-Object {-not $_.asan}).Count) { throw 'Actual compiler command evidence is incomplete.' }
$fixture=Join-Path $root ('out/tests/asan/policy-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $fixture | Out-Null
$guard=Join-Path $root 'tools/build/assert-production-payload.ps1'
& $guard -Directory $fixture
function Require-Rejection([string]$Expected) {
    try { & $guard -Directory $fixture; throw 'Diagnostic payload was accepted.' }
    catch { if($_.Exception.Message -notmatch $Expected) { throw } }
}
$marker=Join-Path $fixture 'fbe-sanitizer.json'; Set-Content $marker '{}'
Require-Rejection 'Sanitizer artifact'; Remove-Item -LiteralPath $marker
$runtime=Join-Path $fixture 'clang_rt.asan_dynamic-i386.dll'; Set-Content $runtime 'fixture'
Require-Rejection 'Sanitizer artifact'; Remove-Item -LiteralPath $runtime
$probe=Join-Path $fixture 'FBE.exe'; Copy-Item -LiteralPath (Join-Path $SanitizerDirectory 'FBE.exe') -Destination $probe
Require-Rejection 'Instrumented binary'; Remove-Item -LiteralPath $probe
. (Join-Path $PSScriptRoot 'SanitizerCatalog.ps1')
$catalog=(& (Join-Path $PSScriptRoot 'get-release-test-catalog.ps1') -AsJson -Validate) | ConvertFrom-Json
$registration=$catalog.tests | Where-Object id -eq 'release.import-epub-registration'
$commands=@(Get-FbeSanitizerCommands -Entry $registration -Matrices @{})
if($commands.Count -ne 1 -or $commands[0] -notmatch 'Plugins/ImportEPUB\.dll' -or $commands[0] -match 'Plugins/ExportEPUB\.dll') { throw 'Import registration must exercise the diagnostic importer DLL.' }
$abi=$catalog.tests | Where-Object id -eq 'release.import-epub-batch-dll-abi'
if(1 -notin $abi.sanitizer.expectedChildExitCodes -or 3 -in $abi.sanitizer.expectedChildExitCodes) { throw 'Missing-plugin exit must be expected without accepting an ASan abort.' }
foreach($pair in @(@('release.fbe-cite-poem-undo-production',1),@('release.fbe-save-decision-runtime',73),@('release.fbe-themed-message-runtime',73))) {
    $entry=$catalog.tests | Where-Object id -eq $pair[0]
    if($pair[1] -notin $entry.sanitizer.expectedChildExitCodes -or 3 -in $entry.sanitizer.expectedChildExitCodes) { throw "Wrong expected native exit metadata: $($pair[0])" }
}
foreach($id in @('release.fbe-table-failure-safety','release.fbe-table-failure-safety.fault-change-colspan-after-normalize')) {
    $entry=$catalog.tests | Where-Object id -eq $id
    if(1 -notin $entry.sanitizer.expectedChildExitCodes -or 3 -in $entry.sanitizer.expectedChildExitCodes) { throw "Table Save rejection must expect exit 1, never an ASan abort: $id" }
}
$view=$catalog.tests | Where-Object id -eq 'release.fbe-editor-view-lifecycle-runtime'
if(@(Get-FbeSanitizerCommands -Entry $view -Matrices @{})[0] -notmatch '-TimeoutSeconds 45\b') { throw 'ASan view lifecycle must use the supported diagnostic timeout.' }
Write-Host "PASS: $checks evaluated TU/mode checks, actual compiler evidence and three production-payload rejection cases."
Write-Host 'PASS: sanitizer importer routing and expected missing-plugin exit metadata.'
