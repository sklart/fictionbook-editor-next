$ErrorActionPreference = 'Stop'
$base = Join-Path $PSScriptRoot ('../../out/rc3-midl-mock-' + [guid]::NewGuid().ToString('N'))
$root = Join-Path $base 'repo'
$relative = 'src/export-docx/ExportDOCX_i.c'
$target = Join-Path $root $relative
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
$original = @('/* generated */','/* at Tue Jan 19 06:14:07 2038',' */','int value = 1;')
$generated = @('/* generated */','/* at Tue Jan 19 03:14:07 2038',' */','int value = 1;')
$global:rc3MockOriginal = $original
$global:rc3MockChanged = @($relative)
$global:rc3MockTarget = $target
$global:rc3MockRestored = $false
$global:rc3MockRestoreFails = $false
$originalBytes = [Text.UTF8Encoding]::new($false).GetBytes(($original -join [Environment]::NewLine) + [Environment]::NewLine)
$blobHeader = [Text.Encoding]::ASCII.GetBytes("blob $($originalBytes.Length)`0")
$global:rc3MockBlob = [Convert]::ToHexString([Security.Cryptography.SHA1]::HashData([byte[]]($blobHeader + $originalBytes))).ToLowerInvariant()
function global:git {
    $global:LASTEXITCODE = 0
    if ($args -contains 'rev-parse' -and @($args | Where-Object { $_ -like 'HEAD:*' }).Count) { $global:rc3MockBlob }
    elseif ($args -contains 'rev-parse') { 'c4bcb76c3cd03d1dff88fa557aa75261b4ec0461' }
    elseif ($args -contains '--name-only') { $global:rc3MockChanged }
    elseif ($args -contains 'show') { $global:rc3MockOriginal }
    elseif ($args -contains 'restore') {
        if ($global:rc3MockRestoreFails) { $global:LASTEXITCODE = 1; return }
        $global:rc3MockRestored = $true
        [IO.File]::WriteAllLines($global:rc3MockTarget, [string[]]$global:rc3MockOriginal)
    }
    elseif ($args -contains '--quiet') { if (-not $global:rc3MockRestored) { $global:LASTEXITCODE = 1 } }
}
try {
    $helper = Join-Path $PSScriptRoot 'ReconcileMidl.ps1'
    [IO.File]::WriteAllLines($target, [string[]]$generated)
    $blocked = $false
    try { & $helper -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'wrong-sha') -ExpectedSource ('0' * 40) } catch { $blocked = $_.ToString() -match 'Unexpected source SHA' }
    if (-not $blocked -or $global:rc3MockRestored) { throw 'Unexpected source SHA was accepted or restored.' }
    & $helper -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'ok')
    if (-not $global:rc3MockRestored -or (Get-Content -Raw (Join-Path $base 'ok/midl-reconciliation.json') | ConvertFrom-Json).status -ne 'restored-clean') { throw 'Expected timestamp reconciliation failed.' }
    $global:rc3MockRestored = $false
    $global:rc3MockRestoreFails = $true
    [IO.File]::WriteAllLines($target, [string[]]$generated)
    $blocked = $false
    try { & $helper -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'restore-failure') } catch { $blocked = $_.ToString() -match 'Cannot restore' }
    $global:rc3MockRestoreFails = $false
    if (-not $blocked -or (Get-Content -Raw (Join-Path $base 'restore-failure/midl-reconciliation.json') | ConvertFrom-Json).status -ne 'verified-differences') { throw 'Restore failure was reported as clean.' }
    [IO.File]::WriteAllLines($target, [string[]]@('/* generated */','/* at Tue Jan 19 03:14:07 2038',' */','int value = 2;'))
    $blocked = $false
    try { & $helper -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'changed-code') } catch { $blocked = $_.ToString() -match 'non-timestamp change' }
    if (-not $blocked -or $global:rc3MockRestored) { throw 'Semantic change was accepted or restored.' }
    [IO.File]::WriteAllText($target, ($generated -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    $blocked = $false
    try { & $helper -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'changed-line-endings') } catch { $blocked = $_.ToString() -match 'non-timestamp byte change' }
    if (-not $blocked -or $global:rc3MockRestored) { throw 'Line-ending change was accepted or restored.' }
    $global:rc3MockChanged = @('src/fbe/mainfrm.cpp')
    $blocked = $false
    try { & $helper -RepositoryRoot $root -EvidenceDirectory (Join-Path $base 'unexpected') } catch { $blocked = $_.ToString() -match 'Unexpected tracked source' }
    if (-not $blocked -or $global:rc3MockRestored) { throw 'Unexpected source file was accepted or restored.' }
    Write-Host 'Mock MIDL reconciliation checks PASS; no native Git or editor was started.'
} finally {
    Remove-Item Function:\global:git -ErrorAction SilentlyContinue
    Remove-Variable rc3MockOriginal,rc3MockChanged,rc3MockTarget,rc3MockRestored,rc3MockRestoreFails,rc3MockBlob -Scope Global -ErrorAction SilentlyContinue
}
