[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot, [Parameter(Mandatory)][string]$EvidenceDirectory)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -cne 'c4bcb76c3cd03d1dff88fa557aa75261b4ec0461') { throw 'Unexpected source SHA.' }
$allowed = @('src/export-docx/ExportDOCX_i.c','src/export-docx/ExportDOCX_i.h','src/export-epub/ExportEPUB_i.c','src/export-epub/ExportEPUB_i.h')
$changed = @(& git -C $root -c diff.ignoreSubmodules=all diff --name-only HEAD --)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect source changes.' }
if (@($changed | Where-Object { $_ -cnotin $allowed }).Count) { throw 'Unexpected tracked source changes; refusing MIDL reconciliation.' }
New-Item -ItemType Directory -Force -Path $EvidenceDirectory | Out-Null
$records = @()
foreach ($name in $changed) {
    $original = @(& git -C $root show ("HEAD:" + $name))
    if ($LASTEXITCODE -ne 0) { throw "Cannot read tracked MIDL file: $name" }
    $path = Join-Path $root $name
    $generated = @([IO.File]::ReadAllLines($path))
    if ($original.Count -ne $generated.Count) { throw "MIDL file has unexpected line count: $name" }
    $differences = 0
    for ($i = 0; $i -lt $original.Count; $i++) {
        if ($original[$i] -ceq $generated[$i]) { continue }
        if ($original[$i] -cne '/* at Tue Jan 19 06:14:07 2038' -or $generated[$i] -cne '/* at Tue Jan 19 03:14:07 2038') {
            throw "MIDL file contains a non-timestamp change: $name line $($i + 1)"
        }
        $differences++
    }
    if ($differences -ne 1) { throw "MIDL file lacks the exact expected timestamp difference: $name" }
    $records += @{path=$name;generatedSha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash;changedLines=$differences}
}
@{status='verified';source=$head;allowedChange='MIDL timestamp comment only';files=$records} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'midl-reconciliation.json') -Encoding utf8
if ($changed.Count) {
    & git -C $root restore --source=HEAD --worktree -- @changed
    if ($LASTEXITCODE -ne 0) { throw 'Cannot restore verified MIDL comments.' }
}
& git -C $root -c diff.ignoreSubmodules=all diff --quiet HEAD --
if ($LASTEXITCODE -ne 0) { throw 'Tracked source worktree remains dirty after MIDL reconciliation.' }
