#pragma once

// ExportDOCX shared quality/reporting vocabulary.
// Kept small on purpose: this is the first low-risk step toward moving
// validation/reporting code out of the large ExportDOCXPlugin.cpp file.
namespace ExportDOCXQuality {
    static const wchar_t* const StatusOK = L"OK";
    static const wchar_t* const StatusOKWithWarnings = L"OK_WITH_WARNINGS";
    static const wchar_t* const StatusDocxInvalid = L"DOCX_INVALID";
    static const wchar_t* const StatusFail = L"FAIL";
    static const wchar_t* const StatusSkip = L"SKIP";
    static const wchar_t* const StatusUpToDate = L"UPTODATE";
    static const wchar_t* const StatusFiltered = L"FILTERED";
    static const wchar_t* const StatusDryRun = L"DRYRUN";
}
