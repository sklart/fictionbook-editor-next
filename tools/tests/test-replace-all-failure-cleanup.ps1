[CmdletBinding()]
param([string]$SourcePath, [ValidateSet('All', 'Global', 'Single')][string]$Operation = 'All')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw $(if ($SourcePath) { $SourcePath } else { Join-Path $root 'src\fbe\FBEview.cpp' })
if ($Operation -eq 'All') {
    $function = [regex]::Match($source, '(?s)int CFBEView::ReplaceAllSearchCore\(CString\* errorText\).*?\r?\n}')
    $tail = [regex]::Match($function.Value, '(?s)\tint replaced = 0;(?<tail>.*?)\r?\n}')
    if (-not $tail.Success) { throw 'Replace All mutation body was not found.' }
    $body = 'int replaced = 0;' + $tail.Groups['tail'].Value
} elseif ($Operation -eq 'Global') {
    $function = [regex]::Match($source, '(?s)int CFBEView::GlobalReplace\(MSHTML::IHTMLElementPtr elem, CString cntTag\).*?\r?\n}')
    $start = [regex]::Match($function.Value, 'bool undoStarted = false;|std::unique_ptr<FbeDom::MarkupUndoUnitScope> undo;')
    if (-not $start.Success) { throw 'GlobalReplace mutation owner was not found.' }
    $tryOpen = $function.Value.IndexOf('{', $function.Value.IndexOf('try', $start.Index))
    $snapshot = $function.Value.IndexOf('const AU::Search::SearchTextSnapshot& snapshot', $tryOpen)
    if ($tryOpen -lt 0 -or $snapshot -lt 0) { throw 'GlobalReplace mutation body was not found.' }
    $prefix = $function.Value.Substring($start.Index, $tryOpen - $start.Index + 1)
    $body = $prefix + $(if ($prefix -notmatch 'int replaced = 0;') { "`nint replaced = 0;`n" }) + $function.Value.Substring($snapshot).TrimEnd()
    $body = $body.Substring(0, $body.Length - 1)
} else {
    $function = [regex]::Match($source, '(?s)void\s+CFBEView::DoReplace\(\)\s*\{(?<body>.*?)\r?\n}')
    if (-not $function.Success) { throw 'Single Replace body was not found.' }
    $body = ($function.Groups['body'].Value -replace '\breturn\s*;', 'return state.writes;') + "`nreturn state.writes;"
}
if ($Operation -ne 'Single' -and $function.Value -notmatch '(?s)const std::size_t count = .*?if \(count == 0\)\s*return 0;') { throw 'Zero-result preflight was not found.' }
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$out = Join-Path $root 'out\tests\replace-all-failure-cleanup'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$test = @'
#include <atlbase.h>
#include <atlstr.h>
#include <comdef.h>
#include <vector>
#include <string>
#include <memory>
#include <new>
#include <cstdio>
struct State {
    int fault = 0, initial = 0, depth = 0, starts = 0, ends = 0, writes = 0;
    int reads = 0, advances = 0, completion = 0, posts = 0, reports = 0;
    HRESULT reported = S_OK;
    bool controlled = false;
};
State* active = nullptr;
constexpr int operation = @@OP@@;
namespace U {
void ReportError(_com_error& error) { ++active->reports; active->reported = error.Error(); }
void ReportError(HRESULT error) { ++active->reports; active->reported = error; }
int MessageBox(HWND, const CString&, const CString&, UINT) { return 0; }
}
CString FbeLoadRuntimeStringByKey(LPCWSTR, LPCWSTR fallback) { return CString(fallback); }
struct Markup {
    State& state;
    void BeginUndoUnit(LPCWSTR) {
        if (state.fault == 5) throw _com_error(E_FAIL);
        ++state.starts; ++state.depth;
    }
    void EndUndoUnit() {
        ++state.ends;
        if ((state.ends == 1 && state.fault == 4) || state.fault == 7) throw _com_error(E_FAIL);
        --state.depth;
    }
};
namespace MSHTML { using IMarkupServices2Ptr = Markup*; }
#include "@@ROOT@@/src/fbe/dom/MarkupUndoUnitScope.h"
namespace AU { namespace Search {
struct SearchCapture { bool Matched = false; size_t Start = 0, Length = 0; };
struct SearchHit { size_t Start = 0, Length = 1; std::vector<SearchCapture> Captures; };
struct SearchResult { SearchHit Hit; };
struct ReplacementFormattingRun {};
struct SearchTextSnapshot { std::wstring Text = L"aa"; };
} struct IMatch2 { long Length = 1; IMatch2(const CString&, int) {} void AddSubMatch(const CString&) {} };
constexpr UINT WM_FINALIZE_REPLACE_ALL_COMPLETION = WM_APP + 1;
}
struct Results {
    State& state; AU::Search::SearchResult result;
    const AU::Search::SearchResult* GetAt(size_t) {
        if (++state.reads == 2 && state.fault == 2) throw std::bad_alloc();
        if (state.reads == 2 && state.fault == 8) throw ATL::CAtlException(E_OUTOFMEMORY);
        return &result;
    }
};
struct Coordinator {
    State& state; Results results; AU::Search::SearchTextSnapshot snapshot;
    const AU::Search::SearchTextSnapshot& GetSnapshot() {
        if (state.fault == 1) throw std::bad_alloc();
        return snapshot;
    }
    Results& GetResults() { return results; }
};
class DesignSearchController {
public:
    State& state; struct Coordinator coordinator;
    void SetControlledReplaceAllMutation(bool value) { state.controlled = value; }
    struct Coordinator& Coordinator() { return coordinator; }
    void ClearReplacePreview() {}
    void SetReplaceAllCompletion(int value) { state.completion = value; }
};
struct Range {
    struct Text {
        State& state;
        void operator=(LPCWSTR) {
            if (state.fault == 6) throw _com_error(E_FAIL);
            ++state.writes;
        }
    } text;
    void moveStart(LPCWSTR, int) { if (active->fault == 2) throw std::bad_alloc(); }
    void select() {}
};
namespace MSHTML { using IHTMLTxtRangePtr = Range*; }
struct Selector {
    Range* range = nullptr;
    Range* createRange() { if (range && active->fault == 1) throw std::bad_alloc(); return range; }
};
struct HtmlDocument { Selector* selection; };
enum class ReplacementPreflightResult { Allowed, CrossParagraph };
ReplacementPreflightResult CheckReplacementRange(Range*, bool) { return ReplacementPreflightResult::Allowed; }
CString CrossParagraphReplacementError() { return CString(L"cross paragraph"); }
struct FindOptions { bool fRegexp = true; CString replacement = L"x"; bool hasMatch = true; AU::IMatch2* match = nullptr; void ClearMatch() {} };
CString PrepareRegexReplacementText(const CString& value, AU::IMatch2*, std::vector<AU::Search::ReplacementFormattingRun>&) {
    if (operation == 2 && active->fault == 8) throw ATL::CAtlException(E_OUTOFMEMORY);
    return value;
}
void NormalizeReplacementNbsp(CString&) {}
void ApplyReplacementFormatting(Range*, const CString&, const std::vector<AU::Search::ReplacementFormattingRun>&) {
    if (active->fault == 3) throw _com_error(E_FAIL);
}
BOOL FakePostMessage(HWND, UINT, WPARAM, LPARAM) { ++active->posts; return TRUE; }
#undef PostMessage
#define PostMessage FakePostMessage
struct Frame {
    State& state;
    DesignSearchController m_design_search;
    Markup* m_mk_srv;
    FindOptions m_fo;
    HWND m_hWnd = nullptr;
    Selector selector;
    HtmlDocument document = { &selector };
    HtmlDocument* Document() { return &document; }
    void AdvanceSearchDocumentGeneration() { ++state.advances; }
    void OnFinalizeReplaceAllCompletion(UINT, WPARAM, LPARAM, BOOL&) {}
    int Run(size_t count, const std::vector<Range*>& ranges, CString* errorText) {
        (void)count; (void)ranges; (void)errorText;
@@BODY@@
    }
};
int main() {
    int cases = 0, failures = 0;
    for (int initial : { 0, 1 }) for (int mode : { 0, 1, 2 }) for (int fault = 0; fault <= 8; ++fault) {
        State state; state.fault = fault; state.initial = initial; state.depth = initial; active = &state;
        Markup markup = { state };
        DesignSearchController search = { state, { state, { state, {} }, {} } };
        if (mode == 2) search.coordinator.results.result.Hit.Length = 0;
        Frame frame = { state, search, &markup, {}, nullptr };
        if (mode == 2) frame.m_fo.replacement.Empty();
        Range range = { { state } };
        const size_t count = mode == 1 ? 0 : 2;
        std::vector<Range*> ranges(count, &range);
        frame.selector.range = count ? &range : nullptr;
        AU::IMatch2 match(CString(L"aa"), 0); match.Length = mode == 2 ? 0 : 1;
        frame.m_fo.match = &match;
        CString error; int result = -99; bool escaped = false;
        try { result = count == 0 && operation != 2 ? 0 : frame.Run(count, ranges, &error); }
        catch (const std::bad_alloc&) { escaped = true; }
        catch (const _com_error&) { escaped = true; }
        catch (const ATL::CAtlException&) { escaped = true; }
        const bool expectedError = mode != 1 && (fault == 1 || fault == 8 ||
            (operation == 2 ? mode == 0 && fault >= 2 : fault == 2 || (mode == 0 && fault >= 3)));
        const int expectedResult = operation == 2 ? state.writes : expectedError ? (operation == 0 ? -1 : 0) : mode == 0 ? 2 : 0;
        ++cases;
        const int expectedDepth = initial + (fault == 7 && mode == 0 ? 1 : 0);
        if (escaped || result != expectedResult || state.controlled || state.depth != expectedDepth ||
            (expectedError && operation == 0 && error.IsEmpty()) || state.advances != ((operation != 0 || expectedError) && state.writes > 0 ? 1 : 0) ||
            state.completion != (operation == 0 && expectedResult > 0 ? expectedResult : 0) ||
            state.reports != (operation != 0 && expectedError ? 1 : 0) || (expectedError && operation != 0 && !FAILED(state.reported))) {
            ++failures;
            fprintf(stderr, "FAIL case=%d mode=%d fault=%d escaped=%d result=%d controlled=%d depth=%d writes=%d advances=%d\n",
                cases, mode, fault, escaped, result, state.controlled, state.depth, state.writes, state.advances);
        }
    }
    printf("Replace failure cleanup: operation=%d cases=%d failures=%d\n", operation, cases, failures);
    return failures ? 1 : 0;
}
'@
$operationId = switch ($Operation) { 'All' { 0 } 'Global' { 1 } 'Single' { 2 } }
$test = $test.Replace('@@ROOT@@', $root.Replace('\', '/')).Replace('@@BODY@@', $body).Replace('@@OP@@', [string]$operationId)
$cpp = Join-Path $out ("failure-cleanup-$Operation.cpp")
[IO.File]::WriteAllText($cpp, $test, [Text.UTF8Encoding]::new($false))
$exe = Join-Path $out ("failure-cleanup-$Operation.exe")
& cl.exe /nologo /EHsc /std:c++17 /MT /W4 /WX /utf-8 /DUNICODE /D_UNICODE "/Fo$out\\" $cpp /link comsuppw.lib "/OUT:$exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
