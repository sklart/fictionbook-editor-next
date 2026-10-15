[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Directory,
    [Parameter(Mandatory)][string]$ExpectedX86Sha256,
    [Parameter(Mandatory)][string]$ExpectedX64Sha256,
    [Parameter(Mandatory)][string]$ExpectedSchemaSha256,
    [Parameter(Mandatory)][string]$StatusFilePath
)
$ErrorActionPreference = 'Stop'
$sessionDirectory=Split-Path -Parent $MyInvocation.MyCommand.Path
$sessionLease=$null
if([IO.File]::Exists((Join-Path $sessionDirectory 'Session.ini'))){
    . (Join-Path $sessionDirectory 'shell-integration-lifecycle.ps1')
    $sessionLease=Open-ShellMaintenanceLease $sessionDirectory
}

function Write-UpdateStatus([string]$Result, [string]$Message) {
    [IO.File]::WriteAllLines($StatusFilePath, [string[]]@('[Payload]', "Result=$Result", "Message=$Message"), [Text.Encoding]::Unicode)
}
function Test-OrdinaryFile([string]$Path) {
    if ([IO.File]::Exists($Path) -and (([IO.File]::GetAttributes($Path) -band [IO.FileAttributes]::ReparsePoint) -ne 0)) { throw "Reparse payload rejected: $Path" }
}
$changed = New-Object System.Collections.ArrayList
$names = @('FBShell.dll','FBShell64.dll','FBE.Sequence.propdesc')
$hashes = @($ExpectedX86Sha256,$ExpectedX64Sha256,$ExpectedSchemaSha256)
try {
    $current = New-Object IO.DirectoryInfo -ArgumentList ([IO.Path]::GetFullPath($Directory))
    while ($null -ne $current) {
        if (($current.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse directory rejected: $($current.FullName)" }
        $current = $current.Parent
    }
    for ($i=0; $i -lt $names.Length; ++$i) {
        if ($hashes[$i] -notmatch '^[0-9a-fA-F]{64}$') { throw "Missing package SHA-256: $($names[$i])" }
        $path = Join-Path $Directory ($names[$i] + '.new')
        Test-OrdinaryFile $path
        Test-OrdinaryFile (Join-Path $Directory $names[$i])
        $stream = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        try {
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
            finally { $sha.Clear() }
        }
        finally { $stream.Close() }
        if (-not [string]::Equals($hash,$hashes[$i],[StringComparison]::OrdinalIgnoreCase)) { throw "Package SHA-256 mismatch: $path" }
    }
    foreach ($name in $names) {
        $target = Join-Path $Directory $name
        $staged = $target + '.new'
        $backup = $target + '.backup-' + [Guid]::NewGuid().ToString('N')
        $existed = [IO.File]::Exists($target)
        if ($existed) { [IO.File]::Replace($staged,$target,$backup) }
        else { [IO.File]::Move($staged,$target) }
        $null = $changed.Add(@($target,$backup,$existed))
    }
    Write-UpdateStatus 'ok' 'Package payload installed immediately.'
    foreach ($item in $changed) {
        if ($item[2]) {
            try { [IO.File]::Delete($item[1]) }
            catch { Write-Host "Previous payload remains in $($item[1]); it may still be open." }
        }
    }
}
catch {
    $failure = $_.Exception
    while ($null -ne $failure.InnerException) { $failure = $failure.InnerException }
    $rollbackFailed = $false
    for ($i=$changed.Count-1; $i -ge 0; --$i) {
        $item = $changed[$i]
        try {
            if ($item[2]) {
                $discard = $item[0] + '.failed-' + [Guid]::NewGuid().ToString('N')
                [IO.File]::Replace($item[1],$item[0],$discard)
                [IO.File]::Delete($discard)
            }
            else { [IO.File]::Delete($item[0]) }
        }
        catch { $rollbackFailed=$true; Write-Host "Could not restore $($item[0]); retained backup $($item[1]): $($_.Exception.Message)" }
    }
    $code = [Runtime.InteropServices.Marshal]::GetHRForException($failure) -band 65535
    if (-not $rollbackFailed -and ($code -eq 32 -or $code -eq 33 -or $code -eq 1224)) {
        Write-UpdateStatus 'pending' 'Existing payload and registration retained. Close Explorer or restart Windows, then run this installer again.'
    }
    else {
        Write-UpdateStatus 'error' ("Payload replacement failed (code=$code; rollbackFailed=$rollbackFailed): " + $failure.Message)
        throw
    }
}
