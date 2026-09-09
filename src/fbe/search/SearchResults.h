#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "SearchTypes.h"

namespace AU {
namespace Search {

// Presentation-neutral data for a single Find All row.  The editor adapter
// supplies the human-readable context; the core never holds a DOM pointer.
struct SearchResult {
	SearchHit Hit;
	std::wstring Section;
	std::wstring Preview;
	// Offset inside Preview used by the Results pane to paint the matched text
	// without adding marker characters to the user-visible context.
	std::size_t PreviewMatchStart = 0;
	std::size_t PreviewMatchLength = 0;
};

class SearchResults {
public:
	SearchResults();

	void SetResults(const std::vector<SearchResult>& results, std::uint64_t documentGeneration);
	void Invalidate();
	bool IsValidFor(std::uint64_t documentGeneration) const;
	std::uint64_t GetRevision() const;

	std::size_t GetCount() const;
	const SearchResult* GetAt(std::size_t index) const;
	const SearchResult* GetSelected() const;
	std::size_t GetSelectedIndex() const;
	const SearchResult* Select(std::size_t index);

private:
	static const std::size_t kNoResult = static_cast<std::size_t>(-1);

	std::vector<SearchResult> m_results;
	std::uint64_t m_documentGeneration;
	std::uint64_t m_revision;
	std::size_t m_selectedIndex;
	bool m_valid;
};

}
}
