#pragma once

#include "DocumentLocation.h"
#include "DocumentOpenSource.h"

class CMainFrame;
class DocumentSession;
namespace FB { class Doc; }

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
	DocumentLifecycleController(CMainFrame& frame, FB::Doc*& document, DocumentSession& session, HWND view);

	DocumentLifecycleResult NewDocument();
	DocumentLifecycleResult Open(const DocumentOpenSource& source);
	DocumentLifecycleResult ReloadNormal(const CString& path);

	static DocumentLifecycleResult Completed(const DocumentLocation& location, bool archive);
	static DocumentLifecycleResult Cancelled(const DocumentLocation& location, bool archive);
	static DocumentLifecycleResult Failed(const DocumentLocation& location, bool archive);

private:
	DocumentLifecycleResult Load(const DocumentOpenSource& source, bool reload);

	CMainFrame& m_frame;
	FB::Doc*& m_document;
	DocumentSession& m_session;
	HWND m_view;
};
