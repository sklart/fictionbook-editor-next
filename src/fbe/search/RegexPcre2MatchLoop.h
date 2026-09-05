#pragma once

// Pure PCRE2-16 traversal shared by the production backend and native smoke
// tests. Consumers own extraction of captures and any UI/COM representation.
#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 16
#endif
#ifndef PCRE2_STATIC
#define PCRE2_STATIC
#endif
#include "pcre2.h"

namespace AU {
namespace RegexPcre2 {

inline PCRE2_SIZE AdvanceUtf16CodePoint(
	PCRE2_SPTR subject,
	PCRE2_SIZE subjectLength,
	PCRE2_SIZE offset)
{
	if (offset >= subjectLength)
		return subjectLength + 1;
	const PCRE2_UCHAR first = subject[offset++];
	if (first >= 0xd800 && first <= 0xdbff && offset < subjectLength) {
		const PCRE2_UCHAR second = subject[offset];
		if (second >= 0xdc00 && second <= 0xdfff)
			++offset;
	}
	return offset;
}

// Returns zero after normal completion, otherwise the PCRE2 matching error.
// The callback receives only collected matches: suppressed non-empty retries
// after an empty match preserve FBE's historical collection semantics.
template <typename MatchHandler>
int ForEachMatch(
	pcre2_code* code,
	PCRE2_SPTR subject,
	PCRE2_SIZE subjectLength,
	bool global,
	pcre2_match_data* matchData,
	pcre2_match_context* matchContext,
	MatchHandler onMatch)
{
	PCRE2_SIZE offset = 0;
	uint32_t globalOptions = 0;
	while (true)
	{
		const int result = pcre2_match(
			code, subject, subjectLength, offset, globalOptions, matchData, matchContext);
		if (result == PCRE2_ERROR_NOMATCH) {
			if ((globalOptions & PCRE2_NOTEMPTY_ATSTART) != 0) {
				offset = AdvanceUtf16CodePoint(subject, subjectLength, offset);
				globalOptions = 0;
				if (offset <= subjectLength)
					continue;
			}
			return 0;
		}
		if (result < 0)
			return result;

		PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(matchData);
		const PCRE2_SIZE matchStart = ovector[0];
		const bool retryAtEmptyMatch =
			(globalOptions & PCRE2_NOTEMPTY_ATSTART) != 0 && offset == matchStart;
		if (retryAtEmptyMatch) {
			// Do not collect the retry. Continue from one full UTF-16 character
			// after the preceding empty match, including a surrogate pair.
			offset = AdvanceUtf16CodePoint(subject, subjectLength, offset);
			globalOptions = 0;
			if (offset <= subjectLength)
				continue;
			return 0;
		}

		onMatch(result, ovector);

		if (!global)
			return 0;

		if (!pcre2_next_match(matchData, &offset, &globalOptions))
			return 0;
	}
}

}
}
