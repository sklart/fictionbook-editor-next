$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$hta = Join-Path $repoRoot 'runtime\Utilities\Save Sections As Separate Documents\SaveSectionsAsSeparateDocuments.hta'
$runner = Join-Path $repoRoot 'tools\tests\save-sections-behavior.js'
$source = Get-Content -Raw -LiteralPath $hta
foreach ($pattern in @('validateSavedFictionBook', 'XMLSchemaCache\.6\.0', 'FictionBook\.xsd', 'FBE_NEXT_TEST_MODE', 'SAVE_SECTIONS_FAIL_REPLACE')) {
    if ($source -notmatch $pattern) { throw "Не найдена обязательная защита Save Sections: $pattern" }
}
if ($source -match 'DeleteFile\(pathForSaving') { throw 'Целевой файл нельзя удалять до успешной замены.' }
if ($source -notmatch 'while \(elementList\.hasChildNodes\(\)\)') { throw 'Список разделов должен вызывать hasChildNodes() перед удалением узла.' }

function Assert-Fb2Schema([string]$Path) {
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $repoRoot 'runtime\FictionBook.xsd'))
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false
    if (-not $document.load($Path)) { throw "MSXML не прочитал Save Sections output: $($document.parseError.reason)" }
    $document.schemas = $cache
    $validation = $document.validate()
    if ($validation.errorCode -ne 0) { throw "Save Sections output не проходит FictionBook.xsd: $($validation.reason)" }
}

function Invoke-Wsh([string]$StatusPath, [string[]]$Arguments) {
    # cscript.exe //U has demonstrated unreliable propagation of
    # WScript.Quit() through redirected Process execution on supported
    # Windows environments. The explicit harness status is therefore the
    # authoritative result of the tested operation; Process.ExitCode remains
    # diagnostic only.
    if (Test-Path -LiteralPath $StatusPath) { Remove-Item -LiteralPath $StatusPath -Force }
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'cscript.exe'
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = [Text.Encoding]::Unicode
    $startInfo.StandardErrorEncoding = [Text.Encoding]::Unicode
    foreach ($argument in $Arguments) { [void]$startInfo.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $processExitCode = $process.ExitCode
    if (-not (Test-Path -LiteralPath $StatusPath)) {
        throw "WSH harness did not produce a status file. Process exit code: $processExitCode`nstdout: $stdout`nstderr: $stderr"
    }
    $statusLines = @(Get-Content -LiteralPath $StatusPath -Encoding UTF8)
    if ($statusLines.Count -lt 2 -or $statusLines[0] -notmatch '^\d+$') {
        throw "WSH harness produced an invalid status file. Process exit code: $processExitCode`nstdout: $stdout`nstderr: $stderr"
    }
    [pscustomobject]@{
        StatusCode = [int]$statusLines[0]
        Message = ($statusLines[1..($statusLines.Count - 1)] -join "`n")
        ProcessExitCode = $processExitCode
        StdOut = $stdout
        StdErr = $stderr
    }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-save-sections-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $fixture = Join-Path $directory 'source.fb2'
    @'
<?xml version="1.0" encoding="UTF-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Save Sections regression</book-title><lang>en</lang></title-info><document-info><author><first-name>T</first-name><last-name>T</last-name></author><program-used>test</program-used><date>2026-08-11</date><id>save-sections-test</id><version>1.0</version></document-info></description><body><section><p>Expected section text.</p></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8

    $successDestination = Join-Path $directory 'success.fb2'
    $successStatus = Join-Path $directory 'success.status'
    [IO.File]::WriteAllText($successDestination, 'old destination', [Text.Encoding]::ASCII)
    $successResult = Invoke-Wsh $successStatus @('//U', '//nologo', $runner, $hta, $fixture, $successDestination, $successStatus)
    if ($successResult.StatusCode -ne 0) {
        throw "Save Sections success path returned status code $($successResult.StatusCode) (process exit code $($successResult.ProcessExitCode)).`nmessage: $($successResult.Message)`nstdout: $($successResult.StdOut)`nstderr: $($successResult.StdErr)"
    }
    if (-not (Test-Path -LiteralPath $successDestination)) { throw 'Save Sections не создал целевой файл.' }
    if ((Get-Content -Raw -LiteralPath $successDestination) -notmatch 'Expected section text') { throw 'Save Sections output не содержит ожидаемые данные.' }
    Assert-Fb2Schema $successDestination
    if (Get-ChildItem -LiteralPath $directory -Filter '.save-sections-*.tmp*' -Force) { throw 'После успешной замены остались временные или parked файлы.' }

    $failureDestination = Join-Path $directory 'failure.fb2'
    $failureStatus = Join-Path $directory 'failure.status'
    [IO.File]::WriteAllText($failureDestination, 'known old destination', [Text.Encoding]::ASCII)
    $oldHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $failureDestination).Hash
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:SAVE_SECTIONS_FAIL_REPLACE = '1'
    try {
        $failureResult = Invoke-Wsh $failureStatus @('//U', '//nologo', $runner, $hta, $fixture, $failureDestination, $failureStatus)
    }
    finally { Remove-Item Env:FBE_NEXT_TEST_MODE -ErrorAction SilentlyContinue; Remove-Item Env:SAVE_SECTIONS_FAIL_REPLACE -ErrorAction SilentlyContinue }
    if (-not (Test-Path -LiteralPath $failureDestination)) { throw 'Rollback не восстановил исходный целевой файл.' }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $failureDestination).Hash -ne $oldHash) { throw 'Rollback изменил исходный целевой файл.' }
    if ($failureResult.StatusCode -eq 0) {
        throw "Injected Save Sections replacement failure unexpectedly succeeded (status code 0; process exit code $($failureResult.ProcessExitCode)).`nmessage: $($failureResult.Message)`nstdout: $($failureResult.StdOut)`nstderr: $($failureResult.StdErr)"
    }
    if (Get-ChildItem -LiteralPath $directory -Filter '.save-sections-*.tmp*' -Force) { throw 'После rollback остались временные или parked файлы.' }
}
finally {
    Remove-Item Env:FBE_NEXT_TEST_MODE -ErrorAction SilentlyContinue
    Remove-Item Env:SAVE_SECTIONS_FAIL_REPLACE -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $directory) { Remove-Item -LiteralPath $directory -Recurse -Force }
}
$global:LASTEXITCODE = 0
Write-Host 'Save Sections behavioural safe replacement regression passed.'
