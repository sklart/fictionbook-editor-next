#pragma once

class CSettings;
class SettingsFileTransaction;

namespace FbeSettings { namespace Store {
	void InitializeRegistry(CRegKey& key, CString& keyPath);
	void CloseRegistry(CRegKey& key);
	void Load(CSettings& settings);
	bool Save(CSettings& settings, CString& error);
	bool PrepareSave(CSettings& settings, SettingsFileTransaction& files, CString& error);
} }
