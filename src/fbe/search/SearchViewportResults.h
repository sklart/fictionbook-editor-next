#pragma once

#include <cstddef>

#include "SearchResults.h"

// This is deliberately presentation-free: the overlay asks it which already
// sorted SearchResults can touch a viewport before it crosses the MSHTML COM
// boundary for geometry.  Keeping the selection here makes the bounded-work
// invariant testable without a hosted browser.
namespace AU {
namespace Search {

struct SearchViewportSubset
{
	std::size_t FirstIndex;
	std::size_t Count;
};

inline SearchViewportSubset SelectViewportResults(const SearchResults& results,
	std::size_t viewportStart, std::size_t viewportEnd, std::size_t limit)
{
	SearchViewportSubset subset = { 0, 0 };
	if (limit == 0 || results.GetCount() == 0)
		return subset;
	if (viewportEnd < viewportStart)
		viewportEnd = viewportStart;

	// FindFirstAtOrAfter is a lower_bound on Hit.Start. Step back once because
	// a hit that begins above the viewport can still intersect its top edge.
	std::size_t index = results.FindFirstAtOrAfter(viewportStart);
	if (index > 0)
		--index;
	for (; index < results.GetCount() && subset.Count < limit; ++index)
	{
		const SearchResult* result = results.GetAt(index);
		if (result == NULL)
			continue;
		if (result->Hit.Start > viewportEnd)
			break;
		const std::size_t hitEnd = result->Hit.Start + result->Hit.Length;
		if (hitEnd < viewportStart)
			continue;
		if (subset.Count == 0)
			subset.FirstIndex = index;
		++subset.Count;
	}
	return subset;
}

}
}
