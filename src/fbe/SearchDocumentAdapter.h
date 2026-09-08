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

	bool SelectHit(
		MSHTML::IHTMLDocument2Ptr document,
		const AU::Search::SearchTextSnapshot& snapshot,
		const AU::Search::SearchHit& hit) const;

private:
	struct SourceRange {
		std::uint64_t Id;
		MSHTML::IHTMLElementPtr Element;
	};

	const SourceRange* FindSource(std::uint64_t id) const;

	std::vector<SourceRange> m_sources;
};
