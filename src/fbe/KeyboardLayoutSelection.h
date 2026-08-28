#pragma once

#include <string>
#include <vector>

enum class KeyboardLayoutSelectionKind
{
	ExactKlid,
	ExactLegacy,
	MigratedLegacy,
	UnavailableKlid,
	UnresolvedLegacy,
	CurrentDefault,
	None
};

struct KeyboardLayoutEntry
{
	DWORD legacyHkl;
	std::wstring klid;
};

struct KeyboardLayoutSelection
{
	KeyboardLayoutSelectionKind kind;
	int index;
};

inline KeyboardLayoutSelection ResolveKeyboardLayoutSelection(const std::wstring& storedKlid, DWORD legacyValue, DWORD activeLayout, const std::vector<KeyboardLayoutEntry>& layouts)
{
	if(!storedKlid.empty())
	{
		for(size_t i = 0; i < layouts.size(); ++i)
			if(layouts[i].klid == storedKlid)
				return { KeyboardLayoutSelectionKind::ExactKlid, static_cast<int>(i) };
		return { KeyboardLayoutSelectionKind::UnavailableKlid, -1 };
	}
	if(legacyValue != 0)
	{
		if((legacyValue & 0xffff0000) != 0)
			for(size_t i = 0; i < layouts.size(); ++i)
				if(layouts[i].legacyHkl == legacyValue)
					return { KeyboardLayoutSelectionKind::ExactLegacy, static_cast<int>(i) };
		int onlyLanguageMatch = -1;
		int activeLanguageMatch = -1;
		int languageMatches = 0;
		for(size_t i = 0; i < layouts.size(); ++i)
			if((layouts[i].legacyHkl & 0xffff) == (legacyValue & 0xffff))
			{
				++languageMatches;
				onlyLanguageMatch = static_cast<int>(i);
				if(layouts[i].legacyHkl == activeLayout)
					activeLanguageMatch = static_cast<int>(i);
			}
		if(activeLanguageMatch >= 0)
			return { KeyboardLayoutSelectionKind::MigratedLegacy, activeLanguageMatch };
		if(languageMatches == 1)
			return { KeyboardLayoutSelectionKind::MigratedLegacy, onlyLanguageMatch };
		return { KeyboardLayoutSelectionKind::UnresolvedLegacy, -1 };
	}
	for(size_t i = 0; i < layouts.size(); ++i)
		if(layouts[i].legacyHkl == activeLayout)
			return { KeyboardLayoutSelectionKind::CurrentDefault, static_cast<int>(i) };
	return { KeyboardLayoutSelectionKind::None, -1 };
}
