#pragma once

#include "..\\document\\DocumentLocation.h"

namespace FbeRecovery
{
class RecoveryStore
{
public:
	CString SnapshotPath() const;
	CString ArchiveSidecarPath() const;
	bool HasSnapshot() const;
	void DeleteFiles() const;
	bool InvalidateArchiveLocation() const;
	bool WriteArchiveLocation(const DocumentLocation& location) const;
	bool ReadArchiveLocation(DocumentLocation& location) const;
};
}
