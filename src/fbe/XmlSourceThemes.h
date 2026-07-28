#pragma once

#include "Settings.h"

struct XmlSourceThemeInfo
{
	CString id;
	CString name;
	bool isUser;
	bool isDark;
};

struct XmlSourceThemeMetadata
{
	bool isDark;
	// True only when the editor background was explicitly changed for this export.
	bool recalculateIsDark;
	CString baseThemeId, author, description, source, license;
};

namespace XmlSourceThemes
{
	const CString& GetThemeIdForPalette(DWORD palette);
	DWORD GetPaletteForThemeId(const CString& id);
	CString NormalizeThemeId(const CString& id);
	const std::vector<XmlSourceThemeInfo>& GetAvailableThemes();
	void ReloadThemes();
	bool GetImportThemeId(const CString& sourcePath, CString& id, CString& error);
	bool IsUserTheme(const CString& id);
	bool GetThemeColor(const CString& id, XmlSrcStyleToken token, DWORD& color);
	bool GetThemeMetadata(const CString& id, XmlSourceThemeMetadata& metadata);
	bool DeleteUserTheme(const CString& id, CString& error);
	bool SaveThemeAsUser(const CString& name, const DWORD* colors, CString& savedId, CString& error,
		const XmlSourceThemeMetadata* metadata = NULL);
	enum ImportThemeConflictMode { IMPORT_THEME_COPY, IMPORT_THEME_REPLACE_USER };
	bool ImportThemeFile(const CString& sourcePath, CString& importedId, CString& error, ImportThemeConflictMode conflictMode = IMPORT_THEME_COPY);
	bool ExportThemeFile(const CString& id, const CString& name, const DWORD* colors, const CString& destinationPath, CString& error, const XmlSourceThemeMetadata* metadata = NULL);
}