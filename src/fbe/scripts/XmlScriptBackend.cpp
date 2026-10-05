#include "stdafx.h"
#include "XmlScriptBackend.h"
#include "../apputils.h"
#include "../FBDoc.h"
#include "../source/SourceDocumentTransfer.h"
#include "../source/ui/SourceEditorControl.h"

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

class UndoManagerDisableScope
{
public:
	explicit UndoManagerDisableScope(IOleUndoManager* manager) : m_manager(manager), m_disabled(false)
	{
		if(m_manager && SUCCEEDED(m_manager->Enable(FALSE))) m_disabled = true;
	}
	~UndoManagerDisableScope() { if(m_disabled) m_manager->Enable(TRUE); }
private:
	CComPtr<IOleUndoManager> m_manager;
	bool m_disabled;
};
}

// This unit is deliberately registered with MSHTML's normal undo manager.
// Consequently the ordinary FBE Undo/Redo commands own its lifetime and drive
// both directions; XmlScriptBackend does not maintain a parallel undo stack.
class XmlScriptUndoUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit
{
public:
	BEGIN_COM_MAP(XmlScriptUndoUnit)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
	END_COM_MAP()

	void Initialize(XmlScriptBackend* backend, const CString& before, const CString& after,
		bool beforeWasDirty, const CString& operationName)
	{
		m_backend = backend; m_before = before; m_after = after;
		m_beforeWasDirty = beforeWasDirty;
		m_description = operationName.IsEmpty() ? L"Apply XML source" : operationName;
		m_applyBefore = true;
	}

	STDMETHOD(Do)(IOleUndoManager* manager)
	{
		if(m_backend == NULL) return E_UNEXPECTED;
		XmlScriptDiagnostic diagnostic;
		{
			// Applying a snapshot recreates MSHTML's DOM. Keep that internal work
			// out of the standard undo stack; this IOleUndoUnit is the sole unit.
			UndoManagerDisableScope suppress(manager);
			diagnostic = m_backend->ApplySnapshot(
				m_applyBefore ? m_before : m_after, m_applyBefore ? m_beforeWasDirty : true);
		}
		if(!diagnostic.valid) return E_FAIL;
		m_applyBefore = !m_applyBefore;
		return manager ? manager->Add(this) : S_OK;
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
	STDMETHOD(OnNextAdd)() { return S_OK; }

private:
	XmlScriptBackend* m_backend = NULL;
	CString m_before, m_after, m_description;
	bool m_beforeWasDirty = false;
	bool m_applyBefore = true;
};

XmlScriptBackend::XmlScriptBackend(FB::Doc*& document, SourceEditorControl& source,
	const std::function<bool()>& sourceIsActive, const SynchronizeCallback& synchronize) :
	m_document(document), m_source(source), m_sourceIsActive(sourceIsActive), m_synchronize(synchronize) {}

bool XmlScriptBackend::GetSourceText(CString& text) const
{
	text.Empty(); if(m_document == NULL) return false;
	if(m_sourceIsActive()) { SourceDocumentText source; if(SourceDocumentTransfer::ReadSourceText(m_source, source) != SourceTransitionResult::Success) return false; text = source.text; return true; }
	MSXML2::IXMLDOMDocument2Ptr dom = m_document->CreateDOM(m_document->m_encoding);
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

	// Suppress transient MSHTML units produced by LoadFromDOM.  The one unit
	// added below is the sole operation exposed through FBE's normal Ctrl+Z.
	{
		UndoManagerDisableScope suppress(GetUndoManager(m_document));
		diagnostic = ApplySnapshot(text, true);
	}
	if(!diagnostic.valid) return diagnostic;

	CComPtr<IOleUndoManager> manager = GetUndoManager(m_document);
	CComObject<XmlScriptUndoUnit>* unit = NULL;
	HRESULT result = manager ? CComObject<XmlScriptUndoUnit>::CreateInstance(&unit) : E_NOINTERFACE;
	if(SUCCEEDED(result))
	{
		unit->AddRef();
		unit->Initialize(this, previous, text, documentWasDirty, operationName);
		result = manager->Add(unit);
		unit->Release();
	}
	if(SUCCEEDED(result)) return diagnostic;

	// Registration is part of the atomic Apply contract: never leave an XML
	// mutation behind that the ordinary FBE Undo command cannot reverse.
	{
		UndoManagerDisableScope suppress(GetUndoManager(m_document));
		ApplySnapshot(previous, documentWasDirty);
	}
	diagnostic.valid = false;
	diagnostic.message = L"Could not register the XML operation with the standard undo manager.";
	return diagnostic;
}
