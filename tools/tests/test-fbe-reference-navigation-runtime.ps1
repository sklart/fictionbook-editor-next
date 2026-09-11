<# Exercises the production CFBEView footnote/reference command path on MSHTML. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-reference-navigation-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $fixture = Join-Path $directory 'references.fb2'
    $report = Join-Path $directory 'references.tsv'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Reference navigation</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>reference-navigation-test</id><version>1.0</version></document-info></description><body><section><p>text <a l:href="#note-1" type="note"><strong>1</strong></a></p></section></body><body name="notes"><section id="note-1"><title><p>1</p></title><p>Note body</p></section></body></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'reference-navigation-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out during reference navigation runtime test.' }
        if($process.ExitCode -ne 0) { throw "FBE reference navigation runtime test failed: exit $($process.ExitCode)." }
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $row = Import-Csv -LiteralPath $report -Delimiter "`t"
    if(@($row).Count -ne 1 -or $row.footnote_check -ne '1' -or $row.footnote_target -ne '1' -or $row.reference_check -ne '1' -or $row.reference_target -ne '1' -or $row.check_unchanged -ne '1' -or $row.dom_unchanged -ne '1' -or $row.result -ne 'pass') { throw "Reference navigation runtime contract failed: $($row | ConvertTo-Json -Compress)" }
    Write-Host 'Reference navigation production runtime passed.'
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
