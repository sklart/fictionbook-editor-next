[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\Words.cpp')
$match = [regex]::Match($source, '(?s)// Replace words(?<body>.*?)\r?\n\treturn true;\r?\n}')
if (-not $match.Success) { throw 'Words replacement loop was not found.' }
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$out = Join-Path $root 'out\tests\words-undo-balance'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$test = @'
#include <atlbase.h>
#include <atlstr.h>
#include <vector>
#include <cstdio>
#include <memory>
#include <new>
#include <comdef.h>

constexpr int HASREPL = 1;
constexpr int IDS_REPL_WORDS_CPT = 0, IDS_REPL_WORDS_MSG = 0;
int messages = 0;
namespace U { void MessageBox(int, int, int, int) { ++messages; } }
struct CWaitCursor { CWaitCursor() {} };
struct FakeDom { int getElementById(LPCWSTR) { return 0; } };
namespace MSHTML { using IHTMLDocument3Ptr = FakeDom*; }
struct Word { CString word, replacement; int flags, count; };
struct Words {
    std::vector<Word> items;
    int GetSize() const { return static_cast<int>(items.size()); }
    Word& operator[](int index) { return items[static_cast<size_t>(index)]; }
    void RemoveAt(int index) { items.erase(items.begin() + index); }
};
struct Body {
    int depth, fault, starts = 0, ends = 0, replacements = 0;
    FakeDom dom;
    Body(int initial, int failure) : depth(initial), fault(failure) {}
    void BeginUndoUnit(LPCWSTR) { if (fault == 3) throw _com_error(E_FAIL); ++starts; ++depth; }
    void EndUndoUnit() { ++ends; --depth; }
    Body* MarkupServices() { return this; }
    FakeDom* Document() { if (fault == 1) throw std::bad_alloc(); return &dom; }
    int ReplaceToolWordsRe(const CString&, const CString&, int, bool, LPCWSTR, void*, void*, int) {
        if (fault == 2) throw _com_error(E_FAIL);
        ++replacements; return 1;
    }
};
namespace MSHTML { using IMarkupServices2Ptr = Body*; }
#include "@@ROOT@@/src/fbe/dom/MarkupUndoUnitScope.h"
struct Document { Body m_body; Document(int initial, int fault) : m_body(initial, fault) {} };
bool Run(Words& hwords, Document& document) {
    int i = 0;
    const wchar_t* pattern = L"%s%s";
@@BODY@@
    return true;
}

int main() {
    int failures = 0, cases = 0;
    const Word skipped = { L"one-word", L"other", 0, 1 };
    const Word identical = { L"one-word", L"one-word", HASREPL, 1 };
    const Word changed = { L"one-word", L"oneword", HASREPL, 1 };
    const std::vector<std::vector<Word>> inputs = {
        {}, { skipped }, { identical }, { identical, identical },
        { changed }, { changed, changed }, { skipped, identical, changed, identical, changed }
    };
    for (int outerDepth : { 0, 1 }) {
      for (int fault : { 0, 1, 2, 3 }) {
        for (const auto& input : inputs) {
            int realChanges = 0, attempted = 0;
            for (const auto& word : input) {
                if (word.flags & HASREPL) { ++attempted; if (word.word != word.replacement) ++realChanges; }
            }
            Words words = { input };
            Document document(outerDepth, fault);
            messages = 0;
            bool result = false, threw = false;
            try { result = Run(words, document); }
            catch (const _com_error&) { threw = true; }
            catch (const std::bad_alloc&) { threw = true; }
            const bool expectedThrow = fault != 0 && realChanges > 0;
            const int expectedGroups = realChanges && fault != 3 ? 1 : 0;
            ++cases;
            if (threw != expectedThrow || result == expectedThrow || document.m_body.starts != expectedGroups || document.m_body.ends != expectedGroups ||
                document.m_body.depth != outerDepth || document.m_body.replacements != (expectedThrow ? 0 : realChanges) ||
                messages != (attempted && !expectedThrow ? 1 : 0)) {
                fprintf(stderr, "FAIL case=%d outer=%d fault=%d begin=%d end=%d depth=%d replacements=%d messages=%d\n",
                    cases, outerDepth, fault, document.m_body.starts, document.m_body.ends, document.m_body.depth,
                    document.m_body.replacements, messages);
                ++failures;
            }
        }
      }
    }
    printf("Words Undo balance: cases=%d failures=%d\n", cases, failures);
    return failures ? 1 : 0;
}
'@
$test = $test.Replace('@@BODY@@', $match.Groups['body'].Value)
$test = $test.Replace('@@ROOT@@', $root.Replace('\', '/'))
$cpp = Join-Path $out 'words-undo-balance.cpp'
[IO.File]::WriteAllText($cpp, $test, [Text.UTF8Encoding]::new($false))
$exe = Join-Path $out 'words-undo-balance.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT /W4 /WX /utf-8 /DUNICODE /D_UNICODE "/Fo$out\\" $cpp /link comsuppw.lib "/OUT:$exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
