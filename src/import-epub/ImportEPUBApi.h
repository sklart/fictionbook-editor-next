#pragma once

// Stable C ABI for clients which use ImportEPUB.dll without linking to its
// ATL, CString, or CRT implementation.  All strings are UTF-16 and all
// storage is owned by the caller.
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMPORT_EPUB_API_VERSION 1u
#define IMPORT_EPUB_SVG_BACKEND_CCH 128u

typedef struct ImportEpubOptionsV1
{
    DWORD cbSize;
    DWORD version;
    DWORD importCover;
    DWORD importImages;
    DWORD importNotes;
    DWORD useNavigationTitles;
    DWORD repairEncoding;
    DWORD skipServicePages;
    DWORD importTables;
    DWORD importLists;
    DWORD importPoemsEpigraphs;
    DWORD importSubtitles;
    DWORD splitSectionsByHeadings;
    DWORD preserveLinks;
    DWORD cleanTypography;
    DWORD importPageBreaks;
    DWORD skipHiddenElements;
    DWORD validateResult;
    DWORD addDiagnosticSection;
    DWORD writeImportLog;
    DWORD saveFb2Copy;
    DWORD useCssSemanticClasses;
    DWORD removeFootnoteBacklinks;
    DWORD removeServiceSections;
    DWORD writeLogOnWarnings;
    DWORD saveIntermediateFb2OnError;
    DWORD keepTempOnError;
    LONG svgConversionMode;
} ImportEpubOptionsV1;

typedef struct ImportEpubRuntimeStatsV1
{
    DWORD cbSize;
    DWORD version;
    LONG svgImages;
    LONG svgConverted;
    LONG svgPlaceholders;
    LONG svgSkipped;
    wchar_t svgBackend[IMPORT_EPUB_SVG_BACKEND_CCH];
} ImportEpubRuntimeStatsV1;

// Converts epubPath to FB2 XML using the ImportEPUB runtime.  Call once with
// fb2XmlBuffer == NULL and fb2XmlBufferCch == 0 to obtain *requiredFb2XmlCch
// (including its terminating NUL), then call again with caller-owned storage.
// Error text follows the same caller-owned-buffer rule.  A failed conversion
// returns a failed HRESULT; ERROR_INSUFFICIENT_BUFFER means a supplied buffer
// was too small.  No C++ object, exception, or allocation crosses the ABI.
HRESULT WINAPI ImportEPUB_BuildFb2XmlW(
    LPCWSTR epubPath,
    const ImportEpubOptionsV1* options,
    wchar_t* fb2XmlBuffer,
    DWORD fb2XmlBufferCch,
    DWORD* requiredFb2XmlCch,
    ImportEpubRuntimeStatsV1* runtimeStats,
    wchar_t* errorBuffer,
    DWORD errorBufferCch,
    DWORD* requiredErrorCch);

#ifdef __cplusplus
}
#endif
