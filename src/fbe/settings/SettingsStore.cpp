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

void Load(CSettings& settings)
{
	CXMLSerializer serializer(FbeSettings::SettingsFilePath(), L"FBE", true);
	settings.SetDefaults();
	if(!serializer.Deserialize(&settings, &settings)) { CString error; Save(settings, error); }
}
} }
