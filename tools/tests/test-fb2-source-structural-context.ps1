[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('Fb2StructuralContext.' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary -Force | Out-Null
try {
    $toolchainArguments = @{ Arch = 'x86'; HostArch = 'x64' }
    $toolchainArguments.VcVarsVersion = '14.44'
    & (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') @toolchainArguments
    $sourcePath = Join-Path $temporary 'structural-context-test.cpp'; $exePath = Join-Path $temporary 'structural-context-test.exe'
    @'
#include <cstdlib>
#include <iostream>
#include <string>
#include "Fb2SourceStructuralContext.h"
struct Reader : Fb2SourceTextReader {
 std::string text; size_t chunk, calls, bytes;
 Reader(const std::string& v, size_t c) : text(v), chunk(c), calls(0), bytes(0) {}
 size_t Length() const { return text.size(); }
 void Read(size_t p, size_t n, std::string& out) const { Reader* self=const_cast<Reader*>(this); n=std::min(n, text.size()-p); out.assign(text, p, n); ++self->calls; self->bytes+=n; }
};
static void Need(bool value, const char* message) { if(!value) { std::cerr << message << std::endl; std::exit(1); } }
static Fb2SourceStructuralContext Resolve(const std::string& text, int ch, size_t chunk) { Reader r(text,chunk); Fb2SourceStructuralContextResolver x(chunk); return x.Resolve(r,text.size(),ch); }
int main() {
 const std::string deep(400 * 1024, 'x');
 Need(Resolve("<section>" + deep + "<", '<', 7).parentElement == "section", "deep parent");
 Need(Resolve("<section>" + deep + "</", '/', 13).closingElement == "section", "deep closing");
 Need(Resolve("<section><cite>" + deep + "</", '/', 31).closingElement == "cite", "nested closing");
 Need(Resolve("<section><cite></cite>" + deep + "</", '/', 7).closingElement == "section", "nested parent after close");
 Need(Resolve("<section><image l:href='#x'/><empty-line/>" + deep + "</", '/', 13).closingElement == "section", "self closing");
 Need(Resolve("<!-- <fake></fake>" + deep + "<", '<', 7).suppressed, "deep comment suppression");
 Need(Resolve("<![CDATA[<fake></fake>" + deep + "<", '<', 13).suppressed, "deep cdata suppression");
 Need(Resolve("<?test x='<fake>' " + deep + "<", '<', 31).suppressed, "deep PI suppression");
 Need(Resolve("<!-- <fake></fake> -->\n<section title=\"1 > 0\">" + deep + "</", '/', 7).closingElement == "section", "comment and quoted greater-than");
 Need(Resolve("<section title='1 > 0'>" + deep + "</", '/', 13).closingElement == "section", "single quote greater-than");
	 Fb2SourceStructuralContext breadcrumb = Resolve("<FictionBook><body><section><title>Text", 0, 17); Need(breadcrumb.breadcrumb.size() == 4 && breadcrumb.breadcrumb[0] == "FictionBook" && breadcrumb.breadcrumb[3] == "title", "breadcrumb");
	 Need(Resolve("<FictionBook><body><section></section>", 0, 17).breadcrumb.size() == 2, "breadcrumb after close");
 Reader near(std::string(4*1024*1024, 'x') + "<section><", 31); Fb2SourceStructuralContextResolver resolver(32*1024); Need(resolver.Resolve(near, near.text.size(), '<').parentElement == "section", "near fixture parent"); Need(near.bytes < near.text.size(), "near context read whole document");
 Reader budget(std::string(8*1024*1024, 'x') + "<section><title>Text", 31); Fb2SourceStructuralContextResolver limited(32*1024); Fb2SourceStructuralContext result=limited.Resolve(budget,budget.text.size(),0); Need(result.breadcrumbTruncated && result.breadcrumb.size() == 2 && result.breadcrumb[0] == "section", "breadcrumb budget result"); Need(budget.bytes <= 512*1024, "breadcrumb read budget");
 return 0;
}
'@ | Set-Content -LiteralPath $sourcePath -Encoding utf8
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /MT "/I$repoRoot\src\fbe\source" "/Fe:$exePath" $sourcePath (Join-Path $repoRoot 'src\fbe\source\Fb2SourceStructuralContext.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Structural context test did not compile.' }
    & $exePath
    if ($LASTEXITCODE -ne 0) { throw 'Structural context test failed.' }
    Write-Host 'FB2 structural context behavioral test passed.'
} finally { Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue }
