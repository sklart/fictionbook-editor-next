#include "stdafx.h"
#include "HotkeyStore.h"
#include "..\\SettingsPaths.h"
#include "../../utils/utils.h"
namespace FbeSettings { namespace Hotkeys {
namespace {
// Deserialize transfers the groups and their nested heap objects to its caller.
// Keep this separate from CHotkeysGroup: live/saved groups also contain borrowed
// pointers into m_hotkeys and must never delete those pointers.
struct DeserializedGroups {
	std::vector<void*> objects;
	~DeserializedGroups() {
		for(void* object : objects) {
			auto* group = static_cast<CHotkeysGroup*>(object);
			for(void* hotkey : group->m_ptr_hotkeys) delete static_cast<CHotkey*>(hotkey);
			delete group;
		}
	}
	DeserializedGroups() = default;
	DeserializedGroups(const DeserializedGroups&) = delete;
	DeserializedGroups& operator=(const DeserializedGroups&) = delete;
};
}
CString CommandTooltip(const std::vector<CHotkeysGroup>& groups, UINT command, const CString& text)
{
	for(const auto& group : groups)
		for(const auto& hotkey : group.m_hotkeys)
			if(hotkey.m_accel.cmd == command) {
				const CString shortcut = U::AccelToString(hotkey.m_accel);
				return shortcut.IsEmpty() ? text : text + L" (" + shortcut + L")";
			}
	return text;
}
bool SaveToFile(const std::vector<CHotkeysGroup>& groups, const CString& path)
{
    try {
        std::vector<CHotkeysGroup> snapshot = groups;
        // A temporarily missing user script must not lose its UID assignment
        // when another shortcut is saved. Keep it on disk, outside the live UI.
        const CString existingPath = FbeSettings::HotkeysFilePath();
        if(::GetFileAttributesW(existingPath) != INVALID_FILE_ATTRIBUTES) {
            CXMLSerializer existing(existingPath, L"FBE", true);
            CHotkeysGroup factory; DeserializedGroups stored;
            existing.Deserialize(&factory, stored.objects);
            CHotkeysGroup* scripts = FindGroup(snapshot, L"Scripts");
            for(void* object : stored.objects) {
                const auto* old = static_cast<CHotkeysGroup*>(object);
                if(scripts && old->m_reg_name == L"Scripts")
                    for(const auto& hotkey : old->m_hotkeys)
                        if(hotkey.m_reg_name.Left(7) == L"script:" && !FindHotkey(*scripts, hotkey.m_reg_name))
                            scripts->m_hotkeys.push_back(hotkey);
            }
        }
        CXMLSerializer serializer(path, L"FBE", false);
        std::vector<void*> pointers;
        for(auto& group : snapshot) pointers.push_back(&group);
        return serializer.Serialize(pointers);
    } catch(const _com_error&) { return false; }
}
void Save(const std::vector<CHotkeysGroup>& groups) { SaveToFile(groups, FbeSettings::HotkeysFilePath()); }

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
	CHotkeysGroup factory; DeserializedGroups owned;
	serializer.Deserialize(&factory, owned.objects);
	const auto& objects = owned.objects;
	bool migratedLegacyScriptHotkey = false;
	for(unsigned int i = 0; i < objects.size(); ++i)
	{
		const CHotkeysGroup& group = *static_cast<CHotkeysGroup*>(objects[i]);
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
				CString relativePath(foundGroup->m_hotkeys[candidateIndex].m_reg_name);
				// Current script names are script:{UID}; their description preserves
				// the legacy relative path for this one-time safe migration.
				if(relativePath.Left(7) == L"script:") relativePath = foundGroup->m_hotkeys[candidateIndex].m_desc;
				relativePath.Replace(L'\\', L'/'); relativePath.MakeLower();
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
		if((groups[i].m_reg_name == L"Scripts" || groups[i].m_reg_name == L"Plugins") &&
			groups[i].m_hotkeys[j].m_reg_name != L"LastScript" && groups[i].m_hotkeys[j].m_reg_name != L"LastPlugin")
		{
			ACCEL defAccel = groups[i].m_hotkeys[j].m_def_accel;
			if(accel.fVirt != defAccel.fVirt || accel.key != defAccel.key) { groups[i].m_hotkeys[j].m_def_accel.fVirt = accel.fVirt; groups[i].m_hotkeys[j].m_def_accel.key = accel.key; groups[i].m_hotkeys[j].m_accel.cmd = groups[i].m_hotkeys[j].m_def_accel.cmd; }
		}
	}
}
} }
