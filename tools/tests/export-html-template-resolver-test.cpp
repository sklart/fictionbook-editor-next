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

static bool CreateEmptyFile(const CString& path)
{
	HANDLE file = ::CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;
	::CloseHandle(file);
	return true;
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

	wchar_t temporaryPath[MAX_PATH] = {};
	if (::GetTempPath(_countof(temporaryPath), temporaryPath) == 0) return 9;
	CString fixtureRoot;
	fixtureRoot.Format(L"%sexport-html-resolver-%lu", temporaryPath, ::GetCurrentProcessId());
	const CString oldBundle = fixtureRoot + L"\\old";
	const CString customDirectory = fixtureRoot + L"\\custom-html";
	const CString oldTemplate = oldBundle + L"\\html.xsl";
	const CString similarTemplate = customDirectory + L"\\myhtml.xsl";
	const CString customTemplate = oldBundle + L"\\custom-html.xsl";
	const CString wrongExtension = oldBundle + L"\\html.xslt";
	::CreateDirectory(fixtureRoot, NULL);
	const bool fixturesCreated = ::CreateDirectory(oldBundle, NULL) != FALSE &&
		::CreateDirectory(customDirectory, NULL) != FALSE &&
		CreateEmptyFile(oldTemplate) && CreateEmptyFile(oldBundle + L"\\ExportHTML.dll") &&
		CreateEmptyFile(similarTemplate) && CreateEmptyFile(customTemplate) && CreateEmptyFile(wrongExtension);
	const bool oldBundleDetected = fixturesCreated && ExportHtmlLooksLikeOldBundledTemplate(oldTemplate) &&
		!ExportHtmlLooksLikeOldBundledTemplate(similarTemplate) && !ExportHtmlLooksLikeOldBundledTemplate(customTemplate) &&
		!ExportHtmlLooksLikeOldBundledTemplate(wrongExtension);
	::DeleteFile(oldBundle + L"\\ExportHTML.dll");
	const bool executableBundleDetected = CreateEmptyFile(oldBundle + L"\\FBE.exe") && ExportHtmlLooksLikeOldBundledTemplate(oldTemplate);
	::DeleteFile(oldBundle + L"\\FBE.exe");
	const bool markerRequired = !ExportHtmlLooksLikeOldBundledTemplate(oldTemplate);
	::DeleteFile(oldTemplate);
	::DeleteFile(similarTemplate);
	::DeleteFile(customTemplate);
	::DeleteFile(wrongExtension);
	::RemoveDirectory(oldBundle);
	::RemoveDirectory(customDirectory);
	::RemoveDirectory(fixtureRoot);
	if (!oldBundleDetected || !executableBundleDetected || !markerRequired) return 10;

	if (FAILED(::CoInitialize(NULL))) return 11;
	IXMLDOMDocument2Ptr without = LoadXsl(L"<xsl:stylesheet xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0'><xsl:param name='saveimages'/></xsl:stylesheet>");
	IXMLDOMDocument2Ptr with = LoadXsl(L"<xsl:stylesheet xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0'><xsl:param name='embedimages'/></xsl:stylesheet>");
	IXMLDOMDocument2Ptr otherPrefix = LoadXsl(L"<X:stylesheet xmlns:X='http://www.w3.org/1999/XSL/Transform' version='1.0'><X:param name='embedimages'/></X:stylesheet>");
	IXMLDOMDocument2Ptr local = LoadXsl(L"<xsl:stylesheet xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0'><xsl:template match='/'><xsl:param name='embedimages'/></xsl:template></xsl:stylesheet>");
	IXMLDOMDocument2Ptr transform = LoadXsl(L"<xsl:transform xmlns:xsl='http://www.w3.org/1999/XSL/Transform' version='1.0'><xsl:param name='embedimages'/></xsl:transform>");
	const bool success = without != NULL && with != NULL && otherPrefix != NULL && local != NULL && transform != NULL &&
		!SupportsEmbeddedImages(without) && SupportsEmbeddedImages(with) && SupportsEmbeddedImages(otherPrefix) &&
		!SupportsEmbeddedImages(local) && SupportsEmbeddedImages(transform);
	without = NULL;
	with = NULL;
	otherPrefix = NULL;
	local = NULL;
	transform = NULL;
	::CoUninitialize();
	return success ? 0 : 12;
}
