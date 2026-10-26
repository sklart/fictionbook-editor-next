[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$bin=Join-Path $root 'out/perf/c4/diagnostic-bin'
$obj=Join-Path $root 'out/perf/c4/diagnostic-obj'
New-Item -ItemType Directory -Force $bin,$obj|Out-Null
Copy-Item (Join-Path $root 'out/perf/c3-2f0113b2/*') $bin -Recurse -Force
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$old=$env:_CL_;$env:_CL_="$old /DFBE_BINARY_STORE_DIAGNOSTICS"
try {
 & MSBuild.exe (Join-Path $root 'src/fbe/FBE.vcxproj') /t:Build /m:2 /nologo /v:minimal '/p:Configuration=Release' '/p:Platform=Win32' '/p:PlatformToolset=v143' '/p:VCToolsVersion=14.44.35207' "/p:SolutionDir=$root\" "/p:OutDir=$bin\" "/p:IntDir=$obj\" /p:BuildProjectReferences=false /p:FbeStrictWarnings=true /p:FbeBuildCommit=2f0113b2566a3510c74e7e1c4536e9d10e4be0c8
 if($LASTEXITCODE){throw 'C4 diagnostic FBE compile failed'}
}finally{$env:_CL_=$old}
Get-ChildItem (Join-Path $root 'out/perf/c2') -File -Filter '*.dll'|Copy-Item -Destination $bin -Force
Add-Content -LiteralPath (Join-Path $bin 'main.js') -Value ([IO.File]::ReadAllText((Join-Path $root 'tools/perf/binary-store-c4-runtime-profile.js'))) -Encoding utf8
@{kind='C4 diagnostic only, not a production acceptance build';base='2f0113b2566a3510c74e7e1c4536e9d10e4be0c8';macro='FBE_BINARY_STORE_DIAGNOSTICS';exeSha256=(Get-FileHash (Join-Path $bin 'FBE.exe')).Hash;mainJsSha256=(Get-FileHash (Join-Path $bin 'main.js')).Hash;productionBehaviorChanged=$false}|ConvertTo-Json|Set-Content (Join-Path $root 'out/perf/c4/diagnostic-build.json') -Encoding utf8
