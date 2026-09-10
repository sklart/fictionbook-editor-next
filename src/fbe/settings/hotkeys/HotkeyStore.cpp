#include "stdafx.h"
#include "HotkeyStore.h"
#include "..\\SettingsPaths.h"
namespace FbeSettings { namespace Hotkeys {
void Save(const std::vector<CHotkeysGroup>& groups) { CXMLSerializer serializer(FbeSettings::HotkeysFilePath(), L"FBE", false); std::vector<void*> pointers; for(size_t index = 0; index < groups.size(); ++index) pointers.push_back(const_cast<CHotkeysGroup*>(&groups[index])); serializer.Serialize(pointers); }
} }
