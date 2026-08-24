#include "../../src/fbe/UpdateVersion.h"
#include "../../src/fbe/UpdateChannel.h"
#include <stdio.h>

static int ExpectCompare(const wchar_t* left, const wchar_t* right, int expected)
{
    const int actual = CompareUpdateVersions(left, right);
    if ((actual < 0 ? -1 : actual > 0 ? 1 : 0) != expected) {
        wprintf(L"Unexpected comparison: %s vs %s (%d)\n", left, right, actual);
        return 1;
    }
    return 0;
}

int wmain()
{
	if (GetUpdateManifestUrl(UpdateChannel::Stable).Find(L"/update.xml") < 0 ||
		GetUpdateManifestUrl(UpdateChannel::Prerelease).Find(L"/update-prerelease.xml") < 0) return 1;
    const wchar_t* invalid[] = { L"3", L"3.1", L"3.1.", L"v3.1.0", L"3.1.0-", L"3.1.0-rc..1" };
    for (int i = 0; i < _countof(invalid); ++i)
        if (IsValidUpdateVersion(invalid[i])) return 1;
    if (!IsValidUpdateVersion(L"3.1.0-rc.2+build.7")) return 1;
    if (GetUpdateBaseVersion(L"3.1.0-rc.2") != L"3.1.0") return 1;
    return ExpectCompare(L"3.0.7", L"3.0.7", 0) ||
        ExpectCompare(L"3.0.8", L"3.0.7", 1) ||
        ExpectCompare(L"3.1.0", L"3.0.9", 1) ||
        ExpectCompare(L"3.1.0-beta.1", L"3.1.0-beta.2", -1) ||
        ExpectCompare(L"3.1.0-beta.2", L"3.1.0-rc.1", -1) ||
        ExpectCompare(L"3.1.0-rc.1", L"3.1.0-rc.2", -1) ||
        ExpectCompare(L"3.1.0-rc.2", L"3.1.0", -1) ||
        ExpectCompare(L"3.1.0-rc.10", L"3.1.0-rc.2", 1) ||
        ExpectCompare(L"3.2.0", L"3.1.99", 1) ||
        ExpectCompare(L"4.0.0", L"3.99.99", 1);
}
