<# XML declaration helper and real FBE Source -> Body -> Save regressions. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-xml-declaration-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
    if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
    function Assert-FbeEncodingRoundTrip([string] $fromEncoding, [string] $targetEncoding) {
        $fixture = Join-Path $directory ("fbe-" + $fromEncoding + '-to-' + $targetEncoding + '.fb2')
        $report = Join-Path $directory ("fbe-" + $fromEncoding + '-to-' + $targetEncoding + '.txt')
        $reopenReport = Join-Path $directory ("fbe-" + $fromEncoding + '-to-' + $targetEncoding + '-reopen.txt')
        $xml = "<?xml version=`"1.0`" encoding=`"$fromEncoding`"?><FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><description><title-info><genre>prose</genre><author><first-name>Тест</first-name><last-name>Кодировки</last-name></author><book-title>Проверка</book-title><lang>ru</lang></title-info><document-info><id>source-encoding-$fromEncoding-$targetEncoding</id><version>1.0</version></document-info></description><body><section><p>Кириллица после Source</p></section></body></FictionBook>"
        [IO.File]::WriteAllBytes($fixture, [Text.Encoding]::GetEncoding($fromEncoding).GetBytes($xml))
        $previousMode, $previousScenario, $previousTarget = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_TARGET_ENCODING
        try {
            $env:FBE_NEXT_TEST_MODE = '1'
            $env:FBE_NEXT_TEST_SCENARIO = 'source-xml-declaration-encoding-runtime'
            $env:FBE_NEXT_TEST_TARGET_ENCODING = $targetEncoding
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -WorkingDirectory (Split-Path -Parent $FbeExe) -PassThru
            if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE не завершил encoding round-trip $fromEncoding -> $targetEncoding." }
            if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { $details = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report missing>' }; throw "FBE encoding round-trip $fromEncoding -> $targetEncoding failed: exit $($process.ExitCode).`n$details" }
            $env:FBE_NEXT_TEST_SCENARIO = 'source-xml-declaration-reopen-runtime'
            $reopened = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $reopenReport, $fixture) -WorkingDirectory (Split-Path -Parent $FbeExe) -PassThru
            if (-not $reopened.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $reopened.Id -Force; throw "FBE не завершил повторное открытие $fromEncoding -> $targetEncoding." }
            if ($reopened.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $reopenReport)) { $details = if (Test-Path -LiteralPath $reopenReport) { Get-Content -LiteralPath $reopenReport -Raw } else { '<report missing>' }; throw "FBE reopen $fromEncoding -> $targetEncoding failed: exit $($reopened.ExitCode).`n$details" }
        }
        finally {
            $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_TARGET_ENCODING = $previousMode, $previousScenario, $previousTarget
        }
        $rows = @{}; foreach ($line in Get-Content -LiteralPath $report) { $parts = $line -split '=', 2; if ($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
        foreach ($key in @('source_edited', 'body', 'saved')) { if ($rows[$key] -ne '1') { throw "FBE encoding round-trip $fromEncoding -> $targetEncoding failed: $key=$($rows[$key])" } }
        $reopenRows = @{}; foreach ($line in Get-Content -LiteralPath $reopenReport) { $parts = $line -split '=', 2; if ($parts.Count -eq 2) { $reopenRows[$parts[0]] = $parts[1] } }
        foreach ($key in @('reopened', 'source_declaration')) { if ($reopenRows[$key] -ne '1') { throw "FBE reopen $fromEncoding -> $targetEncoding failed: $key=$($reopenRows[$key])" } }
        $bytes = [IO.File]::ReadAllBytes($fixture)
        $savedXml = [Text.Encoding]::GetEncoding($targetEncoding).GetString($bytes)
        if ($savedXml -notmatch ('^<\?xml\s+[^?]*encoding\s*=\s*["'']' + [regex]::Escape($targetEncoding) + '["''][^?]*\?>')) { throw "Saved $targetEncoding file does not declare its actual encoding." }
        if ($savedXml -notmatch '<p>Кириллица после Source</p>') { throw "Saved $targetEncoding file cannot be decoded using its XML declaration." }
    }

    Assert-FbeEncodingRoundTrip 'windows-1251' 'utf-8'
    Assert-FbeEncodingRoundTrip 'utf-8' 'windows-1251'
    $exe = Join-Path $directory 'xml-declaration-test.exe'
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /I (Join-Path $root 'src\fbe') "/Fo$directory\\" (Join-Path $root 'tools\tests\xml-declaration-test.cpp') /Fe$exe
    if ($LASTEXITCODE -ne 0) { throw 'XML declaration helper compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'XML declaration helper regression failed.' }
    Write-Host 'XML declaration helper and FBE integration regressions passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
