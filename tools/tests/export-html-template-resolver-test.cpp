#include "../../src/export-html/stdafx.h"
#include "../../src/export-html/TemplateResolver.h"

CComModule _Module;
CRegKey _Settings;
CString _SettingsPath;

CString FormatExportHtmlString(UINT, ...)
{
	return CString();
}

int ShowExportHtmlTaskDialog(HWND, UINT, LPCWSTR, LPCWSTR, int, LPCWSTR)
{
	return 0;
}

static bool Is(const ExportHtmlTemplateSelection& value, const CString& path, bool custom)
{
	return value.custom == custom && ExportHtmlPathsEqual(value.path, path);
}

static IXMLDOMDocument2Ptr LoadXsl(const wchar_t* xml)
{
	IXMLDOMDocument2Ptr document;
	VARIANT_BOOL loaded = VARIANT_FALSE;
	if (FAILED(document.CreateInstance(__uuidof(DOMDocument60))) || FAILED(document->loadXML(_bstr_t(xml), &loaded)) || loaded != VARIANT_TRUE)
		return NULL;
	return document;
}

int wmain()
{
	const CString bundled = L"C:\\Current\\html.xsl";
	const CString custom = L"C:\\UserTemplates\\custom.xsl";
	const CString missing = L"C:\\missing\\html.xsl";
	if (!Is(ResolveExportHtmlTemplateState(bundled, L"", false, false, false, false), bundled, false)) return 1;
	if (!Is(ResolveExportHtmlTemplateState(bundled, bundled, false, false, true, true), bundled, false)) return 2;
	if (!Is(ResolveExportHtmlTemplateState(bundled, missing, false, false, false, false), bundled, false)) return 3;
	if (!Is(ResolveExportHtmlTemplateState(bundled, L"C:\\OldFBE\\html.xsl", false, false, true, true), bundled, false)) return 4;
	if (!Is(ResolveExportHtmlTemplateState(bundled, custom, false, false, true, false), custom, true)) return 5;
	if (!Is(ResolveExportHtmlTemplateState(bundled, custom, true, true, true, false), custom, true)) return 6;
	if (!Is(ResolveExportHtmlTemplateState(bundled, missing, true, true, false, false), bundled, false)) return 7;
	if (!ExportHtmlPathsEqual(L"C:\\FBE\\html.xsl", L"C:\\FBE\\.\\html.xsl")) return 8;

	if (FAILED(::CoInitialize(NULL))) return 9;
	IXMLDOMDocument2Ptr without = LoadXsl(L"<xsl:stylesheet xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0'><xsl:param name='saveimages'/></xsl:stylesheet>");
	IXMLDOMDocument2Ptr with = LoadXsl(L"<xsl:stylesheet xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0'><xsl:param name='embedimages'/></xsl:stylesheet>");
	IXMLDOMDocument2Ptr otherPrefix = LoadXsl(L"<X:stylesheet xmlns:X='http://www.w3.org/1999/XSL/Transform' version='1.0'><X:param name='embedimages'/></X:stylesheet>");
	const bool success = without != NULL && with != NULL && otherPrefix != NULL &&
		!SupportsEmbeddedImages(without) && SupportsEmbeddedImages(with) && SupportsEmbeddedImages(otherPrefix);
	without = NULL;
	with = NULL;
	otherPrefix = NULL;
	::CoUninitialize();
	return success ? 0 : 10;
}
