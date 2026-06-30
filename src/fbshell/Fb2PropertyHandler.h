#pragma once

#include <atlbase.h>
#include <atlcom.h>

#include "resource.h"
#include "Fb2PropertyStoreCom.h"

EXTERN_C const GUID CLSID_Fb2PropertyHandler;

class Fb2PropertyHandler :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<Fb2PropertyHandler, &CLSID_Fb2PropertyHandler>,
    public IPropertyStore,
    public IPropertyStoreCapabilities,
    public IInitializeWithStream
{
public:
    Fb2PropertyHandler();

    DECLARE_NOT_AGGREGATABLE(Fb2PropertyHandler)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    DECLARE_REGISTRY_RESOURCEID(IDR_FB2PROPERTYHANDLER)

    BEGIN_COM_MAP(Fb2PropertyHandler)
        COM_INTERFACE_ENTRY(IPropertyStore)
        COM_INTERFACE_ENTRY(IPropertyStoreCapabilities)
        COM_INTERFACE_ENTRY(IInitializeWithStream)
    END_COM_MAP()

    HRESULT FinalConstruct();
    void FinalRelease();

    STDMETHOD(GetCount)(DWORD* propertyCount);
    STDMETHOD(GetAt)(DWORD propertyIndex, PROPERTYKEY* key);
    STDMETHOD(GetValue)(REFPROPERTYKEY key, PROPVARIANT* value);
    STDMETHOD(SetValue)(REFPROPERTYKEY key, REFPROPVARIANT value);
    STDMETHOD(Commit)();

    STDMETHOD(IsPropertyWritable)(REFPROPERTYKEY key);
    STDMETHOD(Initialize)(IStream* stream, DWORD mode);

private:
    FBShellModern::Fb2PropertyStoreCom* m_store;
};
