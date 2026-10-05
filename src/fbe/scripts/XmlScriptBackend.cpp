#include "stdafx.h"
#include "XmlScriptBackend.h"
#include "../apputils.h"
#include "../FBDoc.h"
#include "../source/SourceDocumentTransfer.h"
#include "../source/ui/SourceEditorControl.h"

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

XmlScriptDiagnostic XmlScriptBackend::ApplyValidatedText(const CString& text, bool recordUndo, bool markDocumentDirty)
{
	XmlScriptDiagnostic diagnostic = ValidateSourceText(text);
	if(!diagnostic.valid) return diagnostic;
	CString previous;
	const bool documentWasDirty = m_document->DocChanged() || m_source.SendMessage(SCI_GETMODIFY) != 0;
	if(recordUndo && !GetSourceText(previous)) { diagnostic.valid = false; diagnostic.message = L"Could not capture the document before applying XML."; return diagnostic; }
	const BSTR candidate = text.AllocSysString();
	if(candidate == NULL) { diagnostic.valid = false; diagnostic.message = L"Out of memory."; return diagnostic; }
	diagnostic.valid = m_document->SetXMLAndValidate(m_source.m_hWnd, false, diagnostic.line, diagnostic.column, &diagnostic.message, candidate);
	::SysFreeString(candidate);
	if(!diagnostic.valid) return diagnostic;
	// LoadFromDOM establishes a production save point. An Apply call is an
	// edit, whereas an undo restores the dirty state captured before it.
	if(markDocumentDirty) m_document->ResetSavePoint();
	if(recordUndo) m_undoSnapshots.push_back({ previous, documentWasDirty });
	if(m_synchronize) m_synchronize(text);
	return diagnostic;
}

XmlScriptDiagnostic XmlScriptBackend::ApplySourceText(const CString& text, const CString& /*operationName*/) { return ApplyValidatedText(text, true, true); }
bool XmlScriptBackend::CanUndo() const { return !m_undoSnapshots.empty(); }
XmlScriptDiagnostic XmlScriptBackend::UndoLastApply()
{
	XmlScriptDiagnostic diagnostic;
	if(m_undoSnapshots.empty()) { diagnostic.message = L"No XML script operation can be undone."; return diagnostic; }
	const UndoSnapshot snapshot = m_undoSnapshots.back(); diagnostic = ApplyValidatedText(snapshot.text, false, snapshot.documentWasDirty);
	if(diagnostic.valid) m_undoSnapshots.pop_back(); return diagnostic;
}
