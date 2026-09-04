#include <stdio.h>

#include "RegexPcre2CodeCache.h"

using AU::RegexPcre2::CodeCacheStatistics;
using AU::RegexPcre2::CodeLease;
using AU::RegexPcre2::CompiledCodeCache;

static bool Acquire(
	CompiledCodeCache& cache,
	const wchar_t* pattern,
	uint32_t options,
	CodeLease& lease)
{
	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;
	bool allocationError = false;
	if (!cache.Acquire(CString(pattern), options, &errorNumber, &errorOffset,
		&allocationError, lease)) {
		wprintf(L"Could not compile '%s' (error %d at %llu, allocation=%d)\n",
			pattern, errorNumber, static_cast<unsigned long long>(errorOffset),
			allocationError ? 1 : 0);
		return false;
	}
	return true;
}

static bool CheckMatch(pcre2_code* code, const wchar_t* subject)
{
	pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(code, NULL);
	if (matchData == NULL)
		return false;
	const int result = pcre2_match(code, reinterpret_cast<PCRE2_SPTR>(subject),
		static_cast<PCRE2_SIZE>(wcslen(subject)), 0, 0, matchData, NULL);
	pcre2_match_data_free(matchData);
	return result >= 0;
}

int wmain()
{
	const uint32_t commonOptions = PCRE2_UTF | PCRE2_UCP;
	CompiledCodeCache cache(2);
	CodeLease alpha;
	if (!Acquire(cache, L"(alpha)", commonOptions, alpha) ||
		!CheckMatch(alpha.Get(), L"alpha"))
		return 1;
	alpha.Reset();

	CodeLease alphaHit;
	if (!Acquire(cache, L"(alpha)", commonOptions, alphaHit))
		return 2;
	CodeCacheStatistics statistics = cache.GetStatistics();
	if (statistics.Misses != 1 || statistics.Hits != 1 || statistics.Entries != 1) {
		printf("Cache hit statistics are incorrect.\n");
		return 3;
	}

	CodeLease caseInsensitive;
	if (!Acquire(cache, L"(alpha)", commonOptions | PCRE2_CASELESS, caseInsensitive) ||
		!CheckMatch(caseInsensitive.Get(), L"ALPHA")) {
		printf("Compile-option cache key was not isolated.\n");
		return 4;
	}
	caseInsensitive.Reset();

	CodeLease multiline;
	if (!Acquire(cache, L"^alpha$", commonOptions | PCRE2_MULTILINE, multiline) ||
		!CheckMatch(multiline.Get(), L"x\nalpha\ny"))
		return 5;
	multiline.Reset();
	statistics = cache.GetStatistics();
	if (statistics.Misses != 3 || statistics.Evictions != 1 || statistics.Entries != 2) {
		printf("Cache eviction statistics are incorrect.\n");
		return 6;
	}

	// alphaHit intentionally keeps the evicted pattern alive. This exercises
	// the lease ownership path that prevents use-after-free during eviction.
	if (!CheckMatch(alphaHit.Get(), L"alpha")) {
		printf("An evicted but leased pattern was freed too early.\n");
		return 7;
	}
	alphaHit.Reset();

	CodeLease alphaMissAfterEviction;
	if (!Acquire(cache, L"(alpha)", commonOptions, alphaMissAfterEviction))
		return 8;
	statistics = cache.GetStatistics();
	if (statistics.Misses != 4 || statistics.Evictions != 2) {
		printf("Evicted entry did not cause a cache miss.\n");
		return 9;
	}

	// Clearing the cache must release its ownership without invalidating a
	// concurrently-held lease; scope destruction then exercises final release.
	cache.Clear();
	if (!CheckMatch(alphaMissAfterEviction.Get(), L"alpha")) {
		printf("A cache clear freed a leased pattern too early.\n");
		return 10;
	}

	printf("PCRE2 compiled-code cache smoke test passed. hits=%u misses=%u evictions=%u\n",
		statistics.Hits, statistics.Misses, statistics.Evictions);
	return 0;
}
