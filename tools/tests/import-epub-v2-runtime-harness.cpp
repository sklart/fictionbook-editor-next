#include "../../src/fbe/FBE.h"
#include <windows.h>
#include <oleauto.h>
#include <msxml6.h>
#include <string>
#include <vector>
#include <cstring>

typedef HRESULT(STDAPICALLTYPE *GetClassObject)(REFCLSID, REFIID, void **);

class Progress final : public IFBEProgressSink {
    LONG refs_ = 1;
public:
    int start = 0, done = 0;
    STDMETHOD(QueryInterface)(REFIID iid, void **out) { if (!out) return E_POINTER; *out = nullptr; if (iid != IID_IUnknown && iid != IID_IFBEProgressSink) return E_NOINTERFACE; *out = this; AddRef(); return S_OK; }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&refs_); }
    STDMETHOD_(ULONG, Release)() { LONG n = InterlockedDecrement(&refs_); if (!n) delete this; return n; }
    STDMETHOD(Report)(ULONG current, ULONG total, BSTR) { if (current == 0 && total == 1) ++start; if (current == 1 && total == 1) ++done; return S_OK; }
};

class Cancel final : public IFBECancellationToken {
    LONG refs_ = 1;
public:
    BOOL cancelled = FALSE; HRESULT result = S_OK;
    STDMETHOD(QueryInterface)(REFIID iid, void **out) { if (!out) return E_POINTER; *out = nullptr; if (iid != IID_IUnknown && iid != IID_IFBECancellationToken) return E_NOINTERFACE; *out = this; AddRef(); return S_OK; }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&refs_); }
    STDMETHOD_(ULONG, Release)() { LONG n = InterlockedDecrement(&refs_); if (!n) delete this; return n; }
    STDMETHOD(IsCancellationRequested)(BOOL *out) { if (!out) return E_POINTER; *out = cancelled; return result; }
};

class Host final : public IFBEPluginHost {
    LONG refs_ = 1;
public:
    Progress *progress = new Progress; Cancel *cancel = new Cancel; std::vector<std::wstring> codes;
    ~Host() { progress->Release(); cancel->Release(); }
    STDMETHOD(QueryInterface)(REFIID iid, void **out) { if (!out) return E_POINTER; *out = nullptr; if (iid != IID_IUnknown && iid != IID_IFBEPluginHost) return E_NOINTERFACE; *out = this; AddRef(); return S_OK; }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&refs_); }
    STDMETHOD_(ULONG, Release)() { LONG n = InterlockedDecrement(&refs_); if (!n) delete this; return n; }
    STDMETHOD(GetHostVersion)(BSTR *out) { if (!out) return E_POINTER; *out = SysAllocString(L"test"); return *out ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetUiLocale)(BSTR *out) { if (!out) return E_POINTER; *out = SysAllocString(L"ru-RU"); return *out ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetOwnerWindow)(LONGLONG *out) { if (!out) return E_POINTER; *out = 0; return S_OK; }
    STDMETHOD(GetProgressSink)(IFBEProgressSink **out) { if (!out) return E_POINTER; *out = progress; progress->AddRef(); return S_OK; }
    STDMETHOD(GetCancellationToken)(IFBECancellationToken **out) { if (!out) return E_POINTER; *out = cancel; cancel->AddRef(); return S_OK; }
    STDMETHOD(ReportMessage)(LONG, BSTR code, BSTR) { if (code) codes.emplace_back(code); return S_OK; }
    STDMETHOD(Trace)(BSTR, BSTR) { return S_OK; }
    bool Has(const wchar_t *code) const { for (const auto& value : codes) if (value == code) return true; return false; }
};

static int Fail(int code) { return code; }
static HRESULT Call(IFBEImportPlugin2 *plugin, Host *host, BSTR *name, IStream **stream) { *name = reinterpret_cast<BSTR>(1); *stream = reinterpret_cast<IStream *>(1); return plugin->Import(host, name, stream); }
static void SetPath(const wchar_t *path) { SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH", path); }
static bool IsCancelled(HRESULT hr) { return hr == HRESULT_FROM_WIN32(ERROR_CANCELLED); }

class TestEnvironment final {
public:
    ~TestEnvironment() {
        SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", nullptr);
        SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", nullptr);
        SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH", nullptr);
        SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_CANCEL", nullptr);
        SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_FAIL", nullptr);
    }
};

static bool ContainsMojibake(const std::wstring& text) {
    static const wchar_t markers[][3] = {
        { 0x00D0, 0 }, // \xC3\x90 (Ð)
        { 0x00D1, 0 }, // \xC3\x91 (Ñ)
        { 0x0420, 0x0452, 0 }, // Рђ
        { 0x0420, 0x00B0, 0 }, // Р°
        { 0x0421, 0x201A, 0 }  // С‚
    };
    for (const auto& marker : markers) if (text.find(marker) != std::wstring::npos) return true;
    return false;
}

static bool ValidateFb2Stream(IStream *stream, const wchar_t *requiredText, bool requireDiagnosticText) {
    if (!stream) return false;
    LARGE_INTEGER zero = {}; ULARGE_INTEGER position = {};
    if (FAILED(stream->Seek(zero, STREAM_SEEK_CUR, &position)) || position.QuadPart != 0) return false;
    STATSTG stat = {};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)) || stat.cbSize.QuadPart == 0 || stat.cbSize.QuadPart > 16 * 1024 * 1024) return false;
    std::vector<char> bytes(static_cast<size_t>(stat.cbSize.QuadPart));
    ULONG read = 0;
    if (FAILED(stream->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read)) || read != bytes.size()) return false;
    const int characters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (characters <= 0) return false;
    std::wstring xml(static_cast<size_t>(characters), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), &xml[0], characters)) return false;
    if (ContainsMojibake(xml) || xml.find(requiredText) == std::wstring::npos) return false;

    IXMLDOMDocument2 *document = nullptr;
    if (FAILED(CoCreateInstance(CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, reinterpret_cast<void **>(&document)))) return false;
    BSTR xmlText = SysAllocStringLen(xml.data(), static_cast<UINT>(xml.size())); VARIANT_BOOL loaded = VARIANT_FALSE;
    HRESULT hr = xmlText ? document->loadXML(xmlText, &loaded) : E_OUTOFMEMORY; SysFreeString(xmlText);
    IXMLDOMElement *root = nullptr; BSTR rootName = nullptr; IXMLDOMNode *body = nullptr;
    if (SUCCEEDED(hr) && loaded == VARIANT_TRUE) hr = document->get_documentElement(&root);
    if (SUCCEEDED(hr) && root) hr = root->get_baseName(&rootName);
    if (SUCCEEDED(hr) && (!rootName || wcscmp(rootName, L"FictionBook"))) hr = E_FAIL;
    BSTR bodyQuery = SysAllocString(L"//*[local-name()='body']");
    if (SUCCEEDED(hr) && bodyQuery) hr = document->selectSingleNode(bodyQuery, &body);
    const bool hasBody = body != nullptr;
    SysFreeString(bodyQuery); SysFreeString(rootName); if (body) body->Release(); if (root) root->Release(); document->Release();
    if (FAILED(hr) || !hasBody) return false;
    return !requireDiagnosticText || xml.find(L"EPUB import") != std::wstring::npos;
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 3) return Fail(2);
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return Fail(3);
    HMODULE module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return Fail(4);
    GetClassObject getClassObject = reinterpret_cast<GetClassObject>(GetProcAddress(module, "DllGetClassObject"));
    CLSID clsid = {}; if (!getClassObject || FAILED(CLSIDFromString(L"{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}", &clsid))) return Fail(5);
    IClassFactory *factory = nullptr; if (FAILED(getClassObject(clsid, IID_IClassFactory, reinterpret_cast<void **>(&factory)))) return Fail(6);
    IUnknown *unknown = nullptr; HRESULT hr = factory->CreateInstance(nullptr, IID_IUnknown, reinterpret_cast<void **>(&unknown)); factory->Release(); if (FAILED(hr)) return Fail(7);
    IFBEPluginInfo2 *info = nullptr; IFBEImportPlugin2 *v2 = nullptr; IFBEImportPlugin *v1 = nullptr;
    if (FAILED(unknown->QueryInterface(IID_IFBEPluginInfo2, reinterpret_cast<void **>(&info))) || FAILED(unknown->QueryInterface(IID_IFBEImportPlugin2, reinterpret_cast<void **>(&v2))) || FAILED(unknown->QueryInterface(IID_IFBEImportPlugin, reinterpret_cast<void **>(&v1)))) return Fail(8);
    BSTR id = nullptr; ULONG api = 0; info->GetPluginId(&id); info->GetApiVersion(&api); if (!id || wcscmp(id, L"import-epub") || api != 2) return Fail(9); SysFreeString(id); info->Release();
    BSTR staleName = reinterpret_cast<BSTR>(1); if (v2->Import(nullptr, &staleName, nullptr) != E_POINTER || staleName) return Fail(19);
    TestEnvironment testEnvironment;
    SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", L"1"); SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", L"import-epub"); SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_CANCEL", nullptr); SetPath(argv[2]);
    Host *host = new Host; BSTR name = nullptr; IStream *stream = nullptr; hr = Call(v2, host, &name, &stream);
    if (hr != S_OK || !name || !stream || host->progress->start == 0 || host->progress->done == 0) return Fail(10);
    const size_t nameLength = SysStringLen(name); if (nameLength < 4 || _wcsicmp(name + nameLength - 4, L".fb2")) return Fail(11);
    if (!ValidateFb2Stream(stream, L"Кириллический smoke test", false)) return Fail(12); stream->Release(); SysFreeString(name); host->Release();
    host = new Host; host->cancel->cancelled = TRUE; hr = Call(v2, host, &name, &stream); if (!IsCancelled(hr) || name || stream || host->progress->done || host->Has(L"import-failed")) return Fail(13); host->Release();
    SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_CANCEL", L"1"); host = new Host; hr = Call(v2, host, &name, &stream); if (!IsCancelled(hr) || name || stream || host->progress->done || host->Has(L"import-failed")) return Fail(14); host->Release(); SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_CANCEL", nullptr);
    host = new Host; host->cancel->result = E_ACCESSDENIED; hr = Call(v2, host, &name, &stream); if (hr != E_ACCESSDENIED || name || stream || host->progress->done) return Fail(15); host->Release();
    SetPath(L"C:\\fbe-import-epub-v2-missing.epub"); host = new Host; hr = Call(v2, host, &name, &stream); if (!FAILED(hr) || name || stream || !host->Has(L"import-failed") || host->progress->done) return Fail(16); host->Release();
    SetPath(argv[2]); SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_FAIL", L"1"); host = new Host; hr = Call(v2, host, &name, &stream); SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_FAIL", nullptr); if (hr != S_OK || !name || !stream || !host->Has(L"import-diagnostic") || host->Has(L"import-failed") || host->progress->done == 0) return Fail(18); const size_t diagnosticNameLength = SysStringLen(name); if (diagnosticNameLength < 4 || _wcsicmp(name + diagnosticNameLength - 4, L".fb2") || !ValidateFb2Stream(stream, L"Test-only recoverable EPUB conversion failure", true)) return Fail(21); stream->Release(); SysFreeString(name); host->Release();
    BSTR legacyName = reinterpret_cast<BSTR>(1); IDispatch *legacyDocument = reinterpret_cast<IDispatch *>(1); if (v1->Import(0, &legacyName, &legacyDocument) != S_FALSE || legacyName || legacyDocument) return Fail(20);
    v1->Release(); v2->Release(); unknown->Release(); FreeLibrary(module); CoUninitialize(); return 0;
}
