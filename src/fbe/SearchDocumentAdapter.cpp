#include "stdafx.h"

#include "SearchDocumentAdapter.h"

namespace {

bool IsSearchParagraph(MSHTML::IHTMLElementPtr element)
{
	if (!element)
		return false;
	_bstr_t tagName(element->tagName);
	return tagName.length() != 0 && _wcsicmp(static_cast<LPCWSTR>(tagName), L"P") == 0;
}

bool IsBodyElement(MSHTML::IHTMLElementPtr element)
{
	if (!element)
		return false;
	_bstr_t tagName(element->tagName);
	return tagName.length() != 0 && _wcsicmp(static_cast<LPCWSTR>(tagName), L"BODY") == 0;
}

bool MoveRangeStartToTextOffset(
	MSHTML::IHTMLBodyElementPtr body,
	MSHTML::IHTMLElementPtr source,
	std::size_t textOffset,
	MSHTML::IHTMLTxtRangePtr& range,
	bool useRightEndpoint = false)
{
	range = body ? body->createTextRange() : MSHTML::IHTMLTxtRangePtr();
	if (!range || !source)
		return false;
	range->moveToElementText(source);
	range->collapse(VARIANT_TRUE);
	if (!IsBodyElement(source))
	{
		range->move(L"character", static_cast<long>(textOffset));
		return true;
	}

	// MSHTML's character movement counts inline controls (notably IMG), while
	// IHTMLTxtRange::text omits them. Find the first DOM position whose text
	// prefix reaches the UTF-16 snapshot offset instead of assuming the two
	// coordinate systems are identical.
	MSHTML::IHTMLTxtRangePtr limit(body->createTextRange());
	if (!limit)
		return false;
	limit->moveToElementText(source);
	_bstr_t bodyText(limit->text);
	limit->collapse(VARIANT_TRUE);
	// htmlfile/MSHTML on older engines does not reliably accept LONG_MAX here;
	// it may corrupt the range stack instead of simply clamping at body end.
	// Text length plus one position per element safely bounds the additional
	// invisible character positions (IMG and block boundaries) that move()
	// counts but IHTMLTxtRange::text does not expose.
	MSHTML::IHTMLDocument2Ptr document(MSHTML::IHTMLElementPtr(body)->document);
	MSHTML::IHTMLElementCollectionPtr elements(document ? document->all : MSHTML::IHTMLElementCollectionPtr());
	const long elementCount = elements ? elements->length : 0;
	const std::size_t boundedLength = static_cast<std::size_t>(bodyText.length()) +
		static_cast<std::size_t>(elementCount < 0 ? 0 : elementCount) + 16;
	const long upperBound = boundedLength > static_cast<std::size_t>(LONG_MAX)
		? LONG_MAX : static_cast<long>(boundedLength);
	long lower = 0;
	long upper = upperBound;
	while (lower < upper)
	{
		const long middle = lower + (upper - lower) / 2;
		MSHTML::IHTMLTxtRangePtr endpoint(body->createTextRange());
		MSHTML::IHTMLTxtRangePtr prefix(body->createTextRange());
		if (!endpoint || !prefix)
			return false;
		endpoint->moveToElementText(source);
		endpoint->collapse(VARIANT_TRUE);
		endpoint->move(L"character", middle);
		prefix->moveToElementText(source);
		prefix->setEndPoint(L"EndToEnd", endpoint);
		_bstr_t prefixText(prefix->text);
		if (static_cast<std::size_t>(prefixText.length()) < textOffset)
			lower = middle + 1;
		else
			upper = middle;
	}
	MSHTML::IHTMLTxtRangePtr resolvedEndpoint(body->createTextRange());
	if (!resolvedEndpoint)
		return false;
	resolvedEndpoint->moveToElementText(source);
	resolvedEndpoint->collapse(VARIANT_TRUE);
	resolvedEndpoint->move(L"character", lower);
	// At a block boundary MSHTML can report the first position *after* the
	// requested text offset. Correct exactly one visual position only when the
	// prefix proves that happened; IMG/control positions leave the prefix length
	// unchanged and therefore need no artificial adjustment.
	MSHTML::IHTMLTxtRangePtr resolvedPrefix(body->createTextRange());
	if (!resolvedPrefix)
		return false;
	resolvedPrefix->moveToElementText(source);
	resolvedPrefix->setEndPoint(L"EndToEnd", resolvedEndpoint);
	_bstr_t resolvedText(resolvedPrefix->text);
	if (static_cast<std::size_t>(resolvedText.length()) > textOffset)
		resolvedEndpoint->move(L"character", -1);
	UNREFERENCED_PARAMETER(useRightEndpoint);
	range = resolvedEndpoint;
	return true;
}

}

AU::Search::SearchTextSnapshot SearchDocumentAdapter::BuildSnapshot(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration)
{
	m_sources.clear();
	m_sections.clear();
	AU::Search::SearchTextSnapshotBuilder builder(documentGeneration);
	if (!document || !document->body)
		return builder.Build();

	MSHTML::IHTMLBodyElementPtr body(document->body);
	MSHTML::IHTMLElementCollectionPtr all(document->all);
	if (!body || !all)
		return builder.Build();
	std::uint64_t nextSourceId = 1;
	bool hasPreviousParagraph = false;
	for (long index = 0; index < all->length; ++index)
	{
		MSHTML::IHTMLElementPtr element(all->item(index));
		if (!IsSearchParagraph(element))
			continue;

		MSHTML::IHTMLTxtRangePtr range(body->createTextRange());
		if (!range)
			continue;
		range->moveToElementText(element);
		_bstr_t rangeText(range->text);
		CString paragraphText(static_cast<LPCWSTR>(rangeText));
		if (hasPreviousParagraph)
			builder.AppendUnmapped(L"\n");
		hasPreviousParagraph = true;

		const std::uint64_t sourceId = nextSourceId++;
		m_sources.push_back({ sourceId, element });
		builder.Append(
			std::wstring(static_cast<LPCWSTR>(paragraphText), paragraphText.GetLength()),
			{ sourceId, 0 });
	}

	return builder.Build();
}

AU::Search::SearchTextSnapshot SearchDocumentAdapter::BuildBodySnapshot(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration)
{
	m_sources.clear();
	AU::Search::SearchTextSnapshotBuilder builder(documentGeneration);
	if (!document || !document->body)
		return builder.Build();

	MSHTML::IHTMLBodyElementPtr body(document->body);
	MSHTML::IHTMLTxtRangePtr range(body ? body->createTextRange() : MSHTML::IHTMLTxtRangePtr());
	if (!body || !range)
		return builder.Build();
	_bstr_t rangeText(range->text);
	CString text(static_cast<LPCWSTR>(rangeText));
	const std::uint64_t sourceId = 1;
	m_sources.push_back({ sourceId, MSHTML::IHTMLElementPtr(body) });
	builder.Append(
		std::wstring(static_cast<LPCWSTR>(text), text.GetLength()),
		{ sourceId, 0 });
	AU::Search::SearchTextSnapshot snapshot = builder.Build();
	return snapshot;

	// Capture presentation-only section labels while building the snapshot.
	// Later result navigation must not touch DOM ancestry after range mapping.
	MSHTML::IHTMLElementCollectionPtr all(document->all);
	std::size_t nextOffset = 0;
	if (all)
	{
		for (long index = 0; index < all->length; ++index)
		{
			MSHTML::IHTMLElementPtr section(all->item(index));
			if (!section || _wcsicmp(static_cast<LPCWSTR>(_bstr_t(section->className)), L"section") != 0)
				continue;
			_bstr_t sectionText(section->innerText);
			if (sectionText.length() == 0)
				continue;
			const std::wstring sectionSource(static_cast<LPCWSTR>(sectionText), sectionText.length());
			const std::size_t start = snapshot.Text.find(sectionSource, nextOffset);
			if (start == std::wstring::npos)
				continue;
			nextOffset = start + sectionSource.size();
			MSHTML::IHTMLElementCollectionPtr children(section->children);
			if (!children)
				continue;
			for (long childIndex = 0; childIndex < children->length; ++childIndex)
			{
				MSHTML::IHTMLElementPtr child(children->item(childIndex));
				if (child && _wcsicmp(static_cast<LPCWSTR>(_bstr_t(child->className)), L"title") == 0)
				{
					m_sections.push_back({ sectionSource,
						std::wstring(static_cast<LPCWSTR>(_bstr_t(child->innerText))) });
					break;
				}
			}
		}
	}
	return snapshot;
}

bool SearchDocumentAdapter::CreateHitRange(
	MSHTML::IHTMLDocument2Ptr document,
	const AU::Search::SearchTextSnapshot& snapshot,
	const AU::Search::SearchHit& hit,
	MSHTML::IHTMLTxtRangePtr& range) const
{
	range = NULL;
	if (!document || !document->body)
		return false;

	AU::Search::SearchDocumentPosition start = {};
	if (!snapshot.TryGetDocumentPosition(hit.Start, &start))
		return false;

	const SourceRange* startSource = FindSource(start.SourceId);
	if (startSource == NULL || !startSource->Element)
		return false;

	MSHTML::IHTMLBodyElementPtr body(document->body);
	if (!MoveRangeStartToTextOffset(body, startSource->Element, start.SourceOffset, range))
		return false;
	if (hit.Length != 0)
	{
		AU::Search::SearchDocumentPosition end = {};
		if (!snapshot.TryGetDocumentPosition(hit.Start + hit.Length, &end))
			return false;
		const SourceRange* endSource = FindSource(end.SourceId);
		if (endSource == NULL || !endSource->Element)
			return false;
		MSHTML::IHTMLTxtRangePtr endRange;
		if (!MoveRangeStartToTextOffset(body, endSource->Element, end.SourceOffset, endRange, true))
			return false;
		range->setEndPoint(L"EndToEnd", endRange);
	}
	return true;
}

bool SearchDocumentAdapter::SelectHit(
	MSHTML::IHTMLDocument2Ptr document,
	const AU::Search::SearchTextSnapshot& snapshot,
	const AU::Search::SearchHit& hit) const
{
	MSHTML::IHTMLTxtRangePtr range;
	if (!CreateHitRange(document, snapshot, hit, range))
		return false;
	range->select();
	return true;
}

std::wstring SearchDocumentAdapter::GetSectionLabel(
	MSHTML::IHTMLDocument2Ptr document,
	const AU::Search::SearchTextSnapshot& snapshot,
	const AU::Search::SearchHit& hit) const
{
	UNREFERENCED_PARAMETER(document);
	for (std::size_t index = 0; index < m_sections.size(); ++index)
	{
		const std::size_t start = snapshot.Text.find(m_sections[index].Text);
		if (start != std::wstring::npos &&
			AU::Search::IsHitInsideRange(hit, AU::Search::SearchRange(start, m_sections[index].Text.size())))
			return m_sections[index].Label;
	}
	return std::wstring();

	try
	{
		// parentElement() on a range reconstructed from a body snapshot crashes
		// in the htmlfile host used by Windows 7-era MSHTML. Determine enclosure
		// from comparable text ranges instead; this also keeps section metadata
		// out of the native matching path.
		MSHTML::IHTMLBodyElementPtr body(document ? document->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLElementCollectionPtr all(document ? document->all : MSHTML::IHTMLElementCollectionPtr());
		if (!body || !all)
			return std::wstring();
		std::size_t nextSectionOffset = 0;
		for (long index = 0; index < all->length; ++index)
		{
			MSHTML::IHTMLElementPtr section(all->item(index));
			if (!section || _wcsicmp(static_cast<LPCWSTR>(_bstr_t(section->className)), L"section") != 0)
				continue;
			_bstr_t sectionText(section->innerText);
			if (sectionText.length() == 0)
				continue;
			const std::wstring text(static_cast<LPCWSTR>(sectionText), sectionText.length());
			const std::size_t start = snapshot.Text.find(text, nextSectionOffset);
			if (start == std::wstring::npos)
				continue;
			nextSectionOffset = start + text.size();
			const AU::Search::SearchRange scope(start, text.size());
			if (!AU::Search::IsHitInsideRange(hit, scope))
				continue;
			MSHTML::IHTMLElementCollectionPtr children(section->children);
			if (!children)
				return std::wstring();
			for (long childIndex = 0; childIndex < children->length; ++childIndex)
			{
				MSHTML::IHTMLElementPtr child(children->item(childIndex));
				if (child && _wcsicmp(static_cast<LPCWSTR>(_bstr_t(child->className)), L"title") == 0)
					return std::wstring(static_cast<LPCWSTR>(_bstr_t(child->innerText)));
			}
			return std::wstring();
		}
		return std::wstring();

		MSHTML::IHTMLTxtRangePtr range;
		if (!CreateHitRange(document, snapshot, hit, range) || !range)
			return std::wstring();
		MSHTML::IHTMLElementPtr element(range->parentElement());
		while (element && _wcsicmp(static_cast<LPCWSTR>(_bstr_t(element->className)), L"section") != 0)
			element = element->parentElement;
		if (!element)
			return std::wstring();
		MSHTML::IHTMLElementCollectionPtr children(element->children);
		if (!children)
			return std::wstring();
		for (long index = 0; index < children->length; ++index)
		{
			MSHTML::IHTMLElementPtr child(children->item(index));
			if (child && _wcsicmp(static_cast<LPCWSTR>(_bstr_t(child->className)), L"title") == 0)
				return std::wstring(static_cast<LPCWSTR>(_bstr_t(child->innerText)));
		}
	}
	catch (const _com_error&)
	{
	}
	return std::wstring();
}

bool SearchDocumentAdapter::TryGetSearchOffset(
	const AU::Search::SearchTextSnapshot& snapshot,
	MSHTML::IHTMLTxtRangePtr range,
	bool useEnd,
	std::size_t* searchOffset) const
{
	if (!range || searchOffset == NULL)
		return false;
	MSHTML::IHTMLTxtRangePtr endpoint(range->duplicate());
	if (!endpoint)
		return false;
	endpoint->collapse(useEnd ? VARIANT_FALSE : VARIANT_TRUE);

	// parentElement() is not reliable for a collapsed range at an element
	// boundary. Compare against every adapter-owned source range instead.
	for (std::size_t index = 0; index < m_sources.size(); ++index)
	{
		const SourceRange& source = m_sources[index];
		if (!source.Element)
			continue;
		MSHTML::IHTMLDocument2Ptr document(source.Element->document);
		MSHTML::IHTMLBodyElementPtr body(document ? document->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLTxtRangePtr sourceRange(body ? body->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (!sourceRange)
			continue;
		sourceRange->moveToElementText(source.Element);
		MSHTML::IHTMLTxtRangePtr sourceStart(sourceRange->duplicate());
		MSHTML::IHTMLTxtRangePtr sourceEnd(sourceRange->duplicate());
		sourceStart->collapse(VARIANT_TRUE);
		sourceEnd->collapse(VARIANT_FALSE);
		if (endpoint->compareEndPoints(L"StartToStart", sourceStart) < 0 ||
			endpoint->compareEndPoints(L"StartToStart", sourceEnd) > 0)
			continue;
		sourceRange->setEndPoint(L"EndToStart", endpoint);
		_bstr_t prefix(sourceRange->text);
		return snapshot.TryGetSearchOffset(
			{ source.Id, static_cast<std::size_t>(prefix.length()) },
			searchOffset);
	}
	return false;
}

const SearchDocumentAdapter::SourceRange* SearchDocumentAdapter::FindSource(std::uint64_t id) const
{
	for (std::size_t index = 0; index < m_sources.size(); ++index)
	{
		if (m_sources[index].Id == id)
			return &m_sources[index];
	}
	return NULL;
}

const SearchDocumentAdapter::SourceRange* SearchDocumentAdapter::FindSource(MSHTML::IHTMLElementPtr element) const
{
	if (!element)
		return NULL;
	const long sourceIndex = element->sourceIndex;
	for (std::size_t index = 0; index < m_sources.size(); ++index)
	{
		if (m_sources[index].Element && m_sources[index].Element->sourceIndex == sourceIndex)
			return &m_sources[index];
	}
	return NULL;
}
