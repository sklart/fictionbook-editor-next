#pragma once

#include "Settings.h"

struct XmlSourceThemeInfo
{
	CString id;
	CString name;
	bool isUser;
};

namespace XmlSourceThemes
{
	const CString& GetThemeIdForPalette(DWORD palette);
	DWORD GetPaletteForThemeId(const CString& id);
	CString NormalizeThemeId(const CString& id);
	const std::vector<XmlSourceThemeInfo>& GetAvailableThemes();
	void ReloadThemes();
	bool GetThemeColor(const CString& id, XmlSrcStyleToken token, DWORD& color);
	bool DeleteUserTheme(const CString& id, CString& error);
	bool SaveThemeAsUser(const CString& name, const DWORD* colors, CString& savedId, CString& error);
	bool ImportThemeFile(const CString& sourcePath, CString& importedId, CString& error);
	bool ExportThemeFile(const CString& id, const CString& name, const DWORD* colors, const CString& destinationPath, CString& error);
}