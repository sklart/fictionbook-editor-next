#include <windows.h>
#include <oleauto.h>
#include "FBE.h"

// Test-only local COM server. mode: 0 valid, 1 no-info, 2 no-export,
// 3 wrong-id, 4 wrong-api, 5 QI failure.
class Fixture : public IFBEPluginInfo2, public IFBEExportPlugin2 {
    LONG refs; int mode;
public:
    Fixture(int value) : refs(1), mode(value) {}
    STDMETHOD(QueryInterface)(REFIID iid, void** value) {
        if (!value) return E_POINTER; *value = NULL;
        if (mode == 5 && iid == IID_IFBEPluginInfo2) return E_FAIL;
        if (iid == IID_IUnknown) *value = static_cast<IFBEPluginInfo2*>(this);
        else if (iid == IID_IFBEPluginInfo2 && mode != 1) *value = static_cast<IFBEPluginInfo2*>(this);
        else if (iid == IID_IFBEExportPlugin2 && mode != 2) *value = static_cast<IFBEExportPlugin2*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&refs); }
    STDMETHOD_(ULONG, Release)() { LONG value = InterlockedDecrement(&refs); if (!value) delete this; return value; }
    STDMETHOD(GetPluginId)(BSTR* value) { if (!value) return E_POINTER; *value = SysAllocString(mode == 3 ? L"wrong" : L"synthetic-v2"); return *value ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetPluginVersion)(BSTR* value) { if (!value) return E_POINTER; *value = SysAllocString(L"1.0"); return *value ? S_OK : E_OUTOFMEMORY; }
    STDMETHOD(GetApiVersion)(ULONG* value) { if (!value) return E_POINTER; *value = mode == 4 ? 99 : 2; return S_OK; }
    STDMETHOD(GetCapabilities)(ULONGLONG* value) { if (!value) return E_POINTER; *value = 0; return S_OK; }
    STDMETHOD(Export)(IFBEPluginHost* host, BSTR, IFBEDocumentSnapshot* document) {
        if (!host || !document) return E_INVALIDARG; BSTR version = NULL, locale = NULL, encoding = NULL; LONGLONG hwnd = 0;
        HRESULT hr = host->GetHostVersion(&version); if (SUCCEEDED(hr)) hr = host->GetUiLocale(&locale); if (SUCCEEDED(hr)) hr = host->GetOwnerWindow(&hwnd);
        IFBEProgressSink* progress = NULL; IFBECancellationToken* cancellation = NULL; IStream* stream = NULL;
        if (SUCCEEDED(hr)) hr = host->GetProgressSink(&progress); if (SUCCEEDED(hr)) hr = host->GetCancellationToken(&cancellation); if (SUCCEEDED(hr)) hr = document->GetEncoding(&encoding); if (SUCCEEDED(hr)) hr = document->OpenXmlStream(&stream);
        BOOL cancelled = TRUE; if (SUCCEEDED(hr)) hr = cancellation->IsCancellationRequested(&cancelled); BSTR stage = SysAllocString(L"test"); if (SUCCEEDED(hr) && !stage) hr = E_OUTOFMEMORY; if (SUCCEEDED(hr)) hr = progress->Report(1, 1, stage); if (stage) SysFreeString(stage);
        char bytes[256] = {}; ULONG read = 0; if (SUCCEEDED(hr)) hr = stream->Read(bytes, sizeof(bytes), &read);
        const char hello[] = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"; bool utf8 = false; for (ULONG n = 0; n + sizeof(hello) - 1 <= read; ++n) if (!memcmp(bytes + n, hello, sizeof(hello) - 1)) { utf8 = true; break; }
        if (SUCCEEDED(hr) && (!version || !locale || !hwnd || cancelled || !encoding || _wcsicmp(encoding, L"utf-8") || read < 5 || memcmp(bytes, "<?xml", 5) || !utf8)) hr = E_FAIL;
        if (version) SysFreeString(version); if (locale) SysFreeString(locale); if (encoding) SysFreeString(encoding); if (progress) progress->Release(); if (cancellation) cancellation->Release(); if (stream) stream->Release(); return hr;
    }
};
extern "C" __declspec(dllexport) HRESULT WINAPI CreateFbePluginV2Fixture(int mode, IUnknown** value) { if (!value) return E_POINTER; *value = static_cast<IFBEPluginInfo2*>(new Fixture(mode)); return *value ? S_OK : E_OUTOFMEMORY; }
