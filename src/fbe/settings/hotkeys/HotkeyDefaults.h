#pragma once

#include "HotkeyGroup.h"

namespace FbeSettings { namespace Hotkeys {
	void BuildDefaults(std::vector<CHotkeysGroup>& groups, DWORD interfaceLanguageId, const CString& nbspChar);
} }
