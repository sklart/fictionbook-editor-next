#pragma once

#include "../StartupTrace.h"

namespace FbeDiagnostics
{
class DiagnosticCommandService
{
public:
	bool OpenCurrentLog() const;
	bool OpenLogFolder() const;
	CString CurrentLogPath() const;
	StartupTrace::DiagnosticLogCleanupResult ClearOldLogSessions() const;
	bool CreatePackage(CString& packagePath, CString& error) const;
	bool IsEnabledForNextLaunch() const;
	bool SetEnabledForNextLaunch(bool enabled) const;
};
}
