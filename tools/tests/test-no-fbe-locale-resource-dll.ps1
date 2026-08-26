<# Prevents the retired FBE locale DLLs from re-entering the production path. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
foreach ($legacyDirectory in @('src\locales\res_rus', 'src\locales\res_ukr')) {
    if (Test-Path -LiteralPath (Join-Path $repoRoot $legacyDirectory)) {
        throw "Retired FBE locale project directory still exists: $legacyDirectory"
    }
}
$paths = @('src\fbe', 'FBE.sln', 'tools\build', 'packaging', 'localization\language-packs.json')
foreach ($relative in $paths) {
    $path = Join-Path $repoRoot $relative
    # Verification scripts may name the retired files solely to reject them.
    $matches = @(rg -n 'res_(rus|ukr)\.(dll|pdb)' $path |
        Where-Object { $_ -notmatch '[\\/]MakeInstaller\.nsi:' -and $_ -notmatch '[\\/]package-manifest\.json:' -and $_ -notmatch '[\\/]verify-release\.ps1:' })
    if ($matches.Count) { throw "Retired FBE locale DLL/PDB reference found in ${relative}:`n$($matches -join "`n")" }
}
Write-Host 'No production dependency on retired FBE locale resource DLLs found.'
