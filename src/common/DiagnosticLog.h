#pragma once
#include <windows.h>
#include <shlobj.h>
#include <string>
#include "DeploymentContext.h"
#include "DiagnosticPrivacy.h"

namespace FbeDiagnostic
{
inline bool WriteUtf8(const std::wstring& path, const std::wstring& line)
{
    const int length = ::WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), NULL, 0, NULL, NULL);
    if (length <= 0) return false;
    std::string bytes(static_cast<size_t>(length), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), &bytes[0], length, NULL, NULL);
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {}; bool ok = ::GetFileSizeEx(file, &size) != FALSE;
    // Bounded fallback: fail closed when full; never truncate existing evidence silently.
    if (!ok || size.QuadPart + length > 1024 * 1024) { ::CloseHandle(file); return false; }
    ::SetFilePointer(file, 0, NULL, FILE_END);
    DWORD written = 0;
    ok = ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) != FALSE && written == bytes.size();
    ::CloseHandle(file); return ok;
}
inline void HResult(const wchar_t* category, const wchar_t* code, HRESULT error, const wchar_t* operation)
{
    // An unavailable logger must never change plug-in behavior, including allocation failures.
    try {
        (void)operation; // Arbitrary operation strings are deliberately not a payload type.
        typedef void (__stdcall* HostSink)(const wchar_t*, const wchar_t*, HRESULT);
        HostSink sink = reinterpret_cast<HostSink>(::GetProcAddress(::GetModuleHandleW(NULL), "_FbeRegisterDiagnosticEvent@12"));
        if (!sink) sink = reinterpret_cast<HostSink>(::GetProcAddress(::GetModuleHandleW(NULL), "FbeRegisterDiagnosticEvent"));
        if (sink) { sink(category, code, error); return; }
        const std::wstring directory = DeploymentContext::DiagnosticsDirectory();
        if (directory.empty()) return;
        const int created = ::SHCreateDirectoryExW(NULL, directory.c_str(), NULL);
        if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS) return;
        SYSTEMTIME now = {}; ::GetSystemTime(&now);
        wchar_t line[512] = {};
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"%04u-%02u-%02uT%02u:%02u:%02uZ; PID=%lu; TID=%lu; session_id=unavailable; source=%s; code=%s; hr=0x%08lX; details=omitted\r\n",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
            ::GetCurrentProcessId(), ::GetCurrentThreadId(),
            FbeDiagnosticPrivacy::Identifier(category ? category : L"") ? category : L"unknown",
            FbeDiagnosticPrivacy::Identifier(code ? code : L"") ? code : L"unknown", static_cast<unsigned long>(error));
        WriteUtf8(directory + L"fbe-diagnostic.log", line);
    } catch (...) { }
}
}
