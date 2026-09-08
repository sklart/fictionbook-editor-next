#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace AU {
namespace Search {

enum class SearchMode {
	Literal,
	Regex
};

enum class SearchDirection {
	Forward,
	Backward
};

enum class SearchScope {
	WholeDocument,
	CurrentSection,
	Selection
};

// This is a UI- and document-independent description of a search.  Text is
// UTF-16 on Windows, matching both the editor and the PCRE2-16 backend.
struct SearchQuery {
	std::wstring Text;
	SearchMode Mode;
	bool MatchCase;
	bool WholeWord;
	SearchDirection Direction;
	SearchScope Scope;
	bool Multiline;
	bool UnicodeProperties;

	SearchQuery()
		: Mode(SearchMode::Literal),
		  MatchCase(false),
		  WholeWord(false),
		  Direction(SearchDirection::Forward),
		  Scope(SearchScope::WholeDocument),
		  Multiline(false),
		  UnicodeProperties(false) {}
};

struct SearchCapture {
	std::size_t Start;
	std::size_t Length;
	std::wstring Name;

	SearchCapture() : Start(0), Length(0) {}
	SearchCapture(std::size_t start, std::size_t length, const std::wstring& name = std::wstring())
		: Start(start), Length(length), Name(name) {}
};

struct SearchHit {
	std::size_t Start;
	std::size_t Length;
	std::vector<SearchCapture> Captures;

	SearchHit() : Start(0), Length(0) {}
	SearchHit(std::size_t start, std::size_t length) : Start(start), Length(length) {}
};

struct SearchRange {
	std::size_t Start;
	std::size_t Length;

	SearchRange() : Start(0), Length(0) {}
	SearchRange(std::size_t start, std::size_t length) : Start(start), Length(length) {}
};

inline bool IsHitInsideRange(const SearchHit& hit, const SearchRange& range)
{
	return hit.Start >= range.Start && hit.Length <= range.Length &&
		hit.Start - range.Start <= range.Length - hit.Length;
}

inline bool operator==(const SearchHit& left, const SearchHit& right)
{
	return left.Start == right.Start && left.Length == right.Length;
}

inline bool operator!=(const SearchHit& left, const SearchHit& right)
{
	return !(left == right);
}

// Direction affects navigation only.  A change to it does not invalidate an
// already computed set of hits for the same matching criteria.
inline bool HasSameSearchCriteria(const SearchQuery& left, const SearchQuery& right)
{
	return left.Text == right.Text &&
		left.Mode == right.Mode &&
		left.MatchCase == right.MatchCase &&
		left.WholeWord == right.WholeWord &&
		left.Scope == right.Scope &&
		left.Multiline == right.Multiline &&
		left.UnicodeProperties == right.UnicodeProperties;
}

}
}
