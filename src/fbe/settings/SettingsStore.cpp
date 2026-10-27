#include "stdafx.h"
#include "SettingsStore.h"
#include "SettingsPaths.h"
#include "SettingsFileTransaction.h"
#include "..\Settings.h"
#include "..\..\common\DeploymentContext.h"

namespace FbeSettings { namespace Store {
void InitializeRegistry(CRegKey& key, CString& keyPath)
{
	keyPath = L"Software\\FBETeam\\FictionBook Editor Next";
	if(DeploymentContext::RegistryPersistenceAllowed()) key.Create(HKEY_CURRENT_USER, keyPath);
}
void CloseRegistry(CRegKey& key) { key.Close(); }
bool PrepareSave(CSettings& settings, SettingsFileTransaction& files, CString& error)
{
    const CString path = FbeSettings::SettingsFilePath();
    const CString temporary = SettingsFileTransaction::Temporary(path);
    if(temporary.IsEmpty()) { error = U::Win32ErrMsg(::GetLastError()); return false; }
    files.Add(path, temporary);
    try {
        CXMLSerializer serializer(temporary, L"FBE", false);
        if(serializer.Serialize(&settings)) return true;
        error = U::Win32ErrMsg(::GetLastError() ? ::GetLastError() : ERROR_WRITE_FAULT);
    } catch(const _com_error& exception) { error = exception.ErrorMessage(); }
    return false;
}
bool Save(CSettings& settings, CString& error)
{
    SettingsFileTransaction files;
    if(!PrepareSave(settings, files, error)) return false;
    if(files.Commit()) return true;
    error = U::Win32ErrMsg(files.Error());
    return false;
}

int LoadFile(CSettings& settings, const CString& path)
{
	CXMLSerializer serializer(path, L"FBE", true);
	settings.SetDefaults();
	// A successfully loaded pre-layout configuration keeps the familiar view.
	// An explicit XML value overrides this fallback through SetPropertyValue.
	settings.SetDescriptionLayout(L"classic");
	const int loaded = serializer.Deserialize(&settings, &settings);
	if(loaded <= 0) settings.SetDescriptionLayout(L"modern");
	return loaded;
}
void Load(CSettings& settings)
{
	const int loaded = LoadFile(settings, FbeSettings::SettingsFilePath());
	if(!loaded) { CString error; Save(settings, error); }
}
} }
