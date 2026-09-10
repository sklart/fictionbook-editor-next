#include "stdafx.h"
#include "ArchiveDocumentWriter.h"

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

	DocumentLocation current;
	if (!CaptureContainerFingerprint(location.storagePath, current))
	{
		error.code = ErrorCode::OpenFailed;
		error.systemError = ::GetLastError();
		return false;
	}
	if (current.containerLastWriteTime != location.containerLastWriteTime || current.containerFileSize != location.containerFileSize)
	{
		error.code = ErrorCode::ModifiedExternally;
		return false;
	}

	Entry entry;
	entry.path = location.entryPath;
	entry.occurrence = location.entryOccurrence;
	entry.documentType = location.documentType;
	if (!RewriteZipEntry(location.storagePath, entry, serialized, error)) return false;
	CaptureContainerFingerprint(location.storagePath, location);
	return true;
}
}
