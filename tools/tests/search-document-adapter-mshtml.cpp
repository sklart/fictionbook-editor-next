#include "stdafx.h"

#include "SearchDocumentAdapter.h"
#include "DocumentSearchCoordinator.h"

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
		L"<html><body><p>first</p><p>second</p><p>plain <strong>strong</strong><emphasis> emphasis</emphasis><a> link</a>&nbsp;\x043A\x043E\x0442 \xD83D\xDE00</p></body></html>"))
		return 2;

	int result = 0;
	{
	SearchDocumentAdapter adapter;
	const AU::Search::SearchTextSnapshot snapshot = adapter.BuildSnapshot(document, 42);
	if (snapshot.DocumentGeneration != 42 || snapshot.Segments.size() != 3) result = 3;
	if (!result && (snapshot.Text.find(L"strong") == std::wstring::npos || snapshot.Text.find(L"emphasis") == std::wstring::npos ||
		snapshot.Text.find(L"link") == std::wstring::npos || snapshot.Text.find(L"\x043A\x043E\x0442") == std::wstring::npos ||
		snapshot.Text.find(L"\x00A0") == std::wstring::npos || snapshot.Text.find(L"\xD83D\xDE00") == std::wstring::npos)) result = 4;

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
	if (!result && (!adapter.CreateHitRange(document, snapshot, AU::Search::SearchHit(0, firstEnd), firstRange) ||
		!adapter.TryGetSearchOffset(snapshot, firstRange, false, &offset) || offset != 0 ||
		!adapter.TryGetSearchOffset(snapshot, firstRange, true, &offset) || offset != firstEnd)) result = 10;

	DocumentSearchCoordinator coordinator;
	AU::Search::SearchQuery query;
	query.Text = L"second";
	if (!result && (!coordinator.Rebuild(document, 43, query) || coordinator.GetSession().GetHitCount() != 1)) result = 11;
	bool wrapped = false;
	if (!result && (!coordinator.SelectFromRange(document, 43, firstRange, AU::Search::SearchDirection::Forward, &wrapped) || wrapped)) result = 12;
	if (!result && coordinator.SelectFromOffset(document, 44, 0, AU::Search::SearchDirection::Forward, &wrapped) != NULL) result = 13;

	query.Mode = AU::Search::SearchMode::Regex;
	query.Text = L"^second$";
	query.Multiline = true;
	if (!result && (!coordinator.Rebuild(document, 45, query) || coordinator.GetSession().GetHitCount() != 1)) result = 14;
	query.Text = L"\\b\\x{043A}\\x{043E}\\x{0442}\\b";
	query.UnicodeProperties = true;
	if (!result && (!coordinator.Rebuild(document, 46, query) || coordinator.GetSession().GetHitCount() != 1)) result = 15;
	query.Text = L"(";
	std::wstring regexError;
	if (!result && (coordinator.Rebuild(document, 47, query, &regexError) || regexError.empty() || coordinator.GetSession().IsValid())) result = 16;
	}

	persist = NULL;
	document = NULL;
	CoUninitialize();
	return result;
}
