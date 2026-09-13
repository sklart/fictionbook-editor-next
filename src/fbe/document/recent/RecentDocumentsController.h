#pragma once

#include <atlctrlw.h>
#include "../DocumentLocation.h"

namespace FbeRecentDocuments
{
class Controller
{
public:
	WTL::CRecentDocumentList& List() { return m_list; }
	const WTL::CRecentDocumentList& List() const { return m_list; }
	bool Resolve(WORD command, CString& normalPath, DocumentLocation& archiveLocation, bool& archive);
	void OnOpened(WORD command, const CString& normalPath, const DocumentLocation& archiveLocation, bool archive);
	void OnFailed(WORD command);
	void OnCancelledArchive(const DocumentLocation& archiveLocation);

private:
	WTL::CRecentDocumentList m_list;
};
}
