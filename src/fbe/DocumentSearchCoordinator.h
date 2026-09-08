#pragma once

#include <cstdint>
#include <string>

#include "SearchDocumentAdapter.h"
#include "search\\SearchResults.h"
#include "search\\SearchSession.h"

// Editor-side bridge for a Design-mode document. It owns MSHTML-aware
// mapping, while the query, matching and navigation contracts stay in Search
// Core. The caller owns generation tracking and must pass the generation of
// the current markup for every operation.
class DocumentSearchCoordinator
{
public:
	bool Rebuild(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration,
		const AU::Search::SearchQuery& query,
		std::wstring* errorText = NULL);

	const AU::Search::SearchTextSnapshot& GetSnapshot() const;
	const AU::Search::SearchSession& GetSession() const;
	const AU::Search::SearchResults& GetResults() const;

	const AU::Search::SearchHit* SelectFromOffset(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration,
		std::size_t offset,
		AU::Search::SearchDirection direction,
		bool* wrapped = NULL);

	const AU::Search::SearchHit* SelectFromRange(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration,
		MSHTML::IHTMLTxtRangePtr range,
		AU::Search::SearchDirection direction,
		bool* wrapped = NULL);

	const AU::Search::SearchResult* SelectResult(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration,
		std::size_t index);

private:
	SearchDocumentAdapter m_adapter;
	AU::Search::SearchTextSnapshot m_snapshot;
	AU::Search::SearchSession m_session;
	AU::Search::SearchResults m_results;
};
