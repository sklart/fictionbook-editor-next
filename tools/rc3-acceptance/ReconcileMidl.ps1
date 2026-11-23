[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot, [Parameter(Mandatory)][string]$EvidenceDirectory, [string]$ExpectedSource = 'c4bcb76c3cd03d1dff88fa557aa75261b4ec0461')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -cne $ExpectedSource) { throw 'Unexpected source SHA.' }
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
    # ReadAllLines alone ignores BOM and line-ending changes. Replace only the
    # verified ASCII timestamp in the raw bytes and compare the complete Git
    # blob, including its original encoding and line endings.
    $trackedBlob = (& git -C $root rev-parse ("HEAD:" + $name)).Trim()
    if ($LASTEXITCODE -ne 0 -or $trackedBlob -notmatch '^[0-9a-f]{40}$') { throw "Cannot identify tracked MIDL blob: $name" }
    $latin1 = [Text.Encoding]::GetEncoding(28591)
    $raw = $latin1.GetString([IO.File]::ReadAllBytes($path))
    $normalized = $raw.Replace('/* at Tue Jan 19 03:14:07 2038', '/* at Tue Jan 19 06:14:07 2038')
    $bytes = $latin1.GetBytes($normalized)
    $header = [Text.Encoding]::ASCII.GetBytes("blob $($bytes.Length)`0")
    $stream = [IO.MemoryStream]::new()
    try {
        $stream.Write($header, 0, $header.Length)
        $stream.Write($bytes, 0, $bytes.Length)
        $blob = [Convert]::ToHexString([Security.Cryptography.SHA1]::HashData($stream.ToArray())).ToLowerInvariant()
    } finally { $stream.Dispose() }
    if ($blob -cne $trackedBlob) { throw "MIDL file contains a non-timestamp byte change: $name" }
    $records += @{path=$name;generatedSha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash;changedLines=$differences}
}
$report = @{status='verified-differences';source=$head;allowedChange='MIDL timestamp comment only';files=$records}
$reportPath = Join-Path $EvidenceDirectory 'midl-reconciliation.json'
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding utf8
if ($changed.Count) {
    & git -C $root restore --source=HEAD --worktree -- @changed
    if ($LASTEXITCODE -ne 0) { throw 'Cannot restore verified MIDL comments.' }
}
& git -C $root -c diff.ignoreSubmodules=all diff --quiet HEAD --
if ($LASTEXITCODE -ne 0) { throw 'Tracked source worktree remains dirty after MIDL reconciliation.' }
$report.status = 'restored-clean'
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding utf8
