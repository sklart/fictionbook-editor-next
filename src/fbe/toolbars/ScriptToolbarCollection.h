#pragma once

#include "PortableToolbarLayout.h"

// Persistent toolbar definitions, deliberately independent from HWND/WTL.
class ScriptToolbarCollection
{
public:
	ScriptToolbarCollection();
	const std::vector<ScriptToolbarDefinition>& Items() const { return m_items; }
	std::vector<ScriptToolbarDefinition>& Items() { return m_items; }
	ScriptToolbarDefinition* Find(const CString& id);
	const ScriptToolbarDefinition* Find(const CString& id) const;
	ScriptToolbarDefinition& Add(const CString& name);
	bool Remove(const CString& id);
	bool Rename(const CString& id, const CString& name);
	bool Move(const CString& id, size_t destination);
	void EnsureMain();

private:
	CString NextId();
	std::vector<ScriptToolbarDefinition> m_items;
};
