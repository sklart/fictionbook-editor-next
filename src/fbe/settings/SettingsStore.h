#pragma once

class CSettings;

namespace FbeSettings { namespace Store {
	void InitializeRegistry(CRegKey& key, CString& keyPath);
	void CloseRegistry(CRegKey& key);
	void Load(CSettings& settings);
	void Save(CSettings& settings);
} }
