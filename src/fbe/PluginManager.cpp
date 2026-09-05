#include "stdafx.h"
#include "PluginManager.h"
#include "RuntimeLocalization.h"
#include "StartupTrace.h"
#include "apputils.h"
#include "utils\\utils.h"
#include "FBE.h"
#include "..\\common\\RuntimeLocalizationCommon.h"

namespace {
bool ReadString(const std::wstring& json, size_t object, const wchar_t* name, CString& value) {
	size_t start = 0; std::wstring result;
	if (!FbeRuntimeLocalization::JsonFindObjectMember(json, object, name, start) ||
		!FbeRuntimeLocalization::JsonParseString(json, start, result) || result.empty()) return false;
	value = result.c_str(); return true;
}
bool SafeModuleName(const CString& value) {
	return !value.IsEmpty() && value.Find(L"..") < 0 && value.FindOneOf(L"\\\\/:") < 0 && value.Right(4).CompareNoCase(L".dll") == 0;
}
bool SameGuid(const CLSID& first, const CLSID& second) { return ::InlineIsEqualGUID(first, second) != FALSE; }
bool IsRegularFile(const CString& path) {
	const DWORD attributes = ::GetFileAttributes(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
bool IsSchemaVersionOne(const std::wstring& json, size_t valueStart) {
	// JSON number grammar is deliberately not coerced: v1 is the integer token 1.
	if (valueStart >= json.size() || json[valueStart] != L'1') return false;
	size_t end = valueStart + 1;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, end);
	return end < json.size() && (json[end] == L',' || json[end] == L'}');
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
		!IsSchemaVersionOne(json, schema)) { Trace(L"unsupported-schema"); return; }
	size_t array = 0;
	if (!FbeRuntimeLocalization::JsonFindObjectMember(json, 0, L"plugins", array)) { Trace(L"manifest-invalid"); return; }
	FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
	if (array >= json.size() || json[array++] != L'[') { Trace(L"manifest-invalid"); return; }
	for (;;) {
		FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
		if (array >= json.size() || json[array] == L']') break;
		const size_t object = array;
		if (!FbeRuntimeLocalization::JsonSkipValue(json, array)) { m_plugins.clear(); Trace(L"manifest-invalid"); return; }
		PluginDescriptor entry = {};
		const bool fields = ReadString(json, object, L"id", entry.id) && ReadString(json, object, L"type", entry.type) &&
			ReadString(json, object, L"module", entry.module) && ReadString(json, object, L"clsid", entry.clsidText) &&
			ReadString(json, object, L"menu", entry.menu) && ReadString(json, object, L"menuKey", entry.menuKey) &&
			ReadString(json, object, L"activation", entry.activation);
		const bool validClsid = ::CLSIDFromString(const_cast<LPOLESTR>(static_cast<LPCWSTR>(entry.clsidText)), &entry.clsid) == S_OK;
		bool duplicate = false;
		for (size_t i = 0; validClsid && i < m_plugins.size(); ++i)
			duplicate |= m_plugins[i].id == entry.id || m_plugins[i].module.CompareNoCase(entry.module) == 0 || SameGuid(m_plugins[i].clsid, entry.clsid);
		if (!fields || (entry.type != L"Import" && entry.type != L"Export") || entry.activation != L"local-com" ||
			!SafeModuleName(entry.module) || !validClsid || duplicate) {
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
const PluginDescriptor* PluginManager::FindPlugin(const CLSID& clsid) const {
	for (size_t i = 0; i < m_plugins.size(); ++i) if (SameGuid(m_plugins[i].clsid, clsid)) return &m_plugins[i];
	return NULL;
}
HRESULT PluginManager::CreateInstance(const CLSID& clsid, IUnknownPtr& instance) {
	const PluginDescriptor* plugin = FindPlugin(clsid);
	if (plugin == NULL) { Trace(L"plugin-skipped", L"unknown-clsid"); return REGDB_E_CLASSNOTREG; }
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
	if (FAILED(result)) return result;
	Trace(L"class-factory-created", plugin->id);
	IUnknown* raw = NULL; result = factory->CreateInstance(NULL, IID_IUnknown, reinterpret_cast<void**>(&raw));
	if (SUCCEEDED(result)) { instance.Attach(raw); Trace(L"instance-created", plugin->id); }
	return result;
}
HRESULT PluginManager::NegotiateApi(const CLSID& clsid, IUnknown* instance) {
	const PluginDescriptor* plugin = FindPlugin(clsid); if (plugin == NULL || instance == NULL) return E_INVALIDARG;
	CComPtr<IFBEPluginInfo2> info; HRESULT hr = instance->QueryInterface(IID_IFBEPluginInfo2, reinterpret_cast<void**>(&info));
	if (FAILED(hr)) { Trace(hr == E_NOINTERFACE ? L"plugin-interface-missing" : L"plugin-interface-query-failed", plugin->id); return hr; }
	CComBSTR pluginId; ULONG apiVersion = 0; hr = info->GetPluginId(&pluginId); if (SUCCEEDED(hr)) hr = info->GetApiVersion(&apiVersion);
	if (FAILED(hr) || pluginId == NULL || plugin->id.Compare(static_cast<LPCWSTR>(pluginId)) != 0 || apiVersion != 2) { Trace(L"plugin-info-mismatch", plugin->id); return E_ACCESSDENIED; }
	if (plugin->type == L"Export") { CComPtr<IFBEExportPlugin2> api; hr = instance->QueryInterface(IID_IFBEExportPlugin2, reinterpret_cast<void**>(&api)); }
	else { CComPtr<IFBEImportPlugin2> api; hr = instance->QueryInterface(IID_IFBEImportPlugin2, reinterpret_cast<void**>(&api)); }
	if (FAILED(hr)) { Trace(hr == E_NOINTERFACE ? L"plugin-interface-missing" : L"plugin-interface-query-failed", plugin->id); return hr; }
	Trace(L"plugin-api-v2-detected", plugin->id); return S_OK;
}
