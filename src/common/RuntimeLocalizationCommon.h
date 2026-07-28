#pragma once

#include <windows.h>
#include <atlstr.h>

#include <map>
#include <string>
#include <vector>

namespace FbeRuntimeLocalization {

inline bool IsKnownRuntimeLocaleName(const wchar_t* localeName)
{
    return localeName != NULL && localeName[0] != 0 &&
        (::lstrcmpiW(localeName, L"en-US") == 0 ||
         ::lstrcmpiW(localeName, L"ru-RU") == 0 ||
         ::lstrcmpiW(localeName, L"uk-UA") == 0 ||
         ::lstrcmpiW(localeName, L"de-DE") == 0 ||
         ::lstrcmpiW(localeName, L"fr-FR") == 0 ||
         ::lstrcmpiW(localeName, L"es-ES") == 0 ||
         ::lstrcmpiW(localeName, L"it-IT") == 0 ||
         ::lstrcmpiW(localeName, L"pl-PL") == 0 ||
         ::lstrcmpiW(localeName, L"cs-CZ") == 0 ||
         ::lstrcmpiW(localeName, L"pt-PT") == 0 ||
         ::lstrcmpiW(localeName, L"nl-NL") == 0 ||
         ::lstrcmpiW(localeName, L"bg-BG") == 0);
}


inline bool RemoveFileSpec(wchar_t* path)
{
    if (path == nullptr || path[0] == 0)
        return false;

    wchar_t* slash = wcsrchr(path, L'\\');
    wchar_t* altSlash = wcsrchr(path, L'/');
    if (altSlash != nullptr && (slash == nullptr || altSlash > slash))
        slash = altSlash;
    if (slash == nullptr)
        return false;

    *slash = 0;
    return true;
}

inline bool AppendPath(wchar_t* path, size_t pathChars, const wchar_t* part)
{
    if (path == nullptr || pathChars == 0 || part == nullptr || part[0] == 0)
        return false;

    const size_t currentLength = wcslen(path);
    const wchar_t* cleanPart = part;
    while (*cleanPart == L'\\' || *cleanPart == L'/')
        ++cleanPart;

    const bool needSlash = currentLength > 0 && path[currentLength - 1] != L'\\' && path[currentLength - 1] != L'/';
    const size_t partLength = wcslen(cleanPart);
    const size_t requiredLength = currentLength + (needSlash ? 1 : 0) + partLength;
    if (requiredLength + 1 > pathChars)
        return false;

    if (needSlash)
        wcscat_s(path, pathChars, L"\\");
    wcscat_s(path, pathChars, cleanPart);
    return true;
}

inline void JsonSkipWhitespace(const std::wstring& text, size_t& pos)
{
    while (pos < text.size()) {
        const wchar_t ch = text[pos];
        if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n')
            break;
        ++pos;
    }
}

inline int JsonHexValue(wchar_t ch)
{
    if (ch >= L'0' && ch <= L'9')
        return ch - L'0';
    if (ch >= L'a' && ch <= L'f')
        return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F')
        return ch - L'A' + 10;
    return -1;
}

inline bool JsonParseString(const std::wstring& text, size_t& pos, std::wstring& value)
{
    JsonSkipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != L'"')
        return false;
    ++pos;

    value.clear();
    while (pos < text.size()) {
        const wchar_t ch = text[pos++];
        if (ch == L'"')
            return true;
        if (ch != L'\\') {
            if (ch < 0x20)
                return false;
            value.push_back(ch);
            continue;
        }
        if (pos >= text.size())
            return false;

        const wchar_t escaped = text[pos++];
        switch (escaped) {
        case L'"': value.push_back(L'"'); break;
        case L'\\': value.push_back(L'\\'); break;
        case L'/': value.push_back(L'/'); break;
        case L'b': value.push_back(L'\b'); break;
        case L'f': value.push_back(L'\f'); break;
        case L'n': value.push_back(L'\n'); break;
        case L'r': value.push_back(L'\r'); break;
        case L't': value.push_back(L'\t'); break;
        case L'u': {
            if (pos + 4 > text.size())
                return false;
            int code = 0;
            for (int i = 0; i < 4; ++i) {
                const int digit = JsonHexValue(text[pos++]);
                if (digit < 0)
                    return false;
                code = (code << 4) | digit;
            }
            value.push_back(static_cast<wchar_t>(code));
            break;
        }
        default:
            return false;
        }
    }

    return false;
}

inline bool JsonSkipValue(const std::wstring& text, size_t& pos)
{
    JsonSkipWhitespace(text, pos);
    if (pos >= text.size())
        return false;

    if (text[pos] == L'"') {
        std::wstring ignored;
        return JsonParseString(text, pos, ignored);
    }

    if (text[pos] == L'{') {
        ++pos;
        JsonSkipWhitespace(text, pos);
        if (pos < text.size() && text[pos] == L'}') {
            ++pos;
            return true;
        }
        while (pos < text.size()) {
            std::wstring key;
            if (!JsonParseString(text, pos, key))
                return false;
            JsonSkipWhitespace(text, pos);
            if (pos >= text.size() || text[pos++] != L':')
                return false;
            if (!JsonSkipValue(text, pos))
                return false;
            JsonSkipWhitespace(text, pos);
            if (pos < text.size() && text[pos] == L',') {
                ++pos;
                continue;
            }
            if (pos < text.size() && text[pos] == L'}') {
                ++pos;
                return true;
            }
            return false;
        }
        return false;
    }

    if (text[pos] == L'[') {
        ++pos;
        JsonSkipWhitespace(text, pos);
        if (pos < text.size() && text[pos] == L']') {
            ++pos;
            return true;
        }
        while (pos < text.size()) {
            if (!JsonSkipValue(text, pos))
                return false;
            JsonSkipWhitespace(text, pos);
            if (pos < text.size() && text[pos] == L',') {
                ++pos;
                continue;
            }
            if (pos < text.size() && text[pos] == L']') {
                ++pos;
                return true;
            }
            return false;
        }
        return false;
    }

    const size_t valueStart = pos;
    const auto matchesLiteral = [&](const wchar_t* literal) {
        const size_t length = wcslen(literal);
        if(text.compare(pos, length, literal) != 0)
            return false;
        pos += length;
        return true;
    };
    if(matchesLiteral(L"true") || matchesLiteral(L"false") || matchesLiteral(L"null"))
        return true;
    pos = valueStart;
    if(text[pos] == L'-') ++pos;
    if(pos >= text.size()) return false;
    if(text[pos] == L'0') ++pos;
    else if(text[pos] >= L'1' && text[pos] <= L'9')
        while(pos < text.size() && text[pos] >= L'0' && text[pos] <= L'9') ++pos;
    else return false;
    if(pos < text.size() && text[pos] == L'.') {
        ++pos;
        const size_t fractionStart = pos;
        while(pos < text.size() && text[pos] >= L'0' && text[pos] <= L'9') ++pos;
        if(pos == fractionStart) return false;
    }
    if(pos < text.size() && (text[pos] == L'e' || text[pos] == L'E')) {
        ++pos;
        if(pos < text.size() && (text[pos] == L'+' || text[pos] == L'-')) ++pos;
        const size_t exponentStart = pos;
        while(pos < text.size() && text[pos] >= L'0' && text[pos] <= L'9') ++pos;
        if(pos == exponentStart) return false;
    }
    return true;
}

inline bool JsonFindObjectMember(const std::wstring& text, size_t objectStart, const wchar_t* memberName, size_t& memberValueStart)
{
    size_t pos = objectStart;
    JsonSkipWhitespace(text, pos);
    if (pos >= text.size() || text[pos++] != L'{')
        return false;

    JsonSkipWhitespace(text, pos);
    if (pos < text.size() && text[pos] == L'}')
        return false;

    while (pos < text.size()) {
        std::wstring key;
        if (!JsonParseString(text, pos, key))
            return false;
        JsonSkipWhitespace(text, pos);
        if (pos >= text.size() || text[pos++] != L':')
            return false;
        JsonSkipWhitespace(text, pos);

        if (key == memberName) {
            memberValueStart = pos;
            return true;
        }

        if (!JsonSkipValue(text, pos))
            return false;
        JsonSkipWhitespace(text, pos);
        if (pos < text.size() && text[pos] == L',') {
            ++pos;
            continue;
        }
        return false;
    }

    return false;
}

inline bool ReadUtf8TextFile(const wchar_t* path, std::wstring& text)
{
    HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size = {};
    if (!::GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 4 * 1024 * 1024) {
        ::CloseHandle(file);
        return false;
    }

    std::vector<char> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = ::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, NULL);
    ::CloseHandle(file);
    if (!ok || read != bytes.size())
        return false;

    const char* data = bytes.data();
    int count = static_cast<int>(bytes.size());
    if (count >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF) {
        data += 3;
        count -= 3;
    }

    const int wideCount = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, count, NULL, 0);
    if (wideCount <= 0)
        return false;

    text.assign(wideCount, L'\0');
    return ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, count, &text[0], wideCount) == wideCount;
}

inline CStringW ReadPublishedRuntimeLocaleName()
{
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};
    const DWORD envLength = ::GetEnvironmentVariableW(L"FBE_NEXT_UI_LOCALE", localeName, _countof(localeName));
    if (envLength > 0 && envLength < _countof(localeName) && IsKnownRuntimeLocaleName(localeName))
        return CStringW(localeName);

    wchar_t localAppData[MAX_PATH] = {};
    const DWORD appDataLength = ::GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, _countof(localAppData));
    if (appDataLength == 0 || appDataLength >= _countof(localAppData))
        return CStringW();

    wchar_t localePath[MAX_PATH] = {};
    wcscpy_s(localePath, localAppData);
    if (!AppendPath(localePath, _countof(localePath), L"FBE Next"))
        return CStringW();
    if (!AppendPath(localePath, _countof(localePath), L"interface-locale.txt"))
        return CStringW();

    std::wstring text;
    if (!ReadUtf8TextFile(localePath, text))
        return CStringW();

    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ' || text.back() == L'\t'))
        text.pop_back();

    return IsKnownRuntimeLocaleName(text.c_str()) ? CStringW(text.c_str()) : CStringW();
}

inline CStringW GetPreferredRuntimeLocaleName()
{
    CStringW localeName = ReadPublishedRuntimeLocaleName();
    if (!localeName.IsEmpty())
        return localeName;

    wchar_t systemLocale[LOCALE_NAME_MAX_LENGTH] = {};
    if (::GetUserDefaultLocaleName(systemLocale, _countof(systemLocale)) > 0 && IsKnownRuntimeLocaleName(systemLocale))
        return CStringW(systemLocale);

    return L"en-US";
}

inline bool RuntimeStringFileExists(HINSTANCE moduleInstance, const wchar_t* localeName, const wchar_t* moduleJsonName)
{
    if (moduleInstance == nullptr || !IsKnownRuntimeLocaleName(localeName) ||
        moduleJsonName == nullptr || moduleJsonName[0] == 0)
        return false;

    wchar_t modulePath[MAX_PATH] = {};
    const DWORD pathLength = ::GetModuleFileNameW(moduleInstance, modulePath, _countof(modulePath));
    if (pathLength == 0 || pathLength >= _countof(modulePath) || !RemoveFileSpec(modulePath))
        return false;

    if (!AppendPath(modulePath, _countof(modulePath), L"Lang") ||
        !AppendPath(modulePath, _countof(modulePath), localeName) ||
        !AppendPath(modulePath, _countof(modulePath), moduleJsonName))
        return false;

    const DWORD attributes = ::GetFileAttributesW(modulePath);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

template <typename Binding>
inline bool LoadRuntimeStringFileByRelativePath(HINSTANCE moduleInstance, const wchar_t* jsonFileName, const Binding* bindings, size_t bindingCount, std::map<UINT, CStringW>& strings)
{
    if (moduleInstance == nullptr || jsonFileName == nullptr || bindings == nullptr || bindingCount == 0)
        return false;

    wchar_t modulePath[MAX_PATH] = {};
    const DWORD pathLength = ::GetModuleFileNameW(moduleInstance, modulePath, _countof(modulePath));
    if (pathLength == 0 || pathLength >= _countof(modulePath))
        return false;

    if (!RemoveFileSpec(modulePath))
        return false;
    if (!AppendPath(modulePath, _countof(modulePath), jsonFileName))
        return false;

    std::wstring json;
    if (!ReadUtf8TextFile(modulePath, json))
        return false;

    size_t stringsObject = 0;
    if (!JsonFindObjectMember(json, 0, L"strings", stringsObject))
        return false;

    bool loadedAny = false;
    for (size_t i = 0; i < bindingCount; ++i) {
        size_t valueStart = 0;
        if (!JsonFindObjectMember(json, stringsObject, bindings[i].key, valueStart))
            continue;

        std::wstring value;
        if (!JsonParseString(json, valueStart, value) || value.empty())
            continue;

        strings[bindings[i].id] = CStringW(value.c_str());
        loadedAny = true;
    }

    return loadedAny;
}

inline bool LoadRuntimeStringFileByRelativePath(HINSTANCE moduleInstance, const wchar_t* jsonFileName, std::map<std::wstring, CStringW>& strings)
{
    if (moduleInstance == nullptr || jsonFileName == nullptr)
        return false;

    wchar_t modulePath[MAX_PATH] = {};
    const DWORD pathLength = ::GetModuleFileNameW(moduleInstance, modulePath, _countof(modulePath));
    if (pathLength == 0 || pathLength >= _countof(modulePath))
        return false;

    if (!RemoveFileSpec(modulePath))
        return false;
    if (!AppendPath(modulePath, _countof(modulePath), jsonFileName))
        return false;

    std::wstring json;
    if (!ReadUtf8TextFile(modulePath, json))
        return false;

    size_t stringsObject = 0;
    if (!JsonFindObjectMember(json, 0, L"strings", stringsObject))
        return false;

    size_t pos = stringsObject;
    JsonSkipWhitespace(json, pos);
    if (pos >= json.size() || json[pos++] != L'{')
        return false;

    bool loadedAny = false;
    while (pos < json.size()) {
        JsonSkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == L'}') {
            ++pos;
            return loadedAny;
        }

        std::wstring key;
        if (!JsonParseString(json, pos, key))
            return loadedAny;

        JsonSkipWhitespace(json, pos);
        if (pos >= json.size() || json[pos++] != L':')
            return loadedAny;

        JsonSkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == L'"') {
            std::wstring value;
            if (!JsonParseString(json, pos, value))
                return loadedAny;
            if (!key.empty() && !value.empty()) {
                strings[key] = CStringW(value.c_str());
                loadedAny = true;
            }
        }
        else if (!JsonSkipValue(json, pos)) {
            return loadedAny;
        }

        JsonSkipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == L',') {
            ++pos;
            continue;
        }
        if (pos < json.size() && json[pos] == L'}') {
            ++pos;
            return loadedAny;
        }
    }

    return loadedAny;
}

template <typename Binding>
inline bool LoadRuntimeStringFile(HINSTANCE moduleInstance, const wchar_t* localeName, const wchar_t* moduleJsonName, const Binding* bindings, size_t bindingCount, std::map<UINT, CStringW>& strings)
{
    if (localeName == nullptr || moduleJsonName == nullptr)
        return false;

    wchar_t relativeJsonPath[MAX_PATH] = {};
    wcscpy_s(relativeJsonPath, L"Lang");
    if (!AppendPath(relativeJsonPath, _countof(relativeJsonPath), localeName))
        return false;
    if (!AppendPath(relativeJsonPath, _countof(relativeJsonPath), moduleJsonName))
        return false;

    return LoadRuntimeStringFileByRelativePath(moduleInstance, relativeJsonPath, bindings, bindingCount, strings);
}

inline bool LoadRuntimeStringFile(HINSTANCE moduleInstance, const wchar_t* localeName, const wchar_t* moduleJsonName, std::map<std::wstring, CStringW>& strings)
{
    if (localeName == nullptr || moduleJsonName == nullptr)
        return false;

    wchar_t relativeJsonPath[MAX_PATH] = {};
    wcscpy_s(relativeJsonPath, L"Lang");
    if (!AppendPath(relativeJsonPath, _countof(relativeJsonPath), localeName))
        return false;
    if (!AppendPath(relativeJsonPath, _countof(relativeJsonPath), moduleJsonName))
        return false;

    return LoadRuntimeStringFileByRelativePath(moduleInstance, relativeJsonPath, strings);
}

template <typename Binding>
inline void LoadRuntimeStringFiles(HINSTANCE moduleInstance, const wchar_t* moduleJsonName, const Binding* bindings, size_t bindingCount, std::map<UINT, CStringW>& strings)
{
    strings.clear();
    LoadRuntimeStringFile(moduleInstance, L"en-US", moduleJsonName, bindings, bindingCount, strings);

    const CStringW localeName = GetPreferredRuntimeLocaleName();
    if (!localeName.IsEmpty() && ::lstrcmpiW(localeName, L"en-US") != 0)
        LoadRuntimeStringFile(moduleInstance, localeName, moduleJsonName, bindings, bindingCount, strings);
}

inline void LoadRuntimeStringFiles(HINSTANCE moduleInstance, const wchar_t* moduleJsonName, std::map<std::wstring, CStringW>& strings)
{
    strings.clear();
    LoadRuntimeStringFile(moduleInstance, L"en-US", moduleJsonName, strings);

    const CStringW localeName = GetPreferredRuntimeLocaleName();
    if (!localeName.IsEmpty() && ::lstrcmpiW(localeName, L"en-US") != 0)
        LoadRuntimeStringFile(moduleInstance, localeName, moduleJsonName, strings);
}

} // namespace FbeRuntimeLocalization
