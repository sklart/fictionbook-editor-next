#pragma once

#include <atlstr.h>

#include <vector>

namespace AU {
namespace Search {

enum ReplacementFormattingFlags
{
	ReplacementFormatStrong = 1,
	ReplacementFormatEmphasis = 2,
	ReplacementFormatUpper = 4,
	ReplacementFormatLower = 8,
	ReplacementFormatTitle = 16
};

struct ReplacementFormattingRun
{
	int Flags = 0;
	int Start = 0;
	int Length = 0;
};

// A deliberately small match view keeps replacement expansion independent of
// MSHTML and of the legacy IRegExp2 compatibility wrapper.
struct ReplacementMatch
{
	CString Value;
	std::vector<CString> SubMatches;
};

// Expands FBE's existing replacement syntax: $0/\0, $1...$9/\1...\9,
// $+/\+, and \T/\U/\L/\S/\E/\Q. It intentionally does not add $10,
// named captures, $$, or unrelated escape syntaxes.
CString ExpandRegexReplacement(
	const CString& replacementTemplate,
	const ReplacementMatch& match,
	std::vector<ReplacementFormattingRun>& formatting);

}
}