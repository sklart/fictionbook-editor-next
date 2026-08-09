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
    Require(autocomplete.CompleteIds("<p id=\"one\"/><p id='two'/>") == "one two", "document IDs were not collected");
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
