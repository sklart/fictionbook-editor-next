<# Native regression for XML declaration encoding parsing. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$directory = Join-Path ([IO.Path]::GetTempPath()) ("fbe-xml-declaration-$PID")
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $exe = Join-Path $directory 'xml-declaration-test.exe'
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /I (Join-Path $root 'src\fbe') "/Fo$directory\\" (Join-Path $root 'tools\tests\xml-declaration-test.cpp') /Fe$exe
    if ($LASTEXITCODE -ne 0) { throw 'XML declaration helper compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'XML declaration helper regression failed.' }

    function Assert-SerializedEncoding([string] $encoding) {
        $source = "<?xml version='1.0' encoding='$encoding' standalone=`"yes`"?><FictionBook>Привет</FictionBook>"
        $document = New-Object -ComObject Msxml2.DOMDocument.6.0
        $document.async = $false
        if (-not $document.loadXML($source)) { throw "MSXML rejected Source fixture for ${encoding}: $($document.parseError.reason)" }
        $path = Join-Path $directory ("saved-" + $encoding + '.xml')
        $document.save($path)
        $bytes = [IO.File]::ReadAllBytes($path)
        $actual = [Text.Encoding]::GetEncoding($encoding).GetString($bytes)
        if ($actual -notmatch ('^<\?xml\s+[^?]*encoding\s*=\s*["'']' + [regex]::Escape($encoding) + '["''][^?]*\?>')) {
            throw "Saved $encoding fixture does not declare the selected encoding."
        }
        if ($actual -notmatch '<FictionBook>Привет</FictionBook>') {
            throw "Saved $encoding fixture is not decodable using its XML declaration."
        }
    }

    # This follows the Source -> Body -> Save route: XML supplied by Source is
    # parsed, then MSXML saves through the declaration selected by the editor.
    Assert-SerializedEncoding 'utf-8'
    Assert-SerializedEncoding 'windows-1251'
    Write-Host 'XML declaration native regression passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
