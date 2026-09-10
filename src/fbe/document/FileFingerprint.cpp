#include "stdafx.h"
#include "FileFingerprint.h"

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
