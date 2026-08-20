#pragma once

// A small shared path service.  It is header-only intentionally: FBE and its
// bundled plug-ins can use the same rules without adding another link-time
// dependency to the legacy projects.
#include <windows.h>
#include <cwctype>
#include <string>

namespace DeploymentContext
{
    enum class Mode { Installed, Portable };

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
        // The switches are diagnostic/test overrides.  Require a token
        // boundary so a path containing "--portable" is not interpreted.
        const std::wstring command(::GetCommandLineW());
        const std::wstring token(name);
        std::wstring::size_type position = command.find(token);
        while (position != std::wstring::npos)
        {
            const bool left = position == 0 || iswspace(command[position - 1]) || command[position - 1] == L'"';
            const std::wstring::size_type end = position + token.length();
            const bool right = end == command.length() || iswspace(command[end]) || command[end] == L'"';
            if (left && right) return true;
            position = command.find(token, end);
        }
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

    inline std::wstring DataRoot()
    {
        if (CurrentMode() != Mode::Portable) return std::wstring();
        const std::wstring root = ExecutableDirectory();
        wchar_t value[32768] = L"Data";
        const std::wstring marker = root + L"portable.ini";
        ::GetPrivateProfileStringW(L"Portable", L"DataPath", L"Data", value, _countof(value), marker.c_str());
        // Relative values are rooted at the executable; absolute values are
        // intentionally accepted for an explicitly configured portable copy.
        std::wstring path(value);
        if (path.empty()) path = L"Data";
        if (!(path.size() > 1 && path[1] == L':') && !(path.size() > 1 && path[0] == L'\\' && path[1] == L'\\')) path = root + path;
        if (path.back() != L'\\') path += L'\\';
        return path;
    }

    inline std::wstring SettingsDirectory()
    {
        if (CurrentMode() == Mode::Portable) return DataRoot() + L"Settings\\";
        wchar_t localAppData[MAX_PATH] = {};
        if (::GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, _countof(localAppData)) == 0) return std::wstring();
        return std::wstring(localAppData) + L"\\FBE Next\\";
    }

    inline std::wstring DiagnosticsDirectory() { return CurrentMode() == Mode::Portable ? DataRoot() + L"Diagnostics\\" : SettingsDirectory() + L"Diagnostics\\"; }
    inline std::wstring RecoveryDirectory() { return CurrentMode() == Mode::Portable ? DataRoot() + L"Recovery\\" : SettingsDirectory() + L"Recovery\\"; }
    inline bool RegistryPersistenceAllowed() { return CurrentMode() == Mode::Installed; }
}
