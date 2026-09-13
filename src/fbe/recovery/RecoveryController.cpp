#include "stdafx.h"
#include "RecoveryController.h"
#include "..\\document\\DocumentSession.h"
#include "..\\apputils.h"
#include "..\\FBDoc.h"

bool FbeRecovery::RecoveryController::Save(FB::Doc& document, const SnapshotRequest& request)
{
	return m_service.Save(&document, request.documentChanged, request.sourceActive, request.sourceXmlInvalid,
		request.sourceText, request.sourceTextLength, request.location);
}
void FbeRecovery::RecoveryController::DeleteIfWritten() { m_service.DeleteIfWritten(); }
bool FbeRecovery::RecoveryController::GetRestoreCandidate(bool hasCommandLineArguments, RestoreCandidate& candidate) const { return m_service.GetRestoreCandidate(hasCommandLineArguments, candidate); }
void FbeRecovery::RecoveryController::CompleteRestore() { m_service.CompleteRestore(); }
CString FbeRecovery::RecoveryController::SnapshotPath() const { return m_service.SnapshotPath(); }
void FbeRecovery::RecoveryController::CommitRestoredIdentity(FB::Doc& document, DocumentSession& session, const RestoreCandidate& candidate)
{
	if (candidate.archiveBacked)
	{
		session.RestoreArchive(candidate.archiveLocation);
		document.m_filename = candidate.archiveLocation.storagePath;
		document.m_namevalid = true;
		document.SetDocumentFileType(candidate.archiveLocation.documentType);
	}
	else
	{
		session.NewDocument();
		document.m_filename = L"Untitled.fb2";
		document.m_namevalid = false;
	}
}
