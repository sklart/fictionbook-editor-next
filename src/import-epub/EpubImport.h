#pragma once
#include "stdafx.h"

// User-visible EPUB import settings. The dialog in ImportOptionsDialog.cpp
// fills this structure before the converter starts.
enum EpubSvgConversionMode
{
    SVG_IMPORT_KEEP = 0,
    SVG_IMPORT_CONVERT_PNG = 1,
    SVG_IMPORT_CONVERT_JPEG = 2,
    SVG_IMPORT_SKIP = 3
};

struct EpubImportOptions
{
    bool importCover;
    bool importImages;
    bool importNotes;
    bool useNavigationTitles;
    bool repairEncoding;
    bool skipServicePages;
    bool importTables;
    bool importLists;
    bool importPoemsEpigraphs;
    bool importSubtitles;
    bool splitSectionsByHeadings;
    bool preserveLinks;
    bool cleanTypography;
    bool importPageBreaks;
    bool skipHiddenElements;
    bool validateResult;
    bool addDiagnosticSection;
    bool writeImportLog;
    bool saveFb2Copy;
    bool useCssSemanticClasses;
    bool removeFootnoteBacklinks;
    bool removeServiceSections;
    bool writeLogOnWarnings;
    bool saveIntermediateFb2OnError;
    bool keepTempOnError;
    int svgConversionMode;

    EpubImportOptions()
        : importCover(true)
        , importImages(true)
        , importNotes(true)
        , useNavigationTitles(true)
        , repairEncoding(true)
        , skipServicePages(true)
        , importTables(true)
        , importLists(true)
        , importPoemsEpigraphs(true)
        , importSubtitles(true)
        , splitSectionsByHeadings(true)
        , preserveLinks(true)
        , cleanTypography(true)
        , importPageBreaks(true)
        , skipHiddenElements(true)
        , validateResult(true)
        , addDiagnosticSection(false)
        , writeImportLog(false)
        , saveFb2Copy(false)
        , useCssSemanticClasses(true)
        , removeFootnoteBacklinks(true)
        , removeServiceSections(true)
        , writeLogOnWarnings(true)
        , saveIntermediateFb2OnError(false)
        , keepTempOnError(false)
        , svgConversionMode(SVG_IMPORT_CONVERT_PNG)
    {
    }
};

struct EpubImportRuntimeStats
{
    int svgImages;
    int svgConverted;
    int svgPlaceholders;
    int svgSkipped;
    CStringW svgBackend;

    EpubImportRuntimeStats()
        : svgImages(0)
        , svgConverted(0)
        , svgPlaceholders(0)
        , svgSkipped(0)
    {
    }
};

EpubImportRuntimeStats GetLastEpubImportRuntimeStats();

// Converts a selected EPUB file to a FictionBook XML string.
//
// Current implementation remains deliberately portable for the first working
// importer stage: EPUB is extracted through the built-in Windows ZIP folder provider, then OPF,
// manifest, spine, nav.xhtml/toc.ncx and XHTML documents are parsed with MSXML.
// No PowerShell Expand-Archive or tar.exe dependency is used.
bool BuildFb2XmlFromEpub(const CStringW& epubPath, const EpubImportOptions& options, CStringW& outFb2Xml, CStringW& errorText);
