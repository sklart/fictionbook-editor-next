<# Verify version synchronization preserves existing line endings and updates stale content. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('fbe-version-sync-' + [guid]::NewGuid().ToString('N'))
try {
    foreach ($dir in @('tools/version', 'src', 'packaging/nsis/Installer')) { New-Item -ItemType Directory -Path (Join-Path $fixture $dir) -Force | Out-Null }
    Copy-Item -LiteralPath (Join-Path $root 'tools/version/sync-version.ps1') -Destination (Join-Path $fixture 'tools/version/sync-version.ps1')
    [IO.File]::WriteAllText((Join-Path $fixture 'src/version.h'), '#define FBE_VERSION_STRING "3.0.11"', [Text.UTF8Encoding]::new($false))
    $target = Join-Path $fixture 'packaging/nsis/Installer/version.nsh'
    $comment = '; Сгенерировано tools/version/sync-version.ps1 на основе src/version.h.'
    $define = '!define PRODUCT_VER_NUM "3.0.11"'
    foreach ($newline in @("`n", "`r`n")) {
        $original = [Text.UTF8Encoding]::new($false).GetBytes($comment + $newline + $define)
        [IO.File]::WriteAllBytes($target, $original)
        & (Join-Path $fixture 'tools/version/sync-version.ps1')
        if (-not [Linq.Enumerable]::SequenceEqual([byte[]]$original, [byte[]][IO.File]::ReadAllBytes($target))) { throw 'Equivalent version file was rewritten.' }
    }
    [IO.File]::WriteAllText($target, ($comment + "`n" + '!define PRODUCT_VER_NUM "3.0.10"'), [Text.UTF8Encoding]::new($false))
    & (Join-Path $fixture 'tools/version/sync-version.ps1')
    if ([IO.File]::ReadAllText($target) -notmatch '(?m)^!define PRODUCT_VER_NUM "3\.0\.11"\r?$') { throw 'Stale version was not updated.' }
    Write-Host 'Version synchronization preserves equivalent line endings and updates stale versions.'
} finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
