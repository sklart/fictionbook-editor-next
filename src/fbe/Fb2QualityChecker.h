#pragma once

#include <atlstr.h>
#include <atomic>
#include <vector>

namespace Fb2Quality {
enum class Severity { Error, Warning, Info };
enum class Category { Xml, Links, Images, Notes, Structure, Metadata };
struct Issue {
	Severity severity;
	CString message;
	Category category = Category::Xml;
	CString details;
	CString recommendation;
	CString code;
	std::vector<int> elementPath;
	CString attributeName;
	CString attributeValue;
	int start = -1; // UTF-16 offsets in the checked XML snapshot.
	int end = -1;
	int line = -1;
	int column = -1;
};
struct SourceRange { int start = -1; int end = -1; };
struct Report {
	std::vector<Issue> issues;
	CString title;
	CString checkedAt;
	bool cancelled = false;
	int ErrorCount() const;
	int WarningCount() const;
	int InfoCount() const;
};
Report Check(const CString& xml, const std::atomic_bool* cancelRequested = nullptr);
bool AnalyzeWithProgress(HWND parent, const CString& xml, Report& report);
bool ProbeAnalysisCancellation(HWND parent, const CString& xml);
CString FormatReport(const Report& report);
CString FormatHtmlReport(const Report& report);
bool SaveReport(const Report& report, const CString& path, bool html, DWORD& error);
int ShowReport(HWND parent, const Report& report);
bool ProbeResultsDialogLayout(HWND parent, const Report& report, CString* diagnostics = nullptr);
bool ProbeResultsDialogVisual(HWND parent, const Report& report, const CString& screenshotPath, CString* diagnostics = nullptr);
bool ProbeReportSaveDialog(HWND parent, const Report& report, bool htmlFilter = false);
bool ResolveSourceRange(const Issue& issue, const CString& currentSource, SourceRange& range);
}
