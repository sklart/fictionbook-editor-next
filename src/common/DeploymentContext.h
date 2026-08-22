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
    enum class CompatibilityTarget { Modern, Win7 };

    inline std::wstring ExecutableDirectory()
    {
        wchar_t path[32768] = {};
        const DWORD length = ::GetModuleFileNameW(NULL, path, _countof(path));
        if (length == 0 || length >= _countof(path)) return std::wstring();
        std::wstring result(path, length);
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

    inline CompatibilityTarget CurrentCompatibilityTarget()
    {
        wchar_t value[32] = L"Modern";
        const std::wstring marker = ExecutableDirectory() + L"deployment.ini";
        ::GetPrivateProfileStringW(L"Deployment", L"CompatibilityTarget", L"Modern", value, _countof(value), marker.c_str());
        // Unknown or malformed metadata must not make a package silently look
        // like a Win7 profile. Modern is the compatible fail-safe default.
        return ::lstrcmpiW(value, L"Win7") == 0 ? CompatibilityTarget::Win7 : CompatibilityTarget::Modern;
    }

    inline const wchar_t* CompatibilityTargetName()
    {
        return CurrentCompatibilityTarget() == CompatibilityTarget::Win7 ? L"Win7" : L"Modern";
    }

    inline std::wstring DataRoot()
    {
        if (CurrentMode() != Mode::Portable) return std::wstring();
        const std::wstring root = ExecutableDirectory();
        wchar_t value[32768] = L"Data";
        const std::wstring marker = root + L"portable.ini";
        ::GetPrivateProfileStringW(L"Portable", L"DataPath", L"Data", value, _countof(value), marker.c_str());
        std::wstring path(value);
        if (path.empty() || path.find(L"..") != std::wstring::npos || path.find_first_of(L":/\\") != std::wstring::npos) path = L"Data";
        path = root + path;
        if (path.back() != L'\\') path += L'\\';
        return path;
    }

    inline std::wstring SettingsDirectory()
    {
        if (CurrentMode() == Mode::Portable) return DataRoot() + L"Settings\\";
        wchar_t localAppData[MAX_PATH] = {};
        if (FAILED(::SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, localAppData))) return std::wstring();
        return std::wstring(localAppData) + L"\\FBE Next\\";
    }

    inline std::wstring DiagnosticsDirectory() { return CurrentMode() == Mode::Portable ? DataRoot() + L"Diagnostics\\" : SettingsDirectory() + L"Diagnostics\\"; }
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
