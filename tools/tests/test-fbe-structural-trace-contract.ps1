<# Guards the TSV emitted by the production StructuralTrace class. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'))

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-structural-trace-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $trace, $report = (Join-Path $directory 'trace.tsv'), (Join-Path $directory 'report.tsv')
    $oldMode, $oldScenario, $oldTrace = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_TRACE
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'structural-trace-contract'; $env:FBE_NEXT_TEST_STRUCTURE_TRACE = $trace
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit(30000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out while writing StructuralTrace.' }
        if($process.ExitCode -ne 0) { throw "FBE failed while writing StructuralTrace: exit $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_TRACE = $oldMode, $oldScenario, $oldTrace }
    $rows = Import-Csv -LiteralPath $trace -Delimiter "`t"
    if(@($rows).Count -ne 3) { throw 'Production StructuralTrace record count is wrong.' }
    foreach($column in @('timestamp','operation','backend','case','phase','event','hresult','details')) { if(-not ($rows[0].PSObject.Properties.Name -contains $column)) { throw "StructuralTrace TSV lacks column: $column" } }
    if($rows[0].operation -ne 'cite trace' -or $rows[0].case -ne 'case  trace' -or $rows[0].details -ne 'TAB CR LF ' -or $rows[0].hresult -ne 'NA') { throw 'Production StructuralTrace sanitization contract is wrong.' }
    if($rows[2].event -ne 'failure' -or $rows[2].hresult -ne '0x80070005') { throw 'Production StructuralTrace HRESULT contract is wrong.' }
    if((Import-Csv -LiteralPath $report -Delimiter "`t").enabled -ne '1') { throw 'Production StructuralTrace did not open the writable file.' }
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host 'Production StructuralTrace TSV contract passed.'
