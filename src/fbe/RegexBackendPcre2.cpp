#include "stdafx.h"

#include "RegexBackend.h"

#define PCRE2_CODE_UNIT_WIDTH 16
#define PCRE2_STATIC
#include "pcre2.h"

namespace AU {
namespace {

// Keep PCRE2's documented defaults. Setting them explicitly makes the UI
// protection independent of a future PCRE2 build-time configuration change.
const uint32_t kRegexMatchLimit = 10000000;
const uint32_t kRegexDepthLimit = 10000000;

static CString BuildPcre2ErrorText(int errorNumber)
{
	PCRE2_UCHAR buffer[256] = {};
	const int result = pcre2_get_error_message(
		errorNumber,
		buffer,
		sizeof(buffer) / sizeof(buffer[0]));
	if (result < 0)
		return CString(L"Unknown PCRE2 error.");

	return CString(reinterpret_cast<const wchar_t*>(buffer));
}

CString BuildCompileErrorText(int errorNumber, PCRE2_SIZE errorOffset)
{
	CString errorText;
	errorText.Format(L"Ошибка регулярного выражения в позиции %llu: %s",
		static_cast<unsigned long long>(errorOffset),
		static_cast<LPCWSTR>(BuildPcre2ErrorText(errorNumber)));
	return errorText;
}

CString BuildMatchErrorText(int errorNumber)
{
	CString errorText;
	errorText.Format(L"Ошибка выполнения регулярного выражения: %s",
		static_cast<LPCWSTR>(BuildPcre2ErrorText(errorNumber)));
	return errorText;
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
	int rc;
	PCRE2_SIZE offset = 0;
	uint32_t globalOptions = 0;

	matches.RemoveAll();
	errorText.Empty();

	compileOptions = BuildCompileOptions(options);

	pcre2_code* re = pcre2_compile(
		reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(options.Pattern)),
		static_cast<PCRE2_SIZE>(options.Pattern.GetLength()),
		compileOptions,
		&errorNumber,
		&errorOffset,
		NULL);
	if (re == NULL)
	{
		errorText = BuildCompileErrorText(errorNumber, errorOffset);
		return false;
	}

	pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(re, NULL);
	pcre2_match_context* matchContext = pcre2_match_context_create(NULL);
	if (matchData == NULL || matchContext == NULL)
	{
		if (matchContext != NULL)
			pcre2_match_context_free(matchContext);
		if (matchData != NULL)
			pcre2_match_data_free(matchData);
		pcre2_code_free(re);
		errorText = L"Failed to allocate PCRE2 match data.";
		return false;
	}
	pcre2_set_match_limit(matchContext, kRegexMatchLimit);
	pcre2_set_depth_limit(matchContext, kRegexDepthLimit);

	while (true)
	{
		rc = pcre2_match(
			re,
			reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(sourceString)),
			static_cast<PCRE2_SIZE>(sourceString.GetLength()),
			offset,
			globalOptions,
			matchData,
			matchContext);

		if (rc == PCRE2_ERROR_NOMATCH)
			break;
		if (rc < 0)
		{
			errorText = BuildMatchErrorText(rc);
			pcre2_match_context_free(matchContext);
			pcre2_match_data_free(matchData);
			pcre2_code_free(re);
			return false;
		}

		const bool retryAtEmptyMatch =
			(globalOptions & PCRE2_NOTEMPTY_ATSTART) != 0 && offset == pcre2_get_ovector_pointer(matchData)[0];
		PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(matchData);
		const PCRE2_SIZE matchStart = ovector[0];
		const PCRE2_SIZE matchEnd = ovector[1];
		// The legacy wrapper advanced by one UTF-16 code unit after an empty
		// result. pcre2_next_match first retries the same position with
		// NOTEMPTY_ATSTART; suppress that retry's non-empty result to retain the
		// established IRegExp2 collection semantics while delegating advancement.
		if (!retryAtEmptyMatch && matchEnd >= matchStart)
		{
			RegexBackend::MatchData item;
			item.Value = CString(static_cast<LPCWSTR>(sourceString) + matchStart,
				static_cast<int>(matchEnd - matchStart));
			item.FirstIndex = static_cast<int>(matchStart);

			for (int i = 1; i < rc; i++)
			{
				const PCRE2_SIZE groupStart = ovector[i * 2];
				const PCRE2_SIZE groupEnd = ovector[i * 2 + 1];
				if (groupStart != PCRE2_UNSET && groupEnd != PCRE2_UNSET)
					item.SubMatches.Add(CString(static_cast<LPCWSTR>(sourceString) + groupStart,
						static_cast<int>(groupEnd - groupStart)));
			}
			matches.Add(item);
		}

		if (!options.Global || !pcre2_next_match(matchData, &offset, &globalOptions))
			break;
	}

	pcre2_match_context_free(matchContext);
	pcre2_match_data_free(matchData);
	pcre2_code_free(re);

	return true;
}

}
