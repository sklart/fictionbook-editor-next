#include "stdafx.h"

#include <algorithm>

#include "DocumentSearchCoordinator.h"

#include "search\\LiteralSearch.h"
#include "search\\RegexBackend.h"

namespace {

struct PreviewData {
	std::wstring Text;
	std::size_t MatchStart = 0;
	std::size_t MatchLength = 0;
};

std::wstring NormalizePreviewText(const std::wstring& text)
{
	std::wstring normalized;
	normalized.reserve(text.size());
	bool previousWasSpace = false;
	for (wchar_t character : text)
	{
		if (character == L'\r' || character == L'\n' || character == L'\t')
			character = L' ';
		if (character == L' ' && previousWasSpace)
			continue;
		normalized += character;
		previousWasSpace = character == L' ';
	}
	return normalized;
}

PreviewData BuildPreview(const std::wstring& text, const AU::Search::SearchHit& hit)
{
	const std::size_t context = 40;
	const std::size_t start = hit.Start > context ? hit.Start - context : 0;
	const std::size_t end = (std::min)(text.size(), hit.Start + hit.Length + context);
	// Paragraph boundaries are searchable, but a list-view cell cannot render
	// them. Keep the context readable and make the matched fragment explicit.
	PreviewData preview;
	preview.Text = NormalizePreviewText(text.substr(start, hit.Start - start));
	preview.MatchStart = preview.Text.size();
	const std::wstring match = NormalizePreviewText(text.substr(hit.Start, hit.Length));
	preview.Text += match;
	preview.MatchLength = match.size();
	preview.Text += NormalizePreviewText(text.substr(hit.Start + hit.Length, end - hit.Start - hit.Length));
	if (start != 0)
	{
		preview.Text.insert(0, 1, L'\x2026');
		++preview.MatchStart;
	}
	if (end != text.size())
		preview.Text += L'\x2026';
	return preview;
}

}

bool DocumentSearchCoordinator::Rebuild(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration,
	const AU::Search::SearchQuery& query,
	std::wstring* errorText,
	const AU::Search::SearchRange* scopeRange)
{
	if (errorText != NULL)
		errorText->clear();
	m_session.SetQuery(query);
	// The editable FB2 content consists of paragraphs in #fbw_body.  Keeping
	// them as separate sources both excludes host metadata and preserves a
	// readable paragraph boundary for results previews and regex matching.
	m_snapshot = m_adapter.BuildSnapshot(document, documentGeneration);

	std::vector<AU::Search::SearchHit> hits;
	if (query.Mode == AU::Search::SearchMode::Literal)
	{
		hits = AU::Search::FindLiteralMatches(m_snapshot.Text, query);
	}
	else
	{
		AU::RegexBackend::Options options;
		options.Pattern = query.Text.c_str();
		options.IgnoreCase = query.MatchCase ? VARIANT_FALSE : VARIANT_TRUE;
		options.Global = VARIANT_TRUE;
		options.Multiline = query.Multiline ? VARIANT_TRUE : VARIANT_FALSE;
		options.UnicodeProperties = query.UnicodeProperties;

		CSimpleArray<AU::RegexBackend::MatchData> matches;
		CString regexError;
		CString source(m_snapshot.Text.data(), static_cast<int>(m_snapshot.Text.size()));
		if (!AU::RegexBackend::Execute(options, source, matches, regexError))
		{
			m_session.Invalidate();
			m_results.Invalidate();
			if (errorText != NULL)
				*errorText = static_cast<LPCWSTR>(regexError);
			return false;
		}
		AU::RegexBackend::BuildSearchHits(matches, hits);
	}

	if (scopeRange != NULL)
	{
		std::vector<AU::Search::SearchHit> filtered;
		for (std::size_t index = 0; index < hits.size(); ++index)
			if (AU::Search::IsHitInsideRange(hits[index], *scopeRange))
				filtered.push_back(hits[index]);
		hits.swap(filtered);
	}
	m_session.SetHits(hits, documentGeneration);
	std::vector<AU::Search::SearchResult> results;
	results.reserve(hits.size());
	for (std::size_t index = 0; index < hits.size(); ++index)
	{
		AU::Search::SearchResult result = {};
		result.Hit = hits[index];
		// Section is optional Results-pane presentation metadata. Matching,
		// navigation and replacement must remain independent of DOM ancestry.
		result.Section.clear();
		const PreviewData preview = BuildPreview(m_snapshot.Text, hits[index]);
		result.Preview = preview.Text;
		result.PreviewMatchStart = preview.MatchStart;
		result.PreviewMatchLength = preview.MatchLength;
		results.push_back(result);
	}
	m_results.SetResults(results, documentGeneration);
	return true;
}

const AU::Search::SearchTextSnapshot& DocumentSearchCoordinator::GetSnapshot() const
{
	return m_snapshot;
}

const AU::Search::SearchSession& DocumentSearchCoordinator::GetSession() const
{
	return m_session;
}

const AU::Search::SearchResults& DocumentSearchCoordinator::GetResults() const
{
	return m_results;
}

std::size_t DocumentSearchCoordinator::GetSelectedResultIndex() const
{
	return m_results.GetSelectedIndex();
}

const AU::Search::SearchHit* DocumentSearchCoordinator::SelectFromOffset(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration,
	std::size_t offset,
	AU::Search::SearchDirection direction,
	bool* wrapped,
	bool skipZeroLengthAtOffset)
{
	const AU::Search::SearchHit* hit = m_session.SelectNearestFor(
		documentGeneration, offset, direction, wrapped, skipZeroLengthAtOffset);
	if (hit == NULL || !m_adapter.SelectHit(document, m_snapshot, *hit))
		return NULL;
	for (std::size_t index = 0; index < m_results.GetCount(); ++index)
	{
		const AU::Search::SearchResult* result = m_results.GetAt(index);
		if (result != NULL && result->Hit == *hit)
		{
			m_results.Select(index);
			break;
		}
	}
	return hit;
}

const AU::Search::SearchHit* DocumentSearchCoordinator::SelectFromRange(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration,
	MSHTML::IHTMLTxtRangePtr range,
	AU::Search::SearchDirection direction,
	bool* wrapped,
	bool skipZeroLengthAtOffset)
{
	std::size_t offset = 0;
	if (!m_adapter.TryGetSearchOffset(
		m_snapshot, range, direction == AU::Search::SearchDirection::Forward, &offset))
	{
		// A newly created MSHTML selection can sit on BODY (outside #fbw_body).
		// It is still a valid Find starting point: begin at the editable boundary
		// rather than reporting "not found" while Find All sees the same hits.
		offset = direction == AU::Search::SearchDirection::Forward
			? 0
			: m_snapshot.Text.size();
	}
	return SelectFromOffset(document, documentGeneration, offset, direction, wrapped, skipZeroLengthAtOffset);
}

const AU::Search::SearchResult* DocumentSearchCoordinator::SelectResult(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration,
	std::size_t index)
{
	if (!m_results.IsValidFor(documentGeneration))
		return NULL;
	const AU::Search::SearchResult* result = m_results.Select(index);
	if (result == NULL || !m_adapter.SelectHit(document, m_snapshot, result->Hit))
		return NULL;
	return result;
}

bool DocumentSearchCoordinator::CreateResultRange(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration,
	std::size_t index,
	MSHTML::IHTMLTxtRangePtr& range) const
{
	range = NULL;
	if (!m_results.IsValidFor(documentGeneration))
		return false;
	const AU::Search::SearchResult* result = m_results.GetAt(index);
	return result != NULL && m_adapter.CreateHitRange(document, m_snapshot, result->Hit, range);
}

void DocumentSearchCoordinator::Invalidate()
{
	m_session.Invalidate();
	m_results.Invalidate();
}

bool DocumentSearchCoordinator::TryGetSearchRange(
	std::uint64_t documentGeneration,
	MSHTML::IHTMLTxtRangePtr range,
	AU::Search::SearchRange* searchRange) const
{
	if (searchRange == NULL || m_snapshot.DocumentGeneration != documentGeneration)
		return false;
	std::size_t start = 0;
	std::size_t end = 0;
	if (!m_adapter.TryGetSearchOffset(m_snapshot, range, false, &start) ||
		!m_adapter.TryGetSearchOffset(m_snapshot, range, true, &end) || end < start)
		return false;
	*searchRange = AU::Search::SearchRange(start, end - start);
	return true;
}
