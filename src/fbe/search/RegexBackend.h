#pragma once


#include <atlcoll.h>
#include <atlstr.h>
#include <vector>

#include "SearchTypes.h"

namespace AU {
namespace RegexBackend {

struct MatchData {
	CString Value;
	int FirstIndex;
	CSimpleArray<CString> SubMatches;
	std::vector<Search::SearchCapture> Captures;

	MatchData() : FirstIndex(0) {}
};

struct Options {
	CString Pattern;
	VARIANT_BOOL IgnoreCase;
	VARIANT_BOOL Global;
	VARIANT_BOOL Multiline;
	bool UnicodeProperties;

	Options()
		: IgnoreCase(VARIANT_FALSE),
		  Global(VARIANT_FALSE),
		  Multiline(VARIANT_FALSE),
		  UnicodeProperties(false) {}
};

const wchar_t* GetBackendDisplayName();

// Whole-word RegExp searches use FBE's Unicode word definition.  Patterns
// that already carry an explicit word boundary or lookaround remain untouched
// so FBE does not silently add a second, potentially contradictory boundary.
inline CString BuildWholeWordRegexPattern(const CString& pattern)
{
	if (pattern.Find(L"\\b") >= 0 || pattern.Find(L"\\B") >= 0 ||
		pattern.Find(L"(?=") >= 0 || pattern.Find(L"(?!") >= 0 ||
		pattern.Find(L"(?<=") >= 0 || pattern.Find(L"(?<!") >= 0)
		return pattern;
	return L"(?<![\\p{L}\\p{N}_])(?:" + pattern + L")(?![\\p{L}\\p{N}_])";
}
bool Execute(
	const Options& options,
	const CString& sourceString,
	CSimpleArray<MatchData>& matches,
	CString& errorText);

// Compatibility adapter for the new Search Core. MatchData remains the
// legacy shape consumed by IRegExp2 and is intentionally not changed.
void BuildSearchHits(
	const CSimpleArray<MatchData>& matches,
	std::vector<Search::SearchHit>& hits);

}
}
