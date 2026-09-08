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

}

AU::Search::SearchTextSnapshot SearchDocumentAdapter::BuildSnapshot(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration)
{
	m_sources.clear();
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
	range = body->createTextRange();
	if (!range)
		return false;
	range->moveToElementText(startSource->Element);
	range->collapse(VARIANT_TRUE);
	range->move(L"character", static_cast<long>(start.SourceOffset));
	if (hit.Length != 0)
	{
		AU::Search::SearchDocumentPosition end = {};
		if (!snapshot.TryGetDocumentPosition(hit.Start + hit.Length, &end))
			return false;
		const SourceRange* endSource = FindSource(end.SourceId);
		if (endSource == NULL || !endSource->Element)
			return false;
		MSHTML::IHTMLTxtRangePtr endRange(body->createTextRange());
		if (!endRange)
			return false;
		endRange->moveToElementText(endSource->Element);
		endRange->collapse(VARIANT_TRUE);
		endRange->move(L"character", static_cast<long>(end.SourceOffset));
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

	MSHTML::IHTMLElementPtr element(endpoint->parentElement());
	const SourceRange* source = NULL;
	while (element && source == NULL)
	{
		source = FindSource(element);
		if (source == NULL)
			element = element->parentElement;
	}
	if (source == NULL || !source->Element)
		return false;

	MSHTML::IHTMLDocument2Ptr document(source->Element->document);
	MSHTML::IHTMLBodyElementPtr body(document ? document->body : MSHTML::IHTMLBodyElementPtr());
	MSHTML::IHTMLTxtRangePtr sourceRange(body ? body->createTextRange() : MSHTML::IHTMLTxtRangePtr());
	if (!sourceRange)
		return false;
	sourceRange->moveToElementText(source->Element);
	if (endpoint->inRange(sourceRange) != VARIANT_TRUE)
		return false;
	sourceRange->setEndPoint(L"EndToStart", endpoint);
	_bstr_t prefix(sourceRange->text);
	return snapshot.TryGetSearchOffset(
		{ source->Id, static_cast<std::size_t>(prefix.length()) },
		searchOffset);
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
