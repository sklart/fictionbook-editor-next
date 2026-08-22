<#
.SYNOPSIS
Runs FBE -> local ExportHTML plugin -> IFBEExportPlugin::Export with the
same production export body used by the interactive Save dialog.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90, [switch]$KeepArtifacts)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe)) { throw "Не найден FBE: $FbeExe" }
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-export-html-e2e-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $png = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScL1kQAAAABJRU5ErkJggg=='
    $jpg = '/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAP//////////////////////////////////////////////////////////////////////////////////////2wBDAf//////////////////////////////////////////////////////////////////////////////////////wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAX/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/9oADAMBAAIQAxAAAAH/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/9oACAEBAAEFAqf/xAAUEQEAAAAAAAAAAAAAAAAAAAAA/9oACAEDAQE/Aaf/xAAUEQEAAAAAAAAAAAAAAAAAAAAA/9oACAECAQE/Aaf/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/9oACAEBAAY/Aqf/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/9oACAEBAAE/IV//2gAMAwEAAgADAAAAEP/EABQRAQAAAAAAAAAAAAAAAAAAABD/2gAIAQMBAT8QH//EABQRAQAAAAAAAAAAAAAAAAAAABD/2gAIAQIBAT8QH//EABQQAQAAAAAAAAAAAAAAAAAAABD/2gAIAQEAAT8QH//Z'
    $fixture = Join-Path $directory 'export-html-images.fb2'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>images</book-title><lang>en</lang><coverpage><image l:href="#cover.png"/></coverpage></title-info><document-info><program-used>test</program-used><id>export-images-test</id><version>1.0</version></document-info></description><body><section><title><p>Images</p></title><p><image l:href="#inline.png"/></p><p><image l:href="#inline.jpg"/></p></section></body><binary id="cover.png" content-type="image/png">$png</binary><binary id="inline.png" content-type="image/png">$png</binary><binary id="inline.jpg" content-type="image/jpeg">$jpg</binary></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    function Get-Sha256([byte[]]$Bytes) {
        return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes))
    }
    $expectedImageHashes = @(
        (Get-Sha256 ([Convert]::FromBase64String($png))),
        (Get-Sha256 ([Convert]::FromBase64String($png))),
        (Get-Sha256 ([Convert]::FromBase64String($jpg)))
    ) | Sort-Object
    function Assert-HtmlImages([string]$HtmlPath, [string]$ResourceRoot, [bool]$Embedded) {
        $html = Get-Content -Raw -LiteralPath $HtmlPath
        $matches = @([regex]::Matches($html, '(?is)<img\b[^>]*?\bsrc\s*=\s*(?:["''](?<src>[^"'']+)["'']|(?<src>[^\s>]+))'))
        if ($matches.Count -ne 3) { throw "Expected three image references in $HtmlPath, got $($matches.Count)." }
        $hashes = foreach ($match in $matches) {
            $src = $match.Groups['src'].Value
            if ($Embedded) {
                if ($src -notmatch '^data:(?:image/png|image/jpeg);base64,(?<data>.+)$') { throw "Expected embedded PNG/JPEG data URI in ${HtmlPath}: $src" }
                Get-Sha256 ([Convert]::FromBase64String($Matches.data))
            } else {
                if ($src -match '^[a-z]+:' -or [IO.Path]::IsPathRooted($src)) { throw "Expected a relative external image reference in ${HtmlPath}: $src" }
                $candidate = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $HtmlPath) ($src.Replace('/', [IO.Path]::DirectorySeparatorChar))))
                $root = [IO.Path]::GetFullPath($ResourceRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
                if (-not $candidate.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $candidate)) { throw "External image is not resolvable below its export directory: $src" }
                Get-Sha256 ([IO.File]::ReadAllBytes($candidate))
            }
        }
        if ((@($hashes | Sort-Object) -join ',') -ne ($expectedImageHashes -join ',')) { throw "Exported image bytes do not match the FB2 binaries in $HtmlPath." }
    }
    function Invoke-Export([int]$Mode, [string]$Output) {
        $old = @($env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_EXPORT_HTML_PATH, $env:FBE_NEXT_TEST_EXPORT_HTML_MODE, $env:FBE_NEXT_TEST_EXPORT_HTML_DOM_PATH)
        try {
            $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='export-html'; $env:FBE_NEXT_TEST_EXPORT_HTML_PATH=$Output; $env:FBE_NEXT_TEST_EXPORT_HTML_MODE="$Mode"; $env:FBE_NEXT_TEST_EXPORT_HTML_DOM_PATH=(Join-Path $directory ("mode-$Mode-dom.xml"))
            $report = Join-Path $directory ("mode-$Mode.tsv")
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
            if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE did not finish ExportHTML mode $Mode." }
            if ($process.ExitCode -ne 0) { throw "FBE returned $($process.ExitCode) for ExportHTML mode $Mode." }
        } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_EXPORT_HTML_PATH,$env:FBE_NEXT_TEST_EXPORT_HTML_MODE,$env:FBE_NEXT_TEST_EXPORT_HTML_DOM_PATH = $old }
    }
    $self = Join-Path $directory 'book.html'; Invoke-Export 4 $self
    $selfText = Get-Content -Raw -LiteralPath $self
    if (@([regex]::Matches($selfText, 'data:image/(?:png|jpeg);base64,')).Count -lt 3 -or $selfText -match '_files/') { throw 'Self-contained ExportHTML did not embed cover, PNG and JPEG.' }
    Assert-HtmlImages $self $directory $true
    $external = Join-Path $directory 'external.html'; Invoke-Export 1 $external
    $externalText = Get-Content -Raw -LiteralPath $external; $resources = Join-Path $directory 'external_files'
    if (-not (Test-Path -LiteralPath $resources) -or @([IO.Directory]::GetFiles($resources)).Count -lt 3 -or $externalText -notmatch 'external_files/') { throw 'External-image ExportHTML did not create resolvable resources.' }
    Assert-HtmlImages $external $directory $false
    $htmlOnly = Join-Path $directory 'html-only.html'; Invoke-Export 3 $htmlOnly
    if (-not (Test-Path -LiteralPath $htmlOnly) -or (Get-Content -Raw -LiteralPath $htmlOnly) -match '<img\b') { throw 'HTML-only ExportHTML left image references.' }
    $mht = Join-Path $directory 'book.mht'; Invoke-Export 2 $mht
    $mhtText = Get-Content -Raw -LiteralPath $mht
    if ($mhtText -notmatch 'multipart/related' -or @([regex]::Matches($mhtText, 'Content-Transfer-Encoding: base64')).Count -lt 3) { throw 'MHT ExportHTML is missing image MIME parts.' }
    $unicodeDirectory = Join-Path $directory 'Проверка HTML'; [void](New-Item -ItemType Directory -Path $unicodeDirectory)
    $unicodeSelf = Join-Path $unicodeDirectory 'книга.html'; Invoke-Export 4 $unicodeSelf
    if (-not (Test-Path -LiteralPath $unicodeSelf) -or @([regex]::Matches((Get-Content -Raw -LiteralPath $unicodeSelf), 'data:image/(?:png|jpeg);base64,')).Count -lt 3) { throw 'Self-contained Unicode ExportHTML path failed.' }
    Assert-HtmlImages $unicodeSelf $unicodeDirectory $true
    $unicodeExternal = Join-Path $unicodeDirectory 'внешние.html'; Invoke-Export 1 $unicodeExternal
    if (-not (Test-Path -LiteralPath $unicodeExternal) -or (Get-Content -Raw -LiteralPath $unicodeExternal) -match 'src="data:;base64,"') { throw 'External-image Unicode ExportHTML path failed.' }
    Assert-HtmlImages $unicodeExternal $unicodeDirectory $false
    Write-Host 'ExportHTML images production E2E passed (modes 1-4).'
} finally { if ($KeepArtifacts) { Write-Host "ExportHTML E2E artifacts: $directory" } else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue } }
