<# Called by NSIS before embedding the verified installer input. #>
[CmdletBinding()]
param(
    [string]$InputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$X86Sha256,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$X64Sha256,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$SchemaSha256,
    [switch]$FormatOnly
)
$ErrorActionPreference='Stop'
if ($FormatOnly) { return }
if (-not $InputDirectory) { throw 'Installer input directory is required.' }
foreach ($pair in @(@('FBShell.dll',$X86Sha256),@('FBShell64.dll',$X64Sha256),@('FBE.Sequence.propdesc',$SchemaSha256))) {
    $path=Join-Path $InputDirectory $pair[0]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) { throw "Missing or empty installer payload: $path" }
    $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        $sha=[Security.Cryptography.SHA256]::Create()
        try { $actual=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
        finally { $sha.Clear() }
    }
    finally { $stream.Close() }
    if ($actual -ne $pair[1]) { throw "Installer package SHA-256 mismatch: $path; expected=$($pair[1]); actual=$actual" }
}
