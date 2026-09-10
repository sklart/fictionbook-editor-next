#include "stdafx.h"
#include "SettingsNormalization.h"
#include "SettingsTypes.h"
#include "..\\utils\\utils.h"

namespace FbeSettings
{
DWORD NormalizeInterfaceLanguageID(DWORD value)
{
	if (value == FBE_INTERFACE_LANGUAGE_AUTO || (value >= FBE_INTERFACE_LANGUAGE_ENGLISH && value <= FBE_INTERFACE_LANGUAGE_BULGARIAN)) return value;
	switch (PRIMARYLANGID(value)) {
	case LANG_RUSSIAN: return FBE_INTERFACE_LANGUAGE_RUSSIAN; case LANG_UKRAINIAN: return FBE_INTERFACE_LANGUAGE_UKRAINIAN;
	case LANG_GERMAN: return FBE_INTERFACE_LANGUAGE_GERMAN; case LANG_FRENCH: return FBE_INTERFACE_LANGUAGE_FRENCH;
	case LANG_SPANISH: return FBE_INTERFACE_LANGUAGE_SPANISH; case LANG_ITALIAN: return FBE_INTERFACE_LANGUAGE_ITALIAN;
	case LANG_POLISH: return FBE_INTERFACE_LANGUAGE_POLISH; case LANG_PORTUGUESE: return FBE_INTERFACE_LANGUAGE_PORTUGUESE;
	case LANG_DUTCH: return FBE_INTERFACE_LANGUAGE_DUTCH; case LANG_CZECH: return FBE_INTERFACE_LANGUAGE_CZECH;
	case LANG_BULGARIAN: return FBE_INTERFACE_LANGUAGE_BULGARIAN; default: return FBE_INTERFACE_LANGUAGE_ENGLISH; }
}

DWORD InterfaceLanguageFromLocaleName(LPCWSTR localeName)
{
	struct Locale { LPCWSTR name; DWORD language; } locales[] = {
		{ L"ru-RU", FBE_INTERFACE_LANGUAGE_RUSSIAN }, { L"uk-UA", FBE_INTERFACE_LANGUAGE_UKRAINIAN }, { L"de-DE", FBE_INTERFACE_LANGUAGE_GERMAN }, { L"fr-FR", FBE_INTERFACE_LANGUAGE_FRENCH }, { L"es-ES", FBE_INTERFACE_LANGUAGE_SPANISH }, { L"it-IT", FBE_INTERFACE_LANGUAGE_ITALIAN }, { L"pl-PL", FBE_INTERFACE_LANGUAGE_POLISH }, { L"pt-PT", FBE_INTERFACE_LANGUAGE_PORTUGUESE }, { L"nl-NL", FBE_INTERFACE_LANGUAGE_DUTCH }, { L"cs-CZ", FBE_INTERFACE_LANGUAGE_CZECH }, { L"bg-BG", FBE_INTERFACE_LANGUAGE_BULGARIAN } };
	if (localeName != NULL) for (size_t index = 0; index < _countof(locales); ++index) if (::lstrcmpiW(localeName, locales[index].name) == 0) return locales[index].language;
	return FBE_INTERFACE_LANGUAGE_ENGLISH;
}

CString NormalizeScriptsFolderStoredPath(const CString& sourcePath)
{
	CString path(sourcePath); path.Trim(); path.Replace(L'/', L'\\'); while(path.GetLength() > 3 && path.Right(1) == L"\\") path.Delete(path.GetLength() - 1); if(!path.IsEmpty() && path.Right(1) != L"\\") path += L"\\"; return path;
}

CString ResolveScriptsFolderPath(const CString& storedPath)
{
	CString path = NormalizeScriptsFolderStoredPath(storedPath); if(!path.IsEmpty() && ::PathIsRelative(path)) path = U::GetProgDir() + path; wchar_t canonical[MAX_PATH] = {}; if(!path.IsEmpty() && ::PathCanonicalize(canonical, path)) path = canonical; return NormalizeScriptsFolderStoredPath(path);
}

DWORD NormalizeImageType(DWORD value) { return value <= 1 ? value : 1; }
DWORD NormalizeJpegQuality(DWORD value) { return value >= 20 && value <= 100 ? value : 75; }
}
