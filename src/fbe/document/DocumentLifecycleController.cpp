#include "stdafx.h"
#include "DocumentLifecycleController.h"
#include "DocumentLoader.h"
#include "DocumentSession.h"
#include "PendingDocument.h"
#include "..\\apputils.h"
#include "..\\FBDoc.h"
#include "..\\mainfrm.h"
#include "../testing/NewDocumentRuntimeProbe.h"

namespace
{
DocumentLifecycleResult MakeResult(DocumentLifecycleStatus status, const DocumentLocation& location, bool archive)
{
	DocumentLifecycleResult result;
	result.status = status;
	result.location = location;
	result.archive = archive;
	return result;
}
}

DocumentLifecycleResult DocumentLifecycleController::Completed(const DocumentLocation& location, bool archive)
{
	return MakeResult(DocumentLifecycleStatus::Success, location, archive);
}

DocumentLifecycleResult DocumentLifecycleController::Cancelled(const DocumentLocation& location, bool archive)
{
	return MakeResult(DocumentLifecycleStatus::Cancelled, location, archive);
}

DocumentLifecycleResult DocumentLifecycleController::Failed(const DocumentLocation& location, bool archive)
{
	return MakeResult(DocumentLifecycleStatus::Failed, location, archive);
}

DocumentLifecycleController::DocumentLifecycleController(CMainFrame& frame, FB::Doc*& document, DocumentSession& session, HWND view)
	: m_frame(frame), m_document(document), m_session(session), m_view(view)
{
}

DocumentLifecycleResult DocumentLifecycleController::NewDocument()
{
	PendingDocument pending(m_frame, m_document);
	FB::Doc* document = &pending.Document();
	if (!document->CreateBlank(m_view)) {
		pending.Rollback();
		return Failed(m_session.Location(), m_session.IsArchive());
	}
	NewDocumentRuntimeProbe::Observe(false);
	// Keep the old document intact until the replacement is fully prepared.
	{
		// Script-backed tree cleanup invokes functions through m_active_doc.
		// It must operate on the discarded book, never on the new template.
		struct ActiveCleanupDocument {
			FB::Doc* replacement;
			ActiveCleanupDocument(FB::Doc* old, FB::Doc* next) : replacement(next) { FB::Doc::m_active_doc=old; }
			~ActiveCleanupDocument() { FB::Doc::m_active_doc=replacement; }
		} cleanup(m_document, document);
		m_frame.DetachDocumentForClose();
		NewDocumentRuntimeProbe::Observe(true);
	}
	FB::Doc* previous = m_document;
	m_document = pending.Commit();
	delete previous;
	m_session.NewDocument();
	return Completed(DocumentLocation(), false);
}

DocumentLifecycleResult DocumentLifecycleController::Open(const DocumentOpenSource& source)
{
	return Load(source, false);
}

DocumentLifecycleResult DocumentLifecycleController::Import(const MSXML2::IXMLDOMDocument2Ptr& dom, const CString& suggestedFilename)
{
	if (!dom) return Failed(m_session.Location(), m_session.IsArchive());
	PendingDocument pending(m_frame, m_document);
	FB::Doc& document = pending.Document();
	if (!document.LoadFromHTML(m_view, L"blank.fb2"))
		return Failed(m_session.Location(), m_session.IsArchive());
	CComDispatchDriver body(document.m_body.Script());
	CComVariant args[2], result;
	args[1] = dom.GetInterfacePtr();
	args[0] = _Settings.GetInterfaceLanguageName();
	CheckError(body.InvokeN(L"LoadFromDOM", args, 2, &result));
	if (result.vt != VT_BOOL || result.boolVal == VARIANT_FALSE || !document.m_body.Init())
		return Failed(m_session.Location(), m_session.IsArchive());
	if (!suggestedFilename.IsEmpty()) {
		document.m_filename = suggestedFilename;
		if (document.m_filename.Right(4).CompareNoCase(L".fb2") != 0) document.m_filename += L".fb2";
	}
	// The plugin supplies a Save As suggestion, not an existing file identity.
	document.m_namevalid = false;
	document.ResetSavePoint();
	// Release editor-owned references only after the replacement is ready.
	m_frame.DetachDocumentForClose();
	FB::Doc* previous = m_document;
	m_document = pending.Commit();
	delete previous;
	m_session.NewDocument();
	return Completed(DocumentLocation(), false);
}

DocumentLifecycleResult DocumentLifecycleController::CloseDocument()
{
	if (!m_session.HasDocument()) return Completed(DocumentLocation(), false);
	// Prepare the private MSHTML host before releasing any part of the book.
	// PendingDocument restores the old active document if preparation fails.
	PendingDocument pending(m_frame, m_document);
	if (!pending.Document().LoadFromHTML(m_view, L"blank.fb2")) {
		pending.Rollback();
		return Failed(m_session.Location(), m_session.IsArchive());
	}
	m_frame.DetachDocumentForClose();
	FB::Doc* previous = m_document;
	m_document = pending.Commit();
	delete previous;
	m_session.CloseDocument();
	FB::Doc::m_active_doc = nullptr;
	return Completed(DocumentLocation(), false);
}

DocumentLifecycleResult DocumentLifecycleController::ReloadNormal(const CString& path)
{
	return Load(DocumentOpenSource::Normal(path), true);
}

DocumentLifecycleResult DocumentLifecycleController::Load(const DocumentOpenSource& source, bool reload)
{
	PendingDocument pending(m_frame, m_document);
	FB::Doc* document = &pending.Document();
	const CString& path = source.location.storagePath;
	const int slash = path.ReverseFind(L'\\');
	if (slash >= 0 && slash < path.GetLength() - 1)
	{
		document->m_body.m_file_path = path.Left(slash + 1);
		document->m_body.m_file_name = path.Mid(slash + 1);
	}
	if (!DocumentLoader::Load(*document, m_view, source))
	{
		pending.Rollback();
		return Failed(source.location, source.IsArchive());
	}

	// Reopening a saved import must release speller/display interfaces while
	// the previous MSHTML host is still alive, just as Import and Close do.
	m_frame.DetachDocumentForClose();
	FB::Doc* previous = m_document;
	m_document = pending.Commit();
	delete previous;
	if (source.IsArchive()) m_session.OpenArchive(source.location);
	else if (reload) m_session.ReloadedNormal(path, m_document->GetDocumentFileType());
	else m_session.OpenNormal(path, m_document->GetDocumentFileType());
	return Completed(source.location, source.IsArchive());
}

// PendingDocument remains the single transactional boundary.  Presentation is
// deliberately left to CMainFrame after this controller commits a document.
