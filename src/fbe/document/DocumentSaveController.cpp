#include "stdafx.h"
#include "DocumentSaveController.h"
#include "DocumentSession.h"
#include "..\\apputils.h"
#include "..\\FBDoc.h"
#include "../archive/ArchiveDocumentWriter.h"

DocumentSaveResult DocumentSaveController::SaveCurrent(FB::Doc& document, DocumentSession& session, const DocumentLocation& location)
{
	if (!location.IsArchive())
	{
		if (!document.Save()) return { DocumentSaveStatus::Failed, DocumentSaveFailureKind::NormalWrite, location, FbeArchive::Error() };
		session.Saved();
		return { DocumentSaveStatus::Success, DocumentSaveFailureKind::None, location, FbeArchive::Error() };
	}

	std::vector<unsigned char> serialized;
	if (!document.SerializeToMemory(serialized, location.documentType)) return { DocumentSaveStatus::Failed, DocumentSaveFailureKind::Serialization, location, FbeArchive::Error() };
	FbeArchive::Error error;
	DocumentLocation savedLocation;
	if (!FbeArchive::SaveDocument(location, serialized, savedLocation, error)) return { DocumentSaveStatus::Failed, DocumentSaveFailureKind::ArchiveWrite, location, error };
	session.SavedArchive(savedLocation);
	return { DocumentSaveStatus::Success, DocumentSaveFailureKind::None, savedLocation, FbeArchive::Error() };
}

DocumentSaveResult DocumentSaveController::SaveAsNormal(FB::Doc& document, DocumentSession& session, const CString& filename)
{
	if (!document.Save(filename)) return { DocumentSaveStatus::Failed, DocumentSaveFailureKind::NormalWrite, session.Location(), FbeArchive::Error() };
	document.m_filename = filename;
	document.m_namevalid = true;
	session.SaveAsNormal(filename, document.GetDocumentFileType());
	return { DocumentSaveStatus::Success, DocumentSaveFailureKind::None, session.Location(), FbeArchive::Error() };
}
