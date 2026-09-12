<# Exercises production MSHTML link resolution, navigation and live return history. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-link-navigation-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $fixture = Join-Path $directory 'links.fb2'
    $report = Join-Path $directory 'links.tsv'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Link navigation</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>link-navigation-test</id><version>1.0</version></document-info></description><body><section><p><a l:href="#note-1"><strong>internal nested</strong></a></p><p><a l:href="#note-1">second source</a></p><p><a l:href="#missing">broken</a></p><section id="note-1"><p>target</p></section></section></body></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'link-navigation-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out during link navigation runtime test.' }
        if($process.ExitCode -ne 0) {
            $detail = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report unavailable>' }
            throw "FBE link navigation runtime test failed: exit $($process.ExitCode). Report: $detail"
        }
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $row = Import-Csv -LiteralPath $report -Delimiter "`t"
    if(@($row).Count -ne 1 -or $row.nested -ne '1' -or $row.target -ne '1' -or $row.same_document -ne '1' -or $row.broken -ne '1' -or $row.returned_second -ne '1' -or $row.inserted_before -ne '1' -or $row.deleted_origin_fallback -ne '1' -or $row.document_replaced_fallback -ne '1' -or $row.unchanged -ne '1' -or $row.result -ne 'pass') { throw "Link navigation runtime contract failed: $($row | ConvertTo-Json -Compress)" }
    Write-Host 'Link navigation production runtime passed.'
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
