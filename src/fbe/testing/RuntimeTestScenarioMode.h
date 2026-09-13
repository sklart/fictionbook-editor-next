#pragma once

#include <cwchar>

namespace RuntimeTests
{
inline bool IsScenario(const wchar_t* expectedScenario)
{
	if (!expectedScenario)
		return false;

	wchar_t testMode[4] = {}, scenario[64] = {};
	const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode));
	const DWORD scenarioLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario));
	return testModeLength == 1 && testMode[0] == L'1' &&
		scenarioLength == std::wcslen(expectedScenario) && std::wcscmp(scenario, expectedScenario) == 0;
}
}
