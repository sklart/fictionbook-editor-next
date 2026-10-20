#pragma once

#include "ScriptRegistry.h"
#include "../toolbars/PortableToolbarStore.h"
#include "../toolbars/ScriptToolbarItemText.h"

namespace FbeScripts
{
// Script metadata is resolved above the toolbar storage boundary. The raw
// store remains usable by registry recovery without a dependency cycle.
namespace ScriptToolbarPersistence
{
namespace Detail
{
inline void DescribeItems(std::vector<PortableToolbarItem>& items, const FbeScripts::ScriptRegistry& registry)
{
	for(auto& item : items)
	{
		if(item.separator || item.scriptUid.IsEmpty()) continue;
		const FbeScripts::ScriptIdentity* identity = registry.FindByUid(item.scriptUid);
		if(identity == NULL) continue; // Сохраняем последний известный снимок.
		item.relativePath = identity->relativePath;
		if(!identity->name.IsEmpty()) item.scriptName = identity->name;
		if(item.scriptName.IsEmpty()) item.scriptName = ScriptToolbarItemText::Name(item);
	}
}

inline void DescribeLayout(PortableToolbarLayout& layout, const FbeScripts::ScriptRegistry& registry)
{
	DescribeItems(layout.commands, registry); DescribeItems(layout.scripts, registry);
	for(auto& toolbar : layout.scriptToolbars) DescribeItems(toolbar.items, registry);
}

}

inline bool Load(PortableToolbarLayout& layout)
{
	if(!PortableToolbarStore::Load(layout)) return false;
	ScriptRegistry registry;
	if(registry.Load()) Detail::DescribeLayout(layout, registry);
	return true;
}

inline bool Save(const PortableToolbarLayout& layout)
{
	ScriptRegistry registry;
	if(!registry.Load()) return false;
	PortableToolbarLayout described(layout);
	Detail::DescribeLayout(described, registry);
	return PortableToolbarStore::Save(described);
}
}
}
