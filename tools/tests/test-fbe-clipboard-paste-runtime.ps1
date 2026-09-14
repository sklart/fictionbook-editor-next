<# Runs the production Body paste scenario.  The runtime harness owns a real
   CF_UNICODETEXT clipboard payload, invokes CFBEView::OnPaste once, and
   reports the resulting text and normalized DOM. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-clipboard-paste-runtime-' + [guid]::NewGuid().ToString('N'))
$fixture = Join-Path $directory 'clipboard-paste.fb2'
$report = Join-Path $directory 'clipboard-paste.tsv'
try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Test</last-name></author><book-title>Clipboard Paste</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>clipboard-paste-test</id><version>1.0</version></document-info></description><body><section><p>Before paste</p></section></body></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'visual-dom-normalizer'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -WindowStyle Hidden -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE clipboard paste runtime timed out.' }
        if($process.ExitCode -ne 0) { throw "FBE clipboard paste runtime exited $($process.ExitCode)." }
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $rows = Import-Csv -LiteralPath $report -Delimiter "`t"
    $paste = @($rows | Where-Object { $_.case -eq 'paste-normal' })
    if($paste.Count -ne 1 -or $paste[0].result -ne 'pass' -or $paste[0].nbsp -ne '1' -or $paste[0].exact_paragraphs -ne '1') {
        throw "Production clipboard paste did not preserve one normalized NBSP insertion: $($paste | ConvertTo-Json -Compress)"
    }
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
Write-Host 'Clipboard paste production runtime passed.'
