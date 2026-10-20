#pragma once

#include <atlbase.h>
#include <atlstr.h>
#include "../common/PluginSettingsStore.h"

// Settings remain intentionally value-only.  The dialog owns editing, the
// exporter owns transformation, and this model owns registry compatibility.
struct HtmlExportSettings
{
	CString templatePath;
	CString customCss;
	CString customFontFamily;
	CString externalImagesFolderName;
	bool usingCustomTemplate = false;
	bool includeToc = true;
	bool includeMetadata = true;
	bool includeDescription = true;
	bool includeAnnotation = true;
	bool includeTitleInfo = true;
	bool includeDocumentInfo = true;
	bool includePublishInfo = true;
	bool includeHistory = true;
	bool includeAuthors = true;
	bool includeTranslators = true;
	bool includeCustomInfo = true;
	int tocDepth = 1;
	int imageMaxWidth = 0;
	int imageMaxHeight = 0;
	int documentStructure = 0; // 0 = one HTML page; split mode is reserved.
	int style = 0;             // classic, book, minimal
	int fontFamily = 0;        // serif, sans-serif, system, custom
	int fontSize = 0;
	int lineHeight = 0;        // 0 = browser default, otherwise percent
	int contentMaxWidth = 0;
	int pageMargins = 1;       // narrow, normal, wide
	int textAlignment = 1;     // left, justified
	int headingAlignment = 0;  // center, left
	int coverMode = 0;         // normal, reading width, viewport
	int externalImagesFolderMode = 0; // automatic, custom name
	int standaloneWarningMiB = 50;
	int notePlacement = 0;     // source structure, end of book, end of section
};

class HtmlExportSettingsStore
{
public:
	static void Load(CRegKey& registry, HtmlExportSettings& settings);
	static void Save(CRegKey& registry, const HtmlExportSettings& settings);
	static void Load(FbePluginSettings::Store& registry, HtmlExportSettings& settings);
	static void Save(FbePluginSettings::Store& registry, const HtmlExportSettings& settings);
};
