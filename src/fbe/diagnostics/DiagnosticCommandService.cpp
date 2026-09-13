#include "../stdafx.h"
#include "DiagnosticCommandService.h"

namespace FbeDiagnostics
{
bool DiagnosticCommandService::OpenCurrentLog() const
{
	const CString path(StartupTrace::CurrentLogPath());
	if (!path.IsEmpty() && ::GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
		return reinterpret_cast<INT_PTR>(::ShellExecute(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL)) > 32;
	return OpenLogFolder();
}

bool DiagnosticCommandService::OpenLogFolder() const
{
	const CString directory(StartupTrace::CurrentLogDirectory());
	return !directory.IsEmpty() && ::GetFileAttributes(directory) != INVALID_FILE_ATTRIBUTES &&
		reinterpret_cast<INT_PTR>(::ShellExecute(NULL, L"open", directory, NULL, NULL, SW_SHOWNORMAL)) > 32;
}

CString DiagnosticCommandService::CurrentLogPath() const { return StartupTrace::CurrentLogPath(); }
StartupTrace::DiagnosticLogCleanupResult DiagnosticCommandService::ClearOldLogSessions() const { return StartupTrace::ClearOldLogSessions(); }
bool DiagnosticCommandService::CreatePackage(CString& packagePath, CString& error) const { return StartupTrace::CreateDiagnosticPackage(packagePath, error); }
bool DiagnosticCommandService::IsEnabledForNextLaunch() const { return StartupTrace::IsEnabledForNextLaunch(); }
bool DiagnosticCommandService::SetEnabledForNextLaunch(bool enabled) const { return StartupTrace::SetEnabledForNextLaunch(enabled); }
}
