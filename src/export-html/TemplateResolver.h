#pragma once

#include "utils.h"

struct ExportHtmlTemplateSelection
{
	CString path;
	bool custom;
};

inline bool ExportHtmlPathsEqual(const CString& left, const CString& right)
{
	return left.CompareNoCase(right) == 0;
}

inline bool ExportHtmlLooksLikeOldBundledTemplate(const CString& path)
{
	if (path.IsEmpty() || path.Right(9).CompareNoCase(L"html.xsl") != 0)
		return false;
	int slash = path.ReverseFind(L'\\');
	if (slash < 0)
		return false;
	const CString directory = path.Left(slash + 1);
	return ::GetFileAttributes(directory + L"ExportHTML.dll") != INVALID_FILE_ATTRIBUTES ||
		::GetFileAttributes(directory + L"FBE.exe") != INVALID_FILE_ATTRIBUTES;
}

inline ExportHtmlTemplateSelection ResolveExportHtmlTemplate(CRegKey& settings)
{
	ExportHtmlTemplateSelection result;
	const CString bundled = U::GetProgDirFile(L"html.xsl");
	const CString stored = U::QuerySV(settings, L"Template", L"");
	DWORD useCustom = 0;
	const bool hasUseCustom = settings.QueryDWORDValue(L"UseCustomTemplate", useCustom) == ERROR_SUCCESS;
	const bool exists = !stored.IsEmpty() && ::GetFileAttributes(stored) != INVALID_FILE_ATTRIBUTES;
	const bool legacyBundled = ExportHtmlPathsEqual(stored, bundled) || ExportHtmlLooksLikeOldBundledTemplate(stored);

	if ((hasUseCustom && useCustom != 0 && exists) || (!hasUseCustom && exists && !legacyBundled)) {
		result.path = stored;
		result.custom = true;
		if (!hasUseCustom)
			settings.SetDWORDValue(L"UseCustomTemplate", 1);
		return result;
	}

	// Missing, current bundled, and another FBE installation are all migrated
	// to the XSL next to the currently loaded ExportHTML.dll.
	result.path = bundled;
	result.custom = false;
	settings.DeleteValue(L"Template");
	settings.SetDWORDValue(L"UseCustomTemplate", 0);
	return result;
}
