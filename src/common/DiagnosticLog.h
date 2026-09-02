#pragma once

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>

#pragma comment(lib, "Shlwapi.lib")

namespace FbeDiagnostic
{
inline bool WriteUtf8(const std::wstring& path, const std::wstring& line)
{
    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return false;
    std::string utf8(static_cast<size_t>(bytes), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), &utf8[0], bytes, nullptr, nullptr);
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    ::SetFilePointer(file, 0, nullptr, FILE_END);
    DWORD written = 0;
    const bool ok = ::WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) != FALSE && written == utf8.size();
    ::FlushFileBuffers(file);
    ::CloseHandle(file);
    return ok;
}

inline void HResult(const wchar_t* category, const wchar_t* code, HRESULT error, const wchar_t* operation)
{
    wchar_t base[MAX_PATH] = {};
    std::wstring directory;
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, base))) {
        directory = base;
        while (!directory.empty() && directory.back() == L'\\') directory.pop_back();
        directory += L"\\FBE Next\\Diagnostics";
    } else {
        wchar_t temp[MAX_PATH] = {};
        if (!::GetTempPathW(_countof(temp), temp)) return;
        directory = temp;
        while (!directory.empty() && directory.back() == L'\\') directory.pop_back();
        directory += L"\\FBE Next Diagnostics";
    }
    if (FAILED(::SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr)) && ::GetLastError() != ERROR_ALREADY_EXISTS)
        return;

    SYSTEMTIME now = {};
    ::GetLocalTime(&now);
    wchar_t line[1024] = {};
    _snwprintf_s(line, _countof(line), _TRUNCATE,
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u; PID=%lu; TID=%lu; level=error; category=%s; code=%s; message=hr=0x%08lX; operation=%s\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
        ::GetCurrentProcessId(), ::GetCurrentThreadId(), category ? category : L"-", code ? code : L"-",
        static_cast<unsigned long>(error), operation ? operation : L"-");
    WriteUtf8(directory + L"\\fbe-diagnostic.log", line);
}
}
