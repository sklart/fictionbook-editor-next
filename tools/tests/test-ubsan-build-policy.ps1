[CmdletBinding()]
param([string]$SanitizerDirectory)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(-not $SanitizerDirectory) { $SanitizerDirectory=Join-Path $root 'out/ubsan/Release' }
$toolchain=& (Join-Path $root 'tools/build/Resolve-UbsanToolchain.ps1')
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$msbuild=Join-Path $env:VSINSTALLDIR 'MSBuild/Current/Bin/MSBuild.exe'
$checks=0
$ignorelist=Join-Path $root 'tools/msbuild/ubsan.ignorelist'
$scope=@(Get-Content -LiteralPath $ignorelist | Where-Object { $_.Trim() -and -not $_.TrimStart().StartsWith('#') })
$exact='fun:\?Create@\?$CWindowImplBaseT@VCWindow@ATL@@V\?$CWinTraits@$0FGAAAAAA@$0A@@2@@ATL@@QAEPAUHWND__@@PAU3@V_U_RECT@2@PBGKKV_U_MENUorID@2@GPAX@Z'
if($scope.Count -ne 2 -or $scope[0] -cne '[signed-integer-overflow]' -or $scope[1] -cne $exact) { throw 'The vendor exception must remain one exact SDK symbol and one check.' }
foreach($project in Get-ChildItem (Join-Path $root src) -Recurse -Filter '*.vcxproj') {
    $xml=Get-Content $project.FullName -Raw
    if($xml -notmatch 'FBE.Common.props') { continue }
    if($xml -match 'ProjectConfiguration Include="(?:ASan|UBSan)') { throw 'Sanitizer must remain a Release mode.' }
    $arguments=@('/nologo','/p:Configuration=Release','/p:Platform=Win32','/p:FbeSanitizer=Undefined',("/p:FbeClangCl="+$toolchain.compilerPath),("/p:FbeLlvmRuntimeDirectory="+$toolchain.runtimeDirectory),("/p:FbeLldLink="+$toolchain.linkerPath),'/getProperty:OutDir;IntDir;CLToolExe;CLToolPath;LinkToolExe;LinkToolPath;LinkIncremental;PlatformToolset;FbeLanguageStandard','/getItem:ClCompile')
    $json=& $msbuild $project.FullName @arguments
    if($LASTEXITCODE) { throw "UBSan policy evaluation failed: $($project.Name)" }
    $data=($json -join "`n")|ConvertFrom-Json
    if($data.Properties.CLToolExe -ne 'clang-cl.exe' -or $data.Properties.CLToolPath.TrimEnd('\') -ne (Split-Path $toolchain.compilerPath) -or $data.Properties.PlatformToolset -ne 'v143' -or $data.Properties.FbeLanguageStandard -ne 'stdcpp17') { throw 'Incorrect compiler/SDK/language selection.' }
    if($data.Properties.LinkToolExe -ne 'lld-link.exe' -or $data.Properties.LinkToolPath.TrimEnd('\') -ne (Split-Path $toolchain.linkerPath)) { throw 'Incorrect UBSan linker selection.' }
    if(-not $data.Properties.OutDir.StartsWith((Join-Path $root 'out\ubsan\'),[StringComparison]::OrdinalIgnoreCase) -or -not $data.Properties.IntDir.StartsWith((Join-Path $root 'build\ubsan\'),[StringComparison]::OrdinalIgnoreCase) -or $data.Properties.LinkIncremental -ne 'false') { throw 'UBSan output isolation failed.' }
    foreach($item in @($data.Items.ClCompile)) {
        if($item.AdditionalOptions -notmatch '(^|\s)-fsanitize=undefined(\s|$)' -or $item.AdditionalOptions -notmatch '-fno-sanitize-recover=undefined' -or $item.AdditionalOptions -match '-fsanitize=(?!undefined\b)|-fno-sanitize=|/fsanitize=address' -or $item.WholeProgramOptimization -ne 'false') { throw "Incorrect UBSan TU flags: $($project.Name)/$($item.Identity)" }
        if($project.BaseName -in @('ImportEPUB','ImportEPUBBatch','ImportEPUBLunaSVG') -and $item.TreatWChar_tAsBuiltInType -ne 'false') { throw 'Importer/adapter function pointer wchar_t types must agree.' }
        ++$checks
    }
}
$manifest=Get-Content (Join-Path $SanitizerDirectory 'fbe-sanitizer.json') -Raw|ConvertFrom-Json
if($manifest.vendorExceptionPolicy.sha256 -ne (Get-FileHash -LiteralPath $ignorelist).Hash) { throw 'Stale compiled vendor exception policy.' }
if($manifest.sanitizer -ne 'Undefined' -or $manifest.compiler -ne 'clang-cl' -or $manifest.publishable -or @($manifest.translationUnits).Count -lt 200) { throw 'UBSan actual-build evidence missing.' }
foreach($module in $manifest.modules) {
    $path=Join-Path $SanitizerDirectory $module.path
    if((Get-FileHash $path).Hash -ne $module.sha256) { throw "Stale UBSan module: $path" }
}
$out=Join-Path $root ('out/tests/ubsan/policy-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out|Out-Null
foreach($kind in @('marker','runtime','static-pe-without-marker')) {
    $directory=Join-Path $out $kind;New-Item -ItemType Directory $directory|Out-Null
    switch($kind) {
        marker {Copy-Item (Join-Path $SanitizerDirectory 'fbe-sanitizer.json') $directory}
        runtime {'diagnostic fixture'|Set-Content (Join-Path $directory 'clang_rt.ubsan_standalone-i386.dll')}
        static-pe-without-marker {Copy-Item (Join-Path $SanitizerDirectory 'FBE.exe') (Join-Path $directory 'ordinary.exe')}
    }
    $rejected=$false
    try {& (Join-Path $root 'tools/build/assert-production-payload.ps1') -Directory $directory}
    catch {if($_ -notmatch 'forbidden in production payload'){throw};$rejected=$true;$_|Out-String|Set-Content (Join-Path $directory 'rejection.log')}
    if(-not $rejected) { throw "Production accepted diagnostic $kind" }
}
Write-Host "PASS: $checks evaluated first-party UBSan translation units, compiler/runtime evidence and three production rejection cases."
