#pragma once

#include <atlstr.h>

struct FileFingerprint
{
	unsigned __int64 lastWriteTime;
	unsigned __int64 fileSize;

	FileFingerprint() : lastWriteTime(static_cast<unsigned __int64>(-1)), fileSize(static_cast<unsigned __int64>(-1)) {}
};

bool GetFileFingerprint(const CString& path, FileFingerprint& fingerprint);
bool SameFileFingerprint(const FileFingerprint& left, const FileFingerprint& right);
