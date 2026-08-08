<#
.SYNOPSIS
Verifies that real COM exception payloads are represented by metadata only.
#>
[CmdletBinding()]
param([string]$Configuration = 'Release', [int]$TimeoutSeconds = 25)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$executable = Join-Path $repoRoot "out\$Configuration\FBE.exe"
if(-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "FBE executable was not found: $executable" }
if(Get-Process FBE -ErrorAction SilentlyContinue) { throw 'Close all FBE instances before running the COM privacy test.' }

$markers = @('BOOK_PRIVATE_PLAIN_TEXT_MARKER', 'BOOK_PRIVATE_XML_<tag>', 'BASE64_PRIVATE_MARKER', 'C:\Users\PrivateUser\secret.fb2', '\\server\private\book.fb2', 'file:///C:/private/book.fb2', 'field=value; level=info')
$directories = @((Join-Path $env:LOCALAPPDATA 'FBE Next\Diagnostics'), (Join-Path $env:TEMP 'FBE Next Diagnostics'))
$traceRegistryPath = 'HKCU:\Software\FBETeam\FictionBook Editor Next\Diagnostics'
$traceRegistryValue = 'TraceNextLaunch'
$previousTrace = Get-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -ErrorAction SilentlyContinue
$hadPreviousTrace = $null -ne $previousTrace
if($hadPreviousTrace) { Remove-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue }
$started = Get-Date
$process = $null
try {
    $env:FBE_NEXT_TRACE = '1'; $env:FBE_NEXT_TRACE_VERBOSE = '1'; $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_FAULT_INJECT = 'api-load-private-exception'
    $process = Start-Process -FilePath $executable -WorkingDirectory (Split-Path $executable) -PassThru
    $deadline = $started.AddSeconds($TimeoutSeconds); $trace = $null
    do {
        Start-Sleep -Milliseconds 200
        $traces = @(foreach($directory in $directories) { if(Test-Path -LiteralPath $directory) { Get-ChildItem -LiteralPath $directory -Filter ("fbe-trace-*-pid{0}*.log" -f $process.Id) -File | Where-Object { $_.LastWriteTimeUtc -ge $started.ToUniversalTime().AddSeconds(-2) } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1 } })
        $trace = $traces | Select-Object -First 1
        if($trace -and (Select-String -LiteralPath $trace.FullName -SimpleMatch 'code=J105' -Quiet)) { break }
        $process.Refresh()
    } while((Get-Date) -lt $deadline -and -not $process.HasExited)
    if(-not $trace) { throw 'No trace was created for the private COM exception fault.' }
    $content = Get-Content -Raw -LiteralPath $trace.FullName
    foreach($field in @('hr=0x', 'operation=', 'description-present=', 'description-length=')) { if($content.IndexOf($field, [StringComparison]::OrdinalIgnoreCase) -lt 0) { throw "Trace lacks required COM metadata '$field'." } }
    foreach($marker in $markers) { if($content.IndexOf($marker, [StringComparison]::OrdinalIgnoreCase) -ge 0) { throw "Private COM marker leaked into trace: $marker" } }
    Write-Host "COM exception privacy test passed: $($trace.FullName)"
}
finally {
    if($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit(10000) | Out-Null }
    Remove-Item Env:FBE_NEXT_TRACE,Env:FBE_NEXT_TRACE_VERBOSE,Env:FBE_NEXT_TEST_MODE,Env:FBE_NEXT_FAULT_INJECT -ErrorAction SilentlyContinue
    if($hadPreviousTrace) { New-Item -Path $traceRegistryPath -Force | Out-Null; New-ItemProperty -LiteralPath $traceRegistryPath -Name $traceRegistryValue -PropertyType DWord -Value ([int]$previousTrace.$traceRegistryValue) -Force | Out-Null }
}
