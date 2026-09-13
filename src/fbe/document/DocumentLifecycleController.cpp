#include "stdafx.h"
#include "DocumentLifecycleController.h"
#include "DocumentLoader.h"
#include "DocumentSession.h"
#include "PendingDocument.h"
#include "..\\apputils.h"
#include "..\\FBDoc.h"

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
	document->CreateBlank(m_view);
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
