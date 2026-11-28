#define _ATL_APARTMENT_THREADED
#include <atlbase.h>
#include <atlcom.h>
#include <filesystem>

#include "FBE.h"
#include "../common/ModernFileDialog.h"
#include "../common/fb3/Fb3Export.h"
#include "../common/fb3/Fb3Package.h"
#include "../version.h"

extern const CLSID CLSID_ExportFB3 = {0xadddaa13, 0x7cfa, 0x49a6, {0x8c, 0x88, 0x67, 0x2a, 0x7e, 0xd8, 0x42, 0xe9}};

namespace {

class ExportFB3Module final : public CAtlDllModuleT<ExportFB3Module> {};
ExportFB3Module module;
HINSTANCE instance = nullptr;
namespace fs = std::filesystem;

HRESULT CheckCancellation(IFBECancellationToken* token) {
    if (!token) return S_OK;
    BOOL cancelled = FALSE;
    const auto hr = token->IsCancellationRequested(&cancelled);
    return SUCCEEDED(hr) && cancelled ? HRESULT_FROM_WIN32(ERROR_CANCELLED) : hr;
}

fs::path RuntimeDirectory() {
    std::wstring path(32768, L'\0');
    const auto size = ::GetModuleFileNameW(instance, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) throw FbeFb3::Error{L"Cannot locate the installed FB3 validation files."};
    path.resize(size);
    return fs::path(path).parent_path().parent_path();
}

HRESULT SelectDestination(IFBEPluginHost* host, BSTR source, fs::path& destination) {
    wchar_t mode[2] = {}, scenario[16] = {};
    if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) == 1 && mode[0] == L'1' &&
        ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario)) > 0 && wcscmp(scenario, L"export-fb3") == 0) {
        std::wstring path(32768, L'\0');
        const auto size = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_EXPORT_FB3_PATH", path.data(), static_cast<DWORD>(path.size()));
        if (!size) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        if (size >= path.size()) return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
        path.resize(size);
        destination = std::move(path);
        return S_OK;
    }
    LONGLONG owner = 0;
    const auto hr = host->GetOwnerWindow(&owner);
    if (FAILED(hr)) return hr;
    const COMDLG_FILTERSPEC filters[] = {{L"FictionBook 3 (*.fb3)", L"*.fb3"}};
    ModernFileDialog::Request request;
    request.save = request.pathMustExist = request.overwritePrompt = true;
    request.defaultExtension = L"fb3";
    request.filters = filters;
    request.filterCount = _countof(filters);
    request.filterIndex = 1;
    if (source && *source) {
        auto path = fs::path(source);
        request.initialFolder = path.parent_path().wstring();
        request.initialFileName = path.filename().replace_extension(L".fb3").wstring();
    }
    const auto result = ModernFileDialog::Show(reinterpret_cast<HWND>(static_cast<INT_PTR>(owner)), request);
    if (result.outcome == ModernFileDialog::Outcome::Cancelled) return HRESULT_FROM_WIN32(ERROR_CANCELLED);
    if (result.outcome == ModernFileDialog::Outcome::Failed) return result.error;
    if (result.paths.size() != 1) return E_UNEXPECTED;
    destination = result.paths.front();
    return S_OK;
}

HRESULT SetString(BSTR* value, const wchar_t* text) {
    if (!value) return E_POINTER;
    *value = ::SysAllocString(text);
    return *value ? S_OK : E_OUTOFMEMORY;
}

void Report(IFBEPluginHost* host, LONG severity, const wchar_t* code, const std::wstring& text) {
    CComBSTR tag, message;
    tag.Attach(::SysAllocString(code));
    message.Attach(::SysAllocString(text.c_str()));
    if (tag && message) host->ReportMessage(severity, tag, message);
}

}

class ATL_NO_VTABLE ExportFB3Plugin : public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<ExportFB3Plugin, &CLSID_ExportFB3>, public IFBEPluginInfo2, public IFBEExportPlugin2 {
public:
    DECLARE_NO_REGISTRY()
    BEGIN_COM_MAP(ExportFB3Plugin)
        COM_INTERFACE_ENTRY(IFBEPluginInfo2)
        COM_INTERFACE_ENTRY(IFBEExportPlugin2)
    END_COM_MAP()

    STDMETHOD(GetPluginId)(BSTR* value) override { return SetString(value, L"export-fb3"); }
    STDMETHOD(GetPluginVersion)(BSTR* value) override { return SetString(value, FBE_VERSION_WSTRING); }
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
    STDMETHOD(Export)(IFBEPluginHost* host, BSTR sourceFileName, IFBEDocumentSnapshot* document) override;
};

OBJECT_ENTRY_AUTO(CLSID_ExportFB3, ExportFB3Plugin)

STDMETHODIMP ExportFB3Plugin::Export(IFBEPluginHost* host, BSTR sourceFileName, IFBEDocumentSnapshot* document) {
    if (!host || !document) return E_POINTER;
    try {
        CComPtr<IFBECancellationToken> token;
        auto hr = host->GetCancellationToken(&token);
        if (FAILED(hr) || FAILED(hr = CheckCancellation(token))) return hr;
        fs::path destination;
        hr = SelectDestination(host, sourceFileName, destination);
        if (FAILED(hr) || FAILED(hr = CheckCancellation(token))) return hr;
        if (::CompareStringOrdinal(destination.extension().c_str(), -1, L".fb3", -1, TRUE) != CSTR_EQUAL)
            throw FbeFb3::Error{L"The export destination must have the .fb3 extension."};
        if (sourceFileName && *sourceFileName) {
            std::error_code error;
            if (fs::equivalent(fs::path(sourceFileName), destination, error))
                throw FbeFb3::Error{L"The FB3 export cannot replace the source document."};
        }
        CComPtr<IStream> stream;
        hr = document->OpenXmlStream(&stream);
        if (FAILED(hr)) return hr;
        if (!stream) return E_UNEXPECTED;
        CComPtr<IFBEProgressSink> progress;
        host->GetProgressSink(&progress);
        if (progress) progress->Report(0, 1, CComBSTR(L"FB3"));
        const auto warnings = FbeFb3::ExportBook(FbeFb3::ReadFb2Stream(*stream.p), destination, RuntimeDirectory(), true, [&] {
            const auto check = CheckCancellation(token);
            if (FAILED(check)) AtlThrow(check);
        });
        if (!warnings.empty()) {
            std::wstring text;
            for (const auto& warning : warnings) text += warning + L"\r\n";
            Report(host, 1, L"export-fb3-warning", text);
        }
        if (progress) progress->Report(1, 1, CComBSTR(L"FB3"));
        return S_OK;
    } catch (const FbeFb3::Error& error) {
        Report(host, 2, L"export-fb3-failed", error.message);
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
