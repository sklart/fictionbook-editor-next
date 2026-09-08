#include <stdio.h>
#include <vector>

#include "RegexPcre2MatchLoop.h"

static bool Collect(
	const wchar_t* pattern,
	const wchar_t* subject,
	bool global,
	const std::vector<PCRE2_SIZE>& expected,
	uint32_t compileOptions = PCRE2_UTF)
{
	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;
	pcre2_code* code = pcre2_compile(
		reinterpret_cast<PCRE2_SPTR>(pattern),
		static_cast<PCRE2_SIZE>(wcslen(pattern)),
		compileOptions,
		&errorNumber,
		&errorOffset,
		NULL);
	if (code == NULL)
		return false;
	pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(code, NULL);
	if (matchData == NULL) {
		pcre2_code_free(code);
		return false;
	}

	std::vector<PCRE2_SIZE> actual;
	const int result = AU::RegexPcre2::ForEachMatch(
		code,
		reinterpret_cast<PCRE2_SPTR>(subject),
		static_cast<PCRE2_SIZE>(wcslen(subject)),
		global,
		matchData,
		NULL,
		[&actual](int, PCRE2_SIZE* ovector) { actual.push_back(ovector[0]); });
	pcre2_match_data_free(matchData);
	pcre2_code_free(code);
	return result == 0 && actual == expected;
}

int wmain()
{
	if (!Collect(L"a", L"baac", true, { 1, 2 }))
		return 1;
	if (!Collect(L"(?:)", L"ab", true, { 0, 1, 2 }))
		return 2;
	if (!Collect(L"(?:|ab)", L"ab", true, { 0, 1, 2 }))
		return 3;
	if (!Collect(L"(?:|\\x{1F600})", L"\xD83D\xDE00", true, { 0, 2 }))
		return 4;
	if (!Collect(L"a", L"baac", false, { 1 }))
		return 5;
	if (!Collect(L"\\b\\x{0441}\\x{043B}\\x{043E}\\x{0432}\\x{043E}\\b", L"\x0441\x043B\x043E\x0432\x043E", true, { 0 }, PCRE2_UTF | PCRE2_UCP))
		return 6;
	if (!Collect(L"\\w+", L"\x0420\x0443\x0441\x0441\x043A\x0438\x0439 English", true, { 0, 8 }, PCRE2_UTF | PCRE2_UCP))
		return 7;
	printf("PCRE2 matching-loop smoke test passed.\n");
	return 0;
}
