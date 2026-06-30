#pragma once
#include <atlbase.h>
#include <atlcom.h>
#include <thumbcache.h>
#include "resource.h"
EXTERN_C const GUID CLSID_Fb2ThumbnailProvider;
class Fb2ThumbnailProvider :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<Fb2ThumbnailProvider, &CLSID_Fb2ThumbnailProvider>,
    public IInitializeWithStream,
    public IThumbnailProvider
{
public:
    Fb2ThumbnailProvider();
    DECLARE_NOT_AGGREGATABLE(Fb2ThumbnailProvider)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    DECLARE_REGISTRY_RESOURCEID(IDR_FB2THUMBNAILPROVIDER)
    BEGIN_COM_MAP(Fb2ThumbnailProvider)
        COM_INTERFACE_ENTRY(IInitializeWithStream)
        COM_INTERFACE_ENTRY(IThumbnailProvider)
    END_COM_MAP()
    STDMETHOD(Initialize)(IStream* stream, DWORD mode);
    STDMETHOD(GetThumbnail)(UINT cx, HBITMAP* bitmap, WTS_ALPHATYPE* alphaType);
private:
    CComPtr<IStream> m_stream;
    DWORD m_mode;
};
