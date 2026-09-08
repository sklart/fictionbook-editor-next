#include "stdafx.h"

#include <algorithm>

#include "DocumentSearchCoordinator.h"

#include "search\\LiteralSearch.h"
#include "search\\RegexBackend.h"

namespace {

std::wstring BuildPreview(const std::wstring& text, const AU::Search::SearchHit& hit)
{
	const std::size_t context = 40;
	const std::size_t start = hit.Start > context ? hit.Start - context : 0;
	const std::size_t end = (std::min)(text.size(), hit.Start + hit.Length + context);
	std::wstring preview = text.substr(start, end - start);
	if (start != 0)
		preview.insert(0, L"…");
	if (end != text.size())
		preview += L"…";
	return preview;
}

}

bool DocumentSearchCoordinator::Rebuild(
	MSHTML::IHTMLDocument2Ptr document,
	std::uint64_t documentGeneration,
	const AU::Search::SearchQuery& query,
	std::wstring* errorText)
{
	if (errorText != NULL)
		errorText->clear();
	m_session.SetQuery(query);
	m_snapshot = m_adapter.BuildBodySnapshot(document, documentGeneration);

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

	m_session.SetHits(hits, documentGeneration);
	std::vector<AU::Search::SearchResult> results;
	results.reserve(hits.size());
	for (std::size_t index = 0; index < hits.size(); ++index)
	{
		AU::Search::SearchResult result = {};
		result.Hit = hits[index];
		result.Preview = BuildPreview(m_snapshot.Text, hits[index]);
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
	bool* wrapped)
{
	const AU::Search::SearchHit* hit = m_session.SelectNearestFor(
		documentGeneration, offset, direction, wrapped);
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
	bool* wrapped)
{
	std::size_t offset = 0;
	if (!m_adapter.TryGetSearchOffset(
		m_snapshot, range, direction == AU::Search::SearchDirection::Forward, &offset))
	{
		if (wrapped != NULL)
			*wrapped = false;
		return NULL;
	}
	return SelectFromOffset(document, documentGeneration, offset, direction, wrapped);
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
