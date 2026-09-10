#include "stdafx.h"
#include "ArchiveDocumentWriter.h"
#include "..\\document\\FileFingerprint.h"

namespace FbeArchive
{
bool SaveDocument(DocumentLocation& location, const std::vector<unsigned char>& serialized, Error& error)
{
	error = Error();
	if (location.containerKind != DocumentContainerKind::Zip)
	{
		error.code = ErrorCode::UnsupportedFormat;
		return false;
	}

	FileFingerprint current;
	if (!GetFileFingerprint(location.storagePath, current))
	{
		error.code = ErrorCode::OpenFailed;
		error.systemError = ::GetLastError();
		return false;
	}
	FileFingerprint expected;
	expected.lastWriteTime = location.containerLastWriteTime;
	expected.fileSize = location.containerFileSize;
	if (!SameFileFingerprint(current, expected))
	{
		error.code = ErrorCode::ModifiedExternally;
		return false;
	}

	Entry entry;
	entry.path = location.entryPath;
	entry.occurrence = location.entryOccurrence;
	entry.documentType = location.documentType;
	if (!RewriteZipEntry(location.storagePath, entry, serialized, error)) return false;
	if (GetFileFingerprint(location.storagePath, current))
	{
		location.containerLastWriteTime = current.lastWriteTime;
		location.containerFileSize = current.fileSize;
	}
	return true;
}
}
