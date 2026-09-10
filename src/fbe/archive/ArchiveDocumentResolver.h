#pragma once

#include "ArchiveReader.h"

namespace FbeArchive
{
struct ResolvedDocument
{
	DocumentLocation location;
	std::vector<unsigned char> rawBytes;
};

bool CaptureContainerFingerprint(const CString& path, DocumentLocation& location);
bool ResolveDocument(const CString& storagePath, const Entry& entry, ResolvedDocument& resolved, Error& failure);
}
