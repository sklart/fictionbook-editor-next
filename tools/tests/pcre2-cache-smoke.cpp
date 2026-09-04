#include <stdio.h>

#include "RegexPcre2CodeCache.h"

using AU::RegexPcre2::CodeCacheStatistics;
using AU::RegexPcre2::CodeLease;
using AU::RegexPcre2::CompiledCodeCache;

static bool Acquire(
	CompiledCodeCache& cache,
	const CString& pattern,
	uint32_t options,
	CodeLease& lease)
{
	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;
	bool allocationError = false;
	if (!cache.Acquire(pattern, options, &errorNumber, &errorOffset,
		&allocationError, lease)) {
		wprintf(L"Could not compile pattern (error %d at %llu, allocation=%d)\n",
			errorNumber, static_cast<unsigned long long>(errorOffset),
			allocationError ? 1 : 0);
		return false;
	}
	return true;
}

static bool CheckMatch(pcre2_code* code, const CString& subject)
{
	pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(code, NULL);
	if (matchData == NULL)
		return false;
	const int result = pcre2_match(code,
		reinterpret_cast<PCRE2_SPTR>(static_cast<LPCWSTR>(subject)),
		static_cast<PCRE2_SIZE>(subject.GetLength()), 0, 0, matchData, NULL);
	pcre2_match_data_free(matchData);
	return result >= 0;
}

int wmain()
{
	const uint32_t commonOptions = PCRE2_UTF;
	CompiledCodeCache cache(2, true);
	CodeLease alpha;
	if (!Acquire(cache, CString(L"(alpha)"), commonOptions, alpha) ||
		!CheckMatch(alpha.Get(), CString(L"alpha")))
		return 1;
	alpha.Reset();

	CodeLease alphaHit;
	if (!Acquire(cache, CString(L"(alpha)"), commonOptions, alphaHit))
		return 2;
	CodeCacheStatistics statistics = cache.GetStatistics();
	if (statistics.Misses != 1 || statistics.Hits != 1 || statistics.Entries != 1 ||
		statistics.JitCompiles != 1 || statistics.JitFallbacks != 0) {
		printf("Cache hit statistics are incorrect.\n");
		return 3;
	}

	CodeLease caseInsensitive;
	if (!Acquire(cache, CString(L"(alpha)"), commonOptions | PCRE2_CASELESS, caseInsensitive) ||
		!CheckMatch(caseInsensitive.Get(), CString(L"ALPHA"))) {
		printf("Compile-option cache key was not isolated.\n");
		return 4;
	}
	caseInsensitive.Reset();

	CodeLease multiline;
	if (!Acquire(cache, CString(L"^alpha$"), commonOptions | PCRE2_MULTILINE, multiline) ||
		!CheckMatch(multiline.Get(), CString(L"x\nalpha\ny")))
		return 5;
	multiline.Reset();
	statistics = cache.GetStatistics();
	if (statistics.Misses != 3 || statistics.Evictions != 1 || statistics.Entries != 2) {
		printf("Cache eviction statistics are incorrect.\n");
		return 6;
	}

	// alphaHit intentionally keeps the evicted pattern alive. This exercises
	// the lease ownership path that prevents use-after-free during eviction.
	if (!CheckMatch(alphaHit.Get(), CString(L"alpha"))) {
		printf("An evicted but leased pattern was freed too early.\n");
		return 7;
	}
	alphaHit.Reset();

	CodeLease alphaMissAfterEviction;
	if (!Acquire(cache, CString(L"(alpha)"), commonOptions, alphaMissAfterEviction))
		return 8;
	statistics = cache.GetStatistics();
	if (statistics.Misses != 4 || statistics.Evictions != 2) {
		printf("Evicted entry did not cause a cache miss.\n");
		return 9;
	}

	// Clearing the cache must release its ownership without invalidating a
	// concurrently-held lease; scope destruction then exercises final release.
	cache.Clear();
	if (!CheckMatch(alphaMissAfterEviction.Get(), CString(L"alpha"))) {
		printf("A cache clear freed a leased pattern too early.\n");
		return 10;
	}

	CompiledCodeCache embeddedNulCache(4, true);
	const CString patternWithNulB(L"a\0b", 3);
	const CString patternWithNulC(L"a\0c", 3);
	CodeLease nulB;
	CodeLease nulC;
	if (!Acquire(embeddedNulCache, patternWithNulB, commonOptions, nulB) ||
		!Acquire(embeddedNulCache, patternWithNulC, commonOptions, nulC) ||
		!CheckMatch(nulB.Get(), CString(L"a\0b", 3)) ||
		!CheckMatch(nulC.Get(), CString(L"a\0c", 3))) {
		printf("Embedded NUL pattern keys were not kept distinct.\n");
		return 11;
	}
	statistics = embeddedNulCache.GetStatistics();
	if (statistics.Misses != 2 || statistics.Hits != 0 || statistics.Entries != 2) {
		printf("Embedded NUL patterns incorrectly shared a cache entry.\n");
		return 12;
	}

	CompiledCodeCache jitFallbackCache(2, true);
	CodeLease jitFallback;
	// PCRE2 documents \C in UTF mode as unsupported by JIT, while the
	// ordinary matcher still supports it in this build.
	if (!Acquire(jitFallbackCache, CString(L"\\C"), commonOptions, jitFallback) ||
		!CheckMatch(jitFallback.Get(), CString(L"f"))) {
		printf("JIT fallback pattern did not use ordinary matching.\n");
		return 13;
	}
	statistics = jitFallbackCache.GetStatistics();
	if (statistics.JitCompiles != 0 || statistics.JitFallbacks != 1) {
		printf("JIT-unsupported pattern did not record a fallback.\n");
		return 14;
	}

	CompiledCodeCache invalidPatternCache(2);
	CodeLease invalidPattern;
	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;
	bool allocationError = false;
	if (invalidPatternCache.Acquire(CString(L"("), commonOptions, &errorNumber, &errorOffset,
		&allocationError, invalidPattern) || allocationError) {
		printf("Invalid pattern was accepted or reported as an allocation error.\n");
		return 15;
	}
	statistics = invalidPatternCache.GetStatistics();
	if (statistics.Entries != 0) {
		printf("Invalid pattern was added to the cache.\n");
		return 16;
	}
	CodeLease validAfterInvalid;
	if (!Acquire(invalidPatternCache, CString(L"valid"), commonOptions, validAfterInvalid) ||
		!CheckMatch(validAfterInvalid.Get(), CString(L"valid"))) {
		printf("Cache did not recover after an invalid pattern.\n");
		return 17;
	}

	CompiledCodeCache oomCache(2);
	oomCache.FailNextEntryAllocationForTesting();
	CodeLease failedAllocation;
	errorNumber = 0;
	errorOffset = 0;
	allocationError = false;
	if (oomCache.Acquire(CString(L"oom"), commonOptions, &errorNumber, &errorOffset,
		&allocationError, failedAllocation) || !allocationError || failedAllocation.Get() != NULL) {
		printf("Cache entry allocation failure was not reported safely.\n");
		return 18;
	}
	CodeLease afterOom;
	if (!Acquire(oomCache, CString(L"oom"), commonOptions, afterOom) ||
		!CheckMatch(afterOom.Get(), CString(L"oom"))) {
		printf("Cache lock or ownership was not recovered after allocation failure.\n");
		return 19;
	}

	printf("PCRE2 compiled-code cache smoke test passed.\n");
	return 0;
}
