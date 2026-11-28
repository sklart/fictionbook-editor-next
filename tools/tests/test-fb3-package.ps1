[CmdletBinding()]
param(
    [string[]]$ReferencePackage = @(),
    [string[]]$ReferenceWithoutCover = @(),
    [string[]]$ImportReference = @(),
    [string[]]$ImportReferenceWithBrokenImage = @(),
    [string[]]$RoundTripReference = @()
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'fb3-package'
$passed = $false
try {
    $archive = Join-Path $root 'build/libarchive/install/Release'
    $zlib = Join-Path $root 'build/zlib/install/Release'
    $svg = Join-Path $root 'build/lib/lunasvg/Win32/Release'
    $exe = Join-Path $scratch.Root 'fb3-package-test.exe'
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /Zc:wchar_t- /DUNICODE /D_UNICODE /DLUNASVG_BUILD_STATIC `
        ('/I' + (Join-Path $root 'build/generated/Win32/Release/fbe-api')) ('/I' + (Join-Path $archive 'include')) `
        ('/I' + (Join-Path $zlib 'include')) `
        ('/I' + (Join-Path $root 'third_party/lunasvg/include')) `
        "/Fo$($scratch.Root)\\" "/Fe$exe" (Join-Path $PSScriptRoot 'fb3-package-test.cpp') `
        (Join-Path $root 'src/common/fb3/Fb3Package.cpp') (Join-Path $root 'src/common/fb3/Fb3Import.cpp') `
        (Join-Path $root 'src/common/fb3/Fb3Image.cpp') `
        (Join-Path $root 'src/common/fb3/Fb3Schema.cpp') `
        (Join-Path $root 'src/common/fb3/Fb3Export.cpp') (Join-Path $root 'src/common/fb3/Fb3Batch.cpp') `
        (Join-Path $root 'src/common/fb2/Fb2BinaryInspector.cpp') `
        (Join-Path $root 'src/import-epub/SecureEpubZip.cpp') `
        /link (Join-Path $archive 'lib/archive.lib') (Join-Path $zlib 'lib/zs.lib') (Join-Path $svg 'lunasvg.lib') `
        (Join-Path $svg 'plutovg.lib') ole32.lib oleaut32.lib shlwapi.lib crypt32.lib xmllite.lib
    if ($LASTEXITCODE -ne 0) { throw 'FB3 package harness did not compile.' }

    $relNs = 'http://schemas.openxmlformats.org/package/2006/relationships'
    $fb3 = 'http://www.fictionbook.org/FictionBook3'
    $manifest = '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">' +
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>' +
        '<Override PartName="/Book/meta.xml" ContentType="application/fb3-description+xml"/>' +
        '<Override PartName="/Text/sample.xml" ContentType="application/fb3-body+xml"/></Types>'
    $rootLinks = "<Relationships xmlns=`"$relNs`"><Relationship Id=`"b`" Type=`"$fb3/relationships/Book`" Target=`"Book/meta.xml`"/></Relationships>"
    $textLinks = "<Relationships xmlns=`"$relNs`"><Relationship Id=`"text`" Type=`"$fb3/relationships/body`" Target=`"../Text/sample.xml`"/></Relationships>"
    $book = [ordered]@{
        '[Content_Types].xml' = $manifest
        '_rels/.rels' = $rootLinks
        'Book/meta.xml' = "<fb3-description xmlns=`"$fb3/description`"/>"
        'Book/_rels/meta.xml.rels' = $textLinks
        'Text/sample.xml' = "<fb3-body xmlns=`"$fb3/body`"><section><p>Проверка</p></section></fb3-body>"
    }
    function Check-Package($name, $files, $expected) {
        $path = Join-Path $scratch.Root "$name.fb3"
        $zip = [IO.Compression.ZipFile]::Open($path, [IO.Compression.ZipArchiveMode]::Create)
        try {
            foreach ($part in $files.GetEnumerator()) {
                $stream = $zip.CreateEntry($part.Key).Open()
                $bytes = if ($part.Value -is [byte[]]) { $part.Value } else { [Text.Encoding]::UTF8.GetBytes($part.Value) }
                try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
            }
        } finally { $zip.Dispose() }
        if ($expected -in @('import', 'invalid-note-import', 'invalid-image-import', 'damaged-image-import')) {
            & $exe $expected $path (Join-Path $root 'runtime/FictionBook.xsd') (Join-Path $scratch.Root "$name.fb2")
        } elseif ($expected -in @('schema', 'invalid-schema', 'mismatched-id')) {
            & $exe $expected $path (Join-Path $root 'runtime/FB3Schemas')
        } else { & $exe $expected $path }
        if ($LASTEXITCODE -ne 0) { throw "FB3 fixture $name failed with exit code $LASTEXITCODE." }
    }
    Check-Package 'relative' $book 'valid'
    $book['Book/_rels/meta.xml.rels'] = $textLinks.Replace('../Text/sample.xml', '/Text/%73ample.xml')
    Check-Package 'encoded' $book 'valid'
    $book['_rels/.rels'] = $rootLinks.Replace('</Relationships>',
        "<Relationship Id=`"cover`" Type=`"$relNs/metadata/thumbnail`" Target=`"cover.png`"/></Relationships>")
    Check-Package 'missing-cover' $book 'missing-cover'
    $book['_rels/.rels'] = $rootLinks
    $book['Book/_rels/meta.xml.rels'] = $textLinks.Replace('../Text/sample.xml', '../../Text/sample.xml')
    Check-Package 'escape' $book 'invalid'
    $book['Book/_rels/meta.xml.rels'] = $textLinks.Replace('../Text/sample.xml', '../Text/missing.xml')
    Check-Package 'missing' $book 'invalid'
    $book['Book/_rels/meta.xml.rels'] = $textLinks
    $book['[Content_Types].xml'] = $manifest.Replace('application/fb3-body+xml','application/xml')
    Check-Package 'wrong-type' $book 'invalid'
    $book['[Content_Types].xml'] = $manifest
    $book['Text/%73ample.xml'] = 'Duplicate escaped spelling'
    Check-Package 'alias' $book 'invalid'
    $book.Remove('Text/%73ample.xml')
    $book['Text/sample.xml'] = '<!DOCTYPE fb3-body [<!ENTITY accidental "unexpected">]>' + $book['Text/sample.xml']
    Check-Package 'doctype' $book 'invalid'
    $broken = Join-Path $scratch.Root 'broken.fb3'
    [IO.File]::WriteAllBytes($broken, [byte[]]@(0x50,0x4b,3,4))
    & $exe invalid $broken
    if ($LASTEXITCODE -ne 0) { throw 'Corrupt container was accepted.' }
    foreach ($package in $ReferencePackage) {
        & $exe valid (Resolve-Path -LiteralPath $package).Path
        if ($LASTEXITCODE -ne 0) { throw "Reference package failed: $package" }
    }
    foreach ($package in $ReferenceWithoutCover) {
        & $exe missing-cover (Resolve-Path -LiteralPath $package).Path
        if ($LASTEXITCODE -ne 0) { throw "Reference package did not diagnose its missing cover: $package" }
    }
    $book['Book/meta.xml'] = @"
<fb3-description xmlns="$fb3/description" id="7027fa9a-0c72-52af-a708-2526bf9c3054" version="2.1">
  <title><main>Проверка импорта</main></title>
  <fb3-relations><subject link="author"><first-name>Иван</first-name><last-name>Иванов</last-name></subject><subject link="translator"><title>Переводчик</title></subject></fb3-relations>
  <fb3-classification><subject>sf</subject><subject>Роботы</subject></fb3-classification>
  <lang>ru</lang><written><lang>en</lang></written><document-info created="2026-11-28T08:00:00Z"/>
  <paper-publish-info title="Печатная книга" year="2000"><isbn>9781234567890</isbn></paper-publish-info>
  <annotation><p>Описание с <em>выделением</em>.</p></annotation>
</fb3-description>
"@
    $book['Text/sample.xml'] = @"
<fb3-body xmlns="$fb3/body" xmlns:l="http://www.w3.org/1999/xlink">
  <section id="main"><title id="heading"><p>Глава</p></title><epigraph><p>Эпиграф</p><subscription><p>Автор эпиграфа</p></subscription></epigraph>
    <p>Начало <strong> <em>текста</em> </strong><note href="note1">[1]</note>. <a l:href="#inner">Внутренняя ссылка</a> <a l:href="https://example.org/">Сайт</a></p>
    <ul><li><p>Элемент списка</p></li></ul><poem><stanza><p>Первая строка</p><p>Вторая строка</p></stanza></poem>
    <p><img src="pic" alt="Пример"/></p><table><title><p>Подпись</p></title><tr><td><p id="inner">Первая ячейка</p><p>Вторая строка ячейки</p></td></tr></table>
    <section><p>Подраздел</p></section><p>Конец</p>
  </section><notes><notebody id="note1"><p>Сноска.</p></notebody></notes>
</fb3-body>
"@
    $book['[Content_Types].xml'] = $manifest.Replace('</Types>', '<Default Extension="png" ContentType="image/png"/></Types>')
    $book['_rels/.rels'] = $rootLinks.Replace('</Relationships>',
        "<Relationship Id=`"cover`" Type=`"$relNs/metadata/thumbnail`" Target=`"Images/a.png`"/></Relationships>")
    $book['Text/_rels/sample.xml.rels'] = "<Relationships xmlns=`"$relNs`"><Relationship Id=`"pic`" Type=`"$fb3/relationships/image`" Target=`"../Images/a.png`"/></Relationships>"
    $book['Images/a.png'] = [Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+ip1sAAAAASUVORK5CYII=')
    Check-Package 'convert' $book 'import'
    [xml]$converted = Get-Content -LiteralPath (Join-Path $scratch.Root 'convert.fb2') -Raw -Encoding UTF8
    $ns = [Xml.XmlNamespaceManager]::new($converted.NameTable)
    $ns.AddNamespace('f', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $ns.AddNamespace('l', 'http://www.w3.org/1999/xlink')
    foreach ($xpath in @('//f:book-title[text()="Проверка импорта"]', '//f:author/f:last-name[text()="Иванов"]',
        '//f:translator/f:nickname[text()="Переводчик"]', '//f:lang[text()="ru"]', '//f:src-lang[text()="en"]',
        '//f:annotation/f:p/f:emphasis', '//f:genre[text()="sf"]', '//f:keywords[text()="Роботы"]', '//f:isbn',
        '//f:epigraph/f:text-author[text()="Автор эпиграфа"]', '//f:strong/f:emphasis[text()="текста"]',
        '//f:table/f:tr/f:td[@id="fb3-anchor-inner"]', '//f:a[@l:href="#fb3-anchor-inner"]',
        '//f:a[@type="note" and @l:href="#fb3-anchor-note1"]', '//f:body[@name="notes"]//f:section[@id="fb3-anchor-note1"]',
        '//f:v[text()="Вторая строка"]', '//f:a[@l:href="https://example.org/"]',
        '//f:coverpage/f:image[@l:href="#fb3-image-1"]', '//f:p/f:image[@l:href="#fb3-image-1"]', '//f:binary[@id="fb3-image-1"]')) {
        if (!$converted.SelectSingleNode($xpath, $ns)) { throw "Missing converted FB2 element for $xpath" }
    }
    if ($converted.SelectNodes('//f:binary', $ns).Count -ne 1) { throw 'Shared cover and inline image must produce one binary.' }
    $runtime = Join-Path $root 'runtime'
    $exported = Join-Path $scratch.Root 'roundtrip.fb3'
    $roundtrip = Join-Path $scratch.Root 'roundtrip.fb2'
    & $exe export (Join-Path $scratch.Root 'convert.fb2') $runtime $exported
    if ($LASTEXITCODE -ne 0) { throw 'Export of converted FB2 failed.' }
    & $exe import $exported (Join-Path $runtime 'FictionBook.xsd') $roundtrip
    if ($LASTEXITCODE -ne 0) { throw 'Reimport of exported FB3 failed.' }
    [xml]$round = Get-Content -LiteralPath $roundtrip -Raw -Encoding UTF8
    $roundNs = [Xml.XmlNamespaceManager]::new($round.NameTable)
    $roundNs.AddNamespace('f', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $roundNs.AddNamespace('l', 'http://www.w3.org/1999/xlink')
    foreach ($xpath in @('//f:book-title[text()="Проверка импорта"]', '//f:author/f:last-name[text()="Иванов"]',
        '//f:table/f:tr/f:td[contains(.,"Первая ячейка") and contains(.,"Вторая строка ячейки")]',
        '//f:v[text()="Вторая строка"]', '//f:body[@name="notes"]//f:p[text()="Сноска."]', '//f:coverpage/f:image',
        '//f:p/f:image', '//f:a[@type="note"]', '//f:a[@l:href="https://example.org/"]')) {
        if (!$round.SelectSingleNode($xpath, $roundNs)) { throw "FB3 round-trip changed semantic element $xpath" }
    }
    if ($round.SelectSingleNode('//f:binary', $roundNs).InnerText -ne $converted.SelectSingleNode('//f:binary', $ns).InnerText) {
        throw 'FB3 round-trip changed the embedded bitmap.'
    }
    $protectedHash = (Get-FileHash -LiteralPath $exported).Hash
    $badLinkFb2 = Join-Path $scratch.Root 'bad-link.fb2'
    $originalFb2 = Get-Content -LiteralPath (Join-Path $scratch.Root 'convert.fb2') -Raw -Encoding UTF8
    [IO.File]::WriteAllText($badLinkFb2, $originalFb2.Replace('href="#fb3-anchor-inner"', 'href="#missing-fb2-anchor"'))
    & $exe export-invalid-link $badLinkFb2 $runtime $exported
    if ($LASTEXITCODE -ne 0 -or (Get-FileHash -LiteralPath $exported).Hash -ne $protectedHash -or
        @(Get-ChildItem -LiteralPath $scratch.Root -Force -Filter '.fb3-export-*.tmp').Count) {
        throw 'Failed FB3 export changed the existing output or leaked temporary data.'
    }
    $validBody = $book['Text/sample.xml']
    $validImage = $book['Images/a.png']
    $book['Images/a.png'] = [Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/l4sAAAAASUVORK5CYII=')
    Check-Package 'damaged-bitmap' $book 'damaged-image-import'
    $book['Images/a.png'] = $validImage
    $book['Text/sample.xml'] = $validBody.Replace('href="note1"', 'href="missing-note"')
    Check-Package 'unresolved-note' $book 'invalid-note-import'
    $book['Text/sample.xml'] = $validBody.Replace('src="pic"', 'src="missing-image"')
    Check-Package 'unresolved-image' $book 'invalid-image-import'
    $book['Text/sample.xml'] = $validBody.Replace('l:href="#inner"', 'l:href="#missing-anchor"')
    Check-Package 'unresolved-link' $book 'import'
    [xml]$link = Get-Content -LiteralPath (Join-Path $scratch.Root 'unresolved-link.fb2') -Raw -Encoding UTF8
    if ($link.OuterXml.Contains('missing-anchor') -or !$link.OuterXml.Contains('Внутренняя ссылка')) {
        throw 'Unresolved ordinary hyperlink did not retain its label as text.'
    }
    $bookId = '7027fa9a-0c72-52af-a708-2526bf9c3054'
    $book['Book/meta.xml'] = @"
<fb3-description xmlns="$fb3/description" id="$bookId" version="1.0">
  <title><main>Схема FB3</main></title>
  <fb3-relations><subject link="author" id="825d6770-55bc-4758-8d9b-70eb34a95774"><title><main>Иванов</main></title><last-name>Иванов</last-name></subject></fb3-relations>
  <fb3-classification><subject>sf</subject></fb3-classification>
  <lang>ru</lang><document-info created="2026-11-28T08:00:00Z" updated="2026-11-28T08:00:00Z"/>
</fb3-description>
"@
    $schemaBody = "<fb3-body xmlns=`"$fb3/body`" id=`"$bookId`"><section id=`"9e85883b-d85a-4d7e-b2bf-64db62d59f6b`"><p>Проверка схемы</p></section></fb3-body>"
    $book['Text/sample.xml'] = $schemaBody
    Check-Package 'schema' $book 'schema'
    $book['Text/sample.xml'] = $schemaBody.Replace('<p>', '<unknown>').Replace('</p>', '</unknown>')
    Check-Package 'invalid-schema' $book 'invalid-schema'
    $book['Text/sample.xml'] = $schemaBody.Replace($bookId, '815c65d8-3d7a-409a-a992-cd58163850d8')
    Check-Package 'mismatched-id' $book 'mismatched-id'
    $failedImports = @()
    foreach ($package in $ImportReference) {
        $path = (Resolve-Path -LiteralPath $package).Path
        Write-Host "Converting $path"
        & $exe import $path (Join-Path $root 'runtime/FictionBook.xsd') (Join-Path $scratch.Root 'reference.fb2')
        if ($LASTEXITCODE -ne 0) { $failedImports += $path }
        else { Write-Host "Validated FB2 from $path" }
    }
    foreach ($package in $ImportReferenceWithBrokenImage) {
        $path = (Resolve-Path -LiteralPath $package).Path
        Write-Host "Checking broken reference image in $path"
        & $exe invalid-image-import $path (Join-Path $root 'runtime/FictionBook.xsd') (Join-Path $scratch.Root 'rejected.fb2')
        if ($LASTEXITCODE -ne 0) { throw "Missing image relationship was not diagnosed: $package" }
    }
    if ($failedImports.Count) { throw ('Reference conversion failed: ' + ($failedImports -join ', ')) }
    function Get-SemanticLinks([Xml.XmlDocument]$document) {
        $prefixes = [Xml.XmlNamespaceManager]::new($document.NameTable)
        $prefixes.AddNamespace('f', 'http://www.gribuser.ru/xml/fictionbook/2.0')
        $targets = [Collections.Generic.Dictionary[string, Xml.XmlElement]]::new([StringComparer]::Ordinal)
        foreach ($target in $document.SelectNodes('/f:FictionBook/f:body//*[@id]', $prefixes)) {
            $targets.Add($target.GetAttribute('id'), $target)
        }
        foreach ($anchor in $document.SelectNodes('/f:FictionBook/f:body//f:a', $prefixes)) {
            $href = $anchor.GetAttribute('href', 'http://www.w3.org/1999/xlink')
            $target = $null
            $type = $anchor.GetAttribute('type')
            if ($href.StartsWith('#') -and $targets.TryGetValue($href.Substring(1), [ref]$target)) {
                $href = '#' + (($target.InnerText -replace '\s+', ' ').Trim())
            }
            $type + '|' + (($anchor.InnerText -replace '\s+', ' ').Trim()) + '|' + $href
        }
    }
    $failedRoundTrips = @()
    foreach ($package in $RoundTripReference) {
        $path = (Resolve-Path -LiteralPath $package).Path
        Write-Host "Round-trip $path"
        $first = Join-Path $scratch.Root 'ref-first.fb2'
        $second = Join-Path $scratch.Root 'ref-second.fb2'
        & $exe import $path (Join-Path $runtime 'FictionBook.xsd') $first
        if ($LASTEXITCODE -ne 0) { $failedRoundTrips += $path; continue }
        & $exe export $first $runtime $exported
        if ($LASTEXITCODE -ne 0) { $failedRoundTrips += $path; continue }
        & $exe import $exported (Join-Path $runtime 'FictionBook.xsd') $second
        if ($LASTEXITCODE -ne 0) { $failedRoundTrips += $path; continue }
        [xml]$a = Get-Content -LiteralPath $first -Raw -Encoding UTF8
        [xml]$b = Get-Content -LiteralPath $second -Raw -Encoding UTF8
        $bodyA = ((@($a.FictionBook.body | ForEach-Object InnerText) -join '') -replace '\s+', ' ').Trim()
        $bodyB = ((@($b.FictionBook.body | ForEach-Object InnerText) -join '') -replace '\s+', ' ').Trim()
        $imageNodes = '/*[local-name()="FictionBook"]/*[local-name()="binary"]'
        $imagesA = @($a.SelectNodes($imageNodes) | ForEach-Object InnerText | Sort-Object)
        $imagesB = @($b.SelectNodes($imageNodes) | ForEach-Object InnerText | Sort-Object)
        if ($bodyA -cne $bodyB -or ($imagesA -join ',') -cne ($imagesB -join ',') -or
            $a.FictionBook.description.'title-info'.'book-title' -cne $b.FictionBook.description.'title-info'.'book-title' -or
            (@(Get-SemanticLinks $a) -join "`n") -cne (@(Get-SemanticLinks $b) -join "`n")) {
            Write-Host "Round-trip changed body text, a link, binary data or title in $path"
            $failedRoundTrips += $path
        } else { Write-Host "Verified round-trip $path" }
    }
    if ($failedRoundTrips.Count) { throw ('Reference round-trip failed: ' + ($failedRoundTrips -join ', ')) }
    $passed = $true
    'FB3 OPC package tests passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
