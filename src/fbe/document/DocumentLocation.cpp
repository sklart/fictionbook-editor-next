#include "stdafx.h"
#include "DocumentLocation.h"

DocumentLocation CreateNormalDocumentLocation(const CString& path, FictionBookFileType documentType)
{
	DocumentLocation location;
	location.storagePath = path;
	location.documentType = documentType;
	UpdateDocumentLocationFingerprint(location);
	return location;
}

void ResetDocumentLocation(DocumentLocation& location) { location = DocumentLocation(); }

bool UpdateDocumentLocationFingerprint(DocumentLocation& location)
{
	FileFingerprint fingerprint;
	if (!GetFileFingerprint(location.storagePath, fingerprint)) return false;
	location.containerLastWriteTime = fingerprint.lastWriteTime;
	location.containerFileSize = fingerprint.fileSize;
	return true;
}

bool IsDocumentLocationModified(const DocumentLocation& location)
{
	FileFingerprint current, expected;
	expected.lastWriteTime = location.containerLastWriteTime;
	expected.fileSize = location.containerFileSize;
	return !GetFileFingerprint(location.storagePath, current) || !SameFileFingerprint(current, expected);
}
