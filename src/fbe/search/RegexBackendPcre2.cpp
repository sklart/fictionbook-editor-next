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
	if (options.UnicodeProperties)
		compileOptions |= PCRE2_UCP;
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

	// PCRE2 exposes named group metadata on the compiled pattern. Preserve it
	// beside absolute UTF-16 capture offsets for Search Core; legacy IRegExp2
	// continues to consume SubMatches unchanged.
	std::vector<std::wstring> captureNames(pcre2_get_ovector_count(matchData));
	uint32_t nameCount = 0;
	uint32_t nameEntrySize = 0;
	PCRE2_SPTR nameTable = NULL;
	pcre2_pattern_info(re, PCRE2_INFO_NAMECOUNT, &nameCount);
	pcre2_pattern_info(re, PCRE2_INFO_NAMEENTRYSIZE, &nameEntrySize);
	pcre2_pattern_info(re, PCRE2_INFO_NAMETABLE, &nameTable);
	if (nameCount != 0 && nameEntrySize >= 3 && nameTable != NULL)
	{
		for (uint32_t nameIndex = 0; nameIndex < nameCount; ++nameIndex)
		{
			const PCRE2_UCHAR* entry = nameTable + nameIndex * nameEntrySize;
			// In the 16-bit PCRE2 table the group number occupies one UTF-16
			// code unit; the familiar two-byte form applies to the 8-bit API.
			const uint16_t captureIndex = static_cast<uint16_t>(entry[0]);
			if (captureIndex < captureNames.size())
				captureNames[captureIndex] = reinterpret_cast<const wchar_t*>(entry + 1);
		}
	}

	const int matchResult = RegexPcre2::ForEachMatch(
		re,
		reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(sourceString)),
		static_cast<PCRE2_SIZE>(sourceString.GetLength()),
		options.Global,
		matchData,
		matchContext,
		[&matches, &sourceString, &captureNames](int, PCRE2_SIZE* ovector)
		{
			const PCRE2_SIZE matchStart = ovector[0];
			const PCRE2_SIZE matchEnd = ovector[1];
			RegexBackend::MatchData item;
			item.Value = CString(static_cast<LPCWSTR>(sourceString) + matchStart,
				static_cast<int>(matchEnd - matchStart));
			item.FirstIndex = static_cast<int>(matchStart);

			for (std::size_t i = 1; i < captureNames.size(); ++i)
			{
				const PCRE2_SIZE groupStart = ovector[i * 2];
				const PCRE2_SIZE groupEnd = ovector[i * 2 + 1];
				const std::wstring name = captureNames[i];
				if (groupStart != PCRE2_UNSET && groupEnd != PCRE2_UNSET)
				{
					item.SubMatches.Add(CString(static_cast<LPCWSTR>(sourceString) + groupStart,
						static_cast<int>(groupEnd - groupStart)));
					item.Captures.push_back(Search::SearchCapture(
						i, static_cast<std::size_t>(groupStart),
						static_cast<std::size_t>(groupEnd - groupStart), name));
				}
				else
				{
					// Preserve capture numbering for legacy $1/$2 replacement syntax.
					item.SubMatches.Add(CString());
					item.Captures.push_back(Search::SearchCapture(i, 0, 0, name, false));
				}
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
