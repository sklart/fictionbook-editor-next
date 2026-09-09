#include "stdafx.h"

#include "SearchDocumentAdapter.h"
#include "DocumentSearchCoordinator.h"
#include "search\\RegexBackend.h"

// RegexBackend localizes diagnostics through the editor runtime. The fixture
// only verifies matching, so its deterministic fallback is sufficient here.
CString FbeLoadRuntimeStringByKey(LPCWSTR, LPCWSTR fallback)
{
	return fallback != NULL ? CString(fallback) : CString();
}

static bool WriteHtml(MSHTML::IHTMLDocument2Ptr document, const wchar_t* html)
{
	SAFEARRAY* values = SafeArrayCreateVector(VT_VARIANT, 0, 1);
	if (values == NULL) return false;
	VARIANT* value = NULL;
	if (FAILED(SafeArrayAccessData(values, reinterpret_cast<void**>(&value)))) { SafeArrayDestroy(values); return false; }
	VariantInit(value);
	value->vt = VT_BSTR;
	value->bstrVal = SysAllocString(html);
	SafeArrayUnaccessData(values);
	const HRESULT result = document->write(values);
	SafeArrayDestroy(values);
	return SUCCEEDED(result) && SUCCEEDED(document->close());
}

static bool IsCollapsedSelection(MSHTML::IHTMLDocument2Ptr document)
{
	MSHTML::IHTMLTxtRangePtr range(document->selection->createRange());
	return range && range->compareEndPoints(L"StartToEnd", range) == 0;
}

int wmain()
{
	if (FAILED(CoInitialize(NULL))) return 1;
	MSHTML::IHTMLDocument2Ptr document;
	document.CreateInstance(L"htmlfile");
	IPersistStreamInitPtr persist(document);
	if (!document || !persist || FAILED(persist->InitNew()) || !WriteHtml(document,
		L"<html><body><div class='title'>title text</div><p>first</p><div class='section'><div class='title'>Section one</div><p>second</p></div><p>plain <strong>strong</strong><emphasis> emphasis</emphasis><a> link</a>&nbsp;\x043A\x043E\x0442 \xD83D\xDE00</p><p>image before <img src='about:blank'>inline-after</p><div class='image'><img src='about:blank'></div><p>block-after</p><table><tr><td>table cell</td></tr></table></body></html>"))
		return 2;

	int result = 0;
	{
	SearchDocumentAdapter adapter;
	const AU::Search::SearchTextSnapshot snapshot = adapter.BuildSnapshot(document, 42);
	if (snapshot.DocumentGeneration != 42 || snapshot.Segments.size() != 5) result = 3;
	if (!result && (snapshot.Text.find(L"strong") == std::wstring::npos || snapshot.Text.find(L"emphasis") == std::wstring::npos ||
		snapshot.Text.find(L"link") == std::wstring::npos || snapshot.Text.find(L"\x043A\x043E\x0442") == std::wstring::npos ||
		snapshot.Text.find(L"\xD83D\xDE00") == std::wstring::npos)) result = 4;

	const std::size_t firstEnd = snapshot.Segments[0].SearchOffset + snapshot.Segments[0].Length;
	if (!result && (!adapter.SelectHit(document, snapshot, AU::Search::SearchHit(0, 0)) || !IsCollapsedSelection(document))) result = 5;
	if (!result && (!adapter.SelectHit(document, snapshot, AU::Search::SearchHit(firstEnd, 0)) || !IsCollapsedSelection(document))) result = 6;

	// The hit starts on the inter-paragraph boundary and finishes in the second
	// paragraph. Endpoints must form one range rather than reject it.
	MSHTML::IHTMLTxtRangePtr crossParagraphRange;
	if (!result && (!adapter.CreateHitRange(document, snapshot, AU::Search::SearchHit(firstEnd, 7), crossParagraphRange) ||
		!crossParagraphRange || wcsstr(static_cast<LPCWSTR>(_bstr_t(crossParagraphRange->text)), L"second") == NULL)) result = 7;

	AU::Search::SearchDocumentPosition position = {};
	std::size_t offset = 0;
	if (!result && (!snapshot.TryGetDocumentPosition(firstEnd, &position) || !snapshot.TryGetSearchOffset(position, &offset) || offset != firstEnd)) result = 9;
	MSHTML::IHTMLTxtRangePtr firstRange;
	if (!result && !adapter.CreateHitRange(document, snapshot, AU::Search::SearchHit(0, firstEnd), firstRange)) result = 10;
	if (!result && (!adapter.TryGetSearchOffset(snapshot, firstRange, false, &offset) || offset != 0)) result = 11;
	if (!result && (!adapter.TryGetSearchOffset(snapshot, firstRange, true, &offset) || offset != firstEnd)) result = 12;

	DocumentSearchCoordinator coordinator;
	AU::Search::SearchQuery query;
	query.Text = L"second";
	if (!result && (!coordinator.Rebuild(document, 43, query) || coordinator.GetSession().GetHitCount() != 1)) result = 13;
	MSHTML::IHTMLTxtRangePtr resultRange;
	if (!result && (!coordinator.CreateResultRange(document, 43, 0, resultRange) || !resultRange ||
		wcsstr(static_cast<LPCWSTR>(_bstr_t(resultRange->text)), L"second") == NULL)) result = 34;
	query.Mode = AU::Search::SearchMode::Regex;
	query.Multiline = true;
	query.Text = L"first\\r?\\n(?:Section one\\r?\\n)?second";
	if (!result && (!coordinator.Rebuild(document, 43, query) || coordinator.GetResults().GetCount() != 1 ||
		!coordinator.CreateResultRange(document, 43, 0, resultRange) || !resultRange ||
		wcsstr(static_cast<LPCWSTR>(_bstr_t(resultRange->htmlText)), L"P") == NULL)) result = 53;
	query.Mode = AU::Search::SearchMode::Literal;
	query.Multiline = false;
	query.Text = L"second";
	if (!result && !coordinator.Rebuild(document, 43, query)) result = 54;
	AU::Search::SearchRange selectionScope;
	MSHTML::IHTMLTxtRangePtr bodySelection(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
	bodySelection->collapse(VARIANT_TRUE);
	bodySelection->moveEnd(L"character", 5);
	if (!result && (!coordinator.TryGetSearchRange(43, bodySelection, &selectionScope) ||
		selectionScope.Length == 0 || coordinator.TryGetSearchRange(44, bodySelection, &selectionScope))) result = 19;
	bool wrapped = false;
	if (!result && (!coordinator.SelectFromRange(document, 43, firstRange, AU::Search::SearchDirection::Forward, &wrapped) || wrapped)) result = 14;
	if (!result && coordinator.SelectFromOffset(document, 44, 0, AU::Search::SearchDirection::Forward, &wrapped) != NULL) result = 15;
	query.Text = L"inline-after";
	if (!result && (!coordinator.Rebuild(document, 44, query) || coordinator.GetResults().GetCount() != 1 ||
		coordinator.CreateResultRange(document, 44, 0, resultRange) == false || !resultRange ||
		wcscmp(static_cast<LPCWSTR>(_bstr_t(resultRange->text)), L"inline-after") != 0)) result = 35;
	if (!result)
	{
		resultRange->text = L"after-replaced";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML)));
		html.MakeUpper();
		if (html.Find(L"IMG") < 0 || html.Find(L"AFTER-REPLACED") < 0) result = 47;
	}
	query.Text = L"image before";
	if (!result && (!coordinator.Rebuild(document, 44, query) || coordinator.GetResults().GetCount() != 1 ||
		!coordinator.CreateResultRange(document, 44, 0, resultRange) || !resultRange)) result = 48;
	if (!result)
	{
		resultRange->text = L"before-replaced";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML)));
		html.MakeUpper();
		if (html.Find(L"IMG") < 0 || html.Find(L"BEFORE-REPLACED") < 0 || html.Find(L"AFTER-REPLACED") < 0) result = 49;
	}
	query.Text = L"strong";
	if (!result && (!coordinator.Rebuild(document, 44, query) || coordinator.GetResults().GetCount() != 1 ||
		!coordinator.CreateResultRange(document, 44, 0, resultRange) || !resultRange)) result = 50;
	if (!result)
	{
		resultRange->text = L"formatted";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML)));
		html.MakeUpper();
		if (html.Find(L"STRONG") < 0 || html.Find(L"EMPHASIS") < 0 || html.Find(L"<A") < 0 ||
			html.Find(L"TABLE") < 0 || html.Find(L"CLASS=IMAGE") < 0) result = 51;
	}
	query.Text = L"block-after";
	if (!result && (!coordinator.Rebuild(document, 44, query) || coordinator.GetResults().GetCount() != 1 ||
		coordinator.CreateResultRange(document, 44, 0, resultRange) == false || !resultRange ||
		wcscmp(static_cast<LPCWSTR>(_bstr_t(resultRange->text)), L"block-after") != 0)) result = 36;
	query.Text = L"table cell";
	if (!result && (!coordinator.Rebuild(document, 44, query) || coordinator.GetSession().GetHitCount() != 1 ||
		coordinator.GetResults().GetCount() != 1 || coordinator.GetResults().GetAt(0)->Preview.find(L"table cell") == std::wstring::npos ||
		coordinator.SelectResult(document, 44, 0) == NULL || coordinator.SelectResult(document, 45, 0) != NULL)) result = 16;
	AU::Search::SearchRange tableOnly(coordinator.GetSnapshot().Text.find(L"table cell"), 10);
	if (!result && (!coordinator.Rebuild(document, 44, query, NULL, &tableOnly) || coordinator.GetResults().GetCount() != 1)) result = 17;
	// Editor scopes are converted to a snapshot range before matching.  A
	// selection scope must exclude hits outside it for both literal and regex
	// backends, while retaining normal result/navigation metadata.
	query.Scope = AU::Search::SearchScope::Selection;
	query.Mode = AU::Search::SearchMode::Literal;
	query.Text = L"first";
	// Include the paragraph separator as well: multiline regex may consume the
	// CR before the LF depending on the MSHTML body-text representation.
	AU::Search::SearchRange secondScope(coordinator.GetSnapshot().Text.find(L"second"), 8);
	if (!result && (!coordinator.Rebuild(document, 44, query, NULL, &secondScope) || coordinator.GetResults().GetCount() != 0)) result = 20;
	query.Text = L"second";
	if (!result && (!coordinator.Rebuild(document, 44, query, NULL, &secondScope) || coordinator.GetResults().GetCount() != 1)) result = 21;

	query.Mode = AU::Search::SearchMode::Regex;
	query.Scope = AU::Search::SearchScope::CurrentSection;
	query.Text = L"^(?<word>second)(?<optional>z)?\\r?$";
	query.Multiline = true;
	if (!result && (!coordinator.Rebuild(document, 45, query, NULL, &secondScope) || coordinator.GetSession().GetHitCount() != 1)) result = 17;
	const AU::Search::SearchResult* namedResult = coordinator.GetResults().GetAt(0);
	if (!result && !namedResult) result = 22;
	if (!result && namedResult->Hit.Captures.size() != 2) result = 23;
	if (!result && (namedResult->Hit.Captures[0].GroupIndex != 1 || namedResult->Hit.Captures[0].Name != L"word" ||
		!namedResult->Hit.Captures[0].Matched)) result = 24;
	if (!result && namedResult->Hit.Captures[0].Length != 6) result = 25;
	if (!result && (namedResult->Hit.Captures[1].GroupIndex != 2 || namedResult->Hit.Captures[1].Name != L"optional" ||
		namedResult->Hit.Captures[1].Matched)) result = 26;
	AU::RegexBackend::Options replacementOptions;
	replacementOptions.Pattern = L"(?<first>second)(?<optional>z)?";
	replacementOptions.Global = VARIANT_TRUE;
	CSimpleArray<AU::RegexBackend::MatchData> replacementMatches;
	CString replacementError;
	if (!result && (!AU::RegexBackend::Execute(replacementOptions, L"second", replacementMatches, replacementError) ||
		replacementMatches.GetSize() != 1 || replacementMatches[0].SubMatches.GetSize() != 2 ||
		replacementMatches[0].SubMatches[0] != L"second" || !replacementMatches[0].SubMatches[1].IsEmpty())) result = 32;
	query.Text = L"\\b\\x{043A}\\x{043E}\\x{0442}\\b";
	query.UnicodeProperties = true;
	if (!result && (!coordinator.Rebuild(document, 46, query) || coordinator.GetSession().GetHitCount() != 1)) result = 18;
	query.Text = L"(";
	std::wstring regexError;
	if (!result && (coordinator.Rebuild(document, 47, query, &regexError) || regexError.empty() || coordinator.GetSession().IsValid())) result = 19;
	}

	persist = NULL;
	document = NULL;
	CoUninitialize();
	return result;
}
