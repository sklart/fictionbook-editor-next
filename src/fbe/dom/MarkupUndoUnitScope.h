#pragma once

#include <mshtml.h>

namespace FbeDom {

// IMarkupServices does not close an undo unit when a DOM operation throws.
// Keep the scope independent from CFBEView so structural DOM editors can use
// the same exception-safe pairing.
class MarkupUndoUnitScope
{
public:
	MarkupUndoUnitScope(MSHTML::IMarkupServices2Ptr services, const wchar_t* name)
		: m_services(services), m_active(true)
	{
		m_services->BeginUndoUnit(const_cast<wchar_t*>(name));
	}

	~MarkupUndoUnitScope()
	{
		if (m_active) {
			try { m_services->EndUndoUnit(); }
			catch (_com_error&) { }
		}
	}

	void Close()
	{
		if (!m_active) return;
		m_services->EndUndoUnit();
		m_active = false;
	}

private:
	MSHTML::IMarkupServices2Ptr m_services;
	bool m_active;
};

} // namespace FbeDom
