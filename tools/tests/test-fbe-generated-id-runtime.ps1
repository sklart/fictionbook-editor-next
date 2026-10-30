[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$FbeExe = (Resolve-Path -LiteralPath $FbeExe).Path
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-generated-id-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $fixture = Join-Path $directory 'id.fb2'; $report = Join-Path $directory 'report.txt'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Generated ID</book-title><lang>en</lang></title-info><document-info><id>original-id</id><version>1.0</version></document-info></description><body><section><p>Body</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'generated-id-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Generated ID runtime timed out.' }
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report missing>' }
        if($process.ExitCode -ne 0) { throw "Generated ID runtime failed ($($process.ExitCode)):`n$text" }
        foreach($key in @('initially-clean','canceled-clean','generated','dirty','source-current','saved','clean-after-save','persisted')) { if($text -notmatch "(?m)^$key=1$") { throw "Generated ID runtime failed ($key):`n$text" } }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    Write-Host 'Generated ID runtime checks passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
