#include <atlbase.h>
#include <algorithm>
#include <atlcom.h>
#include <msxml6.h>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "FBE.h"

namespace {

constexpr CLSID ImportClsid{0xaec1a7a4, 0x5853, 0x4c44, {0x9e, 0xf7, 0xf4, 0x7f, 0x50, 0x7a, 0x79, 0x15}};
constexpr CLSID ExportClsid{0xadddaa13, 0x7cfa, 0x49a6, {0x8c, 0x88, 0x67, 0x2a, 0x7e, 0xd8, 0x42, 0xe9}};

class Host : public CComObjectRootEx<CComSingleThreadModel>, public IFBEPluginHost, public IFBECancellationToken, public IFBEProgressSink {
public:
    BEGIN_COM_MAP(Host)
        COM_INTERFACE_ENTRY(IFBEPluginHost)
        COM_INTERFACE_ENTRY(IFBECancellationToken)
        COM_INTERFACE_ENTRY(IFBEProgressSink)
    END_COM_MAP()

    STDMETHOD(GetHostVersion)(BSTR*) override { return E_NOTIMPL; }
    STDMETHOD(GetUiLocale)(BSTR* value) override { *value = ::SysAllocString(locale); return *value ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetOwnerWindow)(LONGLONG*) override { return E_UNEXPECTED; }
    STDMETHOD(GetProgressSink)(IFBEProgressSink** sink) override { return QueryInterface(__uuidof(IFBEProgressSink), reinterpret_cast<void**>(sink)); }
    STDMETHOD(GetCancellationToken)(IFBECancellationToken** token) override { return QueryInterface(__uuidof(IFBECancellationToken), reinterpret_cast<void**>(token)); }
    STDMETHOD(IsCancellationRequested)(BOOL* value) override { *value = cancelled || (cancelAtStart && starts >= cancelAtStart); return S_OK; }
    STDMETHOD(ReportMessage)(LONG severity, BSTR, BSTR text) override {
        if (text && *text && severity == 0) ++details;
        if (text && *text && severity == 1) { ++warnings; warningText += text; }
        if (text && *text && severity == 2) ++errors;
        return S_OK;
    }
    STDMETHOD(Trace)(BSTR, BSTR) override { return S_OK; }
    STDMETHOD(Report)(ULONG completed, ULONG total, BSTR) override {
        if (completed == 0 && total == 1) ++starts;
        if (completed == 1 && total == 1) ++completions;
        return S_OK;
    }
    const wchar_t* locale = L"ru-RU";
    std::wstring warningText;
    BOOL cancelled = FALSE;
    unsigned cancelAtStart = 0;
    unsigned starts = 0, completions = 0, details = 0, warnings = 0, errors = 0;
};

class Snapshot : public CComObjectRootEx<CComSingleThreadModel>, public IFBEDocumentSnapshot {
public:
    BEGIN_COM_MAP(Snapshot)
        COM_INTERFACE_ENTRY(IFBEDocumentSnapshot)
    END_COM_MAP()
    STDMETHOD(OpenXmlStream)(IStream** stream) override {
        CComPtr<IStream> readable;
        auto hr = xml->Clone(&readable);
        const LARGE_INTEGER start{};
        if (SUCCEEDED(hr)) hr = readable->Seek(start, STREAM_SEEK_SET, nullptr);
        if (SUCCEEDED(hr)) *stream = readable.Detach();
        return hr;
    }
    STDMETHOD(GetSourceFilePath)(BSTR*) override { return E_NOTIMPL; }
    STDMETHOD(GetEncoding)(BSTR*) override { return E_NOTIMPL; }
    CComPtr<IStream> xml;
};

CComPtr<IClassFactory> ClassFactory(HMODULE dll, REFCLSID clsid) {
    using Factory = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
    const auto getFactory = reinterpret_cast<Factory>(::GetProcAddress(dll, "DllGetClassObject"));
    CComPtr<IClassFactory> factory;
    if (getFactory) getFactory(clsid, __uuidof(IClassFactory), reinterpret_cast<void**>(&factory));
    return factory;
}

std::string ReadBytes(const std::filesystem::path& file) {
    std::ifstream data(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(data), std::istreambuf_iterator<char>()};
}

bool CheckExport(HMODULE dll, const wchar_t* input, const wchar_t* broken, IStream* xml) {
    const auto factory = ClassFactory(dll, ExportClsid);
    CComPtr<IFBEExportPlugin2> plugin;
    if (!factory || FAILED(factory->CreateInstance(nullptr, __uuidof(IFBEExportPlugin2), reinterpret_cast<void**>(&plugin)))) return false;
    CComQIPtr<IFBEPluginInfo2> info(plugin);
    CComBSTR id;
    ULONG version = 0;
    if (!info || FAILED(info->GetApiVersion(&version)) || version != 2 || FAILED(info->GetPluginId(&id)) || id != L"export-fb3") return false;
    auto* rawHost = new CComObjectNoLock<Host>;
    CComPtr<IFBEPluginHost> host(rawHost);
    auto* snapshotObject = new CComObjectNoLock<Snapshot>;
    CComPtr<IFBEDocumentSnapshot> snapshot(snapshotObject);
    snapshotObject->xml = xml;
    const auto target = std::filesystem::path(broken).parent_path() / L"Книга (экспорт FB3).fb3";
    if (!::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", L"export-fb3") ||
        !::SetEnvironmentVariableW(L"FBE_NEXT_TEST_EXPORT_FB3_PATH", target.c_str())) return false;
    if (FAILED(plugin->Export(host, CComBSTR(input), snapshot)) || rawHost->completions != 1 || rawHost->errors) return false;
    const auto output = ReadBytes(target);
    if (output.size() < 4 || output.compare(0, 4, "PK\003\004") != 0) return false;
    rawHost->cancelAtStart = 2;
    if (plugin->Export(host, CComBSTR(input), snapshot) != HRESULT_FROM_WIN32(ERROR_CANCELLED) ||
        ReadBytes(target) != output || rawHost->completions != 1 || rawHost->errors) return false;
    rawHost->cancelAtStart = 0;
    snapshotObject->xml.Release();
    if (FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &snapshotObject->xml))) return false;
    const char invalid[] = "<broken>";
    ULONG written = 0;
    if (FAILED(snapshotObject->xml->Write(invalid, sizeof(invalid) - 1, &written)) || written != sizeof(invalid) - 1) return false;
    if (plugin->Export(host, CComBSTR(input), snapshot) != HRESULT_FROM_WIN32(ERROR_INVALID_DATA) ||
        ReadBytes(target) != output || rawHost->errors != 1 || rawHost->completions != 1) return false;

    auto* infoHost = new CComObjectNoLock<Host>;
    CComPtr<IFBEPluginHost> infoHostInterface(infoHost);
    snapshotObject->xml.Release();
    constexpr char minimal[] =
        "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\"><description>"
        "<title-info><genre>sf</genre><author><nickname>Author</nickname></author><book-title>Test</book-title><lang>en</lang></title-info>"
        "<document-info><author><nickname>Editor</nickname></author><date value=\"2026-11-28\">2026-11-28T00:00:00Z</date>"
        "<id>7027fa9a-0c72-52af-a708-2526bf9c3054</id><version>1.0</version></document-info></description>"
        "<body><section><p>Chapter without an ID</p></section></body></FictionBook>";
    if (FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &snapshotObject->xml)) ||
        FAILED(snapshotObject->xml->Write(minimal, sizeof(minimal) - 1, &written)) || written != sizeof(minimal) - 1) return false;
    return SUCCEEDED(plugin->Export(infoHostInterface, CComBSTR(input), snapshot)) &&
        infoHost->details && !infoHost->warnings && !infoHost->errors && infoHost->completions == 1;
}

bool Check(HMODULE dll, const wchar_t* input, const wchar_t* broken, HMODULE exporter) {
    const auto factory = ClassFactory(dll, ImportClsid);
    CComPtr<IFBEImportPlugin2> plugin;
    if (!factory || FAILED(factory->CreateInstance(nullptr, __uuidof(IFBEImportPlugin2), reinterpret_cast<void**>(&plugin)))) return false;
    CComQIPtr<IFBEPluginInfo2> info(plugin);
    CComBSTR id;
    ULONG version = 0;
    if (!info || FAILED(info->GetApiVersion(&version)) || version != 2 || FAILED(info->GetPluginId(&id)) || id != L"import-fb3") return false;
    auto* rawHost = new CComObjectNoLock<Host>;
    CComPtr<IFBEPluginHost> host(rawHost);
    CComBSTR name;
    CComPtr<IStream> stream;
    if (!::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", L"1") ||
        !::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", L"import-fb3") ||
        !::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_FB3_PATH", input)) return false;
    if (FAILED(plugin->Import(host, &name, &stream)) || !stream || !name ||
        std::filesystem::path(static_cast<const wchar_t*>(name)) != std::filesystem::path(input).replace_extension(L".fb2") ||
        rawHost->starts != 1 || rawHost->completions != 1 || !rawHost->warnings || !rawHost->details || rawHost->errors) return false;
    const auto hasCyrillic = [](const std::wstring& text) {
        return std::any_of(text.begin(), text.end(), [](wchar_t ch) { return ch >= 0x0400 && ch <= 0x04ff; });
    };
    auto* englishHost = new CComObjectNoLock<Host>;
    CComPtr<IFBEPluginHost> englishHostInterface(englishHost);
    englishHost->locale = L"en-US";
    CComBSTR englishName;
    CComPtr<IStream> englishStream;
    if (FAILED(plugin->Import(englishHostInterface, &englishName, &englishStream)) || !englishName || !englishStream ||
        !englishHost->warnings || !englishHost->details || englishHost->errors ||
        !hasCyrillic(rawHost->warningText) || hasCyrillic(englishHost->warningText)) return false;
    CComPtr<IXMLDOMDocument2> xml;
    VARIANT_BOOL parsed = VARIANT_FALSE;
    if (FAILED(xml.CoCreateInstance(__uuidof(DOMDocument60))) || FAILED(xml->put_async(VARIANT_FALSE)) ||
        FAILED(xml->load(CComVariant(static_cast<IUnknown*>(stream.p)), &parsed)) || parsed != VARIANT_TRUE) return false;
    CComPtr<IXMLDOMNode> bookTitle;
    CComBSTR titleText;
    if (FAILED(xml->selectSingleNode(CComBSTR(L"//*[local-name()='book-title']"), &bookTitle)) || !bookTitle ||
        FAILED(bookTitle->get_text(&titleText)) || titleText.Length() == 0) return false;
    if (exporter && (!CheckExport(exporter, input, broken, stream) || !::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", L"import-fb3"))) return false;
    name.Empty();
    stream.Release();
    rawHost->cancelled = TRUE;
    const auto cancellation = HRESULT_FROM_WIN32(ERROR_CANCELLED);
    if (plugin->Import(host, &name, &stream) != cancellation || name || stream || rawHost->starts != 1) return false;
    rawHost->cancelled = FALSE;
    if (!::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_FB3_PATH", nullptr) ||
        plugin->Import(host, &name, &stream) != cancellation || name || stream || rawHost->starts != 1) return false;
    if (!::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_FB3_PATH", broken)) return false;
    return plugin->Import(host, &name, &stream) == HRESULT_FROM_WIN32(ERROR_INVALID_DATA) &&
        !name && !stream && rawHost->errors == 1 && rawHost->starts == 2 && rawHost->completions == 1;
}

}

int wmain(int argc, wchar_t** argv) {
    if ((argc != 4 && argc != 5) || FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    const HMODULE dll = ::LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    const HMODULE exporter = argc == 5 ? ::LoadLibraryExW(argv[4], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) : nullptr;
    bool success = false;
    try { success = dll && (argc == 4 || exporter) && Check(dll, argv[2], argv[3], exporter); }
    catch (const ATL::CAtlException& error) { std::fprintf(stderr, "COM test error 0x%08lX\n", static_cast<unsigned long>(error.m_hr)); }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); }
    if (dll) ::FreeLibrary(dll);
    if (exporter) ::FreeLibrary(exporter);
    ::CoUninitialize();
    if (!success) { std::fputs("FB3 plugin test failed.\n", stderr); return 1; }
    std::puts("FB3 local COM import, cancellation and corrupted input passed.");
    if (argc == 5) std::puts("FB3 local COM export, late cancellation and preservation of existing output passed.");
    return 0;
}
