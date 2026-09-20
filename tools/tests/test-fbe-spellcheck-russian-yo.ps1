[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-spellcheck-russian-yo-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $fixture = Join-Path $directory 'russian-yo.fb2'
    $report = Join-Path $directory 'spellcheck-russian-yo.tsv'
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Russian yo spellcheck</book-title><lang>ru</lang></title-info><document-info><program-used>test</program-used><id>spellcheck-russian-yo</id><version>1.0</version></document-info></description><body><section><p>ёжик</p></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'spellcheck-russian-yo'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out during Russian ё→е spellcheck runtime test.' }
        if ($process.ExitCode -ne 0) { throw "FBE Russian ё→е spellcheck runtime test failed: exit $($process.ExitCode)." }
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $expected = @('lower-e', 'lower-yo', 'upper-e', 'upper-yo', 'accent-e', 'accent-yo')
    $actualCases = (@($rows.case | Sort-Object) -join ',')
    $expectedCases = (@($expected | Sort-Object) -join ',')
    $failedCases = @($rows | Where-Object { $_.spell_result -ne '1' -or $_.expected -ne '1' })
    if ($rows.Count -ne $expected.Count -or $actualCases -ne $expectedCases -or $failedCases.Count -ne 0) {
        throw "Russian ё→е SpellCheck production contract failed: $($rows | ConvertTo-Json -Compress)"
    }
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }

Write-Host 'Russian ё→е SpellCheck production runtime regression passed.'
