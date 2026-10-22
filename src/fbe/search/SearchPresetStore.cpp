#include "stdafx.h"
#include "SearchPresetStore.h"
#include "..\\..\\common\\DeploymentContext.h"

#include <set>

namespace
{
const size_t kMaximumFileBytes = 2 * 1024 * 1024;
const size_t kMaximumPresets = 256;
const int kMaximumIdLength = 96;
const int kMaximumNameLength = 256;
const int kMaximumTextLength = 64 * 1024;

CString WithTrailingSlash(const CString& directory)
{
    CString result(directory);
    if (!result.IsEmpty() && result.Right(1) != L"\\" && result.Right(1) != L"/")
        result += L"\\";
    return result;
}

CString EscapeXml(const CString& value)
{
    CString result(value);
    result.Replace(L"&", L"&amp;");
    result.Replace(L"<", L"&lt;");
    result.Replace(L">", L"&gt;");
    result.Replace(L"\"", L"&quot;");
    result.Replace(L"'", L"&apos;");
    // XML normalizes literal line endings. Preserve the exact user value.
    result.Replace(L"\r\n", L"&#13;&#10;");
    result.Replace(L"\r", L"&#13;");
    result.Replace(L"\n", L"&#10;");
    return result;
}

bool ParseBool(const CString& value, bool& result)
{
    if (value == L"0") { result = false; return true; }
    if (value == L"1") { result = true; return true; }
    return false;
}

bool GetAttribute(const MSXML2::IXMLDOMElementPtr& element, const wchar_t* name, CString& value)
{
    value.Empty();
    if (!element) return false;
    MSXML2::IXMLDOMNodePtr attribute = element->attributes->getNamedItem(_bstr_t(name));
    if (!attribute || attribute->nodeType != MSXML2::NODE_ATTRIBUTE) return false;
    _variant_t nodeValue(attribute->nodeValue);
    if (V_VT(&nodeValue) != VT_BSTR || V_BSTR(&nodeValue) == NULL) return false;
    value = V_BSTR(&nodeValue);
    return true;
}

bool DecodeXmlText(const CString& text, CString& value)
{
    value.Empty();
    for (int index = 0; index < text.GetLength(); ++index)
    {
        if (text[index] != L'&') { value += text[index]; continue; }
        const int end = text.Find(L';', index + 1);
        if (end < 0) return false;
        const CString entity = text.Mid(index, end - index + 1);
        if (entity == L"&amp;") value += L'&';
        else if (entity == L"&lt;") value += L'<';
        else if (entity == L"&gt;") value += L'>';
        else if (entity == L"&quot;") value += L'"';
        else if (entity == L"&apos;") value += L'\'';
        else if (entity == L"&#13;") value += L'\r';
        else if (entity == L"&#10;") value += L'\n';
        else return false;
        index = end;
    }
    return true;
}

bool GetChildText(const MSXML2::IXMLDOMElementPtr& element, const wchar_t* name, CString& value)
{
    value.Empty();
    if (!element) return false;
    MSXML2::IXMLDOMNodePtr node = element->selectSingleNode(_bstr_t(name));
    if (!node) return false;
    const CString xml(static_cast<LPCWSTR>(_bstr_t(node->xml)));
    const int start = xml.Find(L'>');
    const int end = xml.ReverseFind(L'<');
    return start >= 0 && end >= start && DecodeXmlText(xml.Mid(start + 1, end - start - 1), value);
}
bool IsValidText(const CString& value, int maximumLength, bool permitEmpty)
{
    return (permitEmpty || !value.IsEmpty()) && value.GetLength() <= maximumLength;
}

bool IsValidPreset(const FbeSearchPresets::SearchPreset& preset)
{
    using namespace FbeSearchPresets;
    if (preset.builtIn || !IsValidText(preset.id, kMaximumIdLength, false) ||
        !IsValidText(preset.name, kMaximumNameLength, false) ||
        !IsValidText(preset.findText, kMaximumTextLength, true) ||
        !IsValidText(preset.replacementText, kMaximumTextLength, true)) return false;
    return preset.context == SearchUiContext::Design || preset.context == SearchUiContext::Source;
}

bool EnsureDirectory(const CString& directory)
{
    if (directory.IsEmpty()) return false;
    const DWORD attributes = ::GetFileAttributesW(directory);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const int result = ::SHCreateDirectoryExW(NULL, directory, NULL);
    return result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS;
}

bool ReadUtf16File(const CString& path, CString& xml, bool& exists)
{
    xml.Empty();
    exists = false;
    HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        const DWORD error = ::GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    exists = true;
    const DWORD length = ::GetFileSize(file, NULL);
    if (length == INVALID_FILE_SIZE || length > kMaximumFileBytes || (length % sizeof(wchar_t)) != 0)
    {
        ::CloseHandle(file);
        return false;
    }
    std::vector<wchar_t> buffer(length / sizeof(wchar_t) + 1, 0);
    DWORD read = 0;
    const bool readOk = ::ReadFile(file, &buffer[0], length, &read, NULL) != FALSE && read == length;
    ::CloseHandle(file);
    if (!readOk) return false;
    const wchar_t* start = &buffer[0];
    if (length >= sizeof(wchar_t) && start[0] == 0xFEFF) ++start;
    xml = start;
    return true;
}

bool WriteUtf16FileAtomically(const CString& path, const CString& xml
#if defined(FBE_SEARCH_PRESET_STORE_TESTING)
    , bool failBeforeReplace
#endif
)
{
    GUID id = {};
    if (FAILED(::CoCreateGuid(&id))) return false;
    wchar_t suffix[40] = {};
    if (::StringFromGUID2(id, suffix, _countof(suffix)) == 0) return false;
    // Each writer owns its staging file, on the same volume as the destination.
    const CString temporary = path + L"." + suffix + L".tmp";
    HANDLE file = ::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    const DWORD bytes = static_cast<DWORD>(xml.GetLength() * sizeof(wchar_t));
    DWORD written = 0;
    const bool wrote = ::WriteFile(file, static_cast<LPCWSTR>(xml), bytes, &written, NULL) != FALSE && written == bytes;
    const bool flushed = wrote && ::FlushFileBuffers(file) != FALSE;
    ::CloseHandle(file);
    if (!flushed
#if defined(FBE_SEARCH_PRESET_STORE_TESTING)
        || failBeforeReplace
#endif
    )
    {
        ::DeleteFileW(temporary);
        return false;
    }
    // Windows may briefly deny replacement; retry only the already flushed rename.
    DWORD replaceError = ERROR_SUCCESS;
    for (unsigned attempt = 0; attempt != 5; ++attempt)
    {
        if (::MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        replaceError = ::GetLastError();
        if (replaceError != ERROR_ACCESS_DENIED &&
            replaceError != ERROR_SHARING_VIOLATION && replaceError != ERROR_LOCK_VIOLATION) break;
        if (replaceError == ERROR_ACCESS_DENIED)
        {
            const DWORD attributes = ::GetFileAttributesW(path);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY)) break;
        }
        if (attempt != 4) ::Sleep(10u << attempt);
    }
    ::DeleteFileW(temporary);
    ::SetLastError(replaceError);
    return false;
}
}

namespace FbeSearchPresets
{
SearchPresetStore::SearchPresetStore(const CString& settingsDirectory)
    : m_settingsDirectory(settingsDirectory.IsEmpty()
        ? CString(DeploymentContext::SettingsDirectory().c_str())
        : settingsDirectory)
#if defined(FBE_SEARCH_PRESET_STORE_TESTING)
    , m_failNextWrite(false)
#endif
{
    m_settingsDirectory = WithTrailingSlash(m_settingsDirectory);
}

CString SearchPresetStore::FilePath() const
{
    return m_settingsDirectory + L"SearchTemplates.xml";
}

bool SearchPresetStore::Load(std::vector<SearchPreset>& presets) const
{
    presets.clear();
    CString xml;
    bool exists = false;
    if (!ReadUtf16File(FilePath(), xml, exists)) return false;
    if (!exists) return true;

    try
    {
        MSXML2::IXMLDOMDocument2Ptr document;
        if (FAILED(document.CreateInstance(__uuidof(MSXML2::DOMDocument60))) || !document) return false;
        document->async = VARIANT_FALSE;
        document->validateOnParse = VARIANT_FALSE;
        document->resolveExternals = VARIANT_FALSE;
        document->setProperty(_bstr_t(L"ProhibitDTD"), _variant_t(true));
        if (document->loadXML(_bstr_t(static_cast<LPCWSTR>(xml))) != VARIANT_TRUE) return false;

        MSXML2::IXMLDOMElementPtr root = document->documentElement;
        CString version;
        if (!root || CString(static_cast<LPCWSTR>(root->nodeName)) != L"SearchTemplates" ||
            !GetAttribute(root, L"version", version) || version != L"1") return false;

        std::set<std::wstring> ids;
        MSXML2::IXMLDOMNodeListPtr nodes = root->selectNodes(_bstr_t(L"Template"));
        for (long index = 0; nodes && index < nodes->length && presets.size() < kMaximumPresets; ++index)
        {
            MSXML2::IXMLDOMElementPtr element(nodes->item[index]);
            SearchPreset preset;
            CString context, value;
            if (!GetAttribute(element, L"id", preset.id) || !GetAttribute(element, L"name", preset.name) ||
                !GetAttribute(element, L"context", context) || !GetAttribute(element, L"regexp", value) || !ParseBool(value, preset.regexp) ||
                !GetAttribute(element, L"matchCase", value) || !ParseBool(value, preset.matchCase) ||
                !GetAttribute(element, L"wholeWord", value) || !ParseBool(value, preset.wholeWord) ||
                !GetAttribute(element, L"unicodeProperties", value) || !ParseBool(value, preset.unicodeProperties) ||
                !GetAttribute(element, L"hasReplacement", value) || !ParseBool(value, preset.hasReplacement) ||
                !GetChildText(element, L"Find", preset.findText)) continue;
            if (context == L"design") preset.context = SearchUiContext::Design;
            else if (context == L"source") preset.context = SearchUiContext::Source;
            else continue;
            if (preset.hasReplacement && !GetChildText(element, L"Replace", preset.replacementText)) continue;
            preset.builtIn = false;
            const std::wstring id(static_cast<LPCWSTR>(preset.id));
            if (!IsValidPreset(preset) || ids.find(id) != ids.end()) continue;
            ids.insert(id);
            presets.push_back(preset);
        }
        return true;
    }
    catch (const _com_error&)
    {
        presets.clear();
        return false;
    }
}

bool SearchPresetStore::Save(const std::vector<SearchPreset>& presets) const
{
    if (presets.size() > kMaximumPresets || !EnsureDirectory(m_settingsDirectory)) return false;
    std::set<std::wstring> ids;
    for (size_t index = 0; index < presets.size(); ++index)
    {
        if (!IsValidPreset(presets[index])) return false;
        const std::wstring id(static_cast<LPCWSTR>(presets[index].id));
        if (!ids.insert(id).second) return false;
    }

    CString xml(L"\xFEFF<?xml version=\"1.0\" encoding=\"utf-16\"?>\r\n<SearchTemplates version=\"1\">\r\n");
    for (size_t index = 0; index < presets.size(); ++index)
    {
        const SearchPreset& preset = presets[index];
        const wchar_t* context = preset.context == SearchUiContext::Design ? L"design" : L"source";
        xml.AppendFormat(L"  <Template id=\"%s\" name=\"%s\" context=\"%s\" regexp=\"%d\" matchCase=\"%d\" wholeWord=\"%d\" unicodeProperties=\"%d\" hasReplacement=\"%d\">\r\n",
            static_cast<LPCWSTR>(EscapeXml(preset.id)), static_cast<LPCWSTR>(EscapeXml(preset.name)), context,
            preset.regexp ? 1 : 0, preset.matchCase ? 1 : 0, preset.wholeWord ? 1 : 0,
            preset.unicodeProperties ? 1 : 0, preset.hasReplacement ? 1 : 0);
        xml.AppendFormat(L"    <Find>%s</Find>\r\n", static_cast<LPCWSTR>(EscapeXml(preset.findText)));
        if (preset.hasReplacement)
            xml.AppendFormat(L"    <Replace>%s</Replace>\r\n", static_cast<LPCWSTR>(EscapeXml(preset.replacementText)));
        xml += L"  </Template>\r\n";
    }
    xml += L"</SearchTemplates>\r\n";

    if (static_cast<size_t>(xml.GetLength()) * sizeof(wchar_t) > kMaximumFileBytes) return false;

#if defined(FBE_SEARCH_PRESET_STORE_TESTING)
    const bool failBeforeReplace = m_failNextWrite;
    m_failNextWrite = false;
    return WriteUtf16FileAtomically(FilePath(), xml, failBeforeReplace);
#else
    return WriteUtf16FileAtomically(FilePath(), xml);
#endif
}
}
