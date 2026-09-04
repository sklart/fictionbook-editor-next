#pragma once

#include <vector>
#include <map>

enum class PluginSource { Bundled, LegacyRegistry };

struct PluginDescriptor
{
	CString id, type, module, modulePath, clsidText, menu, menuKey, activation;
	CLSID clsid;
	PluginSource source;
};

// Owns the bundled manifest and its modules for the lifetime of FBE.  Legacy
// registrations intentionally remain COM activated and are added by the UI.
class PluginManager
{
public:
	PluginManager();
	~PluginManager();
	void DiscoverBundledPlugins();
	const std::vector<PluginDescriptor>& GetPlugins() const { return m_plugins; }
	const PluginDescriptor* FindBundledPlugin(const CLSID& clsid) const;
	HRESULT CreateInstance(const CLSID& clsid, IUnknownPtr& instance);

private:
	std::vector<PluginDescriptor> m_plugins;
	std::map<CString, HMODULE> m_modules;
	void Trace(const wchar_t* event, const CString& detail = CString()) const;
};
