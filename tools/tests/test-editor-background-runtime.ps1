<#
.SYNOPSIS
Runs FBE.exe's unattended editor-background scenario against the real MSHTML DOM.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 180,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }

function Assert-Phase($rows, [string]$phase, [scriptblock]$check) {
    $row = @($rows | Where-Object phase -eq $phase)
    if($row.Count -ne 1) { throw "Отчёт FBE не содержит ровно одну фазу '$phase'." }
    & $check $row[0]
}
function Assert-ColorOnly($row, [string]$phase) {
    if($row.image -notmatch '^(none|)$' -or $row.attachment -notmatch '^(scroll|)$') { throw "$phase должен оставить только ColorBG, получено image='$($row.image)', attachment='$($row.attachment)'." }
    if([int]$row.modified -ne 0) { throw "$phase изменил modified-state документа." }
}
function Assert-Image($row, [string]$phase) {
    if($row.image -notmatch '^url\(' -or $row.attachment -ne 'fixed') { throw "$phase не применил фоновое изображение к MSHTML: '$($row.image)' / '$($row.attachment)'." }
    if([int]$row.modified -ne 0) { throw "$phase изменил modified-state документа." }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-editor-background-runtime-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
$fixture = Join-Path $directory 'background.fb2'
$png = Join-Path $directory 'Фоны FBE # % (тест).png'
$missing = Join-Path $directory 'нет # % (фон).png'
$builtin = Join-Path (Split-Path $FbeExe -Parent) 'EditorBackgrounds\01_clean_white.png'
$completed = $false
try {
    if(-not (Test-Path -LiteralPath $builtin -PathType Leaf)) { throw "В staged runtime отсутствует built-in фон: $builtin" }
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Test</last-name></author><book-title>Background runtime</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>background-runtime-test</id><version>1.0</version></document-info></description><body><section><title><p>Background</p></title><p>Text must stay in FB2.</p></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    Copy-Item -LiteralPath $builtin -Destination $png
    $before = Get-Content -LiteralPath $fixture -Raw

    $oldMode, $oldScenario, $oldPath, $oldMissing, $oldHighContrast = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_BACKGROUND_PATH, $env:FBE_NEXT_TEST_BACKGROUND_MISSING_PATH, $env:FBE_NEXT_TEST_FORCE_HIGH_CONTRAST
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'editor-background-runtime'
        $env:FBE_NEXT_TEST_BACKGROUND_MISSING_PATH = $missing
        foreach($extension in @('.png', '.jpg', '.jpeg')) {
            $custom = [IO.Path]::ChangeExtension($png, $extension)
            if($custom -ne $png) { Copy-Item -LiteralPath $builtin -Destination $custom }
            $env:FBE_NEXT_TEST_BACKGROUND_PATH = $custom
            Remove-Item Env:FBE_NEXT_TEST_FORCE_HIGH_CONTRAST -ErrorAction SilentlyContinue
            $report = Join-Path $directory ("report$extension.tsv")
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE не завершил runtime-проверку $extension." }
            if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE не сформировал runtime-отчёт для $extension (exit=$($process.ExitCode))." }
            $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
            Assert-Phase $rows 'none' { param($r) Assert-ColorOnly $r 'none' }
            Assert-Phase $rows 'unknown-builtin' { param($r) Assert-ColorOnly $r 'unknown-builtin' }
            Assert-Phase $rows 'missing-custom' { param($r) Assert-ColorOnly $r 'missing-custom' }
            foreach($phase in @('builtin-tile', 'builtin-center', 'builtin-contain', 'builtin-cover', 'builtin-after-view-recreate', 'custom', 'before-save', 'after-save')) { Assert-Phase $rows $phase { param($r) Assert-Image $r $phase } }
            Assert-Phase $rows 'builtin-tile' { param($r) if($r.repeat -ne 'repeat' -or $r.size -ne 'auto') { throw 'tile не установил repeat/auto.' } }
            foreach($phase in @('builtin-center', 'builtin-contain', 'builtin-cover')) { Assert-Phase $rows $phase { param($r) if($r.repeat -ne 'no-repeat' -or $r.position -notmatch 'center') { throw "$phase не установил no-repeat/center." } } }
            Assert-Phase $rows 'builtin-contain' { param($r) if($r.size -ne 'contain') { throw 'contain не установил background-size.' } }
            Assert-Phase $rows 'builtin-cover' { param($r) if($r.size -ne 'cover') { throw 'cover не установил background-size.' } }
            Assert-Phase $rows 'custom' { param($r) if($r.image -notmatch '%20|%23|%25' -or $r.image -notmatch '\(' -or $r.image -notmatch '\)') { throw "U::UrlFromPath/CSS url не сохранил special-character URI: $($r.image)" } }
        }
        $env:FBE_NEXT_TEST_BACKGROUND_PATH = $png; $env:FBE_NEXT_TEST_FORCE_HIGH_CONTRAST = '1'
        $highContrastReport = Join-Path $directory 'report-high-contrast.tsv'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $highContrastReport, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил High Contrast runtime-проверку.' }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $highContrastReport)) { throw 'FBE не сформировал High Contrast runtime-отчёт.' }
        $highContrastRows = @(Import-Csv -LiteralPath $highContrastReport -Delimiter "`t")
        Assert-Phase $highContrastRows 'builtin-tile' { param($r) Assert-ColorOnly $r 'High Contrast builtin-tile' }
    } finally {
        foreach($entry in @(@('FBE_NEXT_TEST_MODE',$oldMode), @('FBE_NEXT_TEST_SCENARIO',$oldScenario), @('FBE_NEXT_TEST_BACKGROUND_PATH',$oldPath), @('FBE_NEXT_TEST_BACKGROUND_MISSING_PATH',$oldMissing), @('FBE_NEXT_TEST_FORCE_HIGH_CONTRAST',$oldHighContrast))) { if($null -eq $entry[1]) { Remove-Item ("Env:" + $entry[0]) -ErrorAction SilentlyContinue } else { Set-Item ("Env:" + $entry[0]) $entry[1] } }
    }
    [xml]$saved = Get-Content -LiteralPath $fixture -Raw
    $namespace = [Xml.XmlNamespaceManager]::new($saved.NameTable); $namespace.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    if($saved.SelectSingleNode('/fb:FictionBook/fb:body/fb:section/fb:p[.="Text must stay in FB2."]', $namespace) -eq $null -or @($saved.SelectNodes('//fb:binary', $namespace)).Count -ne 0) { throw 'Save изменил содержимое FB2 или добавил binary.' }
    $after = Get-Content -LiteralPath $fixture -Raw
    foreach($forbidden in @('EditorBackground', '01_clean_white', 'background-image', 'Фоны FBE', 'url(')) { if($after -match [regex]::Escape($forbidden)) { throw "UI-фон сериализовался в FB2: $forbidden" } }
    if($before -notmatch 'Text must stay in FB2.' -or $after -notmatch 'Text must stay in FB2.') { throw 'Save не сохранил исходную семантику FB2.' }
    $completed = $true
    Write-Host 'FBE.exe editor background runtime regression passed (MSHTML DOM, layouts, fallback, custom paths, High Contrast and Save isolation).'
} finally {
    if($completed -or -not $KeepArtifacts) { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue } else { Write-Host "Артефакты runtime-проверки: $directory" }
}
