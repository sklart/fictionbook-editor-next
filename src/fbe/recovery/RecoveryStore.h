#pragma once

#include "..\\DocumentLocation.h"

namespace FbeRecovery
{
class RecoveryStore
{
public:
	CString SnapshotPath() const;
	CString ArchiveSidecarPath() const;
	bool HasSnapshot() const;
	void DeleteFiles() const;
	bool WriteArchiveLocation(const DocumentLocation& location) const;
	bool ReadArchiveLocation(DocumentLocation& location) const;
};
}
