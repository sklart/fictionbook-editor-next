#include "stdafx.h"

#include "apputils.h"
#include "RegexBackend.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include "pcre2.h"

namespace AU {
namespace {

const int kRegexOvectorCount = 300;

static CString BuildPcre2ErrorText(int errorNumber)
{
	PCRE2_UCHAR buffer[256] = {};
	const int result = pcre2_get_error_message(
		errorNumber,
		buffer,
		sizeof(buffer) / sizeof(buffer[0]));
	if (result < 0)
		return CString(L"Неизвестная ошибка PCRE2.");

	return CString(CA2T(reinterpret_cast<const char*>(buffer), CP_UTF8));
}

uint32_t BuildCompileOptions(const RegexBackend::Options& options)
{
	uint32_t compileOptions = options.IgnoreCase ? PCRE2_CASELESS : 0;
	compileOptions |= PCRE2_UTF;
	if (options.Multiline)
		compileOptions |= PCRE2_MULTILINE;
	return compileOptions;
}

}

bool RegexBackend::Execute(
	const RegexBackend::Options& options,
	const CString& sourceString,
	CSimpleArray<RegexBackend::MatchData>& matches,
	CString& errorText)
{
	uint32_t compileOptions;
	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;
	int rc, offset, char_offset;

	matches.RemoveAll();
	errorText.Empty();

	compileOptions = BuildCompileOptions(options);

	CT2A pat(options.Pattern, CP_UTF8);
	pcre2_code* re = pcre2_compile(
		reinterpret_cast<PCRE2_SPTR>(static_cast<LPCSTR>(pat)),
		PCRE2_ZERO_TERMINATED,
		compileOptions,
		&errorNumber,
		&errorOffset,
		NULL);
	if (re == NULL)
	{
		errorText = BuildPcre2ErrorText(errorNumber);
		return false;
	}

	CT2A subj(sourceString, CP_UTF8);
	const int subjectLength = strlen(subj);
	pcre2_match_data* matchData = pcre2_match_data_create(kRegexOvectorCount / 3, NULL);
	if (matchData == NULL)
	{
		pcre2_code_free(re);
		errorText = L"Не удалось выделить память для match data PCRE2.";
		return false;
	}

	offset = char_offset = 0;

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
			char* substring_start = const_cast<char*>(static_cast<LPCSTR>(subj)) + ovector[0];
			int substring_length = static_cast<int>(ovector[1] - ovector[0]);
			if (substring_length >= 0)
			{
				CStringA utf8Match(substring_start, substring_length);
				while (offset < static_cast<int>(ovector[0]))
				{
					offset += UTF8_CHAR_LEN(subj[offset]);
					char_offset++;
				}

				RegexBackend::MatchData item;
				item.Value = CString(CA2T(utf8Match, CP_UTF8));
				item.FirstIndex = char_offset;

				for (int i = 1; i < rc; i++)
				{
					const PCRE2_SIZE groupStart = ovector[i * 2];
					const PCRE2_SIZE groupEnd = ovector[i * 2 + 1];
					if (groupStart != PCRE2_UNSET && groupEnd != PCRE2_UNSET)
					{
						substring_start = const_cast<char*>(static_cast<LPCSTR>(subj)) + groupStart;
						substring_length = static_cast<int>(groupEnd - groupStart);
						CStringA utf8SubMatch(substring_start, substring_length);
						item.SubMatches.Add(CString(CA2T(utf8SubMatch, CP_UTF8)));
					}
				}
				matches.Add(item);
				char_offset += item.Value.GetLength();
				offset = static_cast<int>(ovector[1]);
				if (ovector[0] == ovector[1])
				{
					offset++;
					char_offset++;
				}

				if (!options.Global)
					break;
			}
		}
	} while (rc > 0);

	pcre2_match_data_free(matchData);
	pcre2_code_free(re);

	return true;
}

}

