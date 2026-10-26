[CmdletBinding()]
param([string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/perf/c5/diagnostic'),[string]$RuntimeDirectory=(Join-Path $PSScriptRoot '../../out/perf/c5/baseline-runtime'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory);$bin=Join-Path $out 'runtime';$obj=Join-Path $out 'obj'
New-Item -ItemType Directory -Force $bin,$obj|Out-Null
Copy-Item (Join-Path $RuntimeDirectory '*') $bin -Recurse -Force
Copy-Item (Join-Path $root 'runtime/main.js') (Join-Path $bin 'main.js') -Force
$head=(& git -C $root rev-parse HEAD).Trim();if($LASTEXITCODE){throw 'Cannot resolve source HEAD'}
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$old=$env:_CL_;$env:_CL_="$old /DFBE_BINARY_STORE_DIAGNOSTICS /DFBE_BINARY_STORE_MEMORY_ATTRIBUTION /DFBE_SOURCE_MEMORY_BENCHMARK"
try {
 & MSBuild.exe (Join-Path $root 'src/fbe/FBE.vcxproj') /t:Build /m:2 /nologo /v:minimal /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:VCToolsVersion=14.44.35207 "/p:SolutionDir=$root\" "/p:OutDir=$bin\" "/p:IntDir=$obj\" /p:BuildProjectReferences=false /p:FbeStrictWarnings=true /p:EnableUpx=false "/p:FbeBuildCommit=$head"
 if($LASTEXITCODE){throw 'C5 diagnostic build failed'}
}finally{$env:_CL_=$old}
$hashes=@{};foreach($file in @('FBE.exe','main.js','Scintilla.dll','Lexilla.dll')){$hashes[$file]=(Get-FileHash (Join-Path $bin $file)).Hash}
$source=@{};Get-ChildItem (Join-Path $root 'src/fbe') -Recurse -File|Where-Object {$_.Extension -in '.cpp','.h','.inl'}|ForEach-Object {$source[[IO.Path]::GetRelativePath($root,$_.FullName)]=(Get-FileHash $_.FullName).Hash}
@{status='PASS';head=$head;diagnostic=$true;policyOverride='test-only';hashes=$hashes;sourceHashes=$source;clOptions='FBE_BINARY_STORE_DIAGNOSTICS FBE_BINARY_STORE_MEMORY_ATTRIBUTION FBE_SOURCE_MEMORY_BENCHMARK'}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'build.json') -Encoding utf8
