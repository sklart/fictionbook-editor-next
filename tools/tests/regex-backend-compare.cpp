#include <windows.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "atlstr.h"
#include "atlcoll.h"
#include "pcre.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include "pcre2.h"

#define UTF8_CHAR_LEN( byte ) (( 0xE5000000 >> (( byte >> 3 ) & 0x1e )) & 3 ) + 1

struct MatchResult
{
	std::string Value;
	int FirstIndex = 0;
	std::vector<std::string> SubMatches;
};

static int HexValue(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return -1;
}

static std::string DecodeHex(const char* text)
{
	std::string decoded;
	const size_t length = std::strlen(text);
	if ((length % 2) != 0)
		return decoded;

	decoded.reserve(length / 2);
	for (size_t i = 0; i < length; i += 2)
	{
		const int high = HexValue(text[i]);
		const int low = HexValue(text[i + 1]);
		if (high < 0 || low < 0)
		{
			decoded.clear();
			return decoded;
		}
		decoded.push_back(static_cast<char>((high << 4) | low));
	}
	return decoded;
}

static bool ParseBool(const char* text)
{
	return std::strcmp(text, "1") == 0;
}

static std::string ToUtf8(const CString& text)
{
	return std::string(CT2A(text, CP_UTF8));
}

static CString FromUtf8(const std::string& text)
{
	return CString(CA2T(CStringA(text.c_str()), CP_UTF8));
}

static std::vector<MatchResult> ExecuteWithPcre(
	const CString& pattern,
	const CString& sourceString,
	bool ignoreCase,
	bool global,
	bool multiline)
{
#define OVECCOUNT 300
	std::vector<MatchResult> matches;
	const char* error = NULL;
	int options = ignoreCase ? PCRE_CASELESS : 0;
	options |= PCRE_UTF8;
	if (multiline)
		options |= PCRE_MULTILINE;

	int errorOffset = 0;
	int ovector[OVECCOUNT];
	CT2A pat(pattern, CP_UTF8);
	pcre* re = pcre_compile(pat, options, &error, &errorOffset, NULL);
	if (!re)
	{
		std::fprintf(stderr, "PCRE compile error at offset %d: %s\n", errorOffset, error ? error : "unknown");
		return matches;
	}

	CT2A subj(sourceString, CP_UTF8);
	const int subjectLength = static_cast<int>(std::strlen(subj));
	int rc = 0;
	int offset = 0;
	int charOffset = 0;

	do
	{
		rc = pcre_exec(re, NULL, subj, subjectLength, offset, 0, ovector, OVECCOUNT);
		if (rc > 0)
		{
			char* substringStart = const_cast<char*>(subj.m_psz) + ovector[0];
			int substringLength = ovector[1] - ovector[0];
			if (substringLength >= 0)
			{
				while (offset < ovector[0])
				{
					offset += UTF8_CHAR_LEN(subj[offset]);
					charOffset++;
				}

				CStringA utf8Match(substringStart, substringLength);
				CString value = CString(CA2T(utf8Match, CP_UTF8));

				MatchResult result;
				result.Value = ToUtf8(value);
				result.FirstIndex = charOffset;

				for (int i = 1; i < rc; i++)
				{
					substringStart = const_cast<char*>(subj.m_psz) + ovector[i * 2];
					substringLength = ovector[i * 2 + 1] - ovector[i * 2];
					if (substringLength >= 0)
					{
						CStringA utf8SubMatch(substringStart, substringLength);
						result.SubMatches.push_back(ToUtf8(CString(CA2T(utf8SubMatch, CP_UTF8))));
					}
				}

				matches.push_back(result);
				charOffset += value.GetLength();
				offset = ovector[1];
				if (ovector[0] == ovector[1])
				{
					offset++;
					charOffset++;
				}
			}

			if (!global)
				break;
		}
	} while (rc > 0);

	pcre_free(re);
	return matches;
}

static std::vector<MatchResult> ExecuteWithPcre2(
	const CString& pattern,
	const CString& sourceString,
	bool ignoreCase,
	bool global,
	bool multiline)
{
	std::vector<MatchResult> matches;
	uint32_t options = ignoreCase ? PCRE2_CASELESS : 0;
	options |= PCRE2_UTF;
	if (multiline)
		options |= PCRE2_MULTILINE;

	CT2A pat(pattern, CP_UTF8);
	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;
	pcre2_code* re = pcre2_compile(
		reinterpret_cast<PCRE2_SPTR>(static_cast<LPCSTR>(pat)),
		PCRE2_ZERO_TERMINATED,
		options,
		&errorNumber,
		&errorOffset,
		NULL);
	if (!re)
	{
		PCRE2_UCHAR buffer[256] = {0};
		pcre2_get_error_message(errorNumber, buffer, sizeof(buffer));
		std::fprintf(
			stderr,
			"PCRE2 compile error at offset %zu: %s\n",
			static_cast<size_t>(errorOffset),
			reinterpret_cast<const char*>(buffer));
		return matches;
	}

	CT2A subj(sourceString, CP_UTF8);
	const int subjectLength = static_cast<int>(std::strlen(subj));
	pcre2_match_data* matchData = pcre2_match_data_create(OVECCOUNT / 3, NULL);
	if (!matchData)
	{
		pcre2_code_free(re);
		std::fprintf(stderr, "Не удалось выделить match_data для PCRE2.\n");
		return matches;
	}

	int rc = 0;
	int offset = 0;
	int charOffset = 0;
	do
	{
		rc = pcre2_match(
			re,
			reinterpret_cast<PCRE2_SPTR>(static_cast<LPCSTR>(subj)),
			subjectLength,
			offset,
			0,
			matchData,
			NULL);
		if (rc > 0)
		{
			PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(matchData);
			char* substringStart = const_cast<char*>(static_cast<LPCSTR>(subj)) + ovector[0];
			int substringLength = static_cast<int>(ovector[1] - ovector[0]);
			if (substringLength >= 0)
			{
				while (offset < static_cast<int>(ovector[0]))
				{
					offset += UTF8_CHAR_LEN(subj[offset]);
					charOffset++;
				}

				CStringA utf8Match(substringStart, substringLength);
				CString value = CString(CA2T(utf8Match, CP_UTF8));

				MatchResult result;
				result.Value = ToUtf8(value);
				result.FirstIndex = charOffset;

				for (int i = 1; i < rc; i++)
				{
					substringStart = const_cast<char*>(static_cast<LPCSTR>(subj)) + ovector[i * 2];
					substringLength = static_cast<int>(ovector[i * 2 + 1] - ovector[i * 2]);
					if (substringLength >= 0)
					{
						CStringA utf8SubMatch(substringStart, substringLength);
						result.SubMatches.push_back(ToUtf8(CString(CA2T(utf8SubMatch, CP_UTF8))));
					}
				}

				matches.push_back(result);
				charOffset += value.GetLength();
				offset = static_cast<int>(ovector[1]);
				if (ovector[0] == ovector[1])
				{
					offset++;
					charOffset++;
				}
			}

			if (!global)
				break;
		}
	} while (rc > 0);

	pcre2_match_data_free(matchData);
	pcre2_code_free(re);
	return matches;
}

static bool PrintMismatch(
	const char* label,
	const MatchResult& pcre,
	const MatchResult& pcre2)
{
	bool matched = true;
	if (pcre.Value != pcre2.Value)
	{
		std::fprintf(stderr, "%s: отличаются значения совпадения: PCRE='%s', PCRE2='%s'\n",
			label, pcre.Value.c_str(), pcre2.Value.c_str());
		matched = false;
	}
	if (pcre.FirstIndex != pcre2.FirstIndex)
	{
		std::fprintf(stderr, "%s: отличаются индексы первого символа: PCRE=%d, PCRE2=%d\n",
			label, pcre.FirstIndex, pcre2.FirstIndex);
		matched = false;
	}
	if (pcre.SubMatches.size() != pcre2.SubMatches.size())
	{
		std::fprintf(stderr, "%s: отличается число подгрупп: PCRE=%zu, PCRE2=%zu\n",
			label, pcre.SubMatches.size(), pcre2.SubMatches.size());
		return false;
	}

	for (size_t i = 0; i < pcre.SubMatches.size(); ++i)
	{
		if (pcre.SubMatches[i] != pcre2.SubMatches[i])
		{
			std::fprintf(stderr,
				"%s: отличается подгруппа %zu: PCRE='%s', PCRE2='%s'\n",
				label,
				i,
				pcre.SubMatches[i].c_str(),
				pcre2.SubMatches[i].c_str());
			matched = false;
		}
	}

	return matched;
}

int main(int argc, char* argv[])
{
	if (argc != 6)
		return 30;

	const std::string subject = DecodeHex(argv[1]);
	const std::string pattern = DecodeHex(argv[2]);
	const bool ignoreCase = ParseBool(argv[3]);
	const bool global = ParseBool(argv[4]);
	const bool multiline = ParseBool(argv[5]);

	if (pattern.empty())
		return 31;

	const CString sourceText = FromUtf8(subject);
	const CString patternText = FromUtf8(pattern);
	const std::vector<MatchResult> pcreMatches =
		ExecuteWithPcre(patternText, sourceText, ignoreCase, global, multiline);
	const std::vector<MatchResult> pcre2Matches =
		ExecuteWithPcre2(patternText, sourceText, ignoreCase, global, multiline);

	if (pcreMatches.size() != pcre2Matches.size())
	{
		std::fprintf(stderr,
			"Отличается число совпадений: PCRE=%zu, PCRE2=%zu\n",
			pcreMatches.size(),
			pcre2Matches.size());
		return 1;
	}

	for (size_t i = 0; i < pcreMatches.size(); ++i)
	{
		char label[64] = {0};
		std::snprintf(label, sizeof(label), "Совпадение %zu", i);
		if (!PrintMismatch(label, pcreMatches[i], pcre2Matches[i]))
			return 2;
	}

	std::cout << "PCRE и PCRE2 совпадают на текущем кейсе.\n";
	return 0;
}
