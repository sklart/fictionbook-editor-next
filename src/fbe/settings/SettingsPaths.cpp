#include "stdafx.h"
#include "SettingsPaths.h"
#include "..\\utils\\utils.h"

namespace FbeSettings
{
CString SettingsFilePath() { return U::GetSettingsDir() + L"Settings.xml"; }
CString HotkeysFilePath() { return U::GetSettingsDir() + L"Hotkeys.xml"; }
CString WordsFilePath() { return U::GetUserDataFile(L"Words.xml", CString(), U::GetBuiltInResourceFile(L"Words.xml")); }
}
