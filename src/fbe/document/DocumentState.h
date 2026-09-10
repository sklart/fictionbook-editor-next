#pragma once

#include "DocumentLocation.h"

class DocumentState
{
public:
	DocumentState();
	const DocumentLocation& Location() const;
	DocumentLocation& Location();
	void NewDocument();
	void OpenNormal(const CString& path, FictionBookFileType documentType);
	void OpenArchive(const DocumentLocation& location);
	void SaveAsNormal(const CString& path, FictionBookFileType documentType);
	void Saved();
	void ReloadedNormal(const CString& path, FictionBookFileType documentType);
	void RestoreArchive(const DocumentLocation& location);
	void AcceptExternalVersion();

private:
	DocumentLocation m_location;
};
