#include "stdafx.h"

#include "DocumentSearchCoordinator.h"

#include "search\\LiteralSearch.h"
#include "search\\RegexBackend.h"

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
			if (errorText != NULL)
				*errorText = static_cast<LPCWSTR>(regexError);
			return false;
		}
		AU::RegexBackend::BuildSearchHits(matches, hits);
	}

	m_session.SetHits(hits, documentGeneration);
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
