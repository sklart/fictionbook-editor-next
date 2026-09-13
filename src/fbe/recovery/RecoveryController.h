#pragma once

#include "RecoveryService.h"

class DocumentSession;
namespace FB { class Doc; }

namespace FbeRecovery
{
struct SnapshotRequest
{
	bool documentChanged = false;
	bool sourceActive = false;
	bool sourceXmlInvalid = false;
	const char* sourceText = NULL;
	size_t sourceTextLength = 0;
	DocumentLocation location;
};

class RecoveryController
{
public:
	bool Save(FB::Doc& document, const SnapshotRequest& request);
	void DeleteIfWritten();
	bool GetRestoreCandidate(bool hasCommandLineArguments, RestoreCandidate& candidate) const;
	void CommitRestoredIdentity(FB::Doc& document, DocumentSession& session, const RestoreCandidate& candidate);
	void CompleteRestore();
	CString SnapshotPath() const;

private:
	RecoveryService m_service;
};
}
