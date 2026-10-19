#pragma once
#include "../BinaryId.h"
#include "ScriptJson.h"
#include <set>

namespace FbeScripts { namespace Binary {
struct Object {
    CString id, mime;
    CComVariant data;
    MSHTML::IHTMLElementPtr element;
    ULONG size = 0;
};
inline std::vector<Object> Objects(MSHTML::IHTMLDocument2Ptr document)
{
    MSHTML::IHTMLDocument3Ptr dom(document);
    MSHTML::IHTMLElementPtr container(dom->getElementById(L"binobj"));
    if (!container) CheckError(E_UNEXPECTED);
    MSHTML::IHTMLElementCollectionPtr elements(MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"DIV"));
    std::vector<Object> objects;
    for (long index = 0; index < elements->length; ++index) {
        Object item; item.element = elements->item(index);
        MSHTML::IHTMLElementCollectionPtr inputs(MSHTML::IHTMLElement2Ptr(item.element)->getElementsByTagName(L"INPUT"));
        for (long input = 0; input < inputs->length; ++input) {
            MSHTML::IHTMLInputElementPtr field(inputs->item(input));
            CString name(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(field)->id));
            if (name == L"id") item.id = static_cast<LPCWSTR>(field->value);
            if (name == L"type") item.mime = static_cast<LPCWSTR>(field->value);
        }
        CComDispatchDriver driver(item.element);
        CheckError(driver.GetPropertyByName(L"base64data", &item.data));
        if (item.data.vt != (VT_ARRAY | VT_UI1) || !item.data.parray || ::SafeArrayGetDim(item.data.parray) != 1) CheckError(E_UNEXPECTED);
        LONG lower = 0, upper = -1;
        CheckError(::SafeArrayGetLBound(item.data.parray, 1, &lower));
        CheckError(::SafeArrayGetUBound(item.data.parray, 1, &upper));
        const LONGLONG size = static_cast<LONGLONG>(upper) - lower + 1;
        if (size < 0 || size > ULONG_MAX) CheckError(E_UNEXPECTED);
        item.size = static_cast<ULONG>(size);
        objects.push_back(std::move(item));
    }
    return objects;
}
inline CString UniqueId(const std::vector<Object>& objects, const CString& preferred)
{
    const CString base = FbeBinary::NormalizeXmlId(preferred);
    CString result = base;
    for (unsigned suffix = 0; ; ++suffix) {
        if (std::none_of(objects.begin(), objects.end(), [&](const auto& object) { return object.id == result; })) return result;
        if (suffix == UINT_MAX) CheckError(E_OUTOFMEMORY);
        result.Format(L"%s_%u", static_cast<LPCWSTR>(base), suffix);
    }
}
class Bytes {
    SAFEARRAY* m_array;
public:
    BYTE* data = nullptr;
    explicit Bytes(SAFEARRAY* array) : m_array(array) { CheckError(::SafeArrayAccessData(array, reinterpret_cast<void**>(&data))); }
    ~Bytes() { ::SafeArrayUnaccessData(m_array); }
    Bytes(const Bytes&) = delete;
    Bytes& operator=(const Bytes&) = delete;
};
inline CString Info(const Object& object)
{
    CString dimensions = L"\"width\":null,\"height\":null";
    CString mime = object.mime; mime.MakeLower();
    if (mime.Left(6) == L"image/" && object.size) {
        CComPtr<IStream> stream; CheckError(::CreateStreamOnHGlobal(NULL, TRUE, &stream));
        Bytes bytes(object.data.parray); ULONG written = 0;
        CheckError(stream->Write(bytes.data, object.size, &written));
        if (written != object.size) CheckError(STG_E_WRITEFAULT);
        LARGE_INTEGER start = {}; CheckError(stream->Seek(start, STREAM_SEEK_SET, NULL));
        CImage image;
        if (SUCCEEDED(image.Load(stream))) dimensions.Format(L"\"width\":%d,\"height\":%d", image.GetWidth(), image.GetHeight());
    }
    CString json;
    json.Format(L"{\"schemaVersion\":1,\"id\":\"%s\",\"mimeType\":\"%s\",\"size\":%lu,%s,\"referenceCount\":null}",
        static_cast<LPCWSTR>(JsonEscape(object.id)), static_cast<LPCWSTR>(JsonEscape(object.mime)), object.size, static_cast<LPCWSTR>(dimensions));
    return json;
}
inline CString ExportName(const CString& id, unsigned suffix)
{
    CString name;
    // The prefix also neutralizes DOS device names, including names with extensions.
    for (int index = 0; index < id.GetLength() && index < 100; ++index) {
        const wchar_t character = id[index];
        name += character < 32 || wcschr(L"\\/:*?\"<>|", character) ? L'_' : character;
    }
    if (!name.IsEmpty() && name[name.GetLength()-1] >= 0xd800 && name[name.GetLength()-1] <= 0xdbff) name.Delete(name.GetLength()-1);
    name.TrimRight(L". ");
    if (name.IsEmpty()) name = L"binary";
    if (suffix) { CString tail; tail.Format(L"_%u", suffix); name += tail; }
    return L"_" + name;
}
inline HRESULT ExportOne(const Object& object, const CString& path, bool replace)
{
    CString temporary;
    try {
        GUID guid = {}; CheckError(::CoCreateGuid(&guid)); wchar_t token[40] = {}; ::StringFromGUID2(guid, token, _countof(token));
        const int slash = path.ReverseFind(L'\\'); temporary = path.Left(slash + 1) + L".fbe-" + token + L".tmp";
        CHandle file(::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL));
        if (file == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        Bytes bytes(object.data.parray); DWORD written = 0;
        if (!::WriteFile(file, bytes.data, object.size, &written, NULL)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        if (written != object.size) CheckError(STG_E_WRITEFAULT);
        if (!::FlushFileBuffers(file)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        file.Close();
        if (!::MoveFileExW(temporary, path, MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0))) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        return S_OK;
    } catch (const _com_error& error) { if (!temporary.IsEmpty()) ::DeleteFileW(temporary); return error.Error(); }
    catch (...) { if (!temporary.IsEmpty()) ::DeleteFileW(temporary); throw; }
}
inline CString Export(const std::vector<Object>& objects, CString directory, const CString& policy)
{
    if (policy != L"skip" && policy != L"fail" && policy != L"replace") CheckError(E_INVALIDARG);
    const DWORD attributes = ::GetFileAttributesW(directory);
    if (attributes == INVALID_FILE_ATTRIBUTES) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) CheckError(HRESULT_FROM_WIN32(ERROR_DIRECTORY));
    directory.TrimRight(L"\\"); directory += L"\\";
    if (directory.GetLength() + 145 >= MAX_PATH) CheckError(HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE));
    std::set<std::wstring> names;
    CString rows; unsigned saved = 0, skipped = 0, errors = 0;
    for (const auto& object : objects) {
        CString name, folded;
        for (unsigned suffix = 0; ; ++suffix) {
            name = ExportName(object.id, suffix); folded = name; folded.MakeUpper();
            if (names.insert(std::wstring(folded)).second) break;
            if (suffix == UINT_MAX) CheckError(E_OUTOFMEMORY);
        }
        const CString path = directory + name;
        const DWORD targetAttributes = ::GetFileAttributesW(path);
        const bool exists = targetAttributes != INVALID_FILE_ATTRIBUTES;
        HRESULT error = S_OK; LPCWSTR status = L"saved";
        if (exists && (targetAttributes & FILE_ATTRIBUTE_DIRECTORY)) { error = HRESULT_FROM_WIN32(ERROR_DIRECTORY); status = L"error"; ++errors; }
        else if (exists && policy == L"skip") { status = L"skipped"; ++skipped; }
        else {
            error = exists && policy == L"fail" ? HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) : ExportOne(object, path, policy == L"replace");
            // A competing creator must not turn the safe default into an overwrite.
            if (policy == L"skip" && (error == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) || error == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS))) { error = S_OK; status = L"skipped"; ++skipped; }
            else if (FAILED(error)) { status = L"error"; ++errors; }
            else ++saved;
        }
        CString row; row.Format(L"%s{\"id\":\"%s\",\"fileName\":\"%s\",\"status\":\"%s\",\"hresult\":\"0x%08lX\"}", rows.IsEmpty() ? L"" : L",",
            static_cast<LPCWSTR>(JsonEscape(object.id)), static_cast<LPCWSTR>(JsonEscape(name)), status, static_cast<unsigned long>(error));
        rows += row;
    }
    CString json; json.Format(L"{\"schemaVersion\":1,\"status\":\"%s\",\"saved\":%u,\"skipped\":%u,\"errors\":%u,\"objects\":[%s]}", errors ? L"error" : L"ok", saved, skipped, errors, static_cast<LPCWSTR>(rows));
    return json;
}
inline CComVariant Read(MSHTML::IHTMLDocument2Ptr document, DISPID method, const std::vector<CComVariant>& args)
{
    const auto objects = Objects(document);
    if (method == 148) return CComVariant(UniqueId(objects, args[0].bstrVal));
    if (method == 152) return CComVariant(Export(objects, args[0].bstrVal, args.size() == 2 ? CString(args[1].bstrVal) : CString(L"skip")));
    for (const auto& object : objects) if (object.id == args[0].bstrVal) return CComVariant(Info(object));
    CheckError(HRESULT_FROM_WIN32(ERROR_NOT_FOUND)); return CComVariant();
}
} }
