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
