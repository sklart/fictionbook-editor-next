#include "stdafx.h"
#include "SettingsStore.h"
#include "SettingsPaths.h"
#include "..\Settings.h"
#include "..\..\common\DeploymentContext.h"

namespace FbeSettings { namespace Store {
void InitializeRegistry(CRegKey& key, CString& keyPath)
{
	keyPath = L"Software\\FBETeam\\FictionBook Editor Next";
	if(DeploymentContext::RegistryPersistenceAllowed()) key.Create(HKEY_CURRENT_USER, keyPath);
}
void CloseRegistry(CRegKey& key) { key.Close(); }
void Save(CSettings& settings)
{
	CXMLSerializer serializer(FbeSettings::SettingsFilePath(), L"FBE", false);
	serializer.Serialize(&settings);
}
void Load(CSettings& settings)
{
	CXMLSerializer serializer(FbeSettings::SettingsFilePath(), L"FBE", true);
	settings.SetDefaults();
	if(!serializer.Deserialize(&settings, &settings)) Save(settings);
}
} }
