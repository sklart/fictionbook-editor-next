#include "stdafx.h"
#include "ScriptMenuBuilder.h"
#include "ScriptCommandRegistry.h"
#include "..\\resource.h"

namespace FbeScripts
{
void MenuBuilder::Clear()
{
	m_items.clear();
	m_visuals.clear();
	m_nextFolderCommand = 0;
}

void MenuBuilder::Add(const ScriptDescriptor& descriptor, VisualResource&& visual)
{
	m_items.push_back(descriptor);
	m_visuals.push_back(static_cast<VisualResource&&>(visual));
}

bool MenuBuilder::AssignCommandIds(int capacity, const CString& serialized, CString& updatedSerialized)
{
	CommandRegistry registry(capacity, serialized);
	for (int index = 0; index < Count(); ++index)
	{
		ScriptDescriptor& script = Item(index);
		script.commandId = script.isFolder || script.relativePath.IsEmpty() ? -1 : registry.Assign(script.relativePath);
	}
	if (!registry.IsDirty()) return false;
	updatedSerialized = registry.Serialize();
	return true;
}

void MenuBuilder::Build(HMENU parentMenu,
	const std::function<void(ScriptDescriptor&)>& initializeHotkey,
	const std::function<void(const ScriptDescriptor&, const VisualResource&, UINT)>& addVisual)
{
	ResetFolderCommands();
	BuildSubMenu(parentMenu, L"0", initializeHotkey, addVisual);
}

void MenuBuilder::BuildSubMenu(HMENU parentMenu, const CString& parentId,
	const std::function<void(ScriptDescriptor&)>& initializeHotkey,
	const std::function<void(const ScriptDescriptor&, const VisualResource&, UINT)>& addVisual)
{
	int menuPosition = 0;
	for (int index = 0; index < Count(); ++index)
		if (Item(index).parentId == parentId) ++menuPosition;
	for (int index = 0; index < Count(); ++index)
	{
		ScriptDescriptor& script = Item(index);
		if (script.parentId != parentId) continue;
		MENUITEMINFO item = {};
		item.cbSize = sizeof(item);
		item.fMask = MIIM_TYPE | MIIM_STATE;
		item.fType = MFT_STRING;
		if (script.isFolder)
		{
			item.fMask |= MIIM_SUBMENU | MIIM_ID;
			item.hSubMenu = ::CreateMenu();
			item.wID = NextFolderCommand();
			script.commandId = -1;
			BuildSubMenu(item.hSubMenu, script.id, initializeHotkey, addVisual);
		}
		else
		{
			if (script.commandId < 1) continue;
			item.fMask |= MIIM_ID;
			item.wID = ID_SCRIPT_BASE + script.commandId;
			initializeHotkey(script);
		}
		item.dwTypeData = script.name.GetBuffer();
		item.cch = wcslen(script.name);
		if (script.isFolder) ::InsertMenuItem(parentMenu, 0, TRUE, &item);
		else ::InsertMenuItem(parentMenu, menuPosition--, TRUE, &item);
		if (!script.isFolder || item.wID != 0) addVisual(script, VisualAt(index), item.wID);
	}
}
}
