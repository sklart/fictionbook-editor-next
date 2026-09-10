#include "stdafx.h"
#include "ArchiveDocumentResolver.h"

namespace FbeArchive
{
bool CaptureContainerFingerprint(const CString& path, DocumentLocation& location)
{
	WIN32_FILE_ATTRIBUTE_DATA data = {};
	if (!::GetFileAttributesEx(path, GetFileExInfoStandard, &data)) return false;
	location.containerLastWriteTime = *reinterpret_cast<const unsigned __int64*>(&data.ftLastWriteTime);
	location.containerFileSize = (static_cast<unsigned __int64>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
	return true;
}

bool ResolveDocument(const CString& storagePath, const Entry& entry, ResolvedDocument& resolved, Error& failure)
{
	if (!ReadEntry(storagePath, entry, resolved.rawBytes, failure)) return false;
	resolved.location = DocumentLocation();
	resolved.location.containerKind = DetectDocumentContainerKind(storagePath);
	resolved.location.storagePath = storagePath;
	resolved.location.entryPath = entry.path;
	resolved.location.entryOccurrence = entry.occurrence;
	resolved.location.documentType = DetectFictionBookFileType(entry.path);
	CaptureContainerFingerprint(storagePath, resolved.location);
	return true;
}
}
