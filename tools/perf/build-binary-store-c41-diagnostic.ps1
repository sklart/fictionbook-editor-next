[CmdletBinding()]
param([string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/perf/c41'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
$sourceHead=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE){throw 'Cannot resolve source HEAD'}
$sourceDirty=[bool](& git -C $root status --porcelain)
$bin=Join-Path $out 'diagnostic-bin';$obj=Join-Path $out 'diagnostic-obj'
New-Item -ItemType Directory -Force $bin,$obj|Out-Null
Copy-Item (Join-Path $root 'out/perf/c3-2f0113b2/*') $bin -Recurse -Force
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$old=$env:_CL_;$env:_CL_="$old /DFBE_BINARY_STORE_DIAGNOSTICS /DFBE_BINARY_STORE_MEMORY_ATTRIBUTION"
try {
 & MSBuild.exe (Join-Path $root 'src/fbe/FBE.vcxproj') /t:Build /m:2 /nologo /v:minimal '/p:Configuration=Release' '/p:Platform=Win32' '/p:PlatformToolset=v143' '/p:VCToolsVersion=14.44.35207' "/p:SolutionDir=$root\" "/p:OutDir=$bin\" "/p:IntDir=$obj\" /p:BuildProjectReferences=false /p:FbeStrictWarnings=true "/p:FbeBuildCommit=$sourceHead"
 if($LASTEXITCODE){throw 'C4.1 diagnostic compile failed'}
}finally{$env:_CL_=$old}
Get-ChildItem (Join-Path $root 'out/perf/c2') -File -Filter '*.dll'|Copy-Item -Destination $bin -Force
$sources=@{};Get-ChildItem (Join-Path $root 'src/fbe') -Recurse -File | Where-Object {$_.Extension -in '.cpp','.h','.inl'} | ForEach-Object {$sources[[IO.Path]::GetRelativePath($root,$_.FullName)]=(Get-FileHash $_.FullName).Hash}
@{kind='C4.1 TEST-only lifetime attribution';base='2f0113b2566a3510c74e7e1c4536e9d10e4be0c8';productionBehaviorChanged=$false;sourceHead=$sourceHead;sourceDirty=$sourceDirty;exeSha256=(Get-FileHash (Join-Path $bin 'FBE.exe')).Hash;mainJsSha256=(Get-FileHash (Join-Path $bin 'main.js')).Hash;sourceHashes=$sources}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'diagnostic-build.json') -Encoding utf8
