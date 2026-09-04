<# Checks the PE contract of the actual Win32 ArchHandler artifacts. #>
[CmdletBinding()]
param(
    [string]$PlatformToolset = 'v143',
    [string]$HandlerDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $HandlerDirectory) {
    & (Join-Path $repoRoot 'tools\build\build-archhandler.ps1') -PlatformToolset $PlatformToolset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $HandlerDirectory = Join-Path $repoRoot 'out\archhandler\Win32\Release'
}
$handlerDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($HandlerDirectory)
$version = [regex]::Match((Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\version.h')), '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"').Groups['version'].Value
if (-not $version) { throw 'Cannot determine project version.' }

function Get-PeInfo([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 256 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) { throw "$Path is not a PE file." }
    $offset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ($offset -lt 0 -or $offset + 96 -gt $bytes.Length -or [Text.Encoding]::ASCII.GetString($bytes, $offset, 4) -ne "PE`0`0") { throw "$Path has an invalid PE header." }
    $optional = $offset + 24
    [pscustomobject]@{
        Machine = [BitConverter]::ToUInt16($bytes, $offset + 4)
        Magic = [BitConverter]::ToUInt16($bytes, $optional)
        Subsystem = [BitConverter]::ToUInt16($bytes, $optional + 68)
        DllCharacteristics = [BitConverter]::ToUInt16($bytes, $optional + 70)
    }
}

$mt = Get-Command mt.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1
if (-not $mt) {
    & (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset
    $mt = Get-Command mt.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1
}
if (-not $mt) { throw 'mt.exe is required to inspect the embedded ArchHandler manifest.' }
foreach ($name in 'ZipHandler.exe', 'RarHandler.exe') {
    $path = Join-Path $handlerDirectory $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing ArchHandler artifact: $path" }
    $pe = Get-PeInfo $path
    if ($pe.Machine -ne 0x014c -or $pe.Magic -ne 0x10b) { throw "$name must be PE32/i386." }
    if ($pe.Subsystem -ne 2) { throw "$name must use the GUI subsystem." }
    if (($pe.DllCharacteristics -band 0x40) -eq 0 -or ($pe.DllCharacteristics -band 0x100) -eq 0) { throw "$name must have DYNAMICBASE and NXCOMPAT." }
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)
    if ($info.FileVersion -ne $version -or $info.ProductVersion -ne $version) { throw "$name version metadata does not match $version." }
    if ([string]::IsNullOrWhiteSpace($info.FileDescription) -or [string]::IsNullOrWhiteSpace($info.ProductName)) { throw "$name has incomplete VERSIONINFO." }
    $manifest = Join-Path ([IO.Path]::GetTempPath()) ("fbe-archhandler-$PID-$name.manifest")
    try {
        & $mt -nologo "-inputresource:$path;#1" "-out:$manifest"
        if ($LASTEXITCODE -ne 0) { throw "Cannot extract the embedded manifest from $name." }
        [xml]$xml = Get-Content -Raw -LiteralPath $manifest
        $level = $xml.SelectSingleNode("//*[local-name()='requestedExecutionLevel']")
        if (-not $level -or $level.level -ne 'asInvoker' -or $level.uiAccess -ne 'false') { throw "$name must embed an asInvoker application manifest." }
    } finally { Remove-Item -LiteralPath $manifest -Force -ErrorAction SilentlyContinue }
}
Write-Host 'ArchHandler PE contract passed.'
