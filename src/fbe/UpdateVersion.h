#pragma once

#include <atlstr.h>

// SemVer 2.0 precedence helpers used by the updater. Build metadata is
// accepted but deliberately ignored while comparing versions.
bool IsValidUpdateVersion(const CString& value);
bool IsValidReleaseTag(const CString& value);
bool IsPrereleaseUpdateVersion(const CString& value);
CString GetUpdateBaseVersion(const CString& value);
int CompareUpdateVersions(const CString& left, const CString& right);
