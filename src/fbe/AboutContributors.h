#pragma once
#include <windows.h>
#include <limits.h>
#include <string>

namespace AboutContributors {
// Resource data is a byte span, not necessarily a NUL-terminated string.
// An unavailable or invalid resource leaves the contributors control empty.
inline std::wstring Load(HMODULE module) {
    const HRSRC resource = ::FindResourceW(module, L"ABOUT_FILE", L"ABOUT_FILE");
    if(!resource) return {};
    const DWORD size = ::SizeofResource(module, resource);
    if(!size || size > INT_MAX) return {};
    const HGLOBAL loaded = ::LoadResource(module, resource);
    if(!loaded) return {};
    const char* bytes = static_cast<const char*>(::LockResource(loaded));
    if(!bytes) return {};
    const int length = static_cast<int>(size);
    const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, length, NULL, 0);
    if(count <= 0) return {};
    std::wstring text(static_cast<size_t>(count), L'\0');
    if(::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, length, &text[0], count) != count) return {};
    return text;
}
}
