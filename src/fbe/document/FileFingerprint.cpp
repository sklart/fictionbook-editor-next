#include "stdafx.h"
#include "FileFingerprint.h"
#include "DocumentLocation.h"

bool GetFileFingerprint(const CString& path, FileFingerprint& fingerprint)
{
	WIN32_FILE_ATTRIBUTE_DATA data = {};
	if (!::GetFileAttributesEx(path, GetFileExInfoStandard, &data)) return false;
	fingerprint.lastWriteTime = *reinterpret_cast<const unsigned __int64*>(&data.ftLastWriteTime);
	fingerprint.fileSize = (static_cast<unsigned __int64>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
	return true;
}

bool SameFileFingerprint(const FileFingerprint& left, const FileFingerprint& right)
{
	return left.lastWriteTime == right.lastWriteTime && left.fileSize == right.fileSize;
}

DocumentLocation CreateNormalDocumentLocation(const CString& path, FictionBookFileType documentType)
{
	DocumentLocation location;
	location.storagePath = path;
	location.documentType = documentType;
	UpdateDocumentLocationFingerprint(location);
	return location;
}

void ResetDocumentLocation(DocumentLocation& location)
{
	location = DocumentLocation();
}

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
