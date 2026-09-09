#pragma once

#include <cstdint>
#include <vector>

#include "search\SearchTextSnapshot.h"
#include "search\SearchTypes.h"

// MSHTML is intentionally confined to this editor-side adapter.  Search Core
// receives only UTF-16 text and opaque source coordinates.
class SearchDocumentAdapter
{
public:
	AU::Search::SearchTextSnapshot BuildSnapshot(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration);

	// Uses one body-backed source range. It is the parity snapshot for Design
	// search: all text MSHTML exposes through the body range is searchable,
	// including structural text that is not wrapped in a <p>.
	AU::Search::SearchTextSnapshot BuildBodySnapshot(
		MSHTML::IHTMLDocument2Ptr document,
		std::uint64_t documentGeneration);

	// Creates a range from snapshot coordinates without changing the editor
	// selection.  Keeping this separate makes the DOM bridge testable and lets
	// callers decide when a range should become visible to the user.
	bool CreateHitRange(
		MSHTML::IHTMLDocument2Ptr document,
		const AU::Search::SearchTextSnapshot& snapshot,
		const AU::Search::SearchHit& hit,
		MSHTML::IHTMLTxtRangePtr& range) const;

	bool SelectHit(
		MSHTML::IHTMLDocument2Ptr document,
		const AU::Search::SearchTextSnapshot& snapshot,
		const AU::Search::SearchHit& hit) const;

	// Maps one endpoint of an editor range back into the UTF-16 snapshot.
	// useEnd=false reads the range start; true reads its exclusive end.
	bool TryGetSearchOffset(
		const AU::Search::SearchTextSnapshot& snapshot,
		MSHTML::IHTMLTxtRangePtr range,
		bool useEnd,
		std::size_t* searchOffset) const;

private:
	struct SourceRange {
		std::uint64_t Id;
		MSHTML::IHTMLElementPtr Element;
	};
	const SourceRange* FindSource(std::uint64_t id) const;
	const SourceRange* FindSource(MSHTML::IHTMLElementPtr element) const;

	std::vector<SourceRange> m_sources;
};
