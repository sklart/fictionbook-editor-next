#include "ScriptDocumentResources.h"
#include "ScriptFileCommit.h"
#include "../BackupFileCommit.h"
#include <shlobj.h>
#include <cmath>

namespace FbeScripts {
inline CString Stage2String(const CComVariant& value, bool allowNul = false)
{
    if (value.vt != VT_BSTR) CheckError(DISP_E_TYPEMISMATCH);
    const UINT length = ::SysStringLen(value.bstrVal);
    if (length > 8 * 1024 * 1024 || (!allowNul && value.bstrVal && wcslen(value.bstrVal) != length)) CheckError(E_INVALIDARG);
    return CString(value.bstrVal, length);
}
inline DWORD Stage2Number(const CComVariant& value)
{
    if (value.vt != VT_I4 && value.vt != VT_R8 && value.vt != VT_I2) CheckError(DISP_E_TYPEMISMATCH);
    CComVariant number; CheckError(::VariantChangeType(&number, const_cast<VARIANT*>(static_cast<const VARIANT*>(&value)), 0, VT_R8));
    if (!_finite(number.dblVal) || number.dblVal < 0 || number.dblVal > MAXDWORD || floor(number.dblVal) != number.dblVal) CheckError(E_INVALIDARG);
    return static_cast<DWORD>(number.dblVal);
}
struct ScriptProgress : ScriptResourceLifetime {
    CComPtr<IProgressDialog> dialog; CString token; DWORD maximum = 0;
    ~ScriptProgress() { if (dialog) dialog->StopProgressDialog(); }
};
inline std::weak_ptr<ScriptProgress>& ActiveScriptProgress()
{ static thread_local std::weak_ptr<ScriptProgress> active; return active; }
inline CComVariant ProgressOperation(HWND owner, DISPID method, const std::vector<CComVariant>& args)
{
    auto& contexts = ExecutionContexts(); if (contexts.empty()) CheckError(E_UNEXPECTED);
    auto& resource = contexts.back().progressResources;
    if (method == 127) {
        const CString title = Stage2String(args[0]); const DWORD maximum = Stage2Number(args[1]);
        if (!maximum || !ActiveScriptProgress().expired() || !::IsWindow(owner)) CheckError(E_INVALIDARG);
        auto progress = std::make_shared<ScriptProgress>(); progress->token = ResourceToken(); progress->maximum = maximum;
        CheckError(progress->dialog.CoCreateInstance(CLSID_ProgressDialog));
        CheckError(progress->dialog->SetTitle(title));
        CheckError(progress->dialog->StartProgressDialog(owner, NULL, PROGDLG_MODAL | PROGDLG_AUTOTIME, NULL));
        CheckError(progress->dialog->SetProgress(0, maximum));
        resource = progress; ActiveScriptProgress() = progress; return CComVariant(progress->token);
    }
    const CString token = Stage2String(args[0]);
    auto* progress = static_cast<ScriptProgress*>(resource.get());
    if (!progress || progress->token != token) CheckError(E_INVALIDARG);
    if (method == 130) { resource.reset(); return CComVariant(true); }
    if (method == 129) return CComVariant(progress->dialog->HasUserCancelled() != FALSE);
    const DWORD value = Stage2Number(args[1]); const CString message = Stage2String(args[2]);
    if (value > progress->maximum) CheckError(E_INVALIDARG);
    CheckError(progress->dialog->SetLine(1, message, FALSE, NULL));
    CheckError(progress->dialog->SetProgress(value, progress->maximum)); return CComVariant(true);
}
inline UINT TextEncoding(const CString& name)
{
    if (name.CompareNoCase(L"utf-8") == 0 || name.CompareNoCase(L"utf-8-bom") == 0) return CP_UTF8;
    if (name.CompareNoCase(L"windows-1251") == 0) return 1251;
    if (name.CompareNoCase(L"utf-16le") == 0) return 1200;
    CheckError(E_INVALIDARG); return 0;
}
inline CString ApiFilePath(const CComVariant& value)
{
    const CString path = AbsoluteScriptPath(Stage2String(value)); if (path.IsEmpty()) CheckError(E_INVALIDARG); return path;
}
inline CString ReadScriptText(const CString& path, CString encoding, bool automatic)
{
    CHandle file(::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
    if (file == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    LARGE_INTEGER size = {}; if (!::GetFileSizeEx(file, &size)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    if (size.QuadPart < 0 || size.QuadPart > 8 * 1024 * 1024) CheckError(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
    std::vector<char> bytes(static_cast<size_t>(size.QuadPart)); DWORD read = 0;
    if (!bytes.empty() && (!::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, NULL) || read != bytes.size())) CheckError(STG_E_READFAULT);
    size_t offset = 0;
    if (automatic) {
        encoding = L"utf-8";
        if (bytes.size() >= 2 && static_cast<BYTE>(bytes[0]) == 0xff && static_cast<BYTE>(bytes[1]) == 0xfe) { encoding = L"utf-16le"; offset = 2; }
        else if (bytes.size() >= 3 && static_cast<BYTE>(bytes[0]) == 0xef && static_cast<BYTE>(bytes[1]) == 0xbb && static_cast<BYTE>(bytes[2]) == 0xbf) offset = 3;
    }
    const UINT codepage = TextEncoding(encoding);
    if (!automatic && codepage == CP_UTF8 && bytes.size() >= 3 && static_cast<BYTE>(bytes[0]) == 0xef && static_cast<BYTE>(bytes[1]) == 0xbb && static_cast<BYTE>(bytes[2]) == 0xbf) offset = 3;
    if (!automatic && codepage == 1200 && bytes.size() >= 2 && static_cast<BYTE>(bytes[0]) == 0xff && static_cast<BYTE>(bytes[1]) == 0xfe) offset = 2;
    const size_t length = bytes.size() - offset; if (!length) return CString();
    if (codepage == 1200) {
        if (length % 2) CheckError(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        CString result; auto* output = result.GetBuffer(static_cast<int>(length / 2)); memcpy(output, bytes.data() + offset, length); result.ReleaseBuffer(static_cast<int>(length / 2));
        for (int i = 0; i < result.GetLength(); ++i) {
            const unsigned ch = result[i];
            if (ch >= 0xd800 && ch <= 0xdbff) { if (++i >= result.GetLength() || result[i] < 0xdc00 || result[i] > 0xdfff) CheckError(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION)); }
            else if (ch >= 0xdc00 && ch <= 0xdfff) CheckError(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        }
        return result;
    }
    const DWORD flags = codepage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    const int count = ::MultiByteToWideChar(codepage, flags, bytes.data() + offset, static_cast<int>(length), NULL, 0);
    if (!count) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    CString result; ::MultiByteToWideChar(codepage, flags, bytes.data() + offset, static_cast<int>(length), result.GetBuffer(count), count); result.ReleaseBuffer(count); return result;
}
inline bool WriteScriptText(const CString& path, const CString& text, const CString& encoding, bool overwrite = true)
{
    const UINT codepage = TextEncoding(encoding); std::vector<char> bytes;
    if (codepage == 1200) {
        if (!text.IsEmpty() && !::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, text.GetLength(), NULL, 0, NULL, NULL)) CheckError(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        bytes.resize(2 + text.GetLength() * sizeof(wchar_t)); bytes[0] = '\xff'; bytes[1] = '\xfe'; memcpy(bytes.data() + 2, static_cast<LPCWSTR>(text), bytes.size() - 2);
    } else {
        BOOL usedDefault = FALSE; const DWORD flags = codepage == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
        const int count = ::WideCharToMultiByte(codepage, flags, text, text.GetLength(), NULL, 0, NULL, codepage == CP_UTF8 ? NULL : &usedDefault);
        if ((!count && !text.IsEmpty()) || usedDefault) CheckError(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        const size_t bom = encoding.CompareNoCase(L"utf-8-bom") == 0 ? 3 : 0; bytes.resize(bom + count);
        if (bom) { bytes[0] = '\xef'; bytes[1] = '\xbb'; bytes[2] = '\xbf'; }
        if (count) {
            ::WideCharToMultiByte(codepage, flags, text, text.GetLength(), bytes.data() + bom, count, NULL, codepage == CP_UTF8 ? NULL : &usedDefault);
            if (usedDefault) CheckError(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION));
        }
    }
    if (bytes.size() > 8 * 1024 * 1024) CheckError(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
    const CString temporary = path + ResourceToken() + L".tmp";
    CHandle file(::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL));
    if (file == INVALID_HANDLE_VALUE) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
    DWORD written = 0; const bool saved = bytes.empty() || (::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL) && written == bytes.size());
    file.Close();
    if (!saved) { ::DeleteFileW(temporary); CheckError(STG_E_WRITEFAULT); }
    const auto replace = [](LPCWSTR destination, LPCWSTR source, LPCWSTR, DWORD, LPVOID, LPVOID) -> BOOL {
        return ::MoveFileExW(source, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    };
    try { RetryScriptFileCommit([&]() {
        if (overwrite) FbeBackupFileCommit::CommitSavedFile(temporary, path, false, replace);
        else {
            CHandle completed(::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (completed == INVALID_HANDLE_VALUE || !::FlushFileBuffers(completed)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
            completed.Close();
            if (!::MoveFileExW(temporary, path, MOVEFILE_WRITE_THROUGH)) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        }
    }); }
    catch (...) { ::DeleteFileW(temporary); throw; }
    return true;
}
inline CComVariant FileString(const CString& text)
{ CComVariant value; value.vt = VT_BSTR; value.bstrVal = text.AllocSysString(); if (!value.bstrVal) CheckError(E_OUTOFMEMORY); return value; }
inline CString TempNamePart(const CComVariant& value, bool extension)
{
    CString part = Stage2String(value);
    if (part.GetLength() > 64) CheckError(E_INVALIDARG);
    if (extension && !part.IsEmpty() && part[0] != L'.') part = L"." + part;
    for (int index = 0; index < part.GetLength(); ++index)
        if (part[index] < 32 || wcschr(L"\\/:*?\"<>|", part[index])) CheckError(E_INVALIDARG);
    if (part.Find(L"..") >= 0 || (!part.IsEmpty() && (part.Right(1) == L"." || part.Right(1) == L" "))) CheckError(E_INVALIDARG);
    return part;
}
inline CComVariant FileOperation(DISPID method, const std::vector<CComVariant>& args)
{
    if (method == 140 || method == 141) {
        DWORD size = ::GetTempPathW(0, NULL); if (!size || size > 32768) CheckError(E_FAIL);
        std::vector<wchar_t> directory(size + 1); if (!::GetTempPathW(static_cast<DWORD>(directory.size()), directory.data())) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        if (method == 140) return CComVariant(directory.data());
        const CString prefix = args.empty() ? CString(L"fbe-") : TempNamePart(args[0], false);
        const CString extension = args.size() < 2 ? CString(L".tmp") : TempNamePart(args[1], true);
        for (int attempt = 0; attempt < 8; ++attempt) {
            const CString path = CString(directory.data()) + prefix + ResourceToken() + extension;
            CHandle file(::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL));
            if (file != INVALID_HANDLE_VALUE) return CComVariant(path);
            if (::GetLastError() != ERROR_FILE_EXISTS) CheckError(HRESULT_FROM_WIN32(::GetLastError()));
        }
        CheckError(HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));
    }
    if (method >= 136 && method <= 139) {
        CString path = Stage2String(args[0]); path.Replace(L'/', L'\\');
        if (method == 136) {
            CString second = Stage2String(args[1]); second.Replace(L'/', L'\\');
            if (second.Left(1) == L"\\" || (second.GetLength() >= 2 && second[1] == L':')) path = second;
            else if (path.IsEmpty()) path = second;
            else { path.TrimRight(L"\\"); path += L"\\" + second; }
            path = AbsoluteScriptPath(path); if (path.IsEmpty()) CheckError(E_INVALIDARG); return CComVariant(path);
        }
        const int slash = path.ReverseFind(L'\\');
        if (method == 137) return CComVariant(path.Mid(slash + 1));
        if (method == 138) return CComVariant(slash == 2 && path.GetLength() >= 3 && path[1] == L':' ? path.Left(3) : (slash < 0 ? CString() : path.Left(slash)));
        const CString name = path.Mid(slash + 1); const int dot = name.ReverseFind(L'.'); return CComVariant(dot < 0 ? CString() : name.Mid(dot));
    }
    const CString path = ApiFilePath(args[0]);
    if (method == 131) return FileString(ReadScriptText(path, args.size() == 2 ? Stage2String(args[1]) : CString(), args.size() == 1));
    if (method == 132) return CComVariant(WriteScriptText(path, Stage2String(args[1], true), args.size() == 3 ? Stage2String(args[2]) : CString(L"utf-8")));
    if (method == 142) return CComVariant(WriteScriptText(path, Stage2String(args[1], true), args.size() == 4 ? Stage2String(args[3]) : CString(L"utf-8"), args[2].boolVal != VARIANT_FALSE));
    const DWORD attributes = ::GetFileAttributesW(path);
    if (method == 133 || method == 134) return CComVariant(attributes != INVALID_FILE_ATTRIBUTES && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) == (method == 134));
    if (method == 135) {
        const int error = ::SHCreateDirectoryExW(NULL, path, NULL);
        if (error != ERROR_SUCCESS && error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) CheckError(HRESULT_FROM_WIN32(error));
        const DWORD created = ::GetFileAttributesW(path); if (created == INVALID_FILE_ATTRIBUTES || !(created & FILE_ATTRIBUTE_DIRECTORY)) CheckError(HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
        return CComVariant(true);
    }
    CheckError(DISP_E_MEMBERNOTFOUND); return CComVariant();
}
inline HRESULT InvokeStage2(HWND owner, DISPID method, WORD flags, DISPPARAMS* parameters, VARIANT* result, UINT* argumentError)
{
    if (!(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
    if (parameters && parameters->cNamedArgs) return DISP_E_NONAMEDARGS;
    const UINT count = parameters ? parameters->cArgs : 0;
    const UINT expected[] = {0,1,1,0,0,0,2,1,2,3,1,1,1,2,1,1,1,2,1,1,1,0,0,3};
    const UINT minimum = expected[method - 119];
    const UINT extra = method == 141 ? 2U : (method == 131 || method == 132 || method == 142 ? 1U : 0U);
    if (count < minimum || count > minimum + extra) return DISP_E_BADPARAMCOUNT;
    if (count && (!parameters || !parameters->rgvarg)) return E_POINTER;
    std::vector<CComVariant> args(count);
    for (UINT index = 0; index < count; ++index) {
        CheckError(::VariantCopyInd(&args[index], &parameters->rgvarg[count - index - 1]));
        const bool numeric = (method == 127 && index == 1) || (method == 128 && index == 1);
        const bool boolean = method == 142 && index == 2;
        if ((!numeric && !boolean && args[index].vt != VT_BSTR) || (boolean && args[index].vt != VT_BOOL)) { if (argumentError) *argumentError = count - index - 1; return DISP_E_TYPEMISMATCH; }
        if (!numeric && !boolean) Stage2String(args[index], (method == 132 || method == 142) && index == 1);
    }
    CComVariant value;
    if (method <= 126) {
        XmlScriptApiRequest request(XmlScriptApiOperation::ScriptingDocumentOperation); request.scriptingMethod = method; request.arguments = std::move(args);
        if (!SendXmlScriptRequest(owner, request)) return E_UNEXPECTED;
        if (FAILED(request.error)) return request.error;
        value = request.returnValue;
    } else if (method <= 130) value = ProgressOperation(owner, method, args);
    else value = FileOperation(method, args);
    return result ? value.Detach(result) : S_OK;
}
}
