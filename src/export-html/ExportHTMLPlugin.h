#pragma once
#include "resource.h"
#include "ExportHTML_i.h"
#include "fbe.h"

class ATL_NO_VTABLE CExportHTMLPlugin :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CExportHTMLPlugin, &CLSID_ExportHTMLPlugin>,
	public IFBEExportPlugin,
	public IFBEPluginInfo2,
	public IFBEExportPlugin2
{
public:
	DECLARE_REGISTRY_RESOURCEID(IDR_EXPORTHTML)
	DECLARE_NOT_AGGREGATABLE(CExportHTMLPlugin)

	BEGIN_COM_MAP(CExportHTMLPlugin)
		COM_INTERFACE_ENTRY(IFBEExportPlugin)
		COM_INTERFACE_ENTRY(IFBEPluginInfo2)
		COM_INTERFACE_ENTRY(IFBEExportPlugin2)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	// IFBEExportPlugin
	STDMETHODIMP Export(long hWnd,BSTR filename,IDispatch *doc);
	STDMETHODIMP GetPluginId(BSTR* value);
	STDMETHODIMP GetPluginVersion(BSTR* value);
	STDMETHODIMP GetApiVersion(ULONG* value);
	STDMETHODIMP GetCapabilities(ULONGLONG* value);
	STDMETHODIMP Export(IFBEPluginHost* host, BSTR filename, IFBEDocumentSnapshot* document);
};

OBJECT_ENTRY_AUTO(__uuidof(ExportHTMLPlugin), CExportHTMLPlugin)
