#pragma once

#include "resource.h"
#include "ExportEPUB_i.h"

class ATL_NO_VTABLE CExportEPUBPlugin :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CExportEPUBPlugin, &CLSID_ExportEPUBPlugin>,
    public IFBEExportPlugin
{
public:
    DECLARE_REGISTRY_RESOURCEID(IDR_EXPORTEPUB)
    DECLARE_NOT_AGGREGATABLE(CExportEPUBPlugin)

    // PVS-Studio: ATL COM map macro intentionally expands to ATL-generated static tables.
    //-V835
    //-V1096
    BEGIN_COM_MAP(CExportEPUBPlugin) //-V835 //-V1096
        COM_INTERFACE_ENTRY(IFBEExportPlugin)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    // IFBEExportPlugin
    STDMETHODIMP Export(long hWnd, BSTR filename, IDispatch* doc);
};
