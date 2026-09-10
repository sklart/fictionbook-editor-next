#include "stdafx.h"
#include "ArchiveDocumentResolver.h"
#include "..\\document\\FileFingerprint.h"

namespace FbeArchive
{
bool CaptureContainerFingerprint(const CString& path, DocumentLocation& location)
{
	FileFingerprint fingerprint;
	if (!GetFileFingerprint(path, fingerprint)) return false;
	location.containerLastWriteTime = fingerprint.lastWriteTime;
	location.containerFileSize = fingerprint.fileSize;
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
