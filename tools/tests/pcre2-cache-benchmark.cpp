#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>

#include "RegexPcre2CodeCache.h"

using AU::RegexPcre2::CodeLease;
using AU::RegexPcre2::CompiledCodeCache;

struct Scenario {
	const wchar_t* Name;
	const wchar_t* Pattern;
	std::wstring Subject;
};

static unsigned int FindAll(pcre2_code* code, const std::wstring& subject)
{
	pcre2_match_data* data = pcre2_match_data_create_from_pattern(code, NULL);
	if (data == NULL)
		return 0;
	unsigned int matches = 0;
	PCRE2_SIZE offset = 0;
	while (true) {
		const int rc = pcre2_match(code,
			reinterpret_cast<PCRE2_SPTR>(subject.c_str()),
			static_cast<PCRE2_SIZE>(subject.size()), offset, 0, data, NULL);
		if (rc < 0)
			break;
		++matches;
		PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(data);
		offset = ovector[1];
		if (offset >= subject.size())
			break;
	}
	pcre2_match_data_free(data);
	return matches;
}

static double ElapsedMilliseconds(LARGE_INTEGER start, LARGE_INTEGER frequency)
{
	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);
	return static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 /
		static_cast<double>(frequency.QuadPart);
}

static bool MeasureBaseline(
	const Scenario& scenario,
	uint32_t options,
	int iterations,
	LARGE_INTEGER frequency,
	double& milliseconds,
	unsigned int& matches)
{
	matches = 0;
	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);
	for (int run = 0; run < iterations; ++run) {
		int errorNumber = 0;
		PCRE2_SIZE errorOffset = 0;
		pcre2_code* code = pcre2_compile(
			reinterpret_cast<PCRE2_SPTR>(scenario.Pattern),
			static_cast<PCRE2_SIZE>(wcslen(scenario.Pattern)), options,
			&errorNumber, &errorOffset, NULL);
		if (code == NULL)
			return false;
		matches += FindAll(code, scenario.Subject);
		pcre2_code_free(code);
	}
	milliseconds = ElapsedMilliseconds(start, frequency);
	return true;
}

static bool MeasureCache(
	const Scenario& scenario,
	uint32_t options,
	int iterations,
	LARGE_INTEGER frequency,
	double& milliseconds,
	unsigned int& matches)
{
	CompiledCodeCache cache(48);
	matches = 0;
	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);
	for (int run = 0; run < iterations; ++run) {
		int errorNumber = 0;
		PCRE2_SIZE errorOffset = 0;
		bool allocationError = false;
		CodeLease lease;
		if (!cache.Acquire(CString(scenario.Pattern), options, &errorNumber,
			&errorOffset, &allocationError, lease))
			return false;
		matches += FindAll(lease.Get(), scenario.Subject);
	}
	milliseconds = ElapsedMilliseconds(start, frequency);
	return true;
}

int wmain()
{
	std::wstring largeText;
	for (int i = 0; i < 5000; ++i)
		largeText += L"entry-12345 alpha beta gamma\n";

	std::vector<Scenario> scenarios;
	scenarios.push_back({ L"repeat-single", L"(alpha|beta)-[0-9]+", L"alpha-123 beta-456" });
	scenarios.push_back({ L"find-all-large", L"entry-([0-9]+)\\s+(alpha)\\s+(beta)", largeText });
	scenarios.push_back({ L"groups", L"([A-Z][a-z]+)\\s+([A-Z][a-z]+)\\s+([0-9]{4})", L"John Smith 2026" });
	scenarios.push_back({ L"complex", L"(?:(?:https?)://)?([a-z0-9-]+(?:\\.[a-z0-9-]+)+)(?::([0-9]{2,5}))?(?:/[^\\s]*)?", L"https://example.org:8443/a/b?q=1" });

	// Keep this identical to the production baseline: UCP and JIT are off.
	const uint32_t options = PCRE2_UTF;
	const int iterations = 20;
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	// Warm up PCRE2 and the allocator before timing either implementation.
	for (std::vector<Scenario>::const_iterator it = scenarios.begin(); it != scenarios.end(); ++it) {
		double ignoredMilliseconds = 0;
		unsigned int ignoredMatches = 0;
		if (!MeasureBaseline(*it, options, 1, frequency, ignoredMilliseconds, ignoredMatches) ||
			!MeasureCache(*it, options, 1, frequency, ignoredMilliseconds, ignoredMatches))
			return 1;
	}

	wprintf(L"scenario;baseline_ms;cache_ms;baseline_matches;cache_matches\n");
	for (std::vector<Scenario>::const_iterator it = scenarios.begin(); it != scenarios.end(); ++it) {
		double baselineMilliseconds = 0;
		double cacheMilliseconds = 0;
		unsigned int baselineMatches = 0;
		unsigned int cacheMatches = 0;
		if (!MeasureBaseline(*it, options, iterations, frequency,
			baselineMilliseconds, baselineMatches))
			return 2;
		if (!MeasureCache(*it, options, iterations, frequency,
			cacheMilliseconds, cacheMatches))
			return 3;
		if (baselineMatches != cacheMatches) {
			wprintf(L"Match-count mismatch for %s: baseline=%u cache=%u\n",
				it->Name, baselineMatches, cacheMatches);
			return 4;
		}
		wprintf(L"%s;%.3f;%.3f;%u;%u\n", it->Name, baselineMilliseconds,
			cacheMilliseconds, baselineMatches, cacheMatches);
	}
	return 0;
}
