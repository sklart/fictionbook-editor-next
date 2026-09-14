#pragma once

#include "PortableToolbarLayout.h"

// UI-side companion for ScriptToolbarDefinition. Persistence never stores HWND.
struct ScriptToolbarRuntime
{
	ScriptToolbarDefinition definition;
	HWND window;
	UINT rebarBandId;
	ScriptToolbarRuntime() : window(NULL), rebarBandId(0) {}
};

class ScriptToolbarRuntimeCollection
{
public:
	void Reset();
	ScriptToolbarRuntime& Add(const ScriptToolbarDefinition& definition);
	ScriptToolbarRuntime* Find(const CString& id);
	const std::vector<ScriptToolbarRuntime>& Items() const { return m_items; }
	std::vector<ScriptToolbarRuntime>& Items() { return m_items; }

private:
	std::vector<ScriptToolbarRuntime> m_items;
};
