#pragma once

#include "PluginManager.h"
#include <functional>

class PluginUiController
{
public:
	PluginManager& Manager() { return m_manager; }
	const PluginManager& Manager() const { return m_manager; }
	CSimpleArray<CLSID>& ImportPlugins() { return m_importPlugins; }
	CSimpleArray<CLSID>& ExportPlugins() { return m_exportPlugins; }
	const CSimpleArray<CLSID>& ImportPlugins() const { return m_importPlugins; }
	const CSimpleArray<CLSID>& ExportPlugins() const { return m_exportPlugins; }
	void SetLastCommand(UINT command) { m_lastCommand = command; }
	UINT LastCommand() const { return m_lastCommand; }
	void Initialize(HMENU importMenu, HMENU exportMenu,
		const std::function<CString(const PluginDescriptor&)>& menuText,
		const std::function<void(const PluginDescriptor&, UINT, const CString&)>& registerHotkey,
		const std::function<void(HICON, UINT)>& addMenuIcon);

private:
	PluginManager m_manager;
	CSimpleArray<CLSID> m_importPlugins;
	CSimpleArray<CLSID> m_exportPlugins;
	UINT m_lastCommand = 0;
	void InitializeType(HMENU menu, const TCHAR* type, UINT commandBase, CSimpleArray<CLSID>& list,
		const std::function<CString(const PluginDescriptor&)>& menuText,
		const std::function<void(const PluginDescriptor&, UINT, const CString&)>& registerHotkey,
		const std::function<void(HICON, UINT)>& addMenuIcon);
};
