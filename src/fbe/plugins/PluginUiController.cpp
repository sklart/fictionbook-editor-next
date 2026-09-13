#include "stdafx.h"
#include "..\resource.h"
#include "PluginUiController.h"

void PluginUiController::Initialize(HMENU importMenu, HMENU exportMenu,
	const std::function<CString(const PluginDescriptor&)>& menuText,
	const std::function<void(const PluginDescriptor&, UINT, const CString&)>& registerHotkey,
	const std::function<void(HICON, UINT)>& addMenuIcon)
{
	m_manager.DiscoverBundledPlugins();
	InitializeType(importMenu, L"Import", ID_IMPORT_BASE, m_importPlugins, menuText, registerHotkey, addMenuIcon);
	InitializeType(exportMenu, L"Export", ID_EXPORT_BASE, m_exportPlugins, menuText, registerHotkey, addMenuIcon);
}

void PluginUiController::InitializeType(HMENU menu, const TCHAR* type, UINT commandBase, CSimpleArray<CLSID>& list,
	const std::function<CString(const PluginDescriptor&)>& menuText,
	const std::function<void(const PluginDescriptor&, UINT, const CString&)>& registerHotkey,
	const std::function<void(HICON, UINT)>& addMenuIcon)
{
	const int capacity = static_cast<int>((commandBase == ID_IMPORT_BASE ? ID_PLUGIN_IMPORT_LAST : ID_PLUGIN_EXPORT_LAST) - commandBase + 1);
	const std::vector<PluginDescriptor>& plugins = m_manager.GetPlugins();
	for(size_t index = 0; index < plugins.size() && list.GetSize() < capacity; ++index)
	{
		const PluginDescriptor& plugin = plugins[index]; if(plugin.type != type) continue;
		const UINT command = commandBase + list.GetSize(); const CString caption = menuText(plugin);
		list.Add(plugin.clsid); ::AppendMenu(menu, MF_STRING, command, caption); registerHotkey(plugin, command, caption);
		CString icon(plugin.icon); int iconId = 0; const int comma = icon.ReverseFind(L',');
		if(comma > 0) { if(_stscanf(static_cast<LPCTSTR>(icon) + comma, L",%d", &iconId) != 1) iconId = 0; icon.Delete(comma, icon.GetLength() - comma); }
		HICON handle = NULL; if(!icon.IsEmpty() && ::ExtractIconEx(icon, iconId, NULL, &handle, 1) > 0 && handle) { addMenuIcon(handle, command); ::DestroyIcon(handle); }
	}
	if(list.GetSize() > 0) ::RemoveMenu(menu, 0, MF_BYPOSITION);
}
