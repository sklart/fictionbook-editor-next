#pragma once

#include "utils.h"

inline bool SupportsGlobalXslParameter(IXMLDOMDocument2Ptr document, LPCWSTR name)
{
	document->setProperty(bstr_t(L"SelectionLanguage"), variant_t(L"XPath"));
	IXMLDOMNodePtr parameter;
	CString xpath;
	xpath.Format(L"/*[(local-name()='stylesheet' or local-name()='transform') and namespace-uri()='http://www.w3.org/1999/XSL/Transform']/*[local-name()='param' and namespace-uri()='http://www.w3.org/1999/XSL/Transform' and @name='%s']", name);
	CheckError(document->selectSingleNode(
		bstr_t(static_cast<LPCWSTR>(xpath)),
		&parameter));
	return parameter != NULL;
}

inline bool SupportsEmbeddedImages(IXMLDOMDocument2Ptr document)
{
	return SupportsGlobalXslParameter(document, L"embedimages");
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

	return U::GetFullPathName(left).CompareNoCase(U::GetFullPathName(right)) == 0;
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
	const int backslash = path.ReverseFind(L'\\');
	const int slash = path.ReverseFind(L'/');
	const int separator = backslash > slash ? backslash : slash;
	if (path.IsEmpty() || separator < 0 || path.Mid(separator + 1).CompareNoCase(L"html.xsl") != 0)
		return false;
	const CString directory = path.Left(separator + 1);
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
