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

// Whole-word RegExp searches always use FBE's Unicode word definition.  The
// option is an external constraint: its meaning must not change merely because
// the expression itself happens to contain a boundary or a lookaround.
inline CString BuildWholeWordRegexPattern(const CString& pattern)
{
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
