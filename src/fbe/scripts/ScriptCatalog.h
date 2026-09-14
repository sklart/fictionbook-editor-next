#pragma once

#include "ScriptDescriptor.h"
#include "ScriptRegistry.h"
#include <vector>

namespace FbeScripts
{
class Catalog
{
public:
	bool Discover(const CString& root, const CString& mask, ScriptRegistry* registry = NULL);
	const std::vector<ScriptDescriptor>& Items() const { return m_items; }

private:
	void DiscoverFolder(const CString& root, const CString& path, const CString& mask, const CString& parentId);
	std::vector<ScriptDescriptor> m_items;
};
}
