// PVS-Studio: ATL COM-map macros below intentionally use COM ABI signatures and static entry tables.
//-V::835,1096
#pragma once
#include "resource.h"
#include "ExportDOCX_i.h"
#include "fbe.h"

class ATL_NO_VTABLE CExportDOCXPlugin :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CExportDOCXPlugin, &CLSID_ExportDOCXPlugin>,
	public IFBEExportPlugin,
	public IFBEPluginInfo2,
	public IFBEExportPlugin2
{
public:
	DECLARE_REGISTRY_RESOURCEID(IDR_EXPORTDOCX)
	DECLARE_NOT_AGGREGATABLE(CExportDOCXPlugin)

	BEGIN_COM_MAP(CExportDOCXPlugin) //-V835 //-V835 //-V1096
		COM_INTERFACE_ENTRY(IFBEExportPlugin)
		COM_INTERFACE_ENTRY(IFBEPluginInfo2)
		COM_INTERFACE_ENTRY(IFBEExportPlugin2)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	// IFBEExportPlugin
	STDMETHODIMP Export(long hWnd,BSTR filename,IDispatch *doc);

	// IFBEPluginInfo2
	STDMETHODIMP GetPluginId(BSTR *value);
	STDMETHODIMP GetPluginVersion(BSTR *value);
	STDMETHODIMP GetApiVersion(ULONG *value);
	STDMETHODIMP GetCapabilities(ULONGLONG *value);

	// IFBEExportPlugin2
	STDMETHODIMP Export(IFBEPluginHost *host, BSTR filename, IFBEDocumentSnapshot *document);

private:
	HRESULT ExportCore(long hWnd, BSTR filename, IDispatch *doc);
};

OBJECT_ENTRY_AUTO(__uuidof(ExportDOCXPlugin), CExportDOCXPlugin)
