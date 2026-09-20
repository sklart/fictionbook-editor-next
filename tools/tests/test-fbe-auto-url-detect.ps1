param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'

$viewSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBEview.cpp') -Raw

if ($viewSource -notmatch 'execCommand\(L"AutoUrlDetect",\s*VARIANT_FALSE,\s*_variant_t\(VARIANT_FALSE\)\)') {
    throw 'MSHTML automatic URL detection is not disabled during visual editor initialization.'
}

if ($viewSource -notmatch 'IDM_AUTOURLDETECT_MODE') {
    throw 'The compatibility fallback for MSHTML AutoUrlDetect is missing.'
}

$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-auto-url-detect-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $fixture = Join-Path $directory 'auto-url.fb2'
    $report = Join-Path $directory 'auto-url.tsv'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Auto URL detect</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>auto-url-detect-test</id><version>1.0</version></document-info></description><body><section><p>\\слово</p><p>\\server\share</p><p>C:\Books\book.fb2</p><p>http://example.org</p><p>https://example.org</p><p>user@example.org</p><p id="auto-url-manual">manual-link</p></section></body></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'auto-url-detect-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out during AutoUrlDetect runtime test.' }
        if ($process.ExitCode -ne 0) {
            $detail = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report unavailable>' }
            throw "FBE AutoUrlDetect runtime test failed: exit $($process.ExitCode). Report: $detail"
        }
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $row = Import-Csv -LiteralPath $report -Delimiter "`t"
    if (@($row).Count -ne 1 -or $row.initial -ne '1' -or $row.source_roundtrip -ne '1' -or $row.saved_reopened -ne '1' -or $row.undo -ne '1' -or $row.redo -ne '1' -or $row.manual_link -ne '1' -or $row.result -ne 'pass') {
        throw "AutoUrlDetect runtime contract failed: $($row | ConvertTo-Json -Compress)"
    }
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }

Write-Host 'MSHTML automatic URL detection runtime regression passed.'
