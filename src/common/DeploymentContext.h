#pragma once

// A small shared path service.  It is header-only intentionally: FBE and its
// bundled plug-ins can use the same rules without adding another link-time
// dependency to the legacy projects.
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cwctype>
#include <string>

namespace DeploymentContext
{
    enum class Mode { Installed, Portable };

    inline std::wstring ExecutableDirectory()
    {
        std::wstring path(32768, L'\0');
        const DWORD length = ::GetModuleFileNameW(NULL, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size()) return std::wstring();
        std::wstring result(path.data(), length);
        const std::wstring::size_type slash = result.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring() : result.substr(0, slash + 1);
    }

    inline bool HasCommandLineSwitch(const wchar_t* name)
    {
        int count = 0; LPWSTR* arguments = ::CommandLineToArgvW(::GetCommandLineW(), &count);
        if (arguments == NULL) return false;
        bool found = false;
        for (int index = 1; index < count; ++index) if (::lstrcmpiW(arguments[index], name) == 0) { found = true; break; }
        ::LocalFree(arguments);
        if (found) return true;
        return false;
    }

    inline Mode CurrentMode()
    {
        const bool forcePortable = HasCommandLineSwitch(L"--portable");
        const bool forceInstalled = HasCommandLineSwitch(L"--installed");
        if (forcePortable && forceInstalled) return Mode::Installed; // caller reports invalid command line separately.
        if (forcePortable) return Mode::Portable;
        if (forceInstalled) return Mode::Installed;
        const std::wstring marker = ExecutableDirectory() + L"portable.ini";
        return ::GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES ? Mode::Portable : Mode::Installed;
    }

    inline bool HasInvalidModeOverride() { return HasCommandLineSwitch(L"--portable") && HasCommandLineSwitch(L"--installed"); }

    inline std::wstring TestSettingsDirectory()
    {
        wchar_t testMode[2] = {};
        if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) != 1 || testMode[0] != L'1') return std::wstring();
        std::wstring path(32768, L'\0');
        const DWORD length = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SETTINGS_DIRECTORY", path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size()) return std::wstring();
        std::wstring result(path.data(), length);
        if (result.back() != L'\\') result += L'\\';
        return result;
    }

    inline std::wstring DataRoot()
    {
        if (CurrentMode() != Mode::Portable) return std::wstring();
        const std::wstring root = ExecutableDirectory();
        std::wstring value = L"Data";
        value.resize(32768, L'\0');
        const std::wstring marker = root + L"portable.ini";
        ::GetPrivateProfileStringW(L"Portable", L"DataPath", L"Data", value.data(), static_cast<DWORD>(value.size()), marker.c_str());
        std::wstring path(value.c_str());
        if (path.empty() || path.find(L"..") != std::wstring::npos || path.find_first_of(L":/\\") != std::wstring::npos) path = L"Data";
        path = root + path;
        if (path.back() != L'\\') path += L'\\';
        return path;
    }

    inline std::wstring SettingsDirectory()
    {
        if (CurrentMode() == Mode::Portable) return DataRoot() + L"Settings\\";
        const std::wstring testDirectory = TestSettingsDirectory();
        if (!testDirectory.empty()) return testDirectory;
        wchar_t localAppData[MAX_PATH] = {};
        if (FAILED(::SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, localAppData))) return std::wstring();
        return std::wstring(localAppData) + L"\\FBE Next\\";
    }

    inline std::wstring DiagnosticsDirectory()
    {
        if (CurrentMode() == Mode::Portable) return DataRoot() + L"Diagnostics\\";
        const std::wstring settings = SettingsDirectory();
        if (!settings.empty()) return settings + L"Diagnostics\\";
        wchar_t temp[MAX_PATH] = {};
        return ::GetTempPathW(_countof(temp), temp) ? std::wstring(temp) + L"FBE Next Diagnostics\\" : std::wstring();
    }
    inline std::wstring RecoveryDirectory() { return CurrentMode() == Mode::Portable ? DataRoot() + L"Recovery\\" : SettingsDirectory() + L"Recovery\\"; }
    inline std::wstring LogsDirectory() { return CurrentMode() == Mode::Portable ? DataRoot() + L"Logs\\" : SettingsDirectory() + L"Logs\\"; }
    inline std::wstring CacheDirectory() { return CurrentMode() == Mode::Portable ? DataRoot() + L"Cache\\" : SettingsDirectory() + L"Cache\\"; }
    inline std::wstring TempDirectory() { return CurrentMode() == Mode::Portable ? DataRoot() + L"Temp\\" : SettingsDirectory() + L"Temp\\"; }
    inline std::wstring MutableContentRoot() { return CurrentMode() == Mode::Portable ? DataRoot() : SettingsDirectory(); }
    inline std::wstring UserDictionariesDirectory() { return MutableContentRoot() + L"Dictionaries\\"; }
    inline std::wstring UserThemesDirectory() { return MutableContentRoot() + L"Themes\\"; }
    inline std::wstring UserScriptsDirectory() { return MutableContentRoot() + L"Scripts\\"; }
    inline bool RegistryPersistenceAllowed() { return CurrentMode() == Mode::Installed; }
}
