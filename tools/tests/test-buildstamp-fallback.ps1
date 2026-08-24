<# Guards the MSBuild fallback used by direct Visual Studio builds. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$project = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.vcxproj')
$stamp = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\buildstamp.c')
if (-not $project.Contains('<FbeBuildReleaseDefine Condition="''$(FbeReleaseVersion)''==''''">/DFBE_BUILD_RELEASE_VERSION=FBE_VERSION_STRING')) { throw 'Direct VS build lacks FBE_VERSION_STRING fallback.' }
if (-not $project.Contains('/DFBE_BUILD_RELEASE_VERSION=\&quot;$(FbeReleaseVersion)\&quot;')) { throw 'Explicit FbeReleaseVersion is not passed as a string.' }
if (-not $stamp.Contains('#define FBE_BUILD_RELEASE_VERSION FBE_VERSION_STRING')) { throw 'buildstamp fallback was removed.' }
Write-Host 'Buildstamp fallback contract passed.'
