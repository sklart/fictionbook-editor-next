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
