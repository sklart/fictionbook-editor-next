#include "stdafx.h"
#include "HotkeyStore.h"
#include "..\\SettingsPaths.h"
namespace FbeSettings { namespace Hotkeys {
void Save(const std::vector<CHotkeysGroup>& groups) { CXMLSerializer serializer(FbeSettings::HotkeysFilePath(), L"FBE", false); std::vector<void*> pointers; for(size_t index = 0; index < groups.size(); ++index) pointers.push_back(const_cast<CHotkeysGroup*>(&groups[index])); serializer.Serialize(pointers); }
CHotkeysGroup* FindGroup(std::vector<CHotkeysGroup>& groups, const CString& name)
{
	for(unsigned int i = 0; i < groups.size(); ++i) if(groups[i].m_reg_name == name) return &groups[i];
	return NULL;
}
CHotkey* FindHotkey(CHotkeysGroup& group, const CString& name)
{
	for(unsigned int i = 0; i < group.m_hotkeys.size(); ++i) if(group.m_hotkeys[i].m_reg_name == name) return &group.m_hotkeys[i];
	return NULL;
}
void Load(std::vector<CHotkeysGroup>& groups, int& keycodes)
{
	CXMLSerializer serializer(FbeSettings::HotkeysFilePath(), L"FBE", true);
	CHotkeysGroup group; std::vector<void*> objects; serializer.Deserialize(&group, objects);
	bool migratedLegacyScriptHotkey = false;
	for(unsigned int i = 0; i < objects.size(); ++i)
	{
		group = *static_cast<CHotkeysGroup*>(objects[i]);
		CHotkeysGroup* foundGroup = FindGroup(groups, group.m_reg_name);
		if(foundGroup == NULL) continue;
		for(unsigned int j = 0; j < group.m_hotkeys.size(); ++j)
		{
			CHotkey* foundHotkey = FindHotkey(*foundGroup, group.m_hotkeys[j].m_reg_name);
			if(foundHotkey == NULL && group.m_reg_name == L"Scripts")
			{
				CString legacyPath(group.m_hotkeys[j].m_reg_name); legacyPath.Replace(L'\\', L'/'); legacyPath.MakeLower();
				CHotkey* matchedHotkey = NULL; int longestSuffixLength = -1; bool ambiguousLongestSuffix = false;
				for(unsigned int candidateIndex = 0; candidateIndex < foundGroup->m_hotkeys.size(); ++candidateIndex)
				{
					CString relativePath(foundGroup->m_hotkeys[candidateIndex].m_reg_name); relativePath.Replace(L'\\', L'/'); relativePath.MakeLower();
					const bool matches = legacyPath == relativePath || (legacyPath.GetLength() > relativePath.GetLength() && legacyPath.Right(relativePath.GetLength()) == relativePath && legacyPath[legacyPath.GetLength() - relativePath.GetLength() - 1] == L'/');
					if(matches) { const int suffixLength = relativePath.GetLength(); if(suffixLength > longestSuffixLength) { longestSuffixLength = suffixLength; matchedHotkey = &foundGroup->m_hotkeys[candidateIndex]; ambiguousLongestSuffix = false; } else if(suffixLength == longestSuffixLength) ambiguousLongestSuffix = true; }
				}
				foundHotkey = ambiguousLongestSuffix ? NULL : matchedHotkey; migratedLegacyScriptHotkey |= foundHotkey != NULL;
			}
			if(foundHotkey != NULL) { foundHotkey->m_accel.fVirt = group.m_hotkeys[j].m_accel.fVirt; foundHotkey->m_accel.key = group.m_hotkeys[j].m_accel.key; }
		}
	}
	if(migratedLegacyScriptHotkey) Save(groups);
	for(unsigned int i = 0; i < groups.size(); ++i) for(unsigned int j = 0; j < groups[i].m_hotkeys.size(); ++j)
	{
		ACCEL accel = groups[i].m_hotkeys[j].m_accel;
		if(accel.fVirt != NULL && accel.key != NULL && accel.cmd != NULL) keycodes++;
		if(groups[i].m_reg_name == L"Scripts" || groups[i].m_reg_name == L"Plugins")
		{
			ACCEL defAccel = groups[i].m_hotkeys[j].m_def_accel;
			if(accel.fVirt != defAccel.fVirt || accel.key != defAccel.key) { groups[i].m_hotkeys[j].m_def_accel.fVirt = accel.fVirt; groups[i].m_hotkeys[j].m_def_accel.key = accel.key; groups[i].m_hotkeys[j].m_accel.cmd = groups[i].m_hotkeys[j].m_def_accel.cmd; }
		}
	}
}
} }
