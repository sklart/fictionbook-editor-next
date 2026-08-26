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
$paths = @('src\fbe', 'FBE.sln', 'localization\language-packs.json')
foreach ($relative in $paths) {
    $path = Join-Path $repoRoot $relative
    $matches = @(rg -n 'res_(rus|ukr)\.(dll|pdb)' $path)
    if ($matches.Count) { throw "Retired FBE locale DLL/PDB reference found in ${relative}:`n$($matches -join "`n")" }
}

$buildFiles = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'tools\build') -File |
    Where-Object { $_.Name -ne 'verify-release.ps1' })
foreach ($file in $buildFiles) {
    $matches = @(rg -n 'res_(rus|ukr)\.(dll|pdb)' $file.FullName)
    if ($matches.Count) { throw "Retired FBE locale DLL/PDB reference found in build script $($file.Name):`n$($matches -join "`n")" }
}

$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'packaging\package-manifest.json') | ConvertFrom-Json
$forbidden = @($manifest.core.forbidden)
foreach ($name in @('Lang\ru-RU\res_rus.dll', 'Lang\uk-UA\res_ukr.dll', 'Lang\ru-RU\res_rus.pdb', 'Lang\uk-UA\res_ukr.pdb')) {
    if ($name -notin $forbidden) { throw "Package manifest must forbid retired artifact: $name" }
}
foreach ($name in @($manifest.core.required) + @($manifest.core.optional)) {
    if ($name -match 'res_(rus|ukr)\.(dll|pdb)') { throw "Package manifest must not require retired artifact: $name" }
}

$verifyRelease = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\verify-release.ps1')
foreach ($name in @('res_rus.dll', 'res_ukr.dll', 'res_rus.pdb', 'res_ukr.pdb')) {
    if ($verifyRelease -notmatch ('\$forbiddenFiles\s*=\s*@\([\s\S]*?' + [regex]::Escape($name))) {
        throw "verify-release.ps1 must reject retired artifact: $name"
    }
}

$nsis = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'packaging\nsis\Installer\MakeInstaller.nsi')
$nsisLegacyReferences = [regex]::Matches($nsis, '(?mi)^.*res_(?:rus|ukr)\.dll.*$') |
    ForEach-Object { $_.Value.Trim() }
$expectedNsisLegacyReferences = @(
    'Delete "$INSTDIR\res_rus.dll"',
    'Delete "$INSTDIR\res_ukr.dll"'
)
if (@($nsisLegacyReferences).Count -ne $expectedNsisLegacyReferences.Count -or
    (Compare-Object -ReferenceObject $expectedNsisLegacyReferences -DifferenceObject @($nsisLegacyReferences))) {
    throw 'NSIS may mention retired FBE locale DLLs only for uninstall cleanup.'
}
Write-Host 'No production dependency on retired FBE locale resource DLLs found.'
