#pragma once

#include "DocumentLocation.h"

enum class DocumentLifecycleStatus
{
	Success,
	Cancelled,
	Failed
};

// Lightweight result exchanged between transactional document work and frame presentation.
struct DocumentLifecycleResult
{
	DocumentLifecycleStatus status = DocumentLifecycleStatus::Failed;
	DocumentLocation location;
	bool archive = false;

	bool Succeeded() const { return status == DocumentLifecycleStatus::Success; }
};

// Converts the legacy frame status at the boundary to the lifecycle result
// shared by document and recent-document owners.
class DocumentLifecycleController
{
public:
	static DocumentLifecycleResult Completed(const DocumentLocation& location, bool archive);
	static DocumentLifecycleResult Cancelled(const DocumentLocation& location, bool archive);
	static DocumentLifecycleResult Failed(const DocumentLocation& location, bool archive);
};
