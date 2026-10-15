#pragma once
#include "HotkeyGroup.h"

namespace FbeSettings { namespace Hotkeys {
	bool SaveToFile(const std::vector<CHotkeysGroup>& groups, const CString& path);
	void Save(const std::vector<CHotkeysGroup>& groups);
	void Load(std::vector<CHotkeysGroup>& groups, int& keycodes);
	CHotkeysGroup* FindGroup(std::vector<CHotkeysGroup>& groups, const CString& name);
	CHotkey* FindHotkey(CHotkeysGroup& group, const CString& name);
} }
