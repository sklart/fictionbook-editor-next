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
    const wchar_t* invalid[] = { L"3", L"3.1", L"3.1.", L"v3.1.0", L"3.1.0-", L"3.1.0-.rc", L"3.1.0+.build", L"3.1.0-.rc+build", L"3.1.0.+build.1", L"3.1.0-rc.+build.1", L"3.1.0-rc..1+build.1", L"3.1.0-rc..1", L"3.1.0-rc.01", L"3.1.0-01", L"03.1.0", L"3.01.0", L"3.1.00" };
    for (int i = 0; i < _countof(invalid); ++i)
        if (IsValidUpdateVersion(invalid[i])) return 1;
    const wchar_t* valid[] = { L"3.1.0-alpha", L"3.1.0-alpha.1", L"3.1.0-beta.2", L"3.1.0-rc.10", L"3.1.0+build.1", L"3.1.0+build-foo", L"3.1.0+build.foo-bar", L"3.1.0-rc.2+build.7", L"3.1.0-rc.1+build-foo" };
    for (int i = 0; i < _countof(valid); ++i)
        if (!IsValidUpdateVersion(valid[i])) return 1;
    if (!IsValidUpdateVersion(L"3.1.0-rc.2+build.7")) return 1;
    if (!IsPrereleaseUpdateVersion(L"3.1.0-rc.2+build.7") || !IsPrereleaseUpdateVersion(L"3.1.0-rc.1+build-foo") || IsPrereleaseUpdateVersion(L"3.1.0+build.7") || IsPrereleaseUpdateVersion(L"3.1.0+build-foo") || IsPrereleaseUpdateVersion(L"3.1.0+build.foo-bar")) return 1;
    if (GetUpdateBaseVersion(L"3.1.0-rc.2") != L"3.1.0") return 1;
    return ExpectCompare(L"3.0.7", L"3.0.7", 0) ||
        ExpectCompare(L"3.0.8", L"3.0.7", 1) ||
        ExpectCompare(L"3.1.0", L"3.0.9", 1) ||
        ExpectCompare(L"3.1.0-alpha", L"3.1.0-alpha.1", -1) ||
        ExpectCompare(L"3.1.0-alpha.1", L"3.1.0-alpha.beta", -1) ||
        ExpectCompare(L"3.1.0-alpha.beta", L"3.1.0-beta", -1) ||
        ExpectCompare(L"3.1.0-beta", L"3.1.0-beta.2", -1) ||
        ExpectCompare(L"3.1.0-beta.2", L"3.1.0-beta.11", -1) ||
        ExpectCompare(L"3.1.0-beta.11", L"3.1.0-rc.1", -1) ||
        ExpectCompare(L"3.1.0-rc.1", L"3.1.0-rc.2", -1) ||
        ExpectCompare(L"3.1.0-rc.2", L"3.1.0", -1) ||
        ExpectCompare(L"3.1.0-rc.10", L"3.1.0-rc.2", 1) ||
        ExpectCompare(L"3.2.0", L"3.1.99", 1) ||
        ExpectCompare(L"4.0.0", L"3.99.99", 1);
}
