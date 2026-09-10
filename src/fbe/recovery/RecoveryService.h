#pragma once

#include "RecoveryStore.h"

namespace FB { class Doc; }

namespace FbeRecovery
{
struct RestoreCandidate
{
	CString snapshotPath;
	DocumentLocation archiveLocation;
	bool archiveBacked;
};

class RecoveryService
{
public:
	RecoveryService();
	bool Save(FB::Doc* document, bool documentChanged, bool sourceActive, bool sourceXmlInvalid,
		const char* sourceText, size_t sourceTextLength, const DocumentLocation& location);
	void DeleteIfWritten();
	bool GetRestoreCandidate(bool hasCommandLineArguments, RestoreCandidate& candidate) const;
	void CompleteRestore();
	CString SnapshotPath() const;

private:
	bool SaveSourceSnapshot(const CString& filename, const char* sourceText, size_t sourceTextLength) const;
	RecoveryStore m_store;
	bool m_written;
};
}
