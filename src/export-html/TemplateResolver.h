#pragma once

#include "utils.h"

inline bool SupportsEmbeddedImages(IXMLDOMDocument2Ptr document)
{
	document->setProperty(bstr_t(L"SelectionLanguage"), variant_t(L"XPath"));
	IXMLDOMNodePtr parameter;
	CheckError(document->selectSingleNode(
		bstr_t(L"//*[local-name()='param' and namespace-uri()='http://www.w3.org/1999/XSL/Transform' and @name='embedimages']"),
		&parameter));
	return parameter != NULL;
}

struct ExportHtmlTemplateSelection
{
	CString path;
	bool custom;
};

inline bool ExportHtmlPathsEqual(const CString& left, const CString& right)
{
	if (left.IsEmpty() || right.IsEmpty())
		return left.IsEmpty() && right.IsEmpty();

	auto normalize = [](const CString& path) {
		DWORD length = ::GetFullPathName(path, 0, NULL, NULL);
		if (length == 0)
			return path;
		CString normalized;
		wchar_t* buffer = normalized.GetBuffer(length);
		DWORD written = ::GetFullPathName(path, length, buffer, NULL);
		normalized.ReleaseBuffer(written == 0 ? 0 : written);
		return written == 0 ? path : normalized;
	};
	return normalize(left).CompareNoCase(normalize(right)) == 0;
}

inline ExportHtmlTemplateSelection ResolveExportHtmlTemplateState(const CString& bundled, const CString& stored,
	bool hasUseCustom, bool useCustom, bool templateExists, bool legacyBundled)
{
	ExportHtmlTemplateSelection result;
	if ((hasUseCustom && useCustom && templateExists) || (!hasUseCustom && templateExists && !legacyBundled)) {
		result.path = stored;
		result.custom = true;
		return result;
	}
	result.path = bundled;
	result.custom = false;
	return result;
}

inline bool ExportHtmlLooksLikeOldBundledTemplate(const CString& path)
{
	if (path.IsEmpty() || path.Right(8).CompareNoCase(L"html.xsl") != 0)
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
	const CString bundled = U::GetProgDirFile(L"html.xsl");
	const CString stored = U::QuerySV(settings, L"Template", L"");
	DWORD useCustom = 0;
	const bool hasUseCustom = settings.QueryDWORDValue(L"UseCustomTemplate", useCustom) == ERROR_SUCCESS;
	const bool exists = !stored.IsEmpty() && ::GetFileAttributes(stored) != INVALID_FILE_ATTRIBUTES;
	const bool legacyBundled = ExportHtmlPathsEqual(stored, bundled) || ExportHtmlLooksLikeOldBundledTemplate(stored);

	ExportHtmlTemplateSelection result = ResolveExportHtmlTemplateState(bundled, stored,
		hasUseCustom, useCustom != 0, exists, legacyBundled);
	if (result.custom) {
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
