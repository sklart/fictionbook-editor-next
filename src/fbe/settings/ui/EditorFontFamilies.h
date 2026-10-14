#pragma once

#include <algorithm>
#include <vector>
#include <atlstr.h>
#include <windows.h>

namespace EditorFontFamilies
{
inline int CALLBACK Collect(const ENUMLOGFONTEXW* font, const NEWTEXTMETRICEXW*, DWORD, LPARAM context)
{
	const CString name(font->elfLogFont.lfFaceName);
	if(name.IsEmpty() || name[0] == L'@') return TRUE;
	auto& names = *reinterpret_cast<std::vector<CString>*>(context);
	if(std::find_if(names.begin(), names.end(), [&](const CString& existing) {
		return existing.CompareNoCase(name) == 0;
	}) == names.end()) names.push_back(name);
	return TRUE;
}

inline void Fill(CComboBox& combo, const CString& selected)
{
	std::vector<CString> names;
	HDC display = ::CreateDCW(L"DISPLAY", NULL, NULL, NULL);
	if(display != NULL)
	{
		LOGFONTW font = {};
		font.lfCharSet = DEFAULT_CHARSET;
		::EnumFontFamiliesExW(display, &font, reinterpret_cast<FONTENUMPROCW>(&Collect),
			reinterpret_cast<LPARAM>(&names), 0);
		::DeleteDC(display);
	}
	if(!selected.IsEmpty() && std::find_if(names.begin(), names.end(), [&](const CString& name) {
		return name.CompareNoCase(selected) == 0;
	}) == names.end()) names.push_back(selected);
	std::sort(names.begin(), names.end(), [](const CString& left, const CString& right) {
		return left.CompareNoCase(right) < 0;
	});
	combo.ResetContent();
	int selection = CB_ERR;
	for(const CString& name : names)
	{
		const int index = combo.AddString(name);
		if(name.CompareNoCase(selected) == 0) selection = index;
	}
	if(selection != CB_ERR) combo.SetCurSel(selection);
}
}
