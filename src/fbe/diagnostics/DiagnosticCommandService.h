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
    std::vector<StartupTrace::DiagnosticSession> Sessions() const;
    bool PreparePackage(const StartupTrace::DiagnosticSession& session, const std::vector<CString>& excluded,
        std::shared_ptr<const StartupTrace::DiagnosticPackageSnapshot>& snapshot, CString& error) const;
    bool SavePackage(const StartupTrace::DiagnosticPackageSnapshot& snapshot, const CString& path, CString& error) const;
    bool SavePackageUnique(const StartupTrace::DiagnosticPackageSnapshot& snapshot, const CString& requested, CString& actual, CString& error) const;
	bool IsEnabledForNextLaunch() const;
	bool SetEnabledForNextLaunch(bool enabled) const;
};
}
