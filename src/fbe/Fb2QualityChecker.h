#pragma once

#include <atlstr.h>
#include <vector>

namespace Fb2Quality {
enum class Severity { Error, Warning };
struct Issue {
	Severity severity;
	CString message;
	CString code;
	std::vector<int> elementPath;
	CString attributeName;
	CString attributeValue;
	int start = -1; // UTF-16 offsets in the checked XML snapshot.
	int end = -1;
};
struct SourceRange { int start = -1; int end = -1; };
struct Report {
	std::vector<Issue> issues;
	int ErrorCount() const;
	int WarningCount() const;
};
Report Check(const CString& xml);
CString FormatReport(const Report& report);
int ShowReport(HWND parent, const Report& report);
bool ResolveSourceRange(const Issue& issue, const CString& currentSource, SourceRange& range);
}
