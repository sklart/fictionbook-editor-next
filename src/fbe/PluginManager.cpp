#include "stdafx.h"
#include "PluginManager.h"
#include "RuntimeLocalization.h"
#include "StartupTrace.h"
#include "..\\common\\RuntimeLocalizationCommon.h"

namespace {
bool ReadString(const std::wstring& json, size_t object, const wchar_t* name, CString& value) {
	size_t start = 0; std::wstring result;
	if (!FbeRuntimeLocalization::JsonFindObjectMember(json, object, name, start) ||
		!FbeRuntimeLocalization::JsonParseString(json, start, result) || result.empty()) return false;
	value = result.c_str(); return true;
}
bool SafeModuleName(const CString& value) {
	return !value.IsEmpty() && value.Find(L"..") < 0 && value.FindOneOf(L"\\\\/:") < 0;
}
bool IsRegularFile(const CString& path) {
	const DWORD attributes = ::GetFileAttributes(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
}

PluginManager::PluginManager() {}
PluginManager::~PluginManager() {
	for (std::map<CString, HMODULE>::iterator it = m_modules.begin(); it != m_modules.end(); ++it)
		if (it->second != NULL) ::FreeLibrary(it->second);
}
void PluginManager::Trace(const wchar_t* event, const CString& detail) const {
	if (!StartupTrace::Enabled()) return;
	CString message(event); if (!detail.IsEmpty()) { message += L"; "; message += detail; }
	StartupTrace::Event(L"plugin", L"P200", message);
}
void PluginManager::DiscoverBundledPlugins() {
	m_plugins.clear();
	std::wstring json; const CString manifest = U::GetProgDirFile(L"Plugins\\plugins.json");
	if (!FbeRuntimeLocalization::ReadUtf8TextFile(manifest, json)) { Trace(L"manifest-invalid"); return; }
	size_t schema = 0; FbeRuntimeLocalization::JsonSkipWhitespace(json, schema);
	if (!FbeRuntimeLocalization::JsonFindObjectMember(json, schema, L"schemaVersion", schema) ||
		schema >= json.size() || json[schema] != L'1' ||
		(schema + 1 < json.size() && json[schema + 1] >= L'0' && json[schema + 1] <= L'9')) { Trace(L"unsupported-schema"); return; }
	size_t array = 0;
	if (!FbeRuntimeLocalization::JsonFindObjectMember(json, 0, L"plugins", array)) { Trace(L"manifest-invalid"); return; }
	FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
	if (array >= json.size() || json[array++] != L'[') { Trace(L"manifest-invalid"); return; }
	for (;;) {
		FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
		if (array >= json.size() || json[array] == L']') break;
		const size_t object = array;
		if (!FbeRuntimeLocalization::JsonSkipValue(json, array)) { m_plugins.clear(); Trace(L"manifest-invalid"); return; }
		PluginDescriptor entry = {}; entry.source = PluginSource::Bundled;
		const bool fields = ReadString(json, object, L"id", entry.id) && ReadString(json, object, L"type", entry.type) &&
			ReadString(json, object, L"module", entry.module) && ReadString(json, object, L"clsid", entry.clsidText) &&
			ReadString(json, object, L"menu", entry.menu) && ReadString(json, object, L"menuKey", entry.menuKey) &&
			ReadString(json, object, L"activation", entry.activation);
		bool duplicate = false;
		for (size_t i = 0; i < m_plugins.size(); ++i) duplicate |= m_plugins[i].id == entry.id || m_plugins[i].module == entry.module || m_plugins[i].clsidText == entry.clsidText;
		if (!fields || (entry.type != L"Import" && entry.type != L"Export") || entry.activation != L"local-com" ||
			!SafeModuleName(entry.module) || ::CLSIDFromString(const_cast<LPOLESTR>(static_cast<LPCWSTR>(entry.clsidText)), &entry.clsid) != S_OK || duplicate) {
			Trace(duplicate ? L"duplicate-plugin-skipped" : L"plugin-skipped", entry.id); }
		else {
			entry.modulePath = U::GetProgDirFile(L"Plugins\\") + entry.module;
			if (!IsRegularFile(entry.modulePath)) Trace(L"module-missing", entry.module);
			else { m_plugins.push_back(entry); Trace(L"plugin-discovered", entry.id); }
		}
		FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
		if (array < json.size() && json[array] == L',') { ++array; continue; }
		if (array < json.size() && json[array] == L']') break;
		m_plugins.clear(); Trace(L"manifest-invalid"); return;
	}
	Trace(L"manifest-loaded");
}
const PluginDescriptor* PluginManager::FindBundledPlugin(const CLSID& clsid) const {
	for (size_t i = 0; i < m_plugins.size(); ++i) if (::InlineIsEqualGUID(m_plugins[i].clsid, clsid)) return &m_plugins[i];
	return NULL;
}
HRESULT PluginManager::CreateInstance(const CLSID& clsid, IUnknownPtr& instance) {
	const PluginDescriptor* plugin = FindBundledPlugin(clsid);
	if (plugin == NULL) return instance.CreateInstance(clsid);
	HMODULE module = NULL; std::map<CString, HMODULE>::iterator cached = m_modules.find(plugin->modulePath);
	if (cached != m_modules.end()) module = cached->second;
	else {
		module = ::LoadLibraryEx(plugin->modulePath, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
		if (module == NULL && ::GetLastError() == ERROR_INVALID_PARAMETER) module = ::LoadLibraryEx(plugin->modulePath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (module == NULL) return HRESULT_FROM_WIN32(::GetLastError());
		m_modules[plugin->modulePath] = module; Trace(L"module-loaded", plugin->module);
	}
	typedef HRESULT (STDAPICALLTYPE* GetClassObject)(REFCLSID, REFIID, LPVOID*);
	GetClassObject getClassObject = reinterpret_cast<GetClassObject>(::GetProcAddress(module, "DllGetClassObject"));
	if (getClassObject == NULL) return E_NOINTERFACE;
	CComPtr<IClassFactory> factory; HRESULT result = getClassObject(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory));
	if (FAILED(result)) return result; Trace(L"class-factory-created", plugin->id);
	IUnknown* raw = NULL; result = factory->CreateInstance(NULL, IID_IUnknown, reinterpret_cast<void**>(&raw));
	if (SUCCEEDED(result)) { instance.Attach(raw); Trace(L"instance-created", plugin->id); }
	return result;
}
