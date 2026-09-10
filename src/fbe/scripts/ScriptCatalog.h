#pragma once

#include "ScriptDescriptor.h"
#include <vector>

namespace FbeScripts
{
class Catalog
{
public:
	void Discover(const CString& root, const CString& mask);
	const std::vector<ScriptDescriptor>& Items() const { return m_items; }

private:
	void DiscoverFolder(const CString& root, const CString& path, const CString& mask, const CString& parentId);
	std::vector<ScriptDescriptor> m_items;
};
}
