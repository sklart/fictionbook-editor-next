#include "stdafx.h"

#include <string>
#include <vector>

#include "LiteralSearch.h"

static bool WriteHtml(MSHTML::IHTMLDocument2Ptr document, const wchar_t* html)
{
	SAFEARRAY* values = SafeArrayCreateVector(VT_VARIANT, 0, 1);
	if (values == NULL)
		return false;
	VARIANT* value = NULL;
	if (FAILED(SafeArrayAccessData(values, reinterpret_cast<void**>(&value)))) {
		SafeArrayDestroy(values);
		return false;
	}
	VariantInit(value);
	value->vt = VT_BSTR;
	value->bstrVal = SysAllocString(html);
	SafeArrayUnaccessData(values);
	const HRESULT writeResult = document->write(values);
	SafeArrayDestroy(values);
	return SUCCEEDED(writeResult) && SUCCEEDED(document->close());
}

static bool CreateBodyRange(MSHTML::IHTMLDocument2Ptr document, MSHTML::IHTMLTxtRangePtr& range)
{
	range = NULL;
	MSHTML::IHTMLElementPtr body(document->body);
	if (!body)
		return false;
	MSHTML::IHTMLBodyElementPtr htmlBody(body);
	if (!htmlBody)
		return false;
	range = htmlBody->createTextRange();
	return range != NULL;
}

static std::vector<std::size_t> CollectLegacy(
	MSHTML::IHTMLDocument2Ptr document,
	const std::wstring& pattern,
	long flags)
{
	std::vector<std::size_t> offsets;
	MSHTML::IHTMLTxtRangePtr range;
	if (!CreateBodyRange(document, range))
		return offsets;
	for (;;) {
		if (range->findText(_bstr_t(pattern.c_str()), 1073741824, flags) != VARIANT_TRUE)
			break;
		MSHTML::IHTMLTxtRangePtr prefix;
		if (!CreateBodyRange(document, prefix))
			break;
		prefix->setEndPoint(L"EndToStart", range);
		_bstr_t prefixText(prefix->text);
		const std::size_t offset = prefixText.length();
		const bool reverse = (flags & 1) != 0;
		if ((!offsets.empty() && ((!reverse && offset <= offsets.back()) || (reverse && offset >= offsets.back()))))
			break;
		offsets.push_back(offset);
		range->collapse(reverse ? VARIANT_TRUE : VARIANT_FALSE);
	}
	return offsets;
}

static bool Compare(MSHTML::IHTMLDocument2Ptr document, const wchar_t* pattern, bool matchCase, bool wholeWord)
{
	MSHTML::IHTMLTxtRangePtr fullRange;
	if (!CreateBodyRange(document, fullRange))
		return false;
	_bstr_t text(fullRange->text);
	AU::Search::SearchQuery query;
	query.Text = static_cast<const wchar_t*>(_bstr_t(pattern));
	query.MatchCase = matchCase;
	query.WholeWord = wholeWord;
	const std::vector<AU::Search::SearchHit> native = AU::Search::FindLiteralMatches(static_cast<const wchar_t*>(text), query);
	const long flags = (matchCase ? 4 : 0) | (wholeWord ? 2 : 0);
	const std::vector<std::size_t> forward = CollectLegacy(document, pattern, flags);
	if (native.size() != forward.size())
		return false;
	for (std::size_t index = 0; index < native.size(); ++index) {
		if (native[index].Start != forward[index])
			return false;
	}
	std::vector<std::size_t> backward = CollectLegacy(document, pattern, flags | 1);
	if (backward.size() != forward.size())
		return false;
	for (std::size_t index = 0; index < forward.size(); ++index) {
		if (backward[index] != forward[forward.size() - 1 - index])
			return false;
	}
	return true;
}

int wmain()
{
	if (FAILED(CoInitialize(NULL)))
		return 1;
	MSHTML::IHTMLDocument2Ptr document;
	document.CreateInstance(L"htmlfile");
	IPersistStreamInitPtr persist(document);
	if (!document || !persist || FAILED(persist->InitNew()) ||
		!WriteHtml(document, L"<html><body><p>word Word password word&nbsp;\x043A\x043E\x0442 \x041A\x041E\x0422 \xD83D\xDE00</p></body></html>")) {
		CoUninitialize();
		return 2;
	}
	int result = 0;
	if (!Compare(document, L"word", false, false)) result = 10;
	else if (!Compare(document, L"word", true, false)) result = 11;
	else if (!Compare(document, L"word", false, true)) result = 12;
	else if (!Compare(document, L"\x043A\x043E\x0442", false, true)) result = 13;
	// MSHTML::findText() does not consistently find a supplementary character,
	// while the UTF-16 backend does. Keep this known parity gap explicit until
	// standard search migration has a documented compatibility decision.
	else if (Compare(document, L"\xD83D\xDE00", true, false)) result = 14;
	persist = NULL;
	document = NULL;
	CoUninitialize();
	return result;
}
