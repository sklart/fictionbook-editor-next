<#
.SYNOPSIS
Runs FBE.exe's unattended editor-background scenario against the real MSHTML DOM.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 180,
    [int]$NoProgressSeconds = 45,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
function Get-FileTreeSnapshot([string]$Path) {
    if(-not (Test-Path -LiteralPath $Path -PathType Container)) { return '<absent>' }
    return (Get-ChildItem -LiteralPath $Path -Recurse -File | Sort-Object FullName | ForEach-Object { "$($_.FullName)|$($_.Length)|$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)" }) -join "`n"
}

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
function Save-RuntimeFailureArtifacts([string]$Extension, [string]$Report, [string]$Breadcrumb, [int]$ProcessId, [string]$ExitCode, [string]$LastPhase, [string]$ReportText, [string]$BreadcrumbText) {
    $target = Join-Path $failureArtifactRoot ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $target -Force)
    foreach($path in @($Report, $Breadcrumb, $fixture, $png, $missing, (Join-Path $portableRuntime 'portable.ini'))) {
        if($path -and (Test-Path -LiteralPath $path -PathType Leaf)) { Copy-Item -LiteralPath $path -Destination $target -Force }
    }
	Get-ChildItem -LiteralPath $directory -File -Include '*.png','*.jpg','*.jpeg' -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $target -Force }
    foreach($path in @((Join-Path $portableRuntime 'TestData'), (Join-Path $portableRuntime 'TestData\Diagnostics'))) {
        if(Test-Path -LiteralPath $path -PathType Container) { Copy-Item -LiteralPath $path -Destination $target -Recurse -Force }
    }
    @"
extension=$Extension
pid=$ProcessId
exit=$ExitCode
last-phase=$LastPhase
report=$ReportText
breadcrumb=$BreadcrumbText
"@ | Set-Content -LiteralPath (Join-Path $target 'summary.txt') -Encoding utf8NoBOM
    return $target
}
function Invoke-RuntimeFbe([string]$Extension, [string]$Report) {
    $breadcrumb = Join-Path $directory ("breadcrumb$Extension.log")
    $oldBreadcrumb = $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB; $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB = $breadcrumb
    try {
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $Report, '--portable', $fixture) -WorkingDirectory $portableRuntime -PassThru
        $watch = [Diagnostics.Stopwatch]::StartNew(); $lastProgress = [Diagnostics.Stopwatch]::StartNew(); $lastBreadcrumbText = ''; $reportText = ''; $breadcrumbText = ''; $lastPhase = '<none>'
        while($true) {
            if(Test-Path -LiteralPath $Report) { $reportText = Get-Content -LiteralPath $Report -Raw -ErrorAction SilentlyContinue }
            if(Test-Path -LiteralPath $breadcrumb) { $breadcrumbText = Get-Content -LiteralPath $breadcrumb -Raw -ErrorAction SilentlyContinue; $lastPhase = @($breadcrumbText -split "`r?`n" | Where-Object { $_ } | Select-Object -Last 1)[0]; if(-not $lastPhase){$lastPhase='<none>'} }
			if($breadcrumbText -cne $lastBreadcrumbText) { $lastBreadcrumbText = $breadcrumbText; $lastProgress.Restart() }
            $process.Refresh(); if($process.HasExited){break}
            if($lastProgress.Elapsed.TotalSeconds -ge $NoProgressSeconds -or $watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $artifactDir = Save-RuntimeFailureArtifacts $Extension $Report $breadcrumb $process.Id 'timeout' $lastPhase $reportText $breadcrumbText; throw "extension=$Extension timed out; last phase=$lastPhase; seconds-since-progress=$([math]::Floor($lastProgress.Elapsed.TotalSeconds)); report=$reportText; breadcrumb=$breadcrumbText; pid=$($process.Id); artifact-dir=$artifactDir" }
            Start-Sleep -Milliseconds 100
        }
        if(Test-Path -LiteralPath $Report) { $reportText = Get-Content -LiteralPath $Report -Raw -ErrorAction SilentlyContinue }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $Report)) { $artifactDir = Save-RuntimeFailureArtifacts $Extension $Report $breadcrumb $process.Id $process.ExitCode $lastPhase $reportText $breadcrumbText; throw "extension=$Extension failed; last phase=$lastPhase; report=$reportText; breadcrumb=$breadcrumbText; pid=$($process.Id); artifact-dir=$artifactDir; exit=$($process.ExitCode)" }
    } finally { if($null -eq $oldBreadcrumb){Remove-Item Env:FBE_NEXT_TEST_STARTUP_BREADCRUMB -ErrorAction SilentlyContinue}else{$env:FBE_NEXT_TEST_STARTUP_BREADCRUMB=$oldBreadcrumb} }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-editor-background-runtime-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
$failureArtifactRoot = Join-Path (Split-Path $PSScriptRoot -Parent | Split-Path -Parent) 'out\tests\editor-background-runtime-failure'
$installedProfile = Join-Path $env:LOCALAPPDATA 'FBE Next'
$installedBefore = Get-FileTreeSnapshot $installedProfile
$sourceRuntime = Split-Path $FbeExe -Parent
$portableRuntime = Join-Path $directory 'portable-runtime'
Copy-Item -LiteralPath $sourceRuntime -Destination $portableRuntime -Recurse -Force
$FbeExe = Join-Path $portableRuntime 'FBE.exe'
"[Portable]`r`nDataPath=TestData`r`n" | Set-Content -LiteralPath (Join-Path $portableRuntime 'portable.ini') -Encoding utf8NoBOM
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
            if($extension -eq '.png') { Copy-Item -LiteralPath $builtin -Destination $custom }
            else {
                $image = [System.Drawing.Image]::FromFile($builtin)
                try { $image.Save($custom, [System.Drawing.Imaging.ImageFormat]::Jpeg) } finally { $image.Dispose() }
                $signature = [IO.File]::ReadAllBytes($custom)
                if($signature.Length -lt 4 -or $signature[0] -ne 0xFF -or $signature[1] -ne 0xD8 -or $signature[$signature.Length - 2] -ne 0xFF -or $signature[$signature.Length - 1] -ne 0xD9) { throw "$extension fixture is not a real JPEG." }
            }
            $env:FBE_NEXT_TEST_BACKGROUND_PATH = $custom
            Remove-Item Env:FBE_NEXT_TEST_FORCE_HIGH_CONTRAST -ErrorAction SilentlyContinue
            $report = Join-Path $directory ("report$extension.tsv")
            Invoke-RuntimeFbe $extension $report
            $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
            Assert-Phase $rows 'none' { param($r) Assert-ColorOnly $r 'none' }
            Assert-Phase $rows 'unknown-builtin' { param($r) Assert-ColorOnly $r 'unknown-builtin' }
            Assert-Phase $rows 'missing-custom' { param($r) Assert-ColorOnly $r 'missing-custom' }
            foreach($phase in @('builtin-tile', 'builtin-center', 'builtin-contain', 'builtin-cover', 'builtin-after-view-recreate', 'custom', 'before-save', 'after-save')) { Assert-Phase $rows $phase { param($r) Assert-Image $r $phase } }
            Assert-Phase $rows 'builtin-tile' { param($r) if($r.repeat -ne 'repeat' -or $r.size -ne 'auto') { throw 'tile не установил repeat/auto.' } }
            foreach($phase in @('builtin-center', 'builtin-contain', 'builtin-cover')) { Assert-Phase $rows $phase { param($r) if($r.repeat -ne 'no-repeat' -or $r.position -notmatch 'center') { throw "$phase не установил no-repeat/center." } } }
            Assert-Phase $rows 'builtin-contain' { param($r) if($r.size -ne 'contain') { throw 'contain не установил background-size.' } }
            Assert-Phase $rows 'builtin-cover' { param($r) if($r.size -ne 'cover') { throw 'cover не установил background-size.' } }
            Assert-Phase $rows 'custom' { param($r)
                foreach($escape in @('%20','%23','%25')) { if($r.image -notmatch [regex]::Escape($escape)) { throw "U::UrlFromPath did not preserve ${escape}: $($r.image)" } }
                if($r.image -notmatch 'Фоны' -or $r.image -notmatch '\(' -or $r.image -notmatch '\)') { throw "Unicode or parentheses were lost by MSHTML: $($r.image)" }
                if($r.image -notmatch '^url\(' -or $r.image -notmatch '\.png|\.jpg|\.jpeg') { throw "MSHTML returned an invalid backgroundImage: $($r.image)" }
                if($r.css_url -notmatch '^url\("file:' -or $r.css_url -notmatch '"\)$') { throw "U::UrlFromPath CSS URL was not quoted: $($r.css_url)" }
            }
        }
        $env:FBE_NEXT_TEST_BACKGROUND_PATH = $png; $env:FBE_NEXT_TEST_FORCE_HIGH_CONTRAST = '1'
        $highContrastReport = Join-Path $directory 'report-high-contrast.tsv'
        Invoke-RuntimeFbe '.high-contrast' $highContrastReport
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
    if((Get-FileTreeSnapshot $installedProfile) -cne $installedBefore) { throw '%LOCALAPPDATA%\FBE Next changed during isolated editor background runtime test.' }
    if($completed -and -not $KeepArtifacts) { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue } else { Write-Host "Артефакты runtime-проверки: $directory" }
}
