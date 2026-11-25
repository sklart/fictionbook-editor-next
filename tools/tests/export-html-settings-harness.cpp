#include "../../src/export-html/stdafx.h"
#include "../../src/export-html/HtmlExportSettings.h"

#include <iostream>

CComModule _Module;
CRegKey _Settings;
CString _SettingsPath;

static bool Expect(bool value, const char* message)
{
	if (!value) std::cerr << message << std::endl;
	return value;
}

int main()
{
	_Module.Init(NULL, ::GetModuleHandle(NULL));
	CRegKey registry;
	const CString path = L"Software\\FBETeam\\ExportHtmlSettingsHarness";
	if (registry.Create(HKEY_CURRENT_USER, path) != ERROR_SUCCESS) return 1;
	registry.SetDWORDValue(L"IncludeDesc", 0);
	registry.SetDWORDValue(L"TOCDepth", 99);
	registry.SetDWORDValue(L"ImageMaxWidth", 50000);
	registry.SetDWORDValue(L"ImageMaxHeight", 50000);
	registry.SetDWORDValue(L"Style", 99);
	registry.SetDWORDValue(L"FontFamily", 99);
	registry.SetDWORDValue(L"FontSize", 99);
	registry.SetDWORDValue(L"LineHeight", 99);
	registry.SetDWORDValue(L"ContentMaxWidth", 50000);
	registry.SetDWORDValue(L"PageMargins", 99);
	registry.SetDWORDValue(L"TextAlignment", 99);
	registry.SetDWORDValue(L"HeadingAlignment", 99);
	registry.SetDWORDValue(L"CoverMode", 99);
	registry.SetDWORDValue(L"ExternalImagesFolderMode", 99);
	registry.SetDWORDValue(L"StandaloneWarningMiB", 99999);
	registry.SetDWORDValue(L"NotePlacement", 99);

	HtmlExportSettings settings;
	HtmlExportSettingsStore::Load(registry, settings);
	bool ok = true;
	ok &= Expect(!settings.includeMetadata && !settings.includeDescription && !settings.includeAnnotation && !settings.includeCustomInfo, "IncludeDesc migration failed");
	ok &= Expect(settings.tocDepth == 10 && settings.imageMaxWidth == 10000 && settings.imageMaxHeight == 10000, "numeric clamping failed");
	ok &= Expect(settings.style == 2 && settings.fontFamily == 3 && settings.fontSize == 72 && settings.lineHeight == 100 && settings.contentMaxWidth == 10000, "appearance clamping failed");
	ok &= Expect(settings.pageMargins == 2 && settings.textAlignment == 1 && settings.headingAlignment == 1 && settings.coverMode == 2 && settings.externalImagesFolderMode == 1 && settings.standaloneWarningMiB == 10240 && settings.notePlacement == 2, "enum clamping failed");
	settings.includeMetadata = false;
	settings.includeToc = false;
	settings.includeDescription = true;
	settings.includeAnnotation = false;
	settings.includeTitleInfo = true;
	settings.includeDocumentInfo = false;
	settings.includePublishInfo = true;
	settings.includeHistory = false;
	settings.includeAuthors = true;
	settings.includeTranslators = false;
	settings.includeCustomInfo = true;
	settings.customCss = L"C:\\Тест\\стиль.css";
	settings.externalImagesFolderMode = 1;
	settings.externalImagesFolderName = L"книга_files";
	settings.notePlacement = 2;
	settings.style = 2;
	settings.fontFamily = 3;
	settings.customFontFamily = L"Шрифт";
	settings.fontSize = 18;
	settings.lineHeight = 150;
	settings.contentMaxWidth = 960;
	settings.pageMargins = 2;
	settings.textAlignment = 1;
	settings.headingAlignment = 1;
	settings.coverMode = 2;
	settings.standaloneWarningMiB = 64;
	HtmlExportSettingsStore::Save(registry, settings);

	HtmlExportSettings roundTrip;
	HtmlExportSettingsStore::Load(registry, roundTrip);
	ok &= Expect(roundTrip.includeDescription && !roundTrip.includeAnnotation && roundTrip.includeTitleInfo && !roundTrip.includeDocumentInfo && roundTrip.includePublishInfo && !roundTrip.includeHistory && roundTrip.includeAuthors && !roundTrip.includeTranslators && roundTrip.includeCustomInfo, "per-section metadata persistence failed");
	ok &= Expect(!roundTrip.includeMetadata && !roundTrip.includeToc, "master metadata and TOC persistence failed");
	ok &= Expect(roundTrip.customCss == settings.customCss && roundTrip.externalImagesFolderName == settings.externalImagesFolderName, "Unicode path persistence failed");
	ok &= Expect(roundTrip.notePlacement == 2 && roundTrip.fontSize == 18 && roundTrip.style == 2 && roundTrip.fontFamily == 3 && roundTrip.customFontFamily == settings.customFontFamily && roundTrip.lineHeight == 150 && roundTrip.contentMaxWidth == 960 && roundTrip.pageMargins == 2 && roundTrip.textAlignment == 1 && roundTrip.headingAlignment == 1 && roundTrip.coverMode == 2 && roundTrip.externalImagesFolderMode == 1 && roundTrip.standaloneWarningMiB == 64, "new settings persistence failed");
	registry.Close();
	CRegKey migrationRegistry;
	if (migrationRegistry.Create(HKEY_CURRENT_USER, path + L"Migration") != ERROR_SUCCESS) return 1;
	migrationRegistry.SetDWORDValue(L"IncludeDesc", 1);
	HtmlExportSettings migrated;
	HtmlExportSettingsStore::Load(migrationRegistry, migrated);
	ok &= Expect(migrated.includeMetadata && migrated.includeDescription && migrated.includeAnnotation && migrated.includeTitleInfo && migrated.includeDocumentInfo && migrated.includePublishInfo && migrated.includeHistory && migrated.includeAuthors && migrated.includeTranslators && migrated.includeCustomInfo, "IncludeDesc=1 migration failed");
	ok &= Expect(migrated.tocDepth == 3, "clean TOC depth");
	migrationRegistry.SetDWORDValue(L"IncludeHistory", 0);
	migrationRegistry.SetDWORDValue(L"TOCDepth", 1);
	migrationRegistry.SetDWORDValue(L"UnknownFutureSetting", 19);
	migrationRegistry.SetStringValue(L"Template", L"custom.xsl");
	HtmlExportSettingsStore::Load(migrationRegistry, migrated);
	ok &= Expect(migrated.tocDepth == 1 && migrated.includeAnnotation && !migrated.includeHistory, "partial profile fallback");
	HtmlExportSettingsStore::Save(migrationRegistry, migrated);
	HtmlExportSettings reopened;
	HtmlExportSettingsStore::Load(migrationRegistry, reopened);
	ok &= Expect(reopened.tocDepth == 1 && !reopened.includeHistory, "partial profile after restart");
	ok &= Expect(FbePluginSettings::ReadDword(migrationRegistry, L"UnknownFutureSetting", 0) == 19 &&
	    FbePluginSettings::ReadString(migrationRegistry, L"Template", L"") == L"custom.xsl", "retained profile values");
	migrationRegistry.Close();
	CRegKey parent;
	if (parent.Open(HKEY_CURRENT_USER, L"Software\\FBETeam", KEY_WRITE) == ERROR_SUCCESS) {
		parent.RecurseDeleteKey(L"ExportHtmlSettingsHarness");
		parent.RecurseDeleteKey(L"ExportHtmlSettingsHarnessMigration");
	}
	_Module.Term();
	return ok ? 0 : 1;
}
