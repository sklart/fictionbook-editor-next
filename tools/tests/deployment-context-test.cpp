#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace {
int checks = 0, failures = 0, failLargeAllocation = 0;
bool fakeApi = true, moduleFailure = false, moduleTruncated = false, profileMissing = true;
bool profileFailure = false;
std::wstring modulePath, fakeTestMode, settingsPath, commandLine, profileValue, profileFile;
int moduleCalls = 0, settingsCalls = 0, profileCalls = 0;
void Check(bool value, const char* message) {
    ++checks; if(!value) { ++failures; std::printf("FAIL: %s\n", message); }
}
DWORD Copy(const std::wstring& source, wchar_t* target, DWORD capacity) {
    const DWORD length = static_cast<DWORD>(source.size());
    const DWORD copied = length < capacity ? length : capacity - 1;
    std::memcpy(target, source.data(), copied * sizeof(wchar_t)); target[copied] = 0;
    return length;
}
DWORD WINAPI TestModuleFileName(HMODULE module, LPWSTR target, DWORD capacity) {
    ++moduleCalls;
    if(!fakeApi) return ::GetModuleFileNameW(module, target, capacity);
    Check(capacity == 32768, "module buffer preserves long-path capacity");
    if(moduleFailure) return 0;
    if(moduleTruncated) { std::memset(target, 'X', capacity * sizeof(wchar_t)); return capacity; }
    return Copy(modulePath, target, capacity);
}
DWORD WINAPI TestEnvironment(LPCWSTR name, LPWSTR target, DWORD capacity) {
    if(!fakeApi) return ::GetEnvironmentVariableW(name, target, capacity);
    const bool mode = ::lstrcmpW(name, L"FBE_NEXT_TEST_MODE") == 0;
    if(!mode) { ++settingsCalls; Check(capacity == 32768, "settings buffer preserves long-path capacity"); }
    const std::wstring& value = mode ? fakeTestMode : settingsPath;
    if(value.empty()) return 0;
    const DWORD length = Copy(value, target, capacity);
    return length < capacity ? length : length + 1;
}
LPWSTR WINAPI TestCommandLine() { return fakeApi ? commandLine.data() : ::GetCommandLineW(); }
DWORD WINAPI TestProfile(LPCWSTR section, LPCWSTR key, LPCWSTR fallback, LPWSTR target, DWORD capacity, LPCWSTR file) {
    ++profileCalls;
    if(!fakeApi) return ::GetPrivateProfileStringW(section, key, fallback, target, capacity, file);
    Check(capacity == 32768, "INI buffer preserves long-path capacity");
    Check(::lstrcmpW(section, L"Portable") == 0 && ::lstrcmpW(key, L"DataPath") == 0 && ::lstrcmpW(fallback, L"Data") == 0,
        "INI section, key and default unchanged");
    profileFile = file;
    if(profileFailure) return 0;
    const DWORD length = Copy(profileMissing ? std::wstring(fallback) : profileValue, target, capacity);
    return length < capacity ? length : capacity - 1;
}
}

// The product uses ordinary throwing allocations; fail only its large buffers.
void* operator new(size_t size) {
    if(size >= 32768 * sizeof(wchar_t) && failLargeAllocation > 0 && --failLargeAllocation == 0) throw std::bad_alloc();
    if(void* block = std::malloc(size ? size : 1)) return block;
    throw std::bad_alloc();
}
void operator delete(void* block) noexcept { std::free(block); }

#define GetModuleFileNameW TestModuleFileName
#define GetEnvironmentVariableW TestEnvironment
#define GetCommandLineW TestCommandLine
#define GetPrivateProfileStringW TestProfile
#include "../../src/common/DeploymentContext.h"
#undef GetModuleFileNameW
#undef GetEnvironmentVariableW
#undef GetCommandLineW
#undef GetPrivateProfileStringW

namespace {
template<class Function> void ExpectAllocationFailure(Function function, int allocation, const char* message) {
    failLargeAllocation = allocation;
    bool caught = false;
    try { function(); } catch(const std::bad_alloc&) { caught = true; }
    failLargeAllocation = 0;
    Check(caught, message);
}
void TestExecutableDirectory() {
    using namespace DeploymentContext;
    modulePath = L"C:\\Books\\FBE.exe";
    Check(ExecutableDirectory() == L"C:\\Books\\", "ordinary executable directory");
    modulePath = L"C:/Books/FBE.exe";
    Check(ExecutableDirectory() == L"C:/Books/", "forward slash preserved");
    modulePath = L"C:\\Редактор\\FBE.exe";
    Check(ExecutableDirectory() == L"C:\\Редактор\\", "Unicode executable directory");
    modulePath = L"FBE.exe"; Check(ExecutableDirectory().empty(), "no parent separator returns empty");
    moduleFailure = true; Check(ExecutableDirectory().empty(), "module API failure returns empty"); moduleFailure = false;
    moduleTruncated = true; Check(ExecutableDirectory().empty(), "non-terminated truncated module path rejected"); moduleTruncated = false;
    modulePath = L"C:\\" + std::wstring(32755, L'x') + L"\\FBE.exe";
    Check(modulePath.size() == 32766 && ExecutableDirectory() == modulePath.substr(0, modulePath.size() - 7), "near-capacity executable path preserved");
    modulePath.insert(3, 1, L'x');
    Check(modulePath.size() == 32767 && ExecutableDirectory() == modulePath.substr(0, modulePath.size() - 7), "last fitting executable path preserved");
    modulePath = L"C:\\FBE\\FBE.exe";
    const int before = moduleCalls;
    ExpectAllocationFailure([] { ExecutableDirectory(); }, 1, "module buffer allocation failure propagates bad_alloc");
    Check(moduleCalls == before, "no module API access after failed buffer allocation");
}
void TestSettings() {
    using namespace DeploymentContext;
    settingsPath = L"D:\\Тест";
    for(const wchar_t* mode : {L"", L"0", L"11"}) {
        fakeTestMode = mode;
        const int before = settingsCalls;
        failLargeAllocation = 1;
        Check(TestSettingsDirectory().empty(), "test override disabled unless mode is exactly 1");
        Check(settingsCalls == before && failLargeAllocation == 1, "disabled test override neither allocates buffer nor reads path");
        failLargeAllocation = 0;
    }
    fakeTestMode = L"1";
    Check(TestSettingsDirectory() == L"D:\\Тест\\", "enabled Unicode override adds separator");
    settingsPath += L'\\'; Check(TestSettingsDirectory() == settingsPath, "existing trailing separator retained");
    settingsPath.clear(); Check(TestSettingsDirectory().empty(), "missing environment path returns empty");
    settingsPath.assign(32767, L'x');
    Check(TestSettingsDirectory() == settingsPath + L'\\', "last fitting environment path accepted");
    settingsPath.push_back(L'x'); Check(TestSettingsDirectory().empty(), "oversized environment value rejected");
    const int before = settingsCalls;
    ExpectAllocationFailure([] { TestSettingsDirectory(); }, 1, "settings buffer allocation failure propagates bad_alloc");
    Check(settingsCalls == before, "no environment path access after failed buffer allocation");
    settingsPath.clear(); fakeTestMode.clear();
}
void TestDataRoot() {
    using namespace DeploymentContext;
    commandLine = L"probe.exe --installed";
    const int modulesBefore = moduleCalls, profilesBefore = profileCalls;
    Check(DataRoot().empty() && moduleCalls == modulesBefore && profileCalls == profilesBefore, "installed mode does not read portable path");
    commandLine = L"probe.exe --portable";
    const std::wstring root = L"C:\\FBE\\";
    modulePath = root + L"FBE.exe";
    Check(DataRoot() == root + L"Data\\", "missing INI value uses Data default");
    Check(profileFile == root + L"portable.ini", "marker is relative to executable, not working directory");
    profileFailure = true;
    Check(DataRoot() == root + L"Data\\", "INI API failure retains initialized default");
    profileFailure = false;
    profileMissing = false;
    for(const wchar_t* value : {L"UserData", L"Данные", L"My Data", L".data"}) {
        profileValue = value;
        Check(DataRoot() == root + value + L"\\", "valid single-component DataPath retained");
    }
    for(const wchar_t* value : {L"", L"..", L"../Data", L"Data..old", L"C:Data", L"C:\\Data", L"/Data", L"\\Data", L"Data/Sub", L"Data\\Sub"}) {
        profileValue = value;
        Check(DataRoot() == root + L"Data\\", "unsafe DataPath falls back to Data");
    }
    profileValue.assign(32767, L'x');
    Check(DataRoot() == root + profileValue + L"\\", "long DataPath preserves existing semantics");
    profileValue.push_back(L'x');
    Check(DataRoot() == root + profileValue.substr(0, 32767) + L"\\", "INI truncation preserves existing semantics");
    profileValue = L"UserData";
    const int before = profileCalls;
    ExpectAllocationFailure([] { DataRoot(); }, 2, "INI buffer allocation failure propagates bad_alloc");
    Check(profileCalls == before, "no INI access after failed buffer allocation");
    Check(SettingsDirectory() == root + L"UserData\\Settings\\", "portable settings path unchanged");
    Check(DiagnosticsDirectory() == root + L"UserData\\Diagnostics\\", "portable diagnostics path unchanged");
    Check(!RegistryPersistenceAllowed(), "portable mode disallows registry persistence");
    commandLine = L"probe.exe --portable --installed";
    Check(HasInvalidModeOverride() && CurrentMode() == Mode::Installed && DataRoot().empty(), "conflicting switches retain installed precedence");
}
}
int main() {
    TestExecutableDirectory(); TestSettings(); TestDataRoot();
    fakeApi = false;
    wchar_t actual[MAX_PATH] = {};
    const DWORD length = ::GetModuleFileNameW(NULL, actual, _countof(actual));
    Check(length > 0 && length < _countof(actual), "real executable path available");
    const std::wstring executable(actual, length);
    Check(DeploymentContext::ExecutableDirectory() == executable.substr(0, executable.find_last_of(L"\\/") + 1), "real Win32 executable directory matches");
    std::printf("DeploymentContext: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
