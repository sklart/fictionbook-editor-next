<#
.SYNOPSIS
Exercises production MSHTML insertion of a block image at inline-formatting boundaries.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 180,
    [switch]$DiagnosticUndoRedo
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$schemaPath = Join-Path $repoRoot 'runtime\FictionBook.xsd'

function Assert-Fb2Schema([string]$Path) {
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0', $schemaPath)
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false
    if (-not $document.load($Path)) { throw "MSXML не прочитал FB2: $($document.parseError.reason)" }
    $document.schemas = $cache
    $validation = $document.validate()
    if ($validation.errorCode -ne 0) { throw "FictionBook.xsd validation failed: $($validation.reason)" }
}

function Invoke-FbeScenario([string]$Scenario, [string]$Report, [string]$Fixture, [string]$WorkingDirectory, [int]$Offset, [bool]$UndoRedo) {
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = $Scenario
    $env:FBE_NEXT_TEST_IMAGE_CARET_OFFSET = [string]$Offset
    $env:FBE_NEXT_TEST_IMAGE_UNDO_REDO = if ($UndoRedo) { '1' } else { '0' }
    $process = Start-Process -FilePath $FbeExe -WorkingDirectory $WorkingDirectory -ArgumentList @('-b', $Report, $Fixture) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE не завершил $Scenario." }
    if ($process.ExitCode -ne 0) {
        $details = if (Test-Path -LiteralPath $Report) { Get-Content -LiteralPath $Report -Raw } else { 'report unavailable' }
        throw "FBE вернул код $($process.ExitCode) для $Scenario. Report: $details"
    }
    if (-not (Test-Path -LiteralPath $Report)) { throw "FBE не записал отчёт $Scenario." }
    $text = Get-Content -LiteralPath $Report -Raw
    if ($text -match 'E_POINTER|80004003|import-failed') { throw "Runtime image regression: $text" }
    if ($UndoRedo -and ($text -notmatch 'undo-complete' -or $text -notmatch 'redo-complete')) { throw "Undo/Redo не подтвердились: $text" }
}

function Assert-Case([string]$Path, $Case) {
    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -match 'fbe-block-image-marker') { throw "$($Case.Id): во FB2 остался временный marker." }
    Assert-Fb2Schema $Path
    [xml]$xml = $text
    $ns = [Xml.XmlNamespaceManager]::new($xml.NameTable)
    $ns.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $ns.AddNamespace('l', 'http://www.w3.org/1999/xlink')
    $section = $xml.SelectSingleNode('/fb:FictionBook/fb:body/fb:section', $ns)
    $children = @($section.ChildNodes | Where-Object { $_.NodeType -eq [Xml.XmlNodeType]::Element })
    $actual = [string]::Join(',', @($children | ForEach-Object { $_.LocalName }))
    if ($actual -ne $Case.Children) { throw "$($Case.Id): порядок section-элементов '$actual', ожидается '$($Case.Children)'." }
    $image = $section.SelectSingleNode('./fb:image', $ns)
    if ($null -eq $image) { throw "$($Case.Id): block image не является прямым потомком section." }
    $href = $image.GetAttribute('href', 'http://www.w3.org/1999/xlink')
    if ([string]::IsNullOrWhiteSpace($href) -or $href[0] -ne '#') { throw "$($Case.Id): некорректная ссылка image '$href'." }
    if ($null -eq $xml.SelectSingleNode(('/fb:FictionBook/fb:binary[@id="{0}"]' -f $href.Substring(1)), $ns)) { throw "$($Case.Id): отсутствует binary для $href." }
    foreach ($check in $Case.Checks) {
        $node = $section.SelectSingleNode($check.Path, $ns)
        if ($null -eq $node -or $node.InnerText -ne $check.Text) { throw "$($Case.Id): не сохранён фрагмент $($check.Path)='$($check.Text)'." }
    }
    if ($Case.ParagraphId) {
        $paragraph = $section.SelectSingleNode('./fb:p[1]', $ns)
        if ($null -eq $paragraph -or $paragraph.GetAttribute('id') -ne $Case.ParagraphId) { throw "$($Case.Id): не сохранён id='$($Case.ParagraphId)' исходного P." }
    }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-block-image-inline-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$savedEnvironment = @{}
foreach ($name in 'FBE_NEXT_TEST_MODE', 'FBE_NEXT_TEST_SCENARIO', 'FBE_NEXT_TEST_IMAGE_PATH', 'FBE_NEXT_TEST_IMAGE_INLINE', 'FBE_NEXT_TEST_IMAGE_CARET_OFFSET', 'FBE_NEXT_TEST_IMAGE_UNDO_REDO') { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    Add-Type -AssemblyName System.Drawing
    $imagePath = Join-Path $directory 'formatting-boundary.jpg'
    $bitmap = [Drawing.Bitmap]::new(2, 2)
    try { $bitmap.SetPixel(0, 0, [Drawing.Color]::Red); $bitmap.Save($imagePath, [Drawing.Imaging.ImageFormat]::Jpeg) } finally { $bitmap.Dispose() }
    $env:FBE_NEXT_TEST_IMAGE_PATH = $imagePath
    $env:FBE_NEXT_TEST_IMAGE_INLINE = '0'
    $cases = @(
        @{ Id='keep-p-id-at-start'; Markup='<p id="keep-me"><emphasis>formatted text</emphasis> tail</p>'; Offset=0; Children='image,p'; ParagraphId='keep-me'; Checks=@(@{Path='./fb:p[1]/fb:emphasis';Text='formatted text'},@{Path='./fb:p[1]';Text='formatted text tail'}) },
        @{ Id='strong-start'; Markup='<p><strong>bold text</strong> normal</p>'; Offset=0; Children='image,p'; Checks=@(@{Path='./fb:p[1]/fb:strong';Text='bold text'}) },
        @{ Id='strike-start'; Markup='<p><strikethrough>strike text</strikethrough> normal</p>'; Offset=0; Children='image,p'; Checks=@(@{Path='./fb:p[1]/fb:strikethrough';Text='strike text'}) },
        @{ Id='sup-start'; Markup='<p><sup>sup</sup> normal</p>'; Offset=0; Children='image,p'; Checks=@(@{Path='./fb:p[1]/fb:sup';Text='sup'}) },
        @{ Id='sub-start'; Markup='<p><sub>sub</sub> normal</p>'; Offset=0; Children='image,p'; Checks=@(@{Path='./fb:p[1]/fb:sub';Text='sub'}) },
        @{ Id='em-inside'; Markup='<p><emphasis>one twothree four</emphasis></p>'; Offset=7; Children='p,image,p'; Checks=@(@{Path='./fb:p[1]/fb:emphasis';Text='one two'},@{Path='./fb:p[2]/fb:emphasis';Text='three four'}) },
        @{ Id='nested-em-strong'; Markup='<p>normal <emphasis>italic <strong>boldbold2</strong> italic2</emphasis> normal2</p>'; Offset=18; Children='p,image,p'; Checks=@(@{Path='./fb:p[1]/fb:emphasis/fb:strong';Text='bold'},@{Path='./fb:p[2]/fb:emphasis/fb:strong';Text='bold2'}) ; UndoRedo=[bool]$DiagnosticUndoRedo },
        @{ Id='after-em'; Markup='<p><emphasis>italic</emphasis>normal</p>'; Offset=6; Children='p,image,p'; Checks=@(@{Path='./fb:p[1]/fb:emphasis';Text='italic'},@{Path='./fb:p[2]';Text='normal'}) },
        @{ Id='before-em'; Markup='<p>normal<emphasis>italic</emphasis></p>'; Offset=6; Children='p,image,p'; Checks=@(@{Path='./fb:p[1]';Text='normal'},@{Path='./fb:p[2]/fb:emphasis';Text='italic'}) },
        @{ Id='paragraph-end'; Markup='<p>normal <emphasis>italic</emphasis></p>'; Offset=13; Children='p,image'; Checks=@(@{Path='./fb:p[1]/fb:emphasis';Text='italic'}) }
    )
    foreach ($case in $cases) {
        $fixture = Join-Path $directory ($case.Id + '.fb2')
        $report = Join-Path $directory ($case.Id + '.tsv')
        @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Block image formatting</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>block-image-formatting</id><version>1.0</version></document-info></description><body><section>$($case.Markup)</section></body></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
        Invoke-FbeScenario 'binary-import-image' $report $fixture $directory $case.Offset ([bool]$case.UndoRedo)
        Assert-Case $fixture $case
        Invoke-FbeScenario 'binary-roundtrip' (Join-Path $directory ($case.Id + '-reopen.tsv')) $fixture $directory 0 $false
        Assert-Case $fixture $case
    }
    Write-Host 'Block-image inline-formatting MSHTML Save -> Reopen regression passed.'
}
finally {
    foreach ($name in $savedEnvironment.Keys) { if ($null -eq $savedEnvironment[$name]) { Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue } else { Set-Item -Path "Env:$name" -Value $savedEnvironment[$name] } }
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
