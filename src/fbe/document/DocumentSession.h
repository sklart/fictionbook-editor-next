#pragma once

#include "DocumentState.h"

// A lightweight lifecycle coordinator.  It deliberately does not own FB::Doc
// or any UI surface; callers commit state only after their UI-driven work has
// succeeded.
class DocumentSession
{
public:
	DocumentState& State();
	const DocumentState& State() const;
	void NewDocument();
	void OpenNormal(const CString& path, FictionBookFileType documentType);
	void OpenArchive(const DocumentLocation& location);
	void SaveAsNormal(const CString& path, FictionBookFileType documentType);
	void Saved();
	void ReloadedNormal(const CString& path, FictionBookFileType documentType);
	void RestoreArchive(const DocumentLocation& location);
	void AcceptExternalVersion();

private:
	DocumentState m_state;
};
