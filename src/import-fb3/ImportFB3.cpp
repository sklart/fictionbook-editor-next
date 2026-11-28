#define _ATL_APARTMENT_THREADED
#include <atlbase.h>
#include <atlcom.h>
#include <filesystem>
#include <new>
#include <string>
#include <vector>

#include "FBE.h"
#include "../common/ModernFileDialog.h"
#include "../common/fb3/Fb3Import.h"
#include "../common/fb3/Fb3Package.h"
#include "../version.h"

extern const CLSID CLSID_ImportFB3 = {0xaec1a7a4, 0x5853, 0x4c44, {0x9e, 0xf7, 0xf4, 0x7f, 0x50, 0x7a, 0x79, 0x15}};

namespace {

class ImportFB3Module final : public CAtlDllModuleT<ImportFB3Module> {};
ImportFB3Module module;
HINSTANCE instance = nullptr;

HRESULT Cancelled(IFBECancellationToken* token) {
    if (!token) return S_OK;
    BOOL cancelled = FALSE;
    const auto hr = token->IsCancellationRequested(&cancelled);
    return SUCCEEDED(hr) && cancelled ? HRESULT_FROM_WIN32(ERROR_CANCELLED) : hr;
}

std::filesystem::path SchemaPath() {
    std::wstring path(32768, L'\0');
    const auto length = ::GetModuleFileNameW(instance, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) throw FbeFb3::Error{L"Cannot locate the installed FB2 schema."};
    path.resize(length);
    return std::filesystem::path(path).parent_path().parent_path() / L"FictionBook.xsd";
}

HRESULT SelectSource(IFBEPluginHost* host, std::filesystem::path& source) {
    wchar_t mode[2] = {}, scenario[16] = {};
    if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) == 1 && mode[0] == L'1' &&
        ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario)) > 0 &&
        wcscmp(scenario, L"import-fb3") == 0) {
        std::wstring path(32768, L'\0');
        const auto length = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_FB3_PATH", path.data(), static_cast<DWORD>(path.size()));
        if (!length) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        if (length >= path.size()) return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
        path.resize(length);
        source = std::move(path);
        return S_OK;
    }

    LONGLONG owner = 0;
    const auto hr = host->GetOwnerWindow(&owner);
    if (FAILED(hr)) return hr;
    const COMDLG_FILTERSPEC filters[] = {{L"FictionBook 3 (*.fb3)", L"*.fb3"}};
    ModernFileDialog::Request request;
    request.filters = filters;
    request.filterCount = _countof(filters);
    request.filterIndex = 1;
    request.fileMustExist = true;
    request.pathMustExist = true;
    const auto result = ModernFileDialog::Show(reinterpret_cast<HWND>(static_cast<INT_PTR>(owner)), request);
    if (result.outcome == ModernFileDialog::Outcome::Cancelled) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    if (result.outcome == ModernFileDialog::Outcome::Failed) return result.error;
    if (result.paths.size() != 1) return E_UNEXPECTED;
    source = result.paths.front();
    return S_OK;
}

HRESULT WriteStream(const std::wstring& xml, IStream** result) {
    const auto length = static_cast<int>(xml.size());
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, xml.data(), length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return E_FAIL;
    std::string utf8(static_cast<size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, xml.data(), length, utf8.data(), size, nullptr, nullptr) != size) return E_FAIL;
    CComPtr<IStream> stream;
    auto hr = ::CreateStreamOnHGlobal(nullptr, TRUE, &stream);
    if (FAILED(hr)) return hr;
    ULONG written = 0;
    hr = stream->Write(utf8.data(), static_cast<ULONG>(size), &written);
    if (FAILED(hr)) return hr;
    if (written != static_cast<ULONG>(size)) return STG_E_MEDIUMFULL;
    const LARGE_INTEGER start = {};
    hr = stream->Seek(start, STREAM_SEEK_SET, nullptr);
    if (SUCCEEDED(hr)) *result = stream.Detach();
    return hr;
}

HRESULT CopyString(BSTR* value, const wchar_t* text) {
    if (!value) return E_POINTER;
    *value = ::SysAllocString(text);
    return *value ? S_OK : E_OUTOFMEMORY;
}

}

class ATL_NO_VTABLE ImportFB3Plugin : public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<ImportFB3Plugin, &CLSID_ImportFB3>, public IFBEPluginInfo2, public IFBEImportPlugin2 {
public:
    DECLARE_NO_REGISTRY()
    BEGIN_COM_MAP(ImportFB3Plugin)
        COM_INTERFACE_ENTRY(IFBEPluginInfo2)
        COM_INTERFACE_ENTRY(IFBEImportPlugin2)
    END_COM_MAP()

    STDMETHOD(GetPluginId)(BSTR* value) override { return CopyString(value, L"import-fb3"); }
    STDMETHOD(GetPluginVersion)(BSTR* value) override { return CopyString(value, FBE_VERSION_WSTRING); }
    STDMETHOD(GetApiVersion)(ULONG* value) override {
        if (!value) return E_POINTER;
        *value = 2;
        return S_OK;
    }
    STDMETHOD(GetCapabilities)(ULONGLONG* value) override {
        if (!value) return E_POINTER;
        *value = 0;
        return S_OK;
    }
    STDMETHOD(Import)(IFBEPluginHost* host, BSTR* suggestedFileName, IStream** fb2Xml) override;
};

OBJECT_ENTRY_AUTO(CLSID_ImportFB3, ImportFB3Plugin)

STDMETHODIMP ImportFB3Plugin::Import(IFBEPluginHost* host, BSTR* suggestedFileName, IStream** fb2Xml) {
    if (suggestedFileName) *suggestedFileName = nullptr;
    if (fb2Xml) *fb2Xml = nullptr;
    if (!host || !suggestedFileName || !fb2Xml) return E_POINTER;
    try {
        CComPtr<IFBECancellationToken> token;
        auto hr = host->GetCancellationToken(&token);
        if (FAILED(hr) || FAILED(hr = Cancelled(token))) return hr;
        std::filesystem::path source;
        hr = SelectSource(host, source);
        if (FAILED(hr) || FAILED(hr = Cancelled(token))) return hr;
        CComPtr<IFBEProgressSink> progress;
        host->GetProgressSink(&progress);
        if (progress) progress->Report(0, 1, CComBSTR(L"FB3"));
        const auto book = FbeFb3::ImportBook(source, SchemaPath());
        if (FAILED(hr = Cancelled(token))) return hr;
        const auto suggested = source.replace_extension(L".fb2");
        CComBSTR name(suggested.c_str());
        if (!name) return E_OUTOFMEMORY;
        CComPtr<IStream> stream;
        if (FAILED(hr = WriteStream(book.xml, &stream))) return hr;
        if (!book.warnings.empty()) {
            std::wstring report;
            for (const auto& warning : book.warnings) report += warning + L"\r\n";
            host->ReportMessage(1, CComBSTR(L"import-fb3-warning"), CComBSTR(report.c_str()));
        }
        if (progress) progress->Report(1, 1, CComBSTR(L"FB3"));
        *suggestedFileName = name.Detach();
        *fb2Xml = stream.Detach();
        return S_OK;
    } catch (const FbeFb3::Error& error) {
        CComBSTR code, message;
        code.Attach(::SysAllocString(L"import-fb3-failed"));
        message.Attach(::SysAllocString(error.message.c_str()));
        if (code && message) host->ReportMessage(2, code, message);
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    } catch (const ATL::CAtlException& error) {
        return error.m_hr;
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    } catch (...) {
        return E_FAIL;
    }
}

extern "C" BOOL WINAPI DllMain(HINSTANCE handle, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) instance = handle;
    return module.DllMain(reason, reserved);
}

STDAPI DllCanUnloadNow() { return module.DllCanUnloadNow(); }
STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) { return module.DllGetClassObject(clsid, iid, object); }
