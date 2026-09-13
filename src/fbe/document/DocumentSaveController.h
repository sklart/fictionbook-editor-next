#pragma once

#include "DocumentLocation.h"
#include "../archive/ArchiveError.h"

class DocumentSession;
namespace FB { class Doc; }

enum class DocumentSaveStatus { Success, Failed };
enum class DocumentSaveFailureKind { None, Serialization, NormalWrite, ArchiveWrite };

struct DocumentSaveResult
{
	DocumentSaveStatus status = DocumentSaveStatus::Failed;
	DocumentSaveFailureKind failure = DocumentSaveFailureKind::None;
	DocumentLocation location;
	FbeArchive::Error archiveError;
	bool Succeeded() const { return status == DocumentSaveStatus::Success; }
};

struct DocumentSaveAsRequest
{
	CString filename;
	CString encoding;
};

class DocumentSaveController
{
public:
	DocumentSaveResult SaveCurrent(FB::Doc& document, DocumentSession& session, const DocumentLocation& location);
	DocumentSaveResult SaveAsNormal(FB::Doc& document, DocumentSession& session, const DocumentSaveAsRequest& request);
};
