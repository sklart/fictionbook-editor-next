<# Exercises the actual MSHTML BODY -> SOURCE -> BODY -> Save -> Reopen path for #undefined images. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 120)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-empty-image-placeholder-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $fixture = Join-Path $directory 'empty-image.fb2'; $report = Join-Path $directory 'runtime.txt'
    @'
<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Empty image runtime</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>empty-image-runtime</id><version>1.0</version></document-info></description><body><section><image l:href="#undefined"/><p>Inline <image l:href="#undefined"/> image.</p></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'empty-image-placeholder-runtime'
        $process = Start-Process -FilePath $FbeExe -WorkingDirectory (Split-Path $FbeExe) -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил empty-image placeholder scenario.' }
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report missing>' }
        if($process.ExitCode -ne 0) { throw "Runtime placeholder regression failed: $text" }
        foreach($entry in 'body_open=1','source=1','body_after_source=1','saved=1','reopened=1','body_after_reopen=1') { if($text -notmatch [regex]::Escape($entry)) { throw "Не пройдена runtime-фаза ${entry}: $text" } }
    } finally { $env:FBE_NEXT_TEST_MODE=$savedMode; $env:FBE_NEXT_TEST_SCENARIO=$savedScenario }
    $saved = Get-Content -LiteralPath $fixture -Raw
    if(([regex]::Matches($saved, '(?:xlink|l):href="#undefined"')).Count -ne 2 -or $saved -match 'fbw-internal:#"') { throw 'Save/Reopen нарушил семантику #undefined.' }
    Write-Host 'Empty image placeholder MSHTML runtime round-trip passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
