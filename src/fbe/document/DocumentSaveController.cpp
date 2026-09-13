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
		if (!document.Save()) return { DocumentSaveStatus::Failed, location, false, false };
		session.Saved();
		return { DocumentSaveStatus::Success, location, false, false };
	}

	std::vector<unsigned char> serialized;
	if (!document.SerializeToMemory(serialized, location.documentType)) return { DocumentSaveStatus::Failed, location, true, false };
	FbeArchive::Error error;
	DocumentLocation savedLocation;
	if (!FbeArchive::SaveDocument(location, serialized, savedLocation, error)) return { DocumentSaveStatus::Failed, location, true, true };
	session.SavedArchive(savedLocation);
	return { DocumentSaveStatus::Success, savedLocation, true, true };
}

DocumentSaveResult DocumentSaveController::SaveAsNormal(FB::Doc& document, DocumentSession& session, const CString& filename)
{
	if (!document.Save(filename)) return { DocumentSaveStatus::Failed, session.Location(), false, false };
	document.m_filename = filename;
	document.m_namevalid = true;
	session.SaveAsNormal(filename, document.GetDocumentFileType());
	return { DocumentSaveStatus::Success, session.Location(), false, false };
}
