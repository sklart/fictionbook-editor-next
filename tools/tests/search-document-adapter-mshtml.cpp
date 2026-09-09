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
		L"<html><body><div class='title'>title text</div><p>first</p><div class='section'><div class='title'>Section one</div><p>second</p></div><p>plain <strong>strong</strong><emphasis> emphasis</emphasis><a> link</a>&nbsp;\x043A\x043E\x0442 \xD83D\xDE00</p><p>image before <img id='inline-image' src='about:blank'>inline-after</p><p><img id='twin-first' src='about:blank'><img id='twin-second' src='about:blank'>twins</p><p id='end-image-paragraph'>tail<img id='end-image' src='about:blank'></p><div class='image'><img id='block-image' src='about:blank'></div><p>block-after</p><table><tr><td>table cell</td></tr></table></body></html>"))
		return 2;

	int result = 0;
	{
	SearchDocumentAdapter adapter;
	const AU::Search::SearchTextSnapshot snapshot = adapter.BuildSnapshot(document, 42);
	if (snapshot.DocumentGeneration != 42 || snapshot.Segments.size() != 7) result = 3;
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
	// A collapsed hit immediately after an inline control must resolve on the
	// right of that control.  Inserting there is the MSHTML equivalent of a
	// zero-length replacement and must neither consume nor move the image.
	const std::size_t inlineAfterOffset = snapshot.Text.find(L"inline-after");
	MSHTML::IHTMLTxtRangePtr zeroLengthRange;
	if (!result && (inlineAfterOffset == std::wstring::npos ||
		!adapter.CreateHitRange(document, snapshot, AU::Search::SearchHit(inlineAfterOffset, 0), zeroLengthRange) ||
		!zeroLengthRange)) result = 55;
	if (!result)
	{
		zeroLengthRange->text = L"zero-";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML)));
		html.MakeUpper();
		const int imageAt = html.Find(L"ID=INLINE-IMAGE");
		const int insertedAt = html.Find(L"ZERO-INLINE-AFTER");
		if (imageAt < 0 || insertedAt < 0 || imageAt > insertedAt || html.Find(L"ID=BLOCK-IMAGE") < 0) result = 56;
	}
	const std::size_t tailOffset = snapshot.Text.find(L"tail");
	const std::size_t tailEnd = tailOffset == std::wstring::npos ? std::wstring::npos : tailOffset + 4;
	MSHTML::IHTMLTxtRangePtr endImageRange;
	if (!result && (tailEnd == std::wstring::npos || !adapter.CreateHitRange(document, snapshot, AU::Search::SearchHit(tailEnd, 0), endImageRange))) result = 65;
	if (!result)
	{
		endImageRange->text = L"end-marker";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML))); html.MakeUpper();
		const int image = html.Find(L"ID=END-IMAGE"), marker = html.Find(L"END-MARKER");
		if (image < 0 || marker < image) result = 66;
	}
	// `$` is a real zero-length regex result. Map the Search Core hit back to
	// an MSHTML range at an end-of-paragraph IMG without consuming that IMG.
	MSHTML::IHTMLDocument2Ptr dollarDocument;
	dollarDocument.CreateInstance(L"htmlfile");
	IPersistStreamInitPtr dollarPersist(dollarDocument);
	if (!result && (!dollarDocument || !dollarPersist || FAILED(dollarPersist->InitNew()) || !WriteHtml(dollarDocument,
		L"<html><body><p><img id='dollar-start' src='about:blank'>alpha</p><p>tail<img id='dollar-end' src='about:blank'></p></body></html>"))) result = 67;
	if (!result)
	{
		SearchDocumentAdapter dollarAdapter;
		const AU::Search::SearchTextSnapshot dollarAdapterSnapshot = dollarAdapter.BuildBodySnapshot(dollarDocument, 71);
		AU::Search::SearchQuery dollarQuery; dollarQuery.Mode = AU::Search::SearchMode::Regex; dollarQuery.Multiline = true; dollarQuery.Text = L"$";
		DocumentSearchCoordinator dollarCoordinator;
		if (!dollarCoordinator.Rebuild(dollarDocument, 71, dollarQuery)) result = 68;
		// Rebuild uses BuildBodySnapshot(), so use its exact snapshot for both
		// the backend hit and the MSHTML range mapping.
		const AU::Search::SearchTextSnapshot& dollarSearchSnapshot = dollarCoordinator.GetSnapshot();
		const std::size_t dollarOffset = dollarSearchSnapshot.Text.find(L"tail") + 4;
		const AU::Search::SearchHit* dollarHit = NULL;
		for (std::size_t index = 0; !result && index < dollarCoordinator.GetResults().GetCount(); ++index)
		{
			const AU::Search::SearchResult* candidate = dollarCoordinator.GetResults().GetAt(index);
			if (candidate != NULL && candidate->Hit.Start == dollarOffset && candidate->Hit.Length == 0)
			{
				dollarHit = &candidate->Hit;
				break;
			}
		}
		MSHTML::IHTMLTxtRangePtr dollarRange;
		if (!result && (dollarAdapterSnapshot.Text != dollarSearchSnapshot.Text || dollarHit == NULL ||
			!dollarAdapter.CreateHitRange(dollarDocument, dollarAdapterSnapshot, *dollarHit, dollarRange))) result = 69;
		if (!result)
		{
			dollarRange->text = L"dollar-marker";
			CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(dollarDocument->body)->innerHTML))); html.MakeUpper();
			const int start = html.Find(L"ID=DOLLAR-START"), end = html.Find(L"ID=DOLLAR-END"), marker = html.Find(L"DOLLAR-MARKER");
			if (start < 0 || end < start || marker < end || html.Find(L"ALPHA") < 0) result = 70;
		}
		// `^` has the same collapsed-range contract at an IMG at the beginning
		// of a paragraph: right-affinity keeps the control before the insertion.
		AU::Search::SearchQuery startQuery; startQuery.Mode = AU::Search::SearchMode::Regex; startQuery.Multiline = true; startQuery.Text = L"^";
		DocumentSearchCoordinator startCoordinator;
		if (!result && !startCoordinator.Rebuild(dollarDocument, 72, startQuery)) result = 71;
		SearchDocumentAdapter startAdapter;
		const AU::Search::SearchTextSnapshot startSnapshot = startAdapter.BuildBodySnapshot(dollarDocument, 72);
		const AU::Search::SearchResult* startResult = !result && startCoordinator.GetResults().GetCount() != 0
			? startCoordinator.GetResults().GetAt(0) : NULL;
		MSHTML::IHTMLTxtRangePtr startRange;
		if (!result && (startResult == NULL || startResult->Hit.Start != 0 || startResult->Hit.Length != 0 ||
			startSnapshot.Text != startCoordinator.GetSnapshot().Text ||
			!startAdapter.CreateHitRange(dollarDocument, startSnapshot, startResult->Hit, startRange))) result = 72;
		if (!result)
		{
			startRange->text = L"start-marker";
			CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(dollarDocument->body)->innerHTML))); html.MakeUpper();
			const int start = html.Find(L"ID=DOLLAR-START"), marker = html.Find(L"START-MARKER"), alpha = html.Find(L"ALPHA");
			if (start < 0 || marker < start || alpha < marker) result = 73;
		}
	}
	// Two adjacent IMG have one text offset. The adapter's documented
	// right-affinity inserts after the last zero-text control, never between
	// or before either image.
	const std::size_t twinsOffset = snapshot.Text.find(L"twins");
	MSHTML::IHTMLTxtRangePtr twinsRange;
	if (!result && (twinsOffset == std::wstring::npos || !adapter.CreateHitRange(document, snapshot, AU::Search::SearchHit(twinsOffset, 0), twinsRange))) result = 63;
	if (!result)
	{
		twinsRange->text = L"marker-";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML))); html.MakeUpper();
		const int first = html.Find(L"ID=TWIN-FIRST"), second = html.Find(L"ID=TWIN-SECOND"), marker = html.Find(L"MARKER-TWINS");
		if (first < 0 || second < first || marker < second) result = 64;
	}
	// A positive lookahead immediately after a block image is a collapsed Search
	// Core hit. The same right-affinity must preserve that image and insert on
	// its text side, rather than letting an MSHTML range consume the control.
	SearchDocumentAdapter blockAdapter;
	const AU::Search::SearchTextSnapshot blockSnapshot = blockAdapter.BuildBodySnapshot(document, 73);
	AU::Search::SearchQuery blockQuery; blockQuery.Mode = AU::Search::SearchMode::Regex; blockQuery.Text = L"(?=block-after)";
	DocumentSearchCoordinator blockCoordinator;
	if (!result && !blockCoordinator.Rebuild(document, 73, blockQuery)) result = 74;
	const AU::Search::SearchResult* blockResult = !result && blockCoordinator.GetResults().GetCount() == 1
		? blockCoordinator.GetResults().GetAt(0) : NULL;
	MSHTML::IHTMLTxtRangePtr blockRange;
	if (!result && (blockResult == NULL || blockResult->Hit.Length != 0 || blockSnapshot.Text != blockCoordinator.GetSnapshot().Text ||
		!blockAdapter.CreateHitRange(document, blockSnapshot, blockResult->Hit, blockRange))) result = 75;
	if (!result)
	{
		blockRange->text = L"block-marker";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML))); html.MakeUpper();
		const int image = html.Find(L"ID=BLOCK-IMAGE"), marker = html.Find(L"BLOCK-MARKER"), text = html.Find(L"BLOCK-AFTER");
		if (image < 0 || marker < image || text < marker) result = 76;
	}

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
	if (!result)
	{
		// Find across paragraphs stays valid, while this structural range is the
		// runtime/MSHTML condition that Replace rejects before mutating the DOM.
		CString crossParagraphHtml(static_cast<LPCWSTR>(_bstr_t(resultRange->htmlText)));
		crossParagraphHtml.MakeUpper();
		if (crossParagraphHtml.Find(L"</P") < 0 || crossParagraphHtml.Find(L"<P") < 0) result = 57;
	}
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
		if (html.Find(L"ID=INLINE-IMAGE") < 0 || html.Find(L"ID=BLOCK-IMAGE") < 0 || html.Find(L"AFTER-REPLACED") < 0) result = 47;
	}
	query.Text = L"image before";
	if (!result && (!coordinator.Rebuild(document, 44, query) || coordinator.GetResults().GetCount() != 1 ||
		!coordinator.CreateResultRange(document, 44, 0, resultRange) || !resultRange)) result = 48;
	if (!result)
	{
		resultRange->text = L"before-replaced";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML)));
		html.MakeUpper();
		if (html.Find(L"ID=INLINE-IMAGE") < 0 || html.Find(L"ID=BLOCK-IMAGE") < 0 || html.Find(L"BEFORE-REPLACED") < 0 || html.Find(L"AFTER-REPLACED") < 0) result = 49;
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
			html.Find(L"TABLE") < 0 || html.Find(L"CLASS=IMAGE") < 0 || html.Find(L"ID=INLINE-IMAGE") < 0 ||
			html.Find(L"ID=BLOCK-IMAGE") < 0) result = 51;
	}
	query.Text = L"block-after";
	if (!result && (!coordinator.Rebuild(document, 44, query) || coordinator.GetResults().GetCount() != 1 ||
		coordinator.CreateResultRange(document, 44, 0, resultRange) == false || !resultRange ||
		wcscmp(static_cast<LPCWSTR>(_bstr_t(resultRange->text)), L"block-after") != 0)) result = 36;
	if (!result)
	{
		resultRange->text = L"block-replaced";
		CString html(static_cast<LPCWSTR>(_bstr_t(MSHTML::IHTMLElementPtr(document->body)->innerHTML)));
		html.MakeUpper();
		if (html.Find(L"ID=BLOCK-IMAGE") < 0 || html.Find(L"BLOCK-REPLACED") < 0 || html.Find(L"ID=INLINE-IMAGE") < 0) result = 58;
	}
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
