#pragma once


#include <atlcoll.h>
#include <atlstr.h>

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

	Options() : IgnoreCase(VARIANT_FALSE), Global(VARIANT_FALSE), Multiline(VARIANT_FALSE) {}
};

const wchar_t* GetBackendDisplayName();
bool Execute(
	const Options& options,
	const CString& sourceString,
	CSimpleArray<MatchData>& matches,
	CString& errorText);

}
}


