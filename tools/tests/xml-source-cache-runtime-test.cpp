#include "../../src/fbe/stdafx.h"
#include "../../src/fbe/EditorEngine.h"
#include "../../src/fbe/xmlMatchedTagsHighlighter.h"

#include <iostream>
#include <string>
#include <vector>

CAppModule _Module;

namespace {

bool Check(bool value, const char* message)
{
	if (!value)
		std::cerr << "FAILED: " << message << "\n";
	return value;
}

struct CounterSnapshot {
	unsigned long long documentReads;
	unsigned long long matcherBuilds;
	unsigned long long diagnosticRefreshes;
};

CounterSnapshot Snapshot(const XmlMatchedTagsState& state)
{
	return { state.testCounters.documentReadCount, state.testCounters.matcherBuildCount,
		state.testCounters.diagnosticRefreshCount };
}

bool SameCounters(const XmlMatchedTagsState& state, const CounterSnapshot& expected)
{
	return state.testCounters.documentReadCount == expected.documentReads &&
		state.testCounters.matcherBuildCount == expected.matcherBuilds &&
		state.testCounters.diagnosticRefreshCount == expected.diagnosticRefreshes;
}

void SetDocument(HWND editor, const std::string& text, XmlMatchedTagsState& state)
{
	SendMessage(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
	SendMessage(editor, SCI_EMPTYUNDOBUFFER, 0, 0);
	state.Invalidate();
}

bool VerifyLargeDocumentCache(HWND editor, XmlSourceTagHighlighter& highlighter,
	XmlMatchedTagsState& state, const XmlTagHighlightOptions& options)
{
	std::string document("<FictionBook>");
	document.reserve(900000);
	std::vector<size_t> tagPositions;
	tagPositions.reserve(12000);
	for (int i = 0; i < 12000; ++i) {
		tagPositions.push_back(document.size());
		document += "<section id=\"s" + std::to_string(i) + "\"><p class=\"body\">text</p></section>\n";
	}
	document += "</FictionBook><broken><p>text</broken></p>";
	SetDocument(editor, document, state);
	const size_t firstTag = document.find("<section");
	SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(firstTag + 1), 0);
	if (!Check(highlighter.UpdateHighlight(options), "initial large-document match")) return false;
	if (!Check(state.testCounters.documentReadCount == 1 && state.testCounters.matcherBuildCount == 1,
		"initial parse performs exactly one document read and matcher build")) return false;
	if (!Check(state.testCounters.diagnosticRefreshCount == 1 && !state.diagnosticRanges.empty(),
		"initial parse builds structural diagnostics")) return false;
	const CounterSnapshot initial = Snapshot(state);

	for (int move = 0; move < 1000; ++move) {
		const size_t position = tagPositions[static_cast<size_t>(move % 12000)];
		SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(position + 1 + (move % 7)), 0);
		highlighter.UpdateHighlight(options);
	}
	if (!Check(SameCounters(state, initial), "1000 caret moves reuse the cached document, matcher and diagnostics")) return false;
	if (!Check(state.testCounters.resultLookupCount >= 1000, "caret moves still perform local result lookups")) return false;
	std::cout << "initial documentReadCount=" << initial.documentReads << " matcherBuildCount=" << initial.matcherBuilds
		<< " diagnosticRefreshCount=" << initial.diagnosticRefreshes << std::endl;
	std::cout << "after 1000 caret moves documentReadCount=" << state.testCounters.documentReadCount
		<< " matcherBuildCount=" << state.testCounters.matcherBuildCount
		<< " diagnosticRefreshCount=" << state.testCounters.diagnosticRefreshCount << std::endl;

	for (int scroll = 0; scroll < 100; ++scroll) SendMessage(editor, SCI_LINESCROLL, 0, 1);
	highlighter.UpdateHighlight(options);
	if (!Check(SameCounters(state, initial), "scroll/update activity does not rebuild the XML cache")) return false;

	const size_t text = document.find(">text</p>");
	SendMessage(editor, SCI_SETTARGETSTART, static_cast<WPARAM>(text + 1), 0);
	SendMessage(editor, SCI_SETTARGETEND, static_cast<WPARAM>(text + 5), 0);
	SendMessage(editor, SCI_REPLACETARGET, 7, reinterpret_cast<LPARAM>("changed"));
	const unsigned long long previousRevision = state.documentRevision;
	state.Invalidate(); // Mirrors the SC_MOD_INSERTTEXT/SC_MOD_DELETETEXT handler in CMainFrame.
	if (!Check(state.documentRevision == previousRevision + 1 && state.matcherRevision != state.documentRevision,
		"text edit makes the matcher logically dirty without reading immediately")) return false;
	SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(firstTag + 1), 0);
	highlighter.UpdateHighlight(options);
	if (!Check(state.testCounters.documentReadCount == initial.documentReads + 1 &&
		state.testCounters.matcherBuildCount == initial.matcherBuilds + 1 &&
		state.testCounters.diagnosticRefreshCount == initial.diagnosticRefreshes + 1 &&
		state.matcherRevision == state.documentRevision, "next XML request rebuilds exactly once after edit")) return false;
	std::cout << "after one text edit documentReadCount=" << state.testCounters.documentReadCount
		<< " matcherBuildCount=" << state.testCounters.matcherBuildCount
		<< " diagnosticRefreshCount=" << state.testCounters.diagnosticRefreshCount << std::endl;
	const CounterSnapshot afterEdit = Snapshot(state);
	for (int move = 0; move < 100; ++move) {
		SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(firstTag + 1 + (move % 7)), 0);
		highlighter.UpdateHighlight(options);
	}
	if (!Check(SameCounters(state, afterEdit), "post-edit caret moves reuse the rebuilt cache")) return false;

	XmlTagHighlightOptions noDiagnostics = options;
	noDiagnostics.showErrors = false;
	highlighter.UpdateHighlight(noDiagnostics);
	if (!Check(state.diagnosticRanges.empty() && state.testCounters.diagnosticRefreshCount == afterEdit.diagnosticRefreshes + 1,
		"disabling structural diagnostics clears their indicators once")) return false;
	highlighter.UpdateHighlight(options);
	if (!Check(!state.diagnosticRanges.empty() && state.testCounters.diagnosticRefreshCount == afterEdit.diagnosticRefreshes + 2,
		"enabling structural diagnostics redraws them once")) return false;
	XmlTagHighlightOptions disabled = options;
	disabled.enabled = false;
	highlighter.UpdateHighlight(disabled);
	return Check(state.tagRanges.empty() && state.attributeRanges.empty(), "disabling matching clears current indicators");
}

bool VerifyNavigation(HWND editor, XmlSourceTagHighlighter& highlighter, XmlMatchedTagsState& state,
	const XmlTagHighlightOptions& options)
{
	const std::string document = "<root><ns:section attr=\"v\"><section><section><p>text</p></section></section></ns:section></root>";
	SetDocument(editor, document, state);
	const size_t opening = document.find("<ns:section") + 1;
	const size_t closing = document.find("</ns:section>") + 2;
	SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(opening), 0);
	if (!Check(highlighter.UpdateHighlight(options), "navigation fixture initial parse")) return false;
	const CounterSnapshot cached = Snapshot(state);
	if (!Check(highlighter.GotoMatchingTag() && SendMessage(editor, SCI_GETCURRENTPOS, 0, 0) == static_cast<LRESULT>(closing), "goto matching opening to closing")) return false;
	if (!Check(highlighter.GotoMatchingTag() && SendMessage(editor, SCI_GETCURRENTPOS, 0, 0) == static_cast<LRESULT>(opening), "goto matching closing to opening")) return false;
	SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(opening - 1), 0);
	if (!Check(highlighter.GotoMatchingTag(), "goto matching from opening angle bracket")) return false;
	SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(document.find('>')), 0);
	if (!Check(highlighter.GotoMatchingTag(), "goto matching from closing angle bracket")) return false;
	const size_t attribute = document.find("attr=\"v\"") + 3;
	SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(attribute), 0);
	if (!Check(highlighter.GotoMatchingTag(), "goto matching from attribute")) return false;
	const size_t nested = document.find("<section>", document.find("<section>") + 1) + 1;
	SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(nested), 0);
	if (!Check(highlighter.GotoMatchingTag(), "goto matching for nested same-name tag")) return false;
	if (!Check(SameCounters(state, cached) && SendMessage(editor, SCI_CANUNDO, 0, 0) == 0,
		"cached navigation does not read, rebuild or create undo entries")) return false;

	const std::string invalid = "</missing><section><p>text</section></p>";
	SetDocument(editor, invalid, state);
	SendMessage(editor, SCI_GOTOPOS, 0, 0);
	highlighter.UpdateHighlight(options);
	const CounterSnapshot invalidCached = Snapshot(state);
	highlighter.GotoWrongTag();
	if (!Check(SendMessage(editor, SCI_GETCURRENTPOS, 0, 0) > 0, "goto wrong tag advances to the next diagnostic")) return false;
	SendMessage(editor, SCI_GOTOPOS, static_cast<WPARAM>(invalid.size()), 0);
	highlighter.GotoWrongTag();
	if (!Check(SendMessage(editor, SCI_GETCURRENTPOS, 0, 0) == 0, "goto wrong tag wraps around")) return false;
	return Check(SameCounters(state, invalidCached), "wrong-tag navigation reuses the cached matcher");
}

} // namespace

int main()
{
	HMODULE scintilla = LoadLibraryW(L"Scintilla.dll");
	if (scintilla == NULL) return 1;
	HWND editor = CreateWindowW(L"Scintilla", L"", WS_POPUP, 0, 0, 320, 200, NULL, NULL, GetModuleHandle(NULL), NULL);
	if (editor == NULL) { FreeLibrary(scintilla); return 2; }
	SendMessage(editor, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
	SendMessage(editor, SCI_EMPTYUNDOBUFFER, 0, 0);
	CWindow source(editor);
	XmlMatchedTagsState state;
	XmlSourceTagHighlighter highlighter(&source, &state);
	const XmlTagHighlightOptions options = { true, XmlTagHighlightMode::FullTag, true, true };
	const bool success = VerifyLargeDocumentCache(editor, highlighter, state, options) &&
		VerifyNavigation(editor, highlighter, state, options);
	DestroyWindow(editor);
	FreeLibrary(scintilla);
	if (!success) return 3;
	std::cout << "XML Source runtime cache regression passed: 12,000 elements, 1,000 caret moves and 100 scroll operations reused the matcher cache." << std::endl;
	return 0;
}
