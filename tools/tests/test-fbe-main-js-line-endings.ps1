$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$bytes = [IO.File]::ReadAllBytes((Join-Path $root 'runtime\main.js'))
for($index = 0; $index -lt $bytes.Length; ++$index) {
    if($bytes[$index] -eq 10 -and ($index -eq 0 -or $bytes[$index - 1] -ne 13)) { throw "runtime/main.js contains a non-CRLF line ending at byte $index." }
}
Write-Host 'Canonical runtime/main.js uses CRLF line endings.'
