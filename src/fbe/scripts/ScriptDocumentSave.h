#pragma once
#include "ScriptExecutionContext.h"
#include "ScriptDocumentResources.h"
#include "ScriptExtendedDialogs.h"
#include "../document/DocumentSaveController.h"

namespace FbeScripts { namespace DocumentSave {
struct Options { CString format = L"current", encoding = L"current"; bool overwrite = false; };
inline Options Parse(BSTR source)
{
    using namespace FbeRuntimeLocalization;
    Options result;
    if (::SysStringLen(source) > 4096) CheckError(E_INVALIDARG);
    const std::wstring json(source ? source : L"", ::SysStringLen(source));size_t pos = 0;
    std::set<std::wstring> keys;
    if (!ExtendedDialogs::Take(json, pos, L'{')) CheckError(E_INVALIDARG);
    if (!ExtendedDialogs::Take(json, pos, L'}')) for (;;) {
        CString key;
        if (!ExtendedDialogs::Text(json, pos, key, 32) || !keys.emplace(std::wstring(key)).second ||
            !ExtendedDialogs::Take(json, pos, L':')) CheckError(E_INVALIDARG);
        if (key == L"format" || key == L"encoding") {
            CString value;if (!ExtendedDialogs::Text(json, pos, value, 32)) CheckError(E_INVALIDARG);
            if (key == L"format") result.format = value;else result.encoding = value;
        } else if (key == L"overwrite") {
            JsonSkipWhitespace(json, pos);
            if (json.compare(pos, 4, L"true") == 0) {result.overwrite = true;pos += 4;}
            else if (json.compare(pos, 5, L"false") == 0) {result.overwrite = false;pos += 5;}
            else CheckError(E_INVALIDARG);
        } else CheckError(E_INVALIDARG);
        if (ExtendedDialogs::Take(json, pos, L'}')) break;
        if (!ExtendedDialogs::Take(json, pos, L',')) CheckError(E_INVALIDARG);
    }
    JsonSkipWhitespace(json, pos);
    if (pos != json.size() || (result.format != L"current" && result.format != L"fb2" && result.format != L"fbd") ||
        (result.encoding != L"current" && result.encoding != L"utf-8")) CheckError(E_INVALIDARG);
    return result;
}
inline CString Path(BSTR raw)
{
    CString value(raw, ::SysStringLen(raw));
    if (!ExtendedDialogs::ValidText(value, MAX_PATH - 16) || value.IsEmpty() || ::PathIsRelativeW(value) ||
        value.Find(L':', 2) >= 0) CheckError(E_INVALIDARG);
    wchar_t full[MAX_PATH] = {};
    if (!::GetFullPathNameW(value, _countof(full), full, NULL)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    value = full;
    const CString extension = ::PathFindExtensionW(value);
    if (extension.CompareNoCase(L".fb2") && extension.CompareNoCase(L".fbd")) CheckError(E_INVALIDARG);
    return value;
}
inline bool SameFile(const CString& a, const CString& b)
{
    if (a.IsEmpty() || b.IsEmpty()) return false;
    wchar_t left[MAX_PATH] = {}, right[MAX_PATH] = {};
    if (::GetFullPathNameW(a, _countof(left), left, NULL) && ::GetFullPathNameW(b, _countof(right), right, NULL) && !_wcsicmp(left, right)) return true;
    HANDLE x = ::CreateFileW(a, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    HANDLE y = ::CreateFileW(b, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    BY_HANDLE_FILE_INFORMATION xi = {}, yi = {};
    const bool same = x != INVALID_HANDLE_VALUE && y != INVALID_HANDLE_VALUE && ::GetFileInformationByHandle(x, &xi) &&
        ::GetFileInformationByHandle(y, &yi) && xi.dwVolumeSerialNumber == yi.dwVolumeSerialNumber &&
        xi.nFileIndexHigh == yi.nFileIndexHigh && xi.nFileIndexLow == yi.nFileIndexLow;
    if (x != INVALID_HANDLE_VALUE) ::CloseHandle(x);
    if (y != INVALID_HANDLE_VALUE) ::CloseHandle(y);
    return same;
}
inline std::vector<unsigned char> Bytes(MSXML2::IXMLDOMDocument2Ptr snapshot)
{
    IStreamPtr stream;CheckError(::CreateStreamOnHGlobal(NULL, TRUE, &stream));
    CheckError(snapshot->raw_save(_variant_t(static_cast<IUnknown*>(stream))));
    STATSTG stat = {};CheckError(stream->Stat(&stat, STATFLAG_NONAME));
    if (stat.cbSize.QuadPart < 0 || stat.cbSize.QuadPart > 512LL * 1024 * 1024) CheckError(E_OUTOFMEMORY);
    std::vector<unsigned char> bytes(static_cast<size_t>(stat.cbSize.QuadPart));
    LARGE_INTEGER zero = {};CheckError(stream->Seek(zero, STREAM_SEEK_SET, NULL));
    ULONG read = 0;CheckError(stream->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read));
    if (read != bytes.size()) CheckError(HRESULT_FROM_WIN32(ERROR_READ_FAULT));
    return bytes;
}
inline void CheckContext(FB::Doc* doc)
{
    if (!doc || !CurrentExecutionContext() || HasDocumentUpdates()) CheckError(E_UNEXPECTED);
    if (doc->IsSerializationUnsafe()) CheckError(E_FAIL);
    CComPtr<IOleUndoManager> manager;IServiceProviderPtr services(doc->m_body.Document());
    CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)));
    DWORD state = 0;if (manager->GetOpenParentState(&state) != S_FALSE) CheckError(E_UNEXPECTED);
}
} }
