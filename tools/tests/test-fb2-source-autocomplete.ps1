[CmdletBinding()]
param(
    [ValidateSet('Modern', 'Win7')]
    [string]$CompatibilityTarget = 'Modern'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$temporary = Join-Path ([System.IO.Path]::GetTempPath()) ('Fb2SourceAutocomplete.' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary -Force | Out-Null
try {
    $toolchainArguments = @{ Arch = 'x86'; HostArch = 'x64' }
    if ($CompatibilityTarget -eq 'Win7') { $toolchainArguments.VcVarsVersion = '14.44' }
    & (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') @toolchainArguments
    $sourcePath = Join-Path $temporary 'autocomplete-test.cpp'
    $exePath = Join-Path $temporary 'autocomplete-test.exe'
    @'
#include <cstdlib>
#include <iostream>
#include <string>
#include "Fb2SourceAutocomplete.h"

static void Require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << std::endl; std::exit(1); }
}

static void RequireCandidates(Fb2SourceAutocomplete& autocomplete, const std::string& text, int character, const char* expected) {
    const Fb2AutocompleteResult result = autocomplete.Complete(text, character);
    Require(result.candidates == expected, expected);
}

int main() {
    Fb2SourceAutocomplete autocomplete;
    RequireCandidates(autocomplete, "<!-- <", '<', "");
    RequireCandidates(autocomplete, "<![CDATA[<", '<', "");
    RequireCandidates(autocomplete, "<?test <", '<', "");
    RequireCandidates(autocomplete, "<!-- closed -->\n<", '<', "FictionBook");
    RequireCandidates(autocomplete, "<FictionBook><body></", '/', "body>");
    RequireCandidates(autocomplete, "<section ", ' ', "id= xml:lang=");
    RequireCandidates(autocomplete, "<section id=\"part\" ", ' ', "xml:lang=");
    RequireCandidates(autocomplete, "<image ", ' ', "alt= id= title= xlink:href= xlink:type=");
    RequireCandidates(autocomplete, "<a ", ' ', "type= xlink:href= xlink:type=");
    RequireCandidates(autocomplete, "<a xlink:", ':', "href= type=");
    Require(autocomplete.Complete("<a xlink:href=\"#", '#').needsDocumentIds, "href ID completion was not requested");
    Require(autocomplete.Complete("<a xlink:href='#", '#').needsDocumentIds, "single-quoted href ID completion was not requested");
    Require(autocomplete.Complete("<FictionBook xmlns:foo=\"http://www.w3.org/1999/xlink\"><image foo:href=\"#", '#').needsDocumentIds, "custom XLink prefix ID completion was not requested");
    Require(!autocomplete.Complete("<section id=\"#", '#').needsDocumentIds, "section id must not request document IDs");
    Require(!autocomplete.Complete("<p style=\"#", '#').needsDocumentIds, "style must not request document IDs");
    Require(!autocomplete.Complete("<binary content-type=\"#", '#').needsDocumentIds, "content-type must not request document IDs");
    const std::string idsFixture =
        "<FictionBook><body><section id=\"section-1\"><p id='p-1'>Text</p></section></body>"
        "<binary id=\"image-1\" content-type=\"image/jpeg\"/><p grid=\"false-1\"/>"
        "<p dataid=\"false-2\"/><p some-id=\"false-3\"/>"
        "<!-- <section id=\"false-comment\"/> --><![CDATA[<section id=\"false-cdata\"/>]]>"
        "<?test id=\"false-pi\"?></FictionBook>";
    Require(autocomplete.CompleteIds(idsFixture) == "image-1 p-1 section-1", "XML-aware document ID extraction failed");
    return 0;
}
'@ | Set-Content -LiteralPath $sourcePath -Encoding utf8
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /MT `
        "/I$repoRoot\src\fbe" `
        "/Fe:$exePath" $sourcePath (Join-Path $repoRoot 'src\fbe\Fb2SourceAutocomplete.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Не удалось собрать behavioral test FB2 autocomplete.' }
    & $exePath
    if ($LASTEXITCODE -ne 0) { throw 'Behavioral test FB2 autocomplete завершился с ошибкой.' }
    Write-Host 'FB2 autocomplete behavioral test passed.'
}
finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}
