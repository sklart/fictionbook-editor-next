#pragma once

#include "PluginManager.h"

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

private:
	PluginManager m_manager;
	CSimpleArray<CLSID> m_importPlugins;
	CSimpleArray<CLSID> m_exportPlugins;
	UINT m_lastCommand = 0;
};
