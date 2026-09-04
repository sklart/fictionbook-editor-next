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

	const uint32_t options = PCRE2_UTF | PCRE2_UCP;
	const int iterations = 20;
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	wprintf(L"scenario;baseline_ms;cache_ms;matches\n");
	for (std::vector<Scenario>::const_iterator it = scenarios.begin(); it != scenarios.end(); ++it) {
		unsigned int matches = 0;
		LARGE_INTEGER start;
		QueryPerformanceCounter(&start);
		for (int run = 0; run < iterations; ++run) {
			int errorNumber = 0;
			PCRE2_SIZE errorOffset = 0;
			pcre2_code* code = pcre2_compile(
				reinterpret_cast<PCRE2_SPTR>(it->Pattern),
				static_cast<PCRE2_SIZE>(wcslen(it->Pattern)), options,
				&errorNumber, &errorOffset, NULL);
			if (code == NULL)
				return 1;
			matches += FindAll(code, it->Subject);
			pcre2_code_free(code);
		}
		const double baselineMilliseconds = ElapsedMilliseconds(start, frequency);

		CompiledCodeCache cache(48);
		QueryPerformanceCounter(&start);
		for (int run = 0; run < iterations; ++run) {
			int errorNumber = 0;
			PCRE2_SIZE errorOffset = 0;
			bool allocationError = false;
			CodeLease lease;
			if (!cache.Acquire(CString(it->Pattern), options, &errorNumber,
				&errorOffset, &allocationError, lease))
				return 2;
			matches += FindAll(lease.Get(), it->Subject);
		}
		const double cacheMilliseconds = ElapsedMilliseconds(start, frequency);
		wprintf(L"%s;%.3f;%.3f;%u\n", it->Name, baselineMilliseconds,
			cacheMilliseconds, matches);
	}
	return 0;
}
