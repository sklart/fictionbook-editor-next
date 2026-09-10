#pragma once

#include <cstdint>
#include <string>

#include "SearchDocumentAdapter.h"
#include "SearchResults.h"
#include "SearchSession.h"

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
		std::wstring* errorText = NULL,
		const AU::Search::SearchRange* scopeRange = NULL);

	const AU::Search::SearchTextSnapshot& GetSnapshot() const;
	const AU::Search::SearchSession& GetSession() const;
	const AU::Search::SearchResults& GetResults() const;
	// Results rows are virtual.  Context strings are deliberately generated
	// only when ListView asks for a visible row, rather than during Find All.
	bool GetResultPreview(
		std::size_t index,
		std::wstring* preview,
		std::size_t* matchStart = NULL,
		std::size_t* matchLength = NULL) const;
	// Kept narrow and deterministic for the hosted-MSHTML scale regression.
	// It verifies that virtual rows do not materialize every preview at once.
	std::size_t GetCachedPreviewCountForTest() const;
	std::size_t GetSelectedResultIndex() const;

	const AU::Search::SearchHit* SelectFromOffset(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration,
		std::size_t offset,
		AU::Search::SearchDirection direction,
		bool* wrapped = NULL,
		bool skipZeroLengthAtOffset = false);

	const AU::Search::SearchHit* SelectFromRange(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration,
		MSHTML::IHTMLTxtRangePtr range,
		AU::Search::SearchDirection direction,
		bool* wrapped = NULL,
		bool skipZeroLengthAtOffset = false);

	const AU::Search::SearchResult* SelectResult(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration,
		std::size_t index);

	// Produces an unselected range for a result. Bulk editor operations use
	// this before mutating the DOM so a mapping failure cannot cause a partial
	// Replace All.
	bool CreateResultRange(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration,
		std::size_t index,
		MSHTML::IHTMLTxtRangePtr& range) const;

	void Invalidate();

	// Converts the current editor selection into a pure snapshot range.  Scope
	// callers use this before rebuilding so Search Core never observes MSHTML.
	bool TryGetSearchRange(
		std::uint64_t documentGeneration,
		MSHTML::IHTMLTxtRangePtr range,
		AU::Search::SearchRange* searchRange) const;

private:
	SearchDocumentAdapter m_adapter;
	AU::Search::SearchTextSnapshot m_snapshot;
	AU::Search::SearchSession m_session;
	AU::Search::SearchResults m_results;
	mutable std::vector<std::wstring> m_previewCache;
	mutable std::vector<std::size_t> m_previewMatchStarts;
	mutable std::vector<std::size_t> m_previewMatchLengths;
	mutable std::vector<bool> m_previewCached;
};
