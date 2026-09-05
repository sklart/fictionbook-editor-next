#include "stdafx.h"

#include "RegexBackend.h"
#include "RegexPcre2CodeCache.h"
#include "RegexPcre2MatchLoop.h"
#include "..\\RuntimeLocalization.h"

#define PCRE2_CODE_UNIT_WIDTH 16
#define PCRE2_STATIC
#include "pcre2.h"

namespace AU {
namespace {

// Keep PCRE2's documented defaults. Setting them explicitly makes the UI
// protection independent of a future PCRE2 build-time configuration change.
const uint32_t kRegexMatchLimit = 10000000;
const uint32_t kRegexDepthLimit = 10000000;
const size_t kRegexCodeCacheCapacity = 48;

// The cache owns one reference per entry; each Execute() keeps an independent
// lease so eviction can never free code while PCRE2 is matching with it.
// Benchmarking did not show a stable JIT gain for FBE searches. Keep the
// prepared-code cache enabled and leave optional JIT support for a later pass.
RegexPcre2::CompiledCodeCache g_regexCodeCache(kRegexCodeCacheCapacity, false);

static CString BuildPcre2ErrorText(int errorNumber)
{
	PCRE2_UCHAR buffer[256] = {};
	const int result = pcre2_get_error_message(
		errorNumber,
		buffer,
		sizeof(buffer) / sizeof(buffer[0]));
	if (result < 0)
		return FbeLoadRuntimeStringByKey(L"fbe.regex.error.unknown", L"Unknown PCRE2 error.");

	return CString(reinterpret_cast<const wchar_t*>(buffer));
}

CString BuildCompileErrorText(int errorNumber, PCRE2_SIZE errorOffset)
{
	CString errorText;
	errorText.Format(FbeLoadRuntimeStringByKey(
		L"fbe.regex.error.compile", L"Regular expression error at position %llu: %s"),
		static_cast<unsigned long long>(errorOffset),
		static_cast<LPCWSTR>(BuildPcre2ErrorText(errorNumber)));
	return errorText;
}

CString BuildMatchErrorText(int errorNumber)
{
	LPCWSTR key = L"fbe.regex.error.match";
	LPCWSTR fallback = L"Regular expression matching failed: %s";
	if (errorNumber == PCRE2_ERROR_MATCHLIMIT) {
		key = L"fbe.regex.error.match_limit";
		fallback = L"Regular expression match limit exceeded: %s";
	}
	else if (errorNumber == PCRE2_ERROR_DEPTHLIMIT) {
		key = L"fbe.regex.error.depth_limit";
		fallback = L"Regular expression depth limit exceeded: %s";
	}
	CString errorText;
	errorText.Format(FbeLoadRuntimeStringByKey(key, fallback),
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
	bool cacheAllocationError = false;

	matches.RemoveAll();
	errorText.Empty();

	compileOptions = BuildCompileOptions(options);

	RegexPcre2::CodeLease codeLease;
	if (!g_regexCodeCache.Acquire(
		options.Pattern,
		compileOptions,
		&errorNumber,
		&errorOffset,
		&cacheAllocationError,
		codeLease))
	{
		errorText = cacheAllocationError
			? FbeLoadRuntimeStringByKey(
				L"fbe.regex.error.allocation", L"Failed to allocate PCRE2 resources.")
			: BuildCompileErrorText(errorNumber, errorOffset);
		return false;
	}
	pcre2_code* re = codeLease.Get();

	pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(re, NULL);
	pcre2_match_context* matchContext = pcre2_match_context_create(NULL);
	if (matchData == NULL || matchContext == NULL)
	{
		if (matchContext != NULL)
			pcre2_match_context_free(matchContext);
		if (matchData != NULL)
			pcre2_match_data_free(matchData);
		errorText = FbeLoadRuntimeStringByKey(
			L"fbe.regex.error.allocation", L"Failed to allocate PCRE2 resources.");
		return false;
	}
	pcre2_set_match_limit(matchContext, kRegexMatchLimit);
	pcre2_set_depth_limit(matchContext, kRegexDepthLimit);

	const int matchResult = RegexPcre2::ForEachMatch(
		re,
		reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(sourceString)),
		static_cast<PCRE2_SIZE>(sourceString.GetLength()),
		options.Global,
		matchData,
		matchContext,
		[&matches, &sourceString](int rc, PCRE2_SIZE* ovector)
		{
			const PCRE2_SIZE matchStart = ovector[0];
			const PCRE2_SIZE matchEnd = ovector[1];
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
		});
	if (matchResult < 0)
	{
		errorText = BuildMatchErrorText(matchResult);
		pcre2_match_context_free(matchContext);
		pcre2_match_data_free(matchData);
		return false;
	}

	pcre2_match_context_free(matchContext);
	pcre2_match_data_free(matchData);
	return true;
}

}
