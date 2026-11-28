[CmdletBinding()]
param([string[]]$ReferencePackage = @(), [string[]]$ReferenceWithoutCover = @())

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'fb3-package'
$passed = $false
try {
    $archive = Join-Path $root 'build/libarchive/install/Release'
    $zlib = Join-Path $root 'build/zlib/install/Release'
    $exe = Join-Path $scratch.Root 'fb3-package-test.exe'
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /Zc:wchar_t- /DUNICODE /D_UNICODE `
        ('/I' + (Join-Path $root 'build/generated/Win32/Release/fbe-api')) ('/I' + (Join-Path $archive 'include')) `
        "/Fo$($scratch.Root)\\" "/Fe$exe" (Join-Path $PSScriptRoot 'fb3-package-test.cpp') `
        (Join-Path $root 'src/common/fb3/Fb3Package.cpp') (Join-Path $root 'src/import-epub/SecureEpubZip.cpp') `
        /link (Join-Path $archive 'lib/archive.lib') (Join-Path $zlib 'lib/zs.lib') ole32.lib oleaut32.lib shlwapi.lib
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
                $writer = [IO.StreamWriter]::new($zip.CreateEntry($part.Key).Open(), [Text.UTF8Encoding]::new($false))
                try { $writer.Write($part.Value) } finally { $writer.Dispose() }
            }
        } finally { $zip.Dispose() }
        & $exe $expected $path
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
    $passed = $true
    'FB3 OPC package tests passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
