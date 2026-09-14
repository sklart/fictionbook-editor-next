#pragma once

#include "ScriptToolbarCollection.h"

// Command-facing owner of user panel definitions; it has no HWND dependency.
class ScriptToolbarManager
{
public:
	ScriptToolbarCollection& Collection() { return m_collection; }
	const ScriptToolbarCollection& Collection() const { return m_collection; }
	ScriptToolbarDefinition& Create(const CString& name) { return m_collection.Add(name); }
	bool Delete(const CString& id) { return m_collection.Remove(id); }
	bool Rename(const CString& id, const CString& name) { return m_collection.Rename(id, name); }
	bool SetVisible(const CString& id, bool visible);
	bool Move(const CString& id, size_t destination) { return m_collection.Move(id, destination); }

private:
	ScriptToolbarCollection m_collection;
};
