#include "stdafx.h"
#include "HotkeyTextExport.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\utils\\utils.h"

namespace
{
CString GetHotkeyDisplayName(const CHotkey& hotkey)
{
	return hotkey.m_name_resource_id ? FbeLoadRuntimeString(hotkey.m_name_resource_id, hotkey.m_name) : hotkey.m_name;
}

CString GetHotkeyGroupDisplayName(const CHotkeysGroup& group)
{
	return group.m_name_resource_id ? FbeLoadRuntimeString(group.m_name_resource_id, group.m_name) : group.m_name;
}
}

namespace FbeSettings { namespace Hotkeys {
CString BuildTextExport(const std::vector<CHotkeysGroup>& groups)
{
	CString text(L"FictionBook Editor Next\r\n");
	text += FbeLoadRuntimeStringByKey(L"fbe.hotkey.export.title", L"Hotkeys");
	text += L"\r\n";
	for(size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
	{
		const CHotkeysGroup& group = groups[groupIndex];
		CString rows;
		for(size_t hotkeyIndex = 0; hotkeyIndex < group.m_hotkeys.size(); ++hotkeyIndex)
		{
			const CHotkey& hotkey = group.m_hotkeys[hotkeyIndex];
			if(hotkey.m_accel.key == 0) continue;
			CString row(GetHotkeyDisplayName(hotkey));
			row += L"\t" + U::AccelToString(hotkey.m_accel) + L"\r\n";
			rows += row;
		}
		if(!rows.IsEmpty()) text += L"\r\n" + GetHotkeyGroupDisplayName(group) + L"\r\n" + rows;
	}
	return text;
}
}}
