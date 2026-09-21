<#
.SYNOPSIS
Runs isolated real-MSHTML undo probes for image import components.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateSet('binary', 'binary-fill', 'image', 'image-start', 'image-end', 'image-middle', 'image-hold-start', 'image-clear-start', 'image-discard-start', 'image-inspect-release-start', 'image-inline', 'image-no-url', 'plain-block', 'plain-block-after', 'plain-block-auto', 'plain-block-markup', 'plain-block-custom', 'plain-block-adjacent-html', 'plain-block-adjacent-html-after', 'plain-block-adjacent-html-image', 'plain-block-adjacent-html-image-after', 'plain-block-range-html', 'plain-block-range-html-after', 'fbe285-start', 'fbe285-end', 'fbe285-middle', 'fbe285-return-hold-start', 'fbe285-dispatch-full-start', 'fbe285-dispatch-full-invoke2-start', 'fbe285-dispatch-dom-start', 'fbe285-exec-full-start', 'fbe285-exec-split-start', 'call-insimage-empty', 'fbe285-bisect-full', 'fbe285-bisect-no-normalize', 'fbe285-bisect-no-whole', 'fbe285-bisect-no-move-to-element', 'fbe285-bisect-no-duplicate', 'fbe285-bisect-no-set-endpoint', 'fbe285-bisect-no-text-read', 'fbe285-bisect-no-whole-no-normalize', 'fbe285-bisect-no-whole-unconditional', 'fbe285-bisect-no-whole-if-true', 'fbe285-bisect-compile-label-unconditional', 'fbe285-bisect-minimal', 'fbe285-bisect-compile-exact', 'body-cache-cached', 'body-cache-fresh', 'body-cache-production-undo')]
    [string]$Probe = 'binary',
    [ValidateSet('plain', 'formatted', 'nested', 'id')]
    [string]$Fixture = 'plain',
    [ValidateSet(0, 1, 2, 5)]
    [int]$UndoRedoCycles = 5,
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }

function Get-ProbeSaveTrace([int]$FbeProcessId) {
    $directory = Join-Path $env:LOCALAPPDATA 'FBE Next\Diagnostics'
    if (-not (Test-Path -LiteralPath $directory)) { return 'trace unavailable' }
    $files = Get-ChildItem -LiteralPath $directory -Filter "fbe-trace-*-pid$FbeProcessId*.log" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime
    if (-not $files) { return 'trace unavailable' }
    $lines = foreach ($file in $files) { Get-Content -LiteralPath $file.FullName -ErrorAction SilentlyContinue }
    $saveLines = $lines | Where-Object { $_ -match 'image-undo-save-probe|CreateDOM phase=' }
    if (-not $saveLines) { return 'save probe markers unavailable' }
    return (($saveLines | Select-Object -Last 30) -join "`n")
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-image-undo-probe-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$savedEnvironment = @{}
foreach ($name in 'FBE_NEXT_TEST_MODE', 'FBE_NEXT_TEST_SCENARIO', 'FBE_NEXT_TEST_IMAGE_PATH', 'FBE_NEXT_TEST_IMAGE_PROBE', 'FBE_NEXT_TEST_IMAGE_BINARY_ID', 'FBE_NEXT_TEST_IMAGE_UNDO_REDO_CYCLES', 'FBE_NEXT_TEST_IMAGE_FIXTURE', 'FBE_NEXT_TRACE') { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    Add-Type -AssemblyName System.Drawing
    $imagePath = Join-Path $directory 'undo-probe.jpg'
    $bitmap = [Drawing.Bitmap]::new(2, 2)
    try { $bitmap.SetPixel(0, 0, [Drawing.Color]::Blue); $bitmap.Save($imagePath, [Drawing.Imaging.ImageFormat]::Jpeg) } finally { $bitmap.Dispose() }
    $base64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($imagePath))
    $paragraph = switch ($Fixture) {
        'formatted' { '<p><emphasis>formatted text</emphasis> tail</p>' }
        'nested' { '<p><emphasis><strong>formatted text</strong></emphasis> tail</p>' }
        'id' { '<p id="keep-me"><emphasis>formatted text</emphasis> tail</p>' }
        default { '<p>probe paragraph</p>' }
    }
    $fixturePath = Join-Path $directory 'fixture.fb2'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Undo probe</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>undo-probe</id><version>1.0</version></document-info></description><body><section>$paragraph</section></body><binary id="existing-image" content-type="image/jpeg">$base64</binary></FictionBook>
"@ | Set-Content -LiteralPath $fixturePath -Encoding utf8
    $report = Join-Path $directory ($Probe + '.tsv')
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'image-undo-probe'
    $env:FBE_NEXT_TEST_IMAGE_PATH = $imagePath
    $env:FBE_NEXT_TEST_IMAGE_PROBE = $Probe
    $env:FBE_NEXT_TEST_IMAGE_BINARY_ID = 'existing-image'
    $env:FBE_NEXT_TEST_IMAGE_UNDO_REDO_CYCLES = $UndoRedoCycles
    $env:FBE_NEXT_TEST_IMAGE_FIXTURE = $Fixture
    $env:FBE_NEXT_TRACE = '1'
    $process = Start-Process -FilePath $FbeExe -WorkingDirectory $directory -ArgumentList @('-b', $report, $fixturePath) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $timeoutPhases = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { 'report unavailable' }
        Stop-Process -Id $process.Id -Force
        $saveTrace = Get-ProbeSaveTrace $process.Id
        throw "${Probe}: FBE не завершился. Последняя фаза: $timeoutPhases`nПоследние Save-маркеры:`n$saveTrace"
    }
    $phases = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { 'report unavailable' }
    $saveTrace = Get-ProbeSaveTrace $process.Id
    if ($process.ExitCode -ne 0) { throw "${Probe}: FBE завершился с $($process.ExitCode). Последняя фаза: $phases`nПоследние Save-маркеры:`n$saveTrace" }
    if ($UndoRedoCycles -gt 0 -and $phases -notmatch 'undo-complete') { throw "${Probe}: Undo не завершился. Фазы: $phases" }
    if ($UndoRedoCycles -gt 0 -and ((($Probe -match '^image') -and $Probe -ne 'image-no-url') -or $Probe -match '^plain-block' -or $Probe -match '^fbe285')) { if ($phases -notmatch "redo-complete-$UndoRedoCycles") { throw "${Probe}: Redo не завершился. Фазы: $phases" } }
    if ($Probe -match '^fbe285-bisect-') {
        if ($phases -notmatch 'bisect-complete') { throw "${Probe}: bisect не завершился. Фазы: $phases" }
    } elseif ($Probe -match '^body-cache-') {
        if ($phases -notmatch 'body-cache-complete') { throw "${Probe}: body-cache probe не завершился. Фазы: $phases" }
    } elseif ($phases -notmatch 'idle-complete|save-complete') { throw "${Probe}: idle/Save не завершились. Фазы: $phases" }
    if ($Probe -in 'image', 'image-start', 'image-end', 'image-middle', 'image-hold-start', 'image-clear-start', 'image-discard-start', 'image-inspect-release-start') {
        $expectedPosition = switch ($Probe) { 'image-end' { 'end' } 'image-middle' { 'middle' } default { 'start' } }
        [xml]$savedDocument = Get-Content -LiteralPath $fixturePath -Raw
        $section = $savedDocument.SelectSingleNode('//*[local-name()="body"]/*[local-name()="section"]')
        $children = @($section.ChildNodes | Where-Object { $_.NodeType -eq [System.Xml.XmlNodeType]::Element })
        $actualOrder = ($children | ForEach-Object { $_.LocalName }) -join '|'
        $expectedOrder = switch ($expectedPosition) { 'start' { 'image|p' } 'end' { 'p|image' } default { 'p|image|p' } }
        if ($actualOrder -ne $expectedOrder) { throw "${Probe}: неверный порядок после Save: $actualOrder (ожидался $expectedOrder)" }
        $image = $section.SelectSingleNode('./*[local-name()="image"]')
        $href = @($image.Attributes | Where-Object { $_.LocalName -eq 'href' })[0].Value
        if ($href -ne '#existing-image') { throw "${Probe}: неверный href: $href" }
        if (-not $savedDocument.SelectSingleNode('//*[local-name()="binary" and @id="existing-image"]')) { throw "${Probe}: binary existing-image не сохранён" }
        if ($Fixture -eq 'id' -and -not $section.SelectSingleNode('./*[local-name()="p" and @id="keep-me"]')) { throw "${Probe}: id keep-me не сохранён" }
        if ($phases -notmatch 'reopen-complete') { throw "${Probe}: Save → Reopen не прошёл. Фазы: $phases" }
        Write-Host "${Probe}: real-MSHTML Undo/Redo и Save → Reopen passed.`n$phases"
    } else {
        Write-Host "${Probe}: real-MSHTML undo probe passed.`n$phases"
    }
}
finally {
    foreach ($name in $savedEnvironment.Keys) { if ($null -eq $savedEnvironment[$name]) { Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue } else { Set-Item -Path "Env:$name" -Value $savedEnvironment[$name] } }
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
