#pragma once

namespace FbeSettings
{
DWORD NormalizeInterfaceLanguageID(DWORD langId);
DWORD InterfaceLanguageFromLocaleName(LPCWSTR localeName);
CString NormalizeScriptsFolderStoredPath(const CString& sourcePath);
CString ResolveScriptsFolderPath(const CString& storedPath);
DWORD NormalizeImageType(DWORD value);
DWORD NormalizeJpegQuality(DWORD value);
}
