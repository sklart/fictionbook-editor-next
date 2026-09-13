#include "stdafx.h"
#include "DocumentLifecycleController.h"

namespace
{
DocumentLifecycleResult MakeResult(DocumentLifecycleStatus status, const DocumentLocation& location, bool archive)
{
	DocumentLifecycleResult result;
	result.status = status;
	result.location = location;
	result.archive = archive;
	return result;
}
}

DocumentLifecycleResult DocumentLifecycleController::Completed(const DocumentLocation& location, bool archive)
{
	return MakeResult(DocumentLifecycleStatus::Success, location, archive);
}

DocumentLifecycleResult DocumentLifecycleController::Cancelled(const DocumentLocation& location, bool archive)
{
	return MakeResult(DocumentLifecycleStatus::Cancelled, location, archive);
}

DocumentLifecycleResult DocumentLifecycleController::Failed(const DocumentLocation& location, bool archive)
{
	return MakeResult(DocumentLifecycleStatus::Failed, location, archive);
}

// Transactional creation/loading remains based on PendingDocument.  The frame
// owns MSHTML presentation; this unit owns only lifecycle result vocabulary.
