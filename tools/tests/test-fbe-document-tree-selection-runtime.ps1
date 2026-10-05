<# Exercises native multi-selection and current-item state in the real document tree. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-tree-selection-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $fixture = Join-Path $directory 'document-tree-selection.fb2'
    $report = Join-Path $directory 'document-tree-selection.txt'
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Tree selection</book-title><lang>en</lang></title-info><document-info><id>tree-selection-test</id><version>1.0</version></document-info></description>
  <body><section><title><p>One</p></title><p>First</p></section><section><title><p>Two</p></title><p>Second</p></section><section><title><p>Three</p></title><p>Third</p></section></body>
</FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'document-tree-selection-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил runtime-сценарий выбора дерева.' }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) {
            $diagnostics = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report missing>' }
            throw "FBE tree selection runtime scenario failed: exit $($process.ExitCode).`n$diagnostics"
        }
    } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario }
    $result = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split '=', 2; if($parts.Count -eq 2) { $result[$parts[0]] = $parts[1] } }
    foreach($key in @('multi', 'remove_one', 'select_all', 'current_in_selection')) {
        if($result[$key] -ne '1') { throw "Document Tree runtime check failed: $key (value '$($result[$key])')." }
    }
    if($result['result'] -ne 'pass') { throw "Document Tree runtime result: $($result['result'])" }
    Write-Host 'FBE Document Tree selection runtime passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
