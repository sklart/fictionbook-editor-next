#pragma once
#include "ScriptExecutionContext.h"
#include "ScriptFileCommit.h"
#include "../BackupFileCommit.h"
#include "../../common/DeploymentContext.h"
#include <set>
#include <cmath>
#include <algorithm>

namespace FbeScripts {
namespace SettingsStore {
inline HRESULT& LastFailure() { static thread_local HRESULT value = S_OK; return value; }
inline LPCWSTR& LastFailureOperation() { static thread_local LPCWSTR value = L"none"; return value; }
constexpr DWORD MaximumBytes = 1024 * 1024;
constexpr size_t MaximumEntries = 1024;
constexpr UINT MaximumValueLength = 32768;
struct Entry { CString identity, name, type, data; };
inline CString Hex(const CString& value)
{
    CString result; LPWSTR buffer = result.GetBuffer(value.GetLength() * 4);
    const wchar_t digits[] = L"0123456789abcdef";
    for (int i = 0; i < value.GetLength(); ++i)
        for (int nibble = 0; nibble < 4; ++nibble) buffer[i * 4 + nibble] = digits[(value[i] >> ((3 - nibble) * 4)) & 15];
    result.ReleaseBuffer(value.GetLength() * 4); return result;
}
inline bool Unhex(const CString& value, CString& result)
{
    if (value.GetLength() % 4) return false;
    LPWSTR buffer = result.GetBuffer(value.GetLength() / 4);
    for (int i = 0; i < value.GetLength(); i += 4) {
        unsigned code = 0;
        for (int j = 0; j < 4; ++j) {
            const wchar_t digit = value[i + j];
            if (!(digit >= L'0' && digit <= L'9') && !(digit >= L'a' && digit <= L'f')) { result.ReleaseBuffer(0); return false; }
            code = code * 16 + (digit <= L'9' ? digit - L'0' : digit - L'a' + 10);
        }
        buffer[i / 4] = static_cast<wchar_t>(code);
    }
    result.ReleaseBuffer(value.GetLength() / 4); return true;
}
inline bool Normalize(const VARIANT& source, CComVariant& value, CString& type)
{
    CComVariant input;
    if (FAILED(::VariantCopyInd(&input, const_cast<VARIANT*>(&source)))) return false;
    switch (input.vt) {
    case VT_BSTR:
        if (::SysStringLen(input.bstrVal) > MaximumValueLength) return false;
        value = input; type = L"string"; return true;
    case VT_BOOL: value = input; type = L"boolean"; return true;
    case VT_I1: case VT_UI1: case VT_I2: case VT_UI2: case VT_I4: case VT_UI4:
    case VT_I8: case VT_UI8: case VT_INT: case VT_UINT: case VT_R4: case VT_R8:
        if (FAILED(::VariantChangeTypeEx(&value, &input, LOCALE_INVARIANT, 0, VT_R8)) || !_finite(value.dblVal)) return false;
        type = L"number"; return true;
    default: return false;
    }
}
inline CString Identity()
{
    const ScriptExecutionContext* context = CurrentExecutionContext();
    if (!context || context->path.IsEmpty()) return CString();
    if (!context->uid.IsEmpty()) return CString(L"uid:") + context->uid;
    CString path(context->path); path.MakeLower(); return CString(L"path:") + path;
}
inline CString FilePath() { return CString(DeploymentContext::SettingsDirectory().c_str()) + L"ScriptSettings.xml"; }
inline MSXML2::IXMLDOMDocument2Ptr NewDocument()
{
    MSXML2::IXMLDOMDocument2Ptr document;
    CheckError(document.CreateInstance(__uuidof(MSXML2::DOMDocument60)));
    document->async = VARIANT_FALSE; document->resolveExternals = VARIANT_FALSE; document->validateOnParse = VARIANT_FALSE;
    document->setProperty(L"ProhibitDTD", _variant_t(true));
    return document;
}
inline CString Attribute(MSXML2::IXMLDOMElementPtr element, LPCWSTR name)
{
    _variant_t value = element->getAttribute(name);
    return value.vt == VT_BSTR ? CString(value.bstrVal, ::SysStringLen(value.bstrVal)) : CString();
}
inline bool Load(std::vector<Entry>& entries)
{
    entries.clear();
    CHandle file(::CreateFileW(FilePath(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
    if (file == INVALID_HANDLE_VALUE) return ::GetLastError() == ERROR_FILE_NOT_FOUND || ::GetLastError() == ERROR_PATH_NOT_FOUND;
    LARGE_INTEGER length = {}; if (!::GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > MaximumBytes || length.QuadPart % 2) return false;
    std::vector<wchar_t> bytes(static_cast<size_t>(length.QuadPart / 2) + 1, 0); DWORD read = 0;
    if (!::ReadFile(file, bytes.data(), static_cast<DWORD>(length.QuadPart), &read, NULL) || read != length.QuadPart) return false;
    const size_t size = static_cast<size_t>(length.QuadPart / 2);
    if (wcslen(bytes.data()) != size) return false;
    auto document = NewDocument();
    const wchar_t* xml = bytes.data(); if (*xml == 0xfeff) ++xml;
    if (document->loadXML(_bstr_t(xml)) != VARIANT_TRUE) return false;
    MSXML2::IXMLDOMElementPtr root = document->documentElement;
    if (!root || wcscmp(root->nodeName, L"ScriptSettings") != 0 || Attribute(root, L"version") != L"1") return false;
    auto nodes = root->selectNodes(L"*");
    if (!nodes || nodes->length > static_cast<long>(MaximumEntries)) return false;
    std::set<std::pair<std::wstring, std::wstring>> seen;
    for (long index = 0; index < nodes->length; ++index) {
        MSXML2::IXMLDOMElementPtr element(nodes->item[index]); Entry entry;
        if (!element || wcscmp(element->nodeName, L"Entry") != 0 ||
            !Unhex(Attribute(element, L"identity"), entry.identity) || !Unhex(Attribute(element, L"name"), entry.name)) return false;
        entry.type = Attribute(element, L"type");
        _variant_t data = element->getAttribute(L"data"); if (data.vt != VT_BSTR) return false;
        entry.data = CString(data.bstrVal, ::SysStringLen(data.bstrVal));
        if (entry.identity.IsEmpty() || entry.name.IsEmpty() || entry.name.GetLength() > 256 ||
            !seen.emplace(std::wstring(entry.identity), std::wstring(entry.name)).second) return false;
        if (entry.type == L"string") { CString text; if (!Unhex(entry.data, text) || text.GetLength() > static_cast<int>(MaximumValueLength)) return false; }
        else if (entry.type == L"boolean") { if (entry.data != L"true" && entry.data != L"false") return false; }
        else if (entry.type == L"number") {
            CComVariant encoded(static_cast<LPCWSTR>(entry.data)), number;
            if (entry.data.IsEmpty() || entry.data.GetLength() > 128 ||
                FAILED(::VariantChangeTypeEx(&number, &encoded, LOCALE_INVARIANT, 0, VT_R8)) || !_finite(number.dblVal)) return false;
        } else return false;
        entries.push_back(entry);
    }
    return true;
}
inline bool Save(const std::vector<Entry>& entries)
{
    if (entries.size() > MaximumEntries) return false;
    auto document = NewDocument(); auto root = document->createElement(L"ScriptSettings");
    root->setAttribute(L"version", _variant_t(L"1")); document->appendChild(root);
    for (const auto& entry : entries) {
        auto element = document->createElement(L"Entry");
        element->setAttribute(L"identity", _variant_t(static_cast<LPCWSTR>(Hex(entry.identity))));
        element->setAttribute(L"name", _variant_t(static_cast<LPCWSTR>(Hex(entry.name))));
        element->setAttribute(L"type", _variant_t(static_cast<LPCWSTR>(entry.type)));
        element->setAttribute(L"data", _variant_t(static_cast<LPCWSTR>(entry.data))); root->appendChild(element);
    }
    CString xml(L"\xfeff<?xml version=\"1.0\" encoding=\"utf-16\"?>"); xml += static_cast<LPCWSTR>(root->xml);
    const DWORD bytes = static_cast<DWORD>(xml.GetLength() * sizeof(wchar_t)); if (bytes > MaximumBytes) return false;
    GUID guid = {}; if (FAILED(::CoCreateGuid(&guid))) return false;
    wchar_t suffix[40] = {}; ::StringFromGUID2(guid, suffix, _countof(suffix));
    const CString temporary = FilePath() + suffix + L".tmp";
    CHandle file(::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL));
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0; const bool saved = ::WriteFile(file, static_cast<LPCWSTR>(xml), bytes, &written, NULL) && written == bytes;
    file.Close();
    if (!saved) { ::DeleteFileW(temporary); return false; }
    // Settings need a single same-directory rename, without ReplaceFile's
    // metadata merge and its partially completed replacement failure modes.
    const auto replaceSettings = [](LPCWSTR destination, LPCWSTR source, LPCWSTR, DWORD, LPVOID, LPVOID) -> BOOL {
        LastFailureOperation() = L"rename";
        return ::MoveFileExW(source, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    };
    try { RetryScriptFileCommit([&]() { LastFailureOperation() = L"flush"; FbeBackupFileCommit::CommitSavedFile(temporary, FilePath(), false, replaceSettings); }); }
    catch (const _com_error& error) {
        if (error.Error() != HRESULT_FROM_WIN32(ERROR_UNABLE_TO_MOVE_REPLACEMENT) &&
            error.Error() != HRESULT_FROM_WIN32(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2)) ::DeleteFileW(temporary);
        throw;
    }
    return true;
}
// operation: 0=get, 1=set, 2=remove, 3=clear. No JS objects are coerced.
inline HRESULT Access(int operation, const CString& name, const VARIANT* input, VARIANT* result)
{
    LastFailure() = S_OK;
    LastFailureOperation() = L"validate";
    const CString identity = Identity(); if (identity.IsEmpty()) return E_UNEXPECTED;
    if (operation != 3 && (name.IsEmpty() || name.GetLength() > 256 || wcslen(name) != static_cast<size_t>(name.GetLength()))) return E_INVALIDARG;
    CComVariant value; CString type;
    if ((operation == 0 || operation == 1) && (!input || !Normalize(*input, value, type))) return E_INVALIDARG;
    CComVariant answer(operation == 0 ? value : CComVariant(false));
    try {
        CHandle lock;
        if (operation != 0) {
            const CString directory(DeploymentContext::SettingsDirectory().c_str());
            const int created = ::SHCreateDirectoryExW(NULL, directory, NULL);
            if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS) return answer.Detach(result);
            LastFailureOperation() = L"lock";
            RetryScriptFileCommit([&]() {
                HANDLE handle = ::CreateFileW(FilePath() + L".lock", GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (handle == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
                lock.Attach(handle);
            });
        }
        std::vector<Entry> entries;
        LastFailureOperation() = L"load";
        if (!Load(entries)) { LastFailure() = HRESULT_FROM_WIN32(ERROR_INVALID_DATA); return answer.Detach(result); }
        auto found = entries.end();
        for (auto current = entries.begin(); current != entries.end(); ++current)
            if (current->identity == identity && current->name == name) { found = current; break; }
        if (operation == 0) {
            if (found != entries.end() && found->type == type) {
                if (type == L"string") { CString text; if (Unhex(found->data, text)) { answer.Clear(); answer.vt = VT_BSTR; answer.bstrVal = ::SysAllocStringLen(text, text.GetLength()); if (!answer.bstrVal) return E_OUTOFMEMORY; } }
                else if (type == L"boolean") answer = found->data == L"true";
                else { CComVariant encoded(static_cast<LPCWSTR>(found->data)), number; if (SUCCEEDED(::VariantChangeTypeEx(&number, &encoded, LOCALE_INVARIANT, 0, VT_R8)) && _finite(number.dblVal)) answer = number; }
            }
        } else {
            if (operation == 1) {
                Entry entry = { identity, name, type, CString() };
                if (type == L"string") entry.data = Hex(CString(value.bstrVal, ::SysStringLen(value.bstrVal)));
                else if (type == L"boolean") entry.data = value.boolVal ? L"true" : L"false";
                else { CComVariant encoded; if (FAILED(::VariantChangeTypeEx(&encoded, &value, LOCALE_INVARIANT, 0, VT_BSTR))) return E_INVALIDARG; entry.data = encoded.bstrVal; }
                if (found == entries.end()) entries.push_back(entry); else *found = entry;
            } else if (operation == 2) { if (found != entries.end()) entries.erase(found); }
            else {
                entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const Entry& entry) { return entry.identity == identity; }), entries.end());
            }
            LastFailureOperation() = L"save";
            answer = Save(entries);
        }
    } catch (const _com_error& error) { LastFailure() = error.Error(); }
    return answer.Detach(result);
}
}
}
