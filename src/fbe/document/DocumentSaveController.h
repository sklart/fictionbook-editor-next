#pragma once

#include "DocumentLocation.h"
#include "../archive/ArchiveReader.h"

class DocumentSession;
namespace FB { class Doc; }

enum class DocumentSaveStatus { Success, Cancelled, Failed };

struct DocumentSaveResult
{
	DocumentSaveStatus status = DocumentSaveStatus::Failed;
	DocumentLocation location;
	bool archive = false;
	bool serialized = false;
	FbeArchive::Error archiveError;
	bool Succeeded() const { return status == DocumentSaveStatus::Success; }
};

class DocumentSaveController
{
public:
	DocumentSaveResult SaveCurrent(FB::Doc& document, DocumentSession& session, const DocumentLocation& location);
	DocumentSaveResult SaveAsNormal(FB::Doc& document, DocumentSession& session, const CString& filename);
};
