#pragma once

#include <atlctrlw.h>

namespace FbeRecentDocuments
{
class Controller
{
public:
	WTL::CRecentDocumentList& List() { return m_list; }
	const WTL::CRecentDocumentList& List() const { return m_list; }

private:
	WTL::CRecentDocumentList m_list;
};
}
