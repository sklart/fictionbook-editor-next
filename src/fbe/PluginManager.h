#pragma once

#include <vector>
#include <map>

struct PluginDescriptor
{
	CString id, type, module, modulePath, clsidText, menu, menuKey, activation, icon;
	CLSID clsid;
};

enum PluginApiGeneration { PluginApiV1Fallback, PluginApiV2Detected };

// Owns the bundled manifest and its locally loaded modules for FBE's lifetime.
class PluginManager
{
public:
	PluginManager();
	~PluginManager();
	void DiscoverBundledPlugins();
	const std::vector<PluginDescriptor>& GetPlugins() const { return m_plugins; }
	const PluginDescriptor* FindPlugin(const CLSID& clsid) const;
	HRESULT CreateInstance(const CLSID& clsid, IUnknownPtr& instance);
	HRESULT NegotiateApi(const CLSID& clsid, IUnknown* instance, PluginApiGeneration& generation);

private:
	std::vector<PluginDescriptor> m_plugins;
	std::map<CString, HMODULE> m_modules;
	void Trace(const wchar_t* event, const CString& detail = CString()) const;
};
