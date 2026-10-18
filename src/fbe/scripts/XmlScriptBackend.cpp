#include "stdafx.h"
#include "XmlScriptBackend.h"
#include "../apputils.h"
#include "../FBDoc.h"
#include "../source/SourceDocumentTransfer.h"
#include "../source/ui/SourceEditorControl.h"
#include "../dom/MarkupUndoUnitScope.h"
#include <vector>

namespace
{
CComPtr<IOleUndoManager> GetUndoManager(FB::Doc* document)
{
	CComPtr<IOleUndoManager> manager;
	if(document == NULL || !document->m_body.Document()) return manager;
	IServiceProviderPtr provider(document->m_body.Document());
	if(provider) provider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager));
	return manager;
}

class XmlRecordingBlock : public CComObjectRootEx<CComSingleThreadModel>, public IOleParentUndoUnit
{
public:
	BEGIN_COM_MAP(XmlRecordingBlock)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
		COM_INTERFACE_ENTRY(IOleParentUndoUnit)
	END_COM_MAP()
	STDMETHOD(Do)(IOleUndoManager*) { return E_UNEXPECTED; }
	STDMETHOD(GetDescription)(BSTR* value) { if (!value) return E_POINTER; *value = ::SysAllocString(L"XML internal changes"); return *value ? S_OK : E_OUTOFMEMORY; }
	STDMETHOD(GetUnitType)(CLSID* clsid, LONG* id) { if (!clsid || !id) return E_POINTER; *clsid = CLSID_NULL; *id = 0; return S_OK; }
	STDMETHOD(OnNextAdd)() { return S_OK; }
	STDMETHOD(Open)(IOleParentUndoUnit*) { return S_OK; }
	STDMETHOD(Close)(IOleParentUndoUnit* child, BOOL) { return child == static_cast<IOleParentUndoUnit*>(this) ? S_FALSE : S_OK; }
	STDMETHOD(Add)(IOleUndoUnit*) { return S_OK; }
	STDMETHOD(FindUnit)(IOleUndoUnit*) { return S_FALSE; }
	STDMETHOD(GetParentState)(DWORD* state) { if (!state) return E_POINTER; *state = UAS_BLOCKED; return S_OK; }
};
class XmlRecordingScope
{
	CComPtr<IOleUndoManager> m_manager;
	CComPtr<IOleParentUndoUnit> m_block;
	bool m_open = false;
public:
	explicit XmlRecordingScope(IOleUndoManager* manager) : m_manager(manager) {
		if (!manager) CheckError(E_NOINTERFACE);
		CComObject<XmlRecordingBlock>* raw = nullptr;
		CheckError(CComObject<XmlRecordingBlock>::CreateInstance(&raw)); m_block = raw;
		CheckError(manager->Open(m_block)); m_open = true;
	}
	HRESULT Close() { if (!m_open) return S_OK; const HRESULT hr = m_manager->Close(m_block, FALSE); if (SUCCEEDED(hr)) m_open = false; return hr; }
	~XmlRecordingScope() { Close(); }
};

// Native units retain MSHTML markup pointers. Supplement them with binary
// SAFEARRAY expandos, which native innerHTML undo does not restore.
struct XmlVisualState
{
	struct Binary { CString id; CComVariant data; };
	std::vector<Binary> binaries;
	FB::Doc::UndoSavepoints savepoints;
	static CString BinaryId(MSHTML::IHTMLElementPtr element) {
		MSHTML::IHTMLElementCollectionPtr inputs(MSHTML::IHTMLElement2Ptr(element)->getElementsByTagName(L"INPUT"));
		for(long i=0;inputs && i<inputs->length;++i) {
			MSHTML::IHTMLElementPtr input(inputs->item(_variant_t(i),_variant_t()));
			if(U::scmp(input->id,L"id")==0) return static_cast<const wchar_t*>(MSHTML::IHTMLInputElementPtr(input)->value);
		}
		return CString();
	}
	static MSHTML::IHTMLElementCollectionPtr BinaryElements(FB::Doc* document) {
		MSHTML::IHTMLElementPtr root(MSHTML::IHTMLDocument3Ptr(document->m_body.Document())->getElementById(L"binobj"));
		return root ? MSHTML::IHTMLElement2Ptr(root)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr();
	}
	void Capture(FB::Doc* document) {
		savepoints = document->CaptureUndoSavepoints();
		const auto elements=BinaryElements(document);
		for(long i=0;elements && i<elements->length;++i) {
			MSHTML::IHTMLElementPtr element(elements->item(_variant_t(i),_variant_t()));
			Binary binary; CComDispatchDriver driver(element);
			if(SUCCEEDED(driver.GetPropertyByName(L"base64data",&binary.data)) && binary.data.vt!=VT_EMPTY) { binary.id=BinaryId(element); binaries.push_back(binary); }
		}
	}
	void Restore(FB::Doc* document) const {
		const auto elements=BinaryElements(document);
		for(long i=0;elements && i<elements->length;++i) {
			MSHTML::IHTMLElementPtr element(elements->item(_variant_t(i),_variant_t()));
			const CString id=BinaryId(element);
			for(const auto& binary:binaries) if(binary.id==id) { CComDispatchDriver driver(element); CComVariant data(binary.data); CheckError(driver.PutPropertyByName(L"base64data",&data)); break; }
		}
		if (!document->m_body.Init()) CheckError(E_FAIL);
		document->RestoreUndoSavepoints(savepoints);
	}
};
}

// This unit is deliberately registered with MSHTML's normal undo manager.
// Consequently the ordinary FBE Undo/Redo commands own its lifetime and drive
// both directions; XmlScriptBackend does not maintain a parallel undo stack.
class XmlScriptUndoUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleParentUndoUnit
{
public:
	BEGIN_COM_MAP(XmlScriptUndoUnit)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
		COM_INTERFACE_ENTRY(IOleParentUndoUnit)
	END_COM_MAP()

	void Initialize(XmlScriptBackend* backend, const CString& before, const CString& after,
		bool beforeWasDirty, const CString& operationName, const XmlVisualState& beforeState, const XmlVisualState& afterState)
	{
		m_backend = backend; m_before = before; m_after = after;
		m_beforeWasDirty = beforeWasDirty;
		m_description = operationName.IsEmpty() ? L"Apply XML source" : operationName;
		m_applyBefore = true;
		m_beforeState = beforeState; m_afterState = afterState;
	}

	STDMETHOD(Do)(IOleUndoManager* manager)
	{
		if(m_backend == NULL || manager == NULL || m_open) return E_UNEXPECTED;
		try {
			CComObject<XmlScriptUndoUnit>* raw=nullptr;
			CheckError(CComObject<XmlScriptUndoUnit>::CreateInstance(&raw));
			CComPtr<IOleParentUndoUnit> inverse(raw);
			raw->Initialize(m_backend,m_before,m_after,m_beforeWasDirty,m_description,m_beforeState,m_afterState);
			raw->m_applyBefore=!m_applyBefore;
			CheckError(manager->Open(inverse));
			HRESULT result=S_OK;
			for(auto unit=m_units.rbegin(); unit!=m_units.rend(); ++unit) { result=(*unit)->Do(manager); if(FAILED(result)) break; }
			const HRESULT closed=manager->Close(inverse,TRUE);
			CheckError(result); CheckError(closed);
			{
				XmlRecordingScope suppress(manager);
				(m_applyBefore ? m_beforeState : m_afterState).Restore(m_backend->m_document);
				if (m_backend->m_synchronize) m_backend->m_synchronize(m_applyBefore ? m_before : m_after);
			}
			return S_OK;
		} catch (const _com_error& error) { return error.Error(); }
		catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
	}

	STDMETHOD(GetDescription)(BSTR* description)
	{
		if(description == NULL) return E_POINTER;
		*description = m_description.AllocSysString();
		return *description ? S_OK : E_OUTOFMEMORY;
	}
	STDMETHOD(GetUnitType)(CLSID* classId, LONG* id)
	{
		if(classId == NULL || id == NULL) return E_POINTER;
		*classId = CLSID_NULL; *id = 0; return S_OK;
	}
	STDMETHOD(OnNextAdd)() { return m_units.empty() ? S_OK : m_units.back()->OnNextAdd(); }
	STDMETHOD(Open)(IOleParentUndoUnit* unit) {
		if(!unit) return E_POINTER;
		if(unit==static_cast<IOleParentUndoUnit*>(this) || FindUnit(unit)==S_OK) return E_INVALIDARG;
		if(m_open) return m_open->Open(unit);
		m_open=unit; return S_OK;
	}
	STDMETHOD(Close)(IOleParentUndoUnit* unit, BOOL commit) {
		if(!m_open) return S_FALSE;
		const HRESULT hr=m_open->Close(unit,commit); if(hr!=S_FALSE) return hr;
		if(m_open.p!=unit) return E_INVALIDARG;
		CComPtr<IOleParentUndoUnit> child(m_open);m_open.Release();
		const HRESULT added=commit ? Add(child) : S_OK;
		if(FAILED(added)) m_open=child;
		return added;
	}
	STDMETHOD(Add)(IOleUndoUnit* unit) {
		if(!unit) return E_POINTER;
		if(unit==static_cast<IOleUndoUnit*>(this)) return E_INVALIDARG;
		if(m_open) return m_open->Add(unit);
		try {m_units.push_back(unit);} catch(const std::bad_alloc&) {return E_OUTOFMEMORY;}
		if(m_units.size()>1) m_units[m_units.size()-2]->OnNextAdd();
		return S_OK;
	}
	STDMETHOD(FindUnit)(IOleUndoUnit* unit) {
		if(!unit) return E_POINTER;
		if(m_open && (m_open.p==unit || m_open->FindUnit(unit)==S_OK)) return S_OK;
		for(const auto& child:m_units) {
			if(child.p==unit) return S_OK;
			CComQIPtr<IOleParentUndoUnit> parent(child);
			if(parent && parent->FindUnit(unit)==S_OK) return S_OK;
		}
		return S_FALSE;
	}
	STDMETHOD(GetParentState)(DWORD* state) { if(!state) return E_POINTER; if(m_open) return m_open->GetParentState(state); *state=UAS_NORMAL; return S_OK; }

private:
	XmlScriptBackend* m_backend = NULL;
	CString m_before, m_after, m_description;
	bool m_beforeWasDirty = false;
	bool m_applyBefore = true;
	XmlVisualState m_beforeState, m_afterState;
	std::vector<CComPtr<IOleUndoUnit>> m_units;
	CComPtr<IOleParentUndoUnit> m_open;
};

#include "../testing/XmlUndoProtocolTest.inl"

XmlScriptBackend::XmlScriptBackend(FB::Doc*& document, SourceEditorControl& source,
	const std::function<bool()>& sourceIsActive, const SynchronizeCallback& synchronize) :
	m_document(document), m_source(source), m_sourceIsActive(sourceIsActive), m_synchronize(synchronize) {}

bool XmlScriptBackend::GetSourceText(CString& text) const
{
	text.Empty(); if(m_document == NULL) return false;
	if(m_sourceIsActive()) { SourceDocumentText source; if(SourceDocumentTransfer::ReadSourceText(m_source, source) != SourceTransitionResult::Success) return false; text = source.text; return true; }
	MSXML2::IXMLDOMDocument2Ptr dom = m_document->CreateDOMForAnalysis();
	if(!dom) return false;
	text = static_cast<const wchar_t*>(_bstr_t(dom->xml));
	return !text.IsEmpty();
}

XmlScriptDiagnostic XmlScriptBackend::ValidateSourceText(const CString& text) const
{
	XmlScriptDiagnostic diagnostic;
	if(m_document == NULL) { diagnostic.message = L"No document is open."; return diagnostic; }
	const BSTR candidate = text.AllocSysString();
	if(candidate == NULL) { diagnostic.message = L"Out of memory."; return diagnostic; }
	diagnostic.valid = m_document->SetXMLAndValidate(m_source.m_hWnd, true, diagnostic.line, diagnostic.column, &diagnostic.message, candidate);
	::SysFreeString(candidate); return diagnostic;
}

XmlScriptDiagnostic XmlScriptBackend::ApplySnapshot(const CString& text, bool markDocumentDirty)
{
	XmlScriptDiagnostic diagnostic;
	if(m_document == NULL) { diagnostic.message = L"No document is open."; return diagnostic; }
	const BSTR candidate = text.AllocSysString();
	if(candidate == NULL) { diagnostic.valid = false; diagnostic.message = L"Out of memory."; return diagnostic; }
	diagnostic.valid = m_document->SetXMLAndValidate(m_source.m_hWnd, false, diagnostic.line, diagnostic.column, &diagnostic.message, candidate);
	::SysFreeString(candidate);
	if(!diagnostic.valid) return diagnostic;
	// LoadFromDOM establishes a production save point. The undo unit restores
	// the captured state rather than pretending every transition is an edit.
	if(markDocumentDirty) m_document->ResetSavePoint();
	if(m_synchronize) m_synchronize(text);
	return diagnostic;
}

XmlScriptDiagnostic XmlScriptBackend::ApplySourceText(const CString& text, const CString& operationName)
{
	XmlScriptDiagnostic diagnostic = ValidateSourceText(text);
	if(!diagnostic.valid) return diagnostic;
	CString previous;
	const bool documentWasDirty = m_document->DocChanged() || m_source.SendMessage(SCI_GETMODIFY) != 0;
	if(!GetSourceText(previous)) { diagnostic.valid = false; diagnostic.message = L"Could not capture the document before applying XML."; return diagnostic; }
	if(previous == text) return diagnostic;

	CComPtr<IOleUndoManager> manager = GetUndoManager(m_document);
	CComObject<XmlScriptUndoUnit>* unit = NULL;
	HRESULT result = manager ? CComObject<XmlScriptUndoUnit>::CreateInstance(&unit) : E_NOINTERFACE;
	CComPtr<IOleUndoUnit> owned(unit);
	XmlVisualState before, after;
	bool opened = false;
	try {
		CheckError(result);
		if(!VerifyXmlUndoParentProtocol()) CheckError(E_UNEXPECTED);
		before.Capture(m_document);
		unit->Initialize(this, previous, text, documentWasDirty, operationName, before, before);
		CheckError(manager->Open(unit)); opened=true;
		{
			FbeDom::MarkupUndoUnitScope native(m_document->m_body.MarkupServices(), operationName);
			diagnostic = ApplySnapshot(text, true);
			native.Close();
		}
		if (!diagnostic.valid) CheckError(E_FAIL);
		after.Capture(m_document);
		unit->Initialize(this, previous, text, documentWasDirty, operationName, before, after);
		CheckError(manager->Close(unit,TRUE)); opened=false;
		return diagnostic;
	} catch (const _com_error&) { }
	catch (const std::bad_alloc&) { }
	if (opened) {
		try { CheckError(manager->Close(unit,TRUE)); CheckError(manager->UndoTo(unit)); before.Restore(m_document); if(m_synchronize) m_synchronize(previous); }
		catch (...) { diagnostic.valid = false; diagnostic.message = L"Could not restore the XML operation."; return diagnostic; }
	}
	diagnostic.valid = false;
	diagnostic.message = L"Could not register the XML operation with the standard undo manager.";
	return diagnostic;
}
