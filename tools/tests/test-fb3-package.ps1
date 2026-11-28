[CmdletBinding()]
param(
    [string[]]$ReferencePackage = @(),
    [string[]]$ReferenceWithoutCover = @(),
    [string[]]$ImportReference = @(),
    [string[]]$ImportReferenceWithBrokenImage = @(),
    [string[]]$RoundTripReference = @(),
    [ValidateSet('None', 'Address', 'Undefined')]
    [string]$Sanitizer = 'None'
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
    $compiler = 'cl.exe'
    $instrumentation = @()
    $sanitizerLibraries = @()
    if ($Sanitizer -eq 'Address') {
        $instrumentation = @('/fsanitize=address', '/Zi', '/D_DISABLE_VECTOR_ANNOTATION', '/D_DISABLE_STRING_ANNOTATION')
        Copy-Item -LiteralPath (Join-Path (Split-Path (Get-Command cl.exe).Source) 'clang_rt.asan_dynamic-i386.dll') -Destination $scratch.Root
    } elseif ($Sanitizer -eq 'Undefined') {
        $toolchain = & (Join-Path $root 'tools/build/Resolve-UbsanToolchain.ps1')
        $compiler = $toolchain.compilerPath
        $instrumentation = @('--target=i686-pc-windows-msvc', '-fsanitize=undefined', '-fno-sanitize-recover=undefined',
            ('-fsanitize-ignorelist=' + (Join-Path $root 'tools/msbuild/ubsan.ignorelist')), '/Zi')
        $sanitizerLibraries = @($toolchain.runtimeLibrary, $toolchain.cxxRuntimeLibrary)
    }
    & $compiler @instrumentation /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /Zc:wchar_t- /DUNICODE /D_UNICODE /DLUNASVG_BUILD_STATIC `
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
        (Join-Path $svg 'plutovg.lib') ole32.lib oleaut32.lib shlwapi.lib crypt32.lib xmllite.lib @sanitizerLibraries
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
  <lang>ru</lang><written><lang>en</lang><country>GB</country></written>
  <copyrights><copyright-owner title="Правообладатель"/></copyrights>
  <document-info created="2026-11-28T08:00:00Z" updated="2026-11-28T08:02:00Z" program-used="Fixture" src-url="https://example.org/source" ocr="OCR"/>
  <paper-publish-info title="Печатная книга" publisher="Издатель" city="Город" year="2000"><isbn>9781234567890</isbn></paper-publish-info>
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
    function Read-BookXml($file, $part) {
        if (!$part) { return [xml](Get-Content -LiteralPath $file -Raw -Encoding UTF8) }
        $zip = [IO.Compression.ZipFile]::OpenRead($file)
        try {
            $entry = @($zip.Entries | Where-Object { $_.Name -ceq $part })
            if ($entry.Count -ne 1) { throw "Missing or duplicate $part in $file" }
            $reader = [IO.StreamReader]::new($entry[0].Open())
            try { return [xml]$reader.ReadToEnd() } finally { $reader.Dispose() }
        } finally { $zip.Dispose() }
    }
    # Compare shared properties directly against the original package, before doing any export.
    $sourcePackage = Join-Path $scratch.Root 'convert.fb3'
    $sourceBody = Read-BookXml $sourcePackage 'sample.xml'
    $sourceMetadata = Read-BookXml $sourcePackage 'meta.xml'
    $fb3Ns = [Xml.XmlNamespaceManager]::new($sourceBody.NameTable)
    $fb3Ns.AddNamespace('b', "$fb3/body")
    $fb3Ns.AddNamespace('d', "$fb3/description")
    $fb3Ns.AddNamespace('l', 'http://www.w3.org/1999/xlink')
    foreach ($mapping in @(
        @{ From = $sourceMetadata; Source = '//d:title/d:main'; Target = '//f:book-title' },
        @{ From = $sourceMetadata; Source = '//d:subject[@link="author"]/d:last-name'; Target = '//f:author/f:last-name' },
        @{ From = $sourceMetadata; Source = '//d:subject[@link="translator"]/d:title'; Target = '//f:translator/f:nickname' },
        @{ From = $sourceMetadata; Source = '/d:fb3-description/d:lang'; Target = '//f:title-info/f:lang' },
        @{ From = $sourceMetadata; Source = '//d:written/d:lang'; Target = '//f:src-lang' },
        @{ From = $sourceMetadata; Source = '//d:annotation/d:p'; Target = '//f:description//f:annotation/f:p' },
        @{ From = $sourceMetadata; Source = '//d:paper-publish-info/d:isbn'; Target = '//f:publish-info/f:isbn' },
        @{ From = $sourceMetadata; Source = '//d:paper-publish-info/@publisher'; Target = '//f:publish-info/f:publisher' },
        @{ From = $sourceMetadata; Source = '//d:copyright-owner/@title'; Target = '//f:document-info/f:publisher/f:nickname' },
        @{ From = $sourceMetadata; Source = '//d:document-info/@program-used'; Target = '//f:document-info/f:program-used' },
        @{ From = $sourceMetadata; Source = '//d:document-info/@created'; Target = '//f:document-info/f:date' },
        @{ From = $sourceMetadata; Source = '//d:document-info/@src-url'; Target = '//f:document-info/f:src-url' },
        @{ From = $sourceBody; Source = '//b:section/b:title/b:p[text()]'; Target = '//f:body/f:section/f:title/f:p[text()]' },
        @{ From = $sourceBody; Source = '//b:poem/b:stanza/b:p'; Target = '//f:poem/f:stanza/f:v' },
        @{ From = $sourceBody; Source = '//b:table/b:tr/b:td/b:p'; Target = '//f:table/f:tr/f:td'; JoinWith = ' • ' }
    )) {
        $sourceValues = @(
            $mapping.From.SelectNodes($mapping.Source, $fb3Ns) | ForEach-Object {
                if ($_ -is [Xml.XmlAttribute]) { $_.Value } else { $_.InnerText }
            }
        )
        $importedValues = @($converted.SelectNodes($mapping.Target, $ns) | ForEach-Object InnerText)
        if ($mapping.ContainsKey('JoinWith')) { $sourceValues = @($sourceValues -join $mapping.JoinWith) }
        if (!$sourceValues.Count -or ($sourceValues -join [char]0) -cne ($importedValues -join [char]0)) {
            throw "First FB3 import changed $($mapping.Source) -> $($mapping.Target)."
        }
    }
    foreach ($pair in @(@('//b:table/b:tr','//f:table/f:tr'), @('//b:table/b:tr/b:td','//f:table/f:tr/f:td'),
        @('//b:poem/b:stanza','//f:poem/f:stanza'), @('//b:img','//f:body//f:image'))) {
        if ($sourceBody.SelectNodes($pair[0], $fb3Ns).Count -ne $converted.SelectNodes($pair[1], $ns).Count) {
            throw "First import changed the number of nodes for $($pair[0])."
        }
    }
    $writtenExtra = $converted.SelectSingleNode('//f:custom-info[@info-type="FB3 written"]', $ns)
    $documentExtra = $round.SelectSingleNode('//f:custom-info[@info-type="FB3 document-info"]', $roundNs)
    if (!$writtenExtra -or !$writtenExtra.InnerText.Contains('<country>GB</country>') -or
        !$documentExtra -or !$documentExtra.InnerText.Contains('updated="2026-11-28T08:02:00Z"')) {
        throw 'Unmapped book metadata disappeared during conversion.'
    }
    $sourceTarget = $sourceBody.SelectSingleNode('//*[@id="inner"]')
    $importedTarget = $converted.SelectSingleNode('//*[@id="fb3-anchor-inner"]')
    if (!$importedTarget.InnerText.StartsWith($sourceTarget.InnerText, [StringComparison]::Ordinal)) {
        throw 'The first import changed the text at the target of the table hyperlink.'
    }
    $sourceZip = [IO.Compression.ZipFile]::OpenRead($sourcePackage)
    try {
        $bitmap = $sourceZip.GetEntry('Images/a.png').Open()
        try {
            $data = [IO.MemoryStream]::new()
            try {
                $bitmap.CopyTo($data)
                if ([Convert]::ToBase64String($data.ToArray()) -cne $converted.SelectSingleNode('//f:binary', $ns).InnerText) {
                    throw 'First import changed the actual PNG from the FB3 package.'
                }
            } finally { $data.Dispose() }
        } finally { $bitmap.Dispose() }
    } finally { $sourceZip.Dispose() }

    function Assert-ChapterTree([xml]$document, $format) {
        $rootName = if ($format -eq 'FB3') { 'fb3-body' } else { 'body' }
        $body = $document.SelectSingleNode("(//*[local-name()='$rootName'])[1]")
        $sections = '*[local-name()="section"]'
        $outer = @($body.SelectNodes($sections))
        $inner = @($outer[0].SelectNodes($sections))
        if ($outer.Count -ne 1 -or $inner.Count -ne 1 -or $inner[0].SelectNodes($sections).Count -or
            $outer[0].SelectSingleNode('*[local-name()="title"]').InnerText -cne 'Первая глава' -or
            $inner[0].SelectSingleNode('*[local-name()="p"]').InnerText -cnotlike 'Текст главы *') {
            throw "Changed chapter hierarchy or content in $format."
        }
    }
    function Assert-NoteTargets([xml]$document, $format, [int]$count) {
        $notes = if ($format -eq 'FB3') {
            $document.SelectNodes('//*[local-name()="notebody"]')
        } else {
            $document.SelectNodes('//*[local-name()="body" and @name="notes"]//*[local-name()="section"][@id]')
        }
        $targets = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
        foreach ($note in $notes) {
            if (!$note.GetAttribute('id') -or !$note.InnerText.Trim()) { throw "Empty or anonymous $format note in case with $count notes." }
            $targets.Add($note.GetAttribute('id'), $note.InnerText)
        }
        $links = @($document.SelectNodes('//*[local-name()="note" or (local-name()="a" and @type="note")]'))
        if (@($notes).Count -ne $count -or $links.Count -ne $count) {
            throw "Expected $count notes and links in $format, found $(@($notes).Count) notes and $($links.Count) links."
        }
        for ($index = 0; $index -lt $count; $index++) {
            $link = $links[$index]
            $href = $link.GetAttribute('href')
            if (!$href) { $href = $link.GetAttribute('href', 'http://www.w3.org/1999/xlink') }
            if ($targets[$href.TrimStart('#')] -cne "Текст сноски $($index + 1)") {
                throw "Wrong $format note target for $($link.InnerText): $href"
            }
        }
    }
    # A link to the FB3 book root needs a single FB2 wrapper that is removed on export.
    $rootLinkedBook = [ordered]@{}
    foreach ($part in $book.GetEnumerator()) { $rootLinkedBook.Add($part.Key, $part.Value) }
    $rootLinkedBook['Text/sample.xml'] = $schemaBody.Replace('<p>Проверка схемы</p>', "<p><a xmlns:l='http://www.w3.org/1999/xlink' l:href='#$bookId'>К началу книги</a></p>")
    Check-Package 'book-root-link' $rootLinkedBook 'import'
    $rootLinkedExport = Join-Path $scratch.Root 'book-root-link-export.fb3'
    & $exe export (Join-Path $scratch.Root 'book-root-link.fb2') $runtime $rootLinkedExport
    if ($LASTEXITCODE -ne 0) { throw 'Export of a link to the book root failed.' }
    $rootRoundTrip = Read-BookXml $rootLinkedExport 'body.xml'
    $rootTarget = $rootRoundTrip.SelectSingleNode('//*[local-name()="a"]').GetAttribute('href', 'http://www.w3.org/1999/xlink')
    if ($rootTarget -cne "#$bookId" -or $rootRoundTrip.SelectNodes('//*[local-name()="section"]').Count -ne 1) {
        throw 'Converted book root was duplicated or stopped being the target of the book link.'
    }
    $cycleFailures = [Collections.Generic.List[string]]::new()
    foreach ($noteCount in @(0, 1, 2, 3)) {
        $testBook = [ordered]@{}
        foreach ($part in $book.GetEnumerator()) { $testBook.Add($part.Key, $part.Value) }
        $heading = if ($noteCount -gt 1) { '<title><p>Примечания к главе</p></title>' } else { '' }
        $links = ''
        $notes = ''
        if ($noteCount) {
            foreach ($number in 1..$noteCount) {
                $links += "<note href='n$number'>[$number]</note>"
                $notes += "<notebody id='n$number'><p>Текст сноски $number</p></notebody>"
            }
            if ($noteCount -eq 3) {
                $separate = $notes.IndexOf("<notebody id='n3'>", [StringComparison]::Ordinal)
                $notes = $notes.Insert($separate, '</notes><notes><title><p>Комментарии</p></title>')
            }
            $notes = "<notes>$heading$notes</notes>"
        }
        $testBook['Text/sample.xml'] = "<fb3-body xmlns='$fb3/body' id='$bookId'><section id='9e85883b-d85a-4d7e-b2bf-64db62d59f6b'><title><p>Первая глава</p></title><section id='857466dd-7cbc-4b97-8a32-e438ec65939d'><p>Текст главы $links</p></section></section>$notes</fb3-body>"
        $seriesId = 'aff8a990-59cc-4110-a816-a505ef2ab5da'
        $testBook['Book/meta.xml'] = $book['Book/meta.xml'].
            Replace('<fb3-relations>', "<sequence id='$seriesId' number='2'><title><main>Серия</main></title></sequence><fb3-relations>").
            Replace('</fb3-description>', '<custom-info info-type="Пользователь">Оставить эти данные</custom-info><custom-info info-type="Пустое поле"></custom-info></fb3-description>')
        Check-Package "notes-$noteCount" $testBook 'import'
        $fb2Path = Join-Path $scratch.Root "notes-$noteCount.fb2"
        $sourceXml = Read-BookXml (Join-Path $scratch.Root "notes-$noteCount.fb3") 'sample.xml'
        Assert-NoteTargets $sourceXml 'FB3' $noteCount
        Assert-ChapterTree $sourceXml 'FB3'
        $metadataCounts = [Collections.Generic.List[int]]::new()
        foreach ($cycle in 1..3) {
            $importXml = Read-BookXml $fb2Path ''
            try { Assert-NoteTargets $importXml 'FB2' $noteCount } catch { $cycleFailures.Add($_.Exception.Message) }
            Assert-ChapterTree $importXml 'FB2'
            $custom = $importXml.SelectNodes('//*[local-name()="custom-info"]')
            $metadataCounts.Add($custom.Count)
            if (@($custom | Where-Object { $_.'info-type' -ceq 'Пользователь' -and $_.InnerText -ceq 'Оставить эти данные' }).Count -ne 1) {
                throw "User metadata lost after import $cycle."
            }
            $fb3Path = Join-Path $scratch.Root "notes-$noteCount-export-$cycle.fb3"
            & $exe export $fb2Path $runtime $fb3Path
            if ($LASTEXITCODE -ne 0) { throw "Export $cycle failed for $noteCount notes." }
            $exportXml = Read-BookXml $fb3Path 'body.xml'
            $exportMeta = Read-BookXml $fb3Path 'description.xml'
            $series = $exportMeta.SelectSingleNode('//*[local-name()="sequence"]')
            if ($series.GetAttribute('id') -cne $seriesId -or $series.GetAttribute('number') -cne '2' -or $series.InnerText -cne 'Серия' -or
                !$exportMeta.SelectSingleNode('//*[local-name()="custom-info" and @info-type="Пустое поле"]')) {
                throw 'The source series identity or an empty user metadata field was lost.'
            }
            Assert-NoteTargets $exportXml 'FB3' $noteCount
            Assert-ChapterTree $exportXml 'FB3'
            $groupTitles = @($exportXml.SelectNodes('//*[local-name()="notes"]/*[local-name()="title"]') | ForEach-Object InnerText)
            if ($noteCount -eq 3 -and ($groupTitles -join '|') -cne 'Примечания к главе|Комментарии') {
                throw 'Separate FB3 note groups lost their titles or order.'
            }
            $fb2Path = Join-Path $scratch.Root "notes-$noteCount-import-$cycle.fb2"
            & $exe import $fb3Path (Join-Path $runtime 'FictionBook.xsd') $fb2Path
            if ($LASTEXITCODE -ne 0) { throw "Import $cycle failed for $noteCount notes." }
        }
        $final = Read-BookXml $fb2Path ''
        Assert-NoteTargets $final 'FB2' $noteCount
        Assert-ChapterTree $final 'FB2'
        $metadataCounts.Add($final.SelectNodes('//*[local-name()="custom-info"]').Count)
        if (@($metadataCounts | Sort-Object -Unique).Count -ne 1) {
            $cycleFailures.Add("Custom metadata counts across three cycles: $($metadataCounts -join ', ').")
        }
    }
    if ($cycleFailures.Count) { throw ($cycleFailures -join [Environment]::NewLine) }

    # Exercise FB2 files produced with the old note wrapper, plus a comments body.
    $legacy = Read-BookXml (Join-Path $scratch.Root 'notes-3.fb2') ''
    $legacyBodies = @($legacy.SelectNodes('//*[local-name()="body" and @name="notes"]'))
    $legacyGroup = $legacy.CreateElement('section', $legacyBodies[0].NamespaceURI)
    $legacyOuter = $legacy.CreateElement('section', $legacyBodies[0].NamespaceURI)
    while ($legacyBodies[0].FirstChild) { [void]$legacyGroup.AppendChild($legacyBodies[0].FirstChild) }
    [void]$legacyOuter.AppendChild($legacyGroup)
    [void]$legacyBodies[0].AppendChild($legacyOuter)
    $legacyBodies[1].SetAttribute('name', 'comments')
    $legacyPath = Join-Path $scratch.Root 'legacy-nested-notes.fb2'
    $legacy.Save($legacyPath)
    $legacyExport = Join-Path $scratch.Root 'legacy-nested-notes-export.fb3'
    & $exe export $legacyPath $runtime $legacyExport
    if ($LASTEXITCODE -ne 0) { throw 'Export of legacy nested FB2 groups failed.' }
    $legacyResult = Read-BookXml $legacyExport 'body.xml'
    Assert-NoteTargets $legacyResult 'FB3' 3
    $legacyTitles = @($legacyResult.SelectNodes('//*[local-name()="notes"]/*[local-name()="title"]') | ForEach-Object InnerText)
    if (($legacyTitles -join '|') -cne 'Примечания к главе|Комментарии') {
        throw 'Legacy nested note headers or comments were lost.'
    }

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
    function Get-SemanticStructure([Xml.XmlElement]$node) {
        $structural = @('body', 'section', 'title', 'annotation', 'epigraph', 'cite', 'poem', 'stanza',
            'table', 'tr', 'td', 'th', 'p', 'subtitle', 'v', 'text-author', 'date', 'empty-line', 'image')
        $kind = $node.LocalName
        if ($kind -notin $structural) { return }
        $signature = $kind
        foreach ($attribute in @('name', 'colspan', 'rowspan', 'align', 'valign', 'alt')) {
            $value = $node.GetAttribute($attribute)
            # Both forms have the same effective table alignment.
            if ($attribute -eq 'align' -and $value -eq 'left') { continue }
            if ($attribute -eq 'valign' -and $value -eq 'top') { continue }
            if ($value) { $signature += "|$attribute=$value" }
        }
        if ($kind -in @('p', 'subtitle', 'v', 'text-author', 'date', 'td', 'th')) {
            # Compare individual text blocks, with their positions and meaningful intra-block spacing.
            $signature += '|' + $node.InnerText
        }
        $signature
        foreach ($child in $node.ChildNodes) {
            if ($child -is [Xml.XmlElement]) { Get-SemanticStructure $child }
        }
        '/' + $kind
    }
    function Get-SemanticSnapshot([Xml.XmlDocument]$document) {
        $prefixes = [Xml.XmlNamespaceManager]::new($document.NameTable)
        $prefixes.AddNamespace('f', 'http://www.gribuser.ru/xml/fictionbook/2.0')
        $xlink = 'http://www.w3.org/1999/xlink'
        $targets = [Collections.Generic.Dictionary[string, Xml.XmlElement]]::new([StringComparer]::Ordinal)
        foreach ($target in $document.SelectNodes('//*[@id]', $prefixes)) { $targets.Add($target.GetAttribute('id'), $target) }
        $links = @(
            foreach ($anchor in $document.SelectNodes('//f:a', $prefixes)) {
                $href = $anchor.GetAttribute('href', $xlink)
                if ($href.StartsWith('#')) {
                    $target = $targets[$href.Substring(1)]
                    if (!$target) { throw "Unresolved converted link $href" }
                    $address = ''
                    for ($node = $target; $node -is [Xml.XmlElement]; $node = $node.ParentNode) {
                        $ordinal = 0
                        for ($sibling = $node.PreviousSibling; $sibling; $sibling = $sibling.PreviousSibling) {
                            if ($sibling -is [Xml.XmlElement] -and $sibling.LocalName -ceq $node.LocalName) { ++$ordinal }
                        }
                        $address = '/' + $node.LocalName + '[' + $ordinal + ']' + $address
                    }
                    $href = $address + '|' + $target.InnerText
                }
                $anchor.GetAttribute('type') + '|' + $anchor.InnerText + '|' + $href
            }
        )
        $images = @(
            foreach ($image in $document.SelectNodes('//f:image', $prefixes)) {
                $href = $image.GetAttribute('href', $xlink)
                $binary = $targets[$href.TrimStart('#')]
                if (!$binary -or $binary.LocalName -cne 'binary') { throw "Unresolved converted image $href" }
                $binary.GetAttribute('content-type') + '|' + $image.GetAttribute('alt') + '|' + $binary.InnerText
            }
        )
        $fields = @(
            $document.SelectNodes('/f:FictionBook/f:description/f:title-info/*[not(self::f:coverpage)] | ' +
                '/f:FictionBook/f:description/f:document-info/* | /f:FictionBook/f:description/f:publish-info/*', $prefixes) |
                ForEach-Object { $_.OuterXml -creplace ' id="[^"]+"', '' }
        )
        $custom = @(
            foreach ($field in $document.SelectNodes('/f:FictionBook/f:description/f:custom-info', $prefixes)) {
                $field.GetAttribute('info-type') + '|' + $field.InnerText
            }
        )
        return [ordered]@{
            Structure = @($document.SelectNodes('/f:FictionBook/f:body', $prefixes) | ForEach-Object { Get-SemanticStructure $_ })
            Links = $links
            Images = $images
            Binaries = @($document.SelectNodes('/f:FictionBook/f:binary', $prefixes) | ForEach-Object InnerText | Sort-Object -CaseSensitive)
            Metadata = $fields
            Custom = $custom
        }
    }
    $failedRoundTrips = @()
    foreach ($package in $RoundTripReference) {
        $path = (Resolve-Path -LiteralPath $package).Path
        Write-Host "Round-trip $path"
        try {
            $inputPackage = $path
            $initial = $null
            foreach ($cycle in 1..3) {
                $fb2 = Join-Path $scratch.Root "reference-$cycle.fb2"
                & $exe import $inputPackage (Join-Path $runtime 'FictionBook.xsd') $fb2
                if ($LASTEXITCODE -ne 0) { throw "Import failed in cycle $cycle." }
                $snapshot = Get-SemanticSnapshot (Read-BookXml $fb2 '')
                if ($cycle -eq 1) { $initial = $snapshot }
                else {
                    $differences = @(
                        foreach ($key in $snapshot.Keys) {
                            if (($initial[$key] -join [char]0) -cne ($snapshot[$key] -join [char]0)) { $key }
                        }
                    )
                    if ($differences.Count) {
                        foreach ($key in $differences) {
                            $change = @(Compare-Object $initial[$key] $snapshot[$key] -CaseSensitive | Select-Object -First 4)
                            foreach ($item in $change) {
                                $value = [string]$item.InputObject
                                Write-Host "$key $($item.SideIndicator) $($value.Substring(0, [Math]::Min(150, $value.Length)))"
                            }
                        }
                        $prefix = Join-Path $scratch.Root ("comparison-" + [IO.Path]::GetFileNameWithoutExtension($path))
                        $initial | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$prefix-first.json" -Encoding utf8
                        $snapshot | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$prefix-cycle-$cycle.json" -Encoding utf8
                        throw "Cycle $cycle changed $($differences -join ', '). See $prefix."
                    }
                }
                if ($cycle -lt 3) {
                    $inputPackage = Join-Path $scratch.Root "reference-$cycle.fb3"
                    & $exe export $fb2 $runtime $inputPackage
                    if ($LASTEXITCODE -ne 0) { throw "Export failed in cycle $cycle." }
                }
            }
            Write-Host "Verified three imports and two exports of $path"
        } catch {
            Write-Host "Round-trip failed for $path. $($_.Exception.Message)"
            $failedRoundTrips += $path
        }
    }

    if ($failedRoundTrips.Count) { throw ('Reference round-trip failed: ' + ($failedRoundTrips -join ', ')) }
    $passed = $true
    "FB3 OPC package tests passed. Sanitizer: $Sanitizer."
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
