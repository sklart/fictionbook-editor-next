#pragma once

#include "ScriptDescriptor.h"
#include "ScriptVisualResources.h"
#include <vector>
#include <functional>

namespace FbeScripts
{
class MenuBuilder
{
public:
	MenuBuilder(UINT folderCommandBase, UINT folderCommandCount)
		: m_folderCommandBase(folderCommandBase), m_folderCommandCount(folderCommandCount), m_nextFolderCommand(0) {}
	void Clear();
	void Add(const ScriptDescriptor& descriptor, VisualResource&& visual);
	bool AssignCommandIds(int capacity, const CString& serialized, CString& updatedSerialized);
	void Build(HMENU parentMenu,
		const std::function<void(ScriptDescriptor&)>& initializeHotkey,
		const std::function<void(const ScriptDescriptor&, const VisualResource&, UINT)>& addVisual);
	int Count() const { return static_cast<int>(m_items.size()); }
	ScriptDescriptor& Item(int index) { return m_items[index]; }
	const ScriptDescriptor& Item(int index) const { return m_items[index]; }
	const VisualResource& VisualAt(int index) const { return m_visuals[index]; }
	void ResetFolderCommands() { m_nextFolderCommand = 0; }
	UINT NextFolderCommand() { return m_nextFolderCommand < m_folderCommandCount ? m_folderCommandBase + m_nextFolderCommand++ : 0; }

private:
	UINT m_folderCommandBase;
	UINT m_folderCommandCount;
	UINT m_nextFolderCommand;
	std::vector<ScriptDescriptor> m_items;
	std::vector<VisualResource> m_visuals;
	void BuildSubMenu(HMENU parentMenu, const CString& parentId,
		const std::function<void(ScriptDescriptor&)>& initializeHotkey,
		const std::function<void(const ScriptDescriptor&, const VisualResource&, UINT)>& addVisual);
};
}
