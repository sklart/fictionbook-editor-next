#pragma once

#include "DocumentLocation.h"
#include "../archive/ArchiveError.h"
#include <vector>

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
	static void WriteSnapshot(const std::vector<unsigned char>& bytes, const CString& path, bool overwrite, bool backup = false);
	DocumentSaveResult SaveCurrent(FB::Doc& document, DocumentSession& session, const DocumentLocation& location);
	DocumentSaveResult SaveAsNormal(FB::Doc& document, DocumentSession& session, const DocumentSaveAsRequest& request);
	DocumentSaveResult SaveCurrentSnapshot(DocumentSession& session, const std::vector<unsigned char>& bytes);
	void SaveAsSnapshot(DocumentSession& session, const CString& path, FictionBookFileType type,
		const std::vector<unsigned char>& bytes, bool overwrite);
};
