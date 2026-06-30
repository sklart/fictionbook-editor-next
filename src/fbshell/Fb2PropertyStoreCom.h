#pragma once

#include <windows.h>
#include <propsys.h>

#include "Fb2PropertyStore.h"

namespace FBShellModern {

class Fb2PropertyStoreCom :
    public IPropertyStore,
    public IPropertyStoreCapabilities,
    public IInitializeWithStream {
public:
    Fb2PropertyStoreCom();

    static HRESULT CreateInstance(REFIID riid, void** object);

    STDMETHODIMP QueryInterface(REFIID riid, void** object);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    STDMETHODIMP GetCount(DWORD* propertyCount);
    STDMETHODIMP GetAt(DWORD propertyIndex, PROPERTYKEY* key);
    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* value);
    STDMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT value);
    STDMETHODIMP Commit();

    STDMETHODIMP IsPropertyWritable(REFPROPERTYKEY key);
    STDMETHODIMP Initialize(IStream* stream, DWORD mode);

private:
    ~Fb2PropertyStoreCom();

    HRESULT InitializeFromFile(const wchar_t* filePath);
    HRESULT SaveStreamToTemporaryFile(IStream* stream, ATL::CString& filePath) const;

private:
    LONG m_referenceCount;
    bool m_initialized;
    Fb2PropertyStoreCache m_cache;
};

} // namespace FBShellModern
