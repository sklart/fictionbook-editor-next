#pragma once

#include <atlstr.h>
#include <vector>

namespace Fb2Quality {
enum class Severity { Error, Warning };
struct Issue { Severity severity; CString message; CString locator; };
struct Report {
	std::vector<Issue> issues;
	int ErrorCount() const;
	int WarningCount() const;
};
Report Check(const CString& xml);
CString FormatReport(const Report& report);
CString ShowReport(HWND parent, const Report& report);
}
