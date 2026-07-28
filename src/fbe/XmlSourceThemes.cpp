#include "stdafx.h"
#include "XmlSourceThemes.h"
#include "..\common\RuntimeLocalizationCommon.h"
#include "RuntimeLocalization.h"

namespace
{
CString ThemeString(LPCWSTR key, LPCWSTR fallback)
{
	return FbeLoadRuntimeStringByKey(key, fallback);
}

const wchar_t kThemeSystem[] = L"system-auto";
const wchar_t kThemeFbeLight[] = L"fbe-light";
const wchar_t kThemeFbeDark[] = L"fbe-dark";
const wchar_t kThemeFbeHighContrastLight[] = L"fbe-high-contrast-light";
const wchar_t kThemeFbeHighContrastDark[] = L"fbe-high-contrast-dark";
const wchar_t kThemeHistorical[] = L"fbe-historical";

struct ThemeRecord
{
	XmlSourceThemeInfo info;
	XmlSourceThemeMetadata metadata;
	DWORD colors[XML_SRC_STYLE_TOKEN_COUNT];
};

const ThemeRecord* FindExternalTheme(const CString& id);

const wchar_t* const kStyleTokenNames[XML_SRC_STYLE_TOKEN_COUNT] = {
	L"editor.background", L"editor.foreground", L"editor.selection.background",
	L"editor.selection.foreground", L"editor.currentLine.background", L"editor.caret",
	L"editor.lineNumber", L"editor.lineNumber.active", L"editor.matchingTag.background",
	L"editor.matchingTag.border", L"xml.text", L"xml.tag.name", L"xml.tag.delimiter",
	L"xml.attribute.name", L"xml.attribute.value", L"xml.namespace", L"xml.comment",
	L"xml.entity", L"xml.cdata", L"xml.processingInstruction", L"xml.doctype",
	L"xml.error", L"xml.warning",
};

int GetBuiltInThemeIndex(const CString& id)
{
	if(id.CompareNoCase(kThemeHistorical) == 0) return 0;
	if(id.CompareNoCase(kThemeFbeDark) == 0) return 1;
	if(id.CompareNoCase(kThemeFbeHighContrastLight) == 0) return 3;
	if(id.CompareNoCase(kThemeFbeHighContrastDark) == 0) return 4;
	return 2;
}

const DWORD kBuiltInThemeColors[][XML_SRC_STYLE_TOKEN_COUNT] = {
	// Историческая FBE.
	{
		RGB(255,255,255), RGB(0,0,0), RGB(0,120,215), RGB(255,255,255), RGB(245,245,245),
		RGB(0,0,0), RGB(128,128,128), RGB(0,0,0), RGB(225,240,255), RGB(0,120,215),
		RGB(0,0,0), RGB(128,0,0), RGB(128,128,128), RGB(128,128,0), RGB(0,128,0),
		RGB(0,128,128), RGB(0,128,128), RGB(180,100,0), RGB(128,0,128), RGB(128,0,128),
		RGB(128,0,128), RGB(192,0,0), RGB(128,96,0),
	},
	// FBE Dark.
	{
		RGB(30,30,30), RGB(220,220,220), RGB(38,79,120), RGB(255,255,255), RGB(42,45,46),
		RGB(255,255,255), RGB(133,133,133), RGB(220,220,220), RGB(38,79,120), RGB(86,156,214),
		RGB(220,220,220), RGB(86,156,214), RGB(128,128,128), RGB(156,220,254), RGB(206,145,120),
		RGB(78,201,176), RGB(106,153,85), RGB(215,186,125), RGB(197,134,192), RGB(197,134,192),
		RGB(197,134,192), RGB(244,71,71), RGB(204,167,0),
	},
	// FBE Light: нейтральная схема для XML и больших книжных фрагментов.
	{
		RGB(255,255,255), RGB(32,34,36), RGB(0,104,190), RGB(255,255,255), RGB(247,248,250),
		RGB(32,34,36), RGB(110,118,129), RGB(32,34,36), RGB(223,239,255), RGB(0,104,190),
		RGB(32,34,36), RGB(94,74,154), RGB(120,124,130), RGB(5,99,193), RGB(1,112,75),
		RGB(5,99,193), RGB(103,80,164), RGB(148,95,0), RGB(99,73,152), RGB(99,73,152),
		RGB(99,73,152), RGB(197,34,31), RGB(145,95,0),
	},
	// FBE Высокая контрастность — светлая.
	{
		RGB(255,255,255), RGB(0,0,0), RGB(0,0,128), RGB(255,255,255), RGB(230,240,255),
		RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(255,255,0), RGB(0,0,0),
		RGB(0,0,0), RGB(0,0,128), RGB(0,0,0), RGB(128,0,128), RGB(0,96,0),
		RGB(0,0,128), RGB(0,96,0), RGB(128,64,0), RGB(128,0,128), RGB(128,0,128),
		RGB(128,0,128), RGB(180,0,0), RGB(128,64,0),
	},
	// FBE Высокая контрастность — тёмная.
	{
		RGB(0,0,0), RGB(255,255,255), RGB(0,0,128), RGB(255,255,255), RGB(32,32,32),
		RGB(255,255,255), RGB(255,255,255), RGB(255,255,255), RGB(0,96,96), RGB(255,255,0),
		RGB(255,255,255), RGB(255,255,0), RGB(192,192,192), RGB(0,255,255), RGB(255,160,0),
		RGB(0,255,255), RGB(0,255,128), RGB(255,160,255), RGB(255,160,255), RGB(255,160,255),
		RGB(255,160,255), RGB(255,80,80), RGB(255,255,0),
	},
};

bool IsBuiltInThemeId(const CString& id)
{
	return id.CompareNoCase(kThemeSystem) == 0 || id.CompareNoCase(kThemeFbeLight) == 0 ||
		id.CompareNoCase(kThemeFbeDark) == 0 || id.CompareNoCase(kThemeFbeHighContrastLight) == 0 ||
		id.CompareNoCase(kThemeFbeHighContrastDark) == 0 || id.CompareNoCase(kThemeHistorical) == 0;
}

bool IsValidThemeId(const std::wstring& id)
{
	if(id.empty() || id.size() > 64) return false;
	for(size_t i = 0; i < id.size(); ++i)
	{
		const wchar_t ch = id[i];
		if(!((ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9') || ch == L'-'))
			return false;
	}
	return true;
}

bool ParseHexColor(const std::wstring& text, DWORD& color)
{
	if(text.size() != 7 || text[0] != L'#') return false;
	int rgb[3] = {};
	for(int i = 0; i < 3; ++i)
	{
		const int hi = FbeRuntimeLocalization::JsonHexValue(text[1 + i * 2]);
		const int lo = FbeRuntimeLocalization::JsonHexValue(text[2 + i * 2]);
		if(hi < 0 || lo < 0) return false;
		rgb[i] = (hi << 4) | lo;
	}
	color = RGB(rgb[0], rgb[1], rgb[2]);
	return true;
}

bool ReadJsonStringMember(const std::wstring& json, size_t objectStart, const wchar_t* name, std::wstring& value)
{
	size_t valueStart = 0;
	return FbeRuntimeLocalization::JsonFindObjectMember(json, objectStart, name, valueStart) &&
		FbeRuntimeLocalization::JsonParseString(json, valueStart, value);
}

CString EscapeJsonString(const CString& value)
{
	CString escaped;
	for(int index = 0; index < value.GetLength(); ++index)
	{
		const wchar_t ch = value[index];
		switch(ch)
		{
		case L'\\': escaped += L"\\\\"; break;
		case L'\"': escaped += L"\\\""; break;
		case L'\b': escaped += L"\\b"; break;
		case L'\f': escaped += L"\\f"; break;
		case L'\n': escaped += L"\\n"; break;
		case L'\r': escaped += L"\\r"; break;
		case L'\t': escaped += L"\\t"; break;
		default:
			if(ch < 0x20) { CString item; item.Format(L"\\u%04X", static_cast<unsigned int>(ch)); escaped += item; }
			else escaped += ch;
			break;
		}
	}
	return escaped;
}

bool ReadJsonBooleanMember(const std::wstring& json, size_t objectStart, const wchar_t* name, bool& value)
{
	size_t valueStart = 0;
	if(!FbeRuntimeLocalization::JsonFindObjectMember(json, objectStart, name, valueStart)) return false;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, valueStart);
	if(json.compare(valueStart, 4, L"true") == 0) { value = true; return true; }
	if(json.compare(valueStart, 5, L"false") == 0) { value = false; return true; }
	return false;
}

bool ReadJsonIntegerMember(const std::wstring& json, size_t objectStart, const wchar_t* name, int& value)
{
	size_t valueStart = 0;
	if(!FbeRuntimeLocalization::JsonFindObjectMember(json, objectStart, name, valueStart)) return false;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, valueStart);
	if(valueStart >= json.size() || json[valueStart] < L'0' || json[valueStart] > L'9') return false;
	int result = 0;
	if(json[valueStart] == L'0')
		++valueStart;
	else
		while(valueStart < json.size() && json[valueStart] >= L'0' && json[valueStart] <= L'9')
		{
			if(result > 100000) return false;
			result = result * 10 + (json[valueStart++] - L'0');
		}
	// A JSON integer member must end at a JSON member delimiter.  This rejects
	// decimals, exponents, signs, quoted values and arbitrary suffixes.
	FbeRuntimeLocalization::JsonSkipWhitespace(json, valueStart);
	if(valueStart >= json.size() || (json[valueStart] != L',' && json[valueStart] != L'}')) return false;
	value = result;
	return true;
}

bool ReadOptionalJsonStringMember(const std::wstring& json, size_t objectStart, const wchar_t* name,
	size_t maxLength, CString& output, CString* error)
{
	size_t valueStart = 0;
	if(!FbeRuntimeLocalization::JsonFindObjectMember(json, objectStart, name, valueStart))
		return true;
	std::wstring value;
	if(!FbeRuntimeLocalization::JsonParseString(json, valueStart, value))
	{
		if(error) error->Format(ThemeString(L"fbe.theme.error.invalid_metadata",
			L"Invalid %s value: expected a string."), name);
		return false;
	}
	if(value.size() > maxLength)
	{
		if(error) error->Format(ThemeString(L"fbe.theme.error.metadata_too_long",
			L"Value of %s is too long."), name);
		return false;
	}
	output = value.c_str();
	return true;
}

bool ReadLegacyAnsiThemeFile(const wchar_t* path, std::wstring& text)
{
	HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) return false;
	LARGE_INTEGER size = {};
	if(!::GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024)
	{
		::CloseHandle(file);
		return false;
	}
	std::vector<char> bytes(static_cast<size_t>(size.QuadPart));
	DWORD read = 0;
	const BOOL readOk = ::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, NULL);
	::CloseHandle(file);
	if(!readOk || read != bytes.size()) return false;
	const int wideCount = ::MultiByteToWideChar(1251, 0, bytes.data(), static_cast<int>(bytes.size()), NULL, 0);
	if(wideCount <= 0) return false;
	text.assign(wideCount, L'\0');
	return ::MultiByteToWideChar(1251, 0, bytes.data(), static_cast<int>(bytes.size()), &text[0], wideCount) == wideCount;
}

bool ParseThemeFile(const wchar_t* path, ThemeRecord& record, bool allowLegacyAnsi = false, CString* error = NULL)
{
	// Theme files are data only. Keep malformed or unexpectedly large files from
	// delaying startup; a user can still fix or remove the offending file.
	WIN32_FILE_ATTRIBUTE_DATA attributes = {};
	if(!::GetFileAttributesExW(path, GetFileExInfoStandard, &attributes)) { if(error) *error = ThemeString(L"fbe.theme.error.read", L"Cannot read theme file."); return false; }
	const ULONGLONG fileSize = (static_cast<ULONGLONG>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
	if(fileSize > 1024 * 1024) { if(error) *error = ThemeString(L"fbe.theme.error.too_large", L"Theme file exceeds 1 MB."); return false; }
	std::wstring json;
	if(!FbeRuntimeLocalization::ReadUtf8TextFile(path, json) &&
		(!allowLegacyAnsi || !ReadLegacyAnsiThemeFile(path, json))) { if(error) *error = ThemeString(L"fbe.theme.error.invalid_utf8_json", L"Invalid UTF-8 JSON."); return false; }
	size_t jsonEnd = 0;
	if(!FbeRuntimeLocalization::JsonSkipValue(json, jsonEnd)) { if(error) *error = ThemeString(L"fbe.theme.error.invalid_json", L"Invalid JSON."); return false; }
	FbeRuntimeLocalization::JsonSkipWhitespace(json, jsonEnd);
	if(jsonEnd != json.size()) { if(error) *error = ThemeString(L"fbe.theme.error.trailing_json", L"Unexpected data after the JSON object."); return false; }
	int formatVersion = 0;
	std::wstring id, name, format;
	bool isDark = false;
	size_t colorsStart = 0;
	size_t isDarkStart = 0;
	const bool hasIsDark = FbeRuntimeLocalization::JsonFindObjectMember(json, 0, L"isDark", isDarkStart);
	const bool hasDark = hasIsDark && ReadJsonBooleanMember(json, 0, L"isDark", isDark);
	if(!ReadJsonStringMember(json, 0, L"format", format))
	{
		if(error) *error = ThemeString(L"fbe.theme.error.missing_format", L"Missing required field: format.");
		return false;
	}
	if(format != L"FictionBookEditorNext.CodeTheme")
	{
		if(error) *error = ThemeString(L"fbe.theme.error.invalid_format", L"Unsupported theme format.");
		return false;
	}
	if(!ReadJsonIntegerMember(json, 0, L"formatVersion", formatVersion))
	{
		if(error) *error = ThemeString(L"fbe.theme.error.missing_version", L"Missing required field: formatVersion.");
		return false;
	}
	if(formatVersion != 1)
	{
		if(error) error->Format(ThemeString(L"fbe.theme.error.unsupported_version", L"Unsupported theme format version: %d."), formatVersion);
		return false;
	}
	if(!hasIsDark)
	{
		if(error) *error = ThemeString(L"fbe.theme.error.missing_is_dark", L"Missing required field: isDark.");
		return false;
	}
	if(!hasDark)
	{
		if(error) *error = ThemeString(L"fbe.theme.error.invalid_is_dark", L"Invalid value for isDark.");
		return false;
	}
	if(!ReadJsonStringMember(json, 0, L"id", id))
	{
		if(error) *error = ThemeString(L"fbe.theme.error.missing_id", L"Missing required field: id.");
		return false;
	}
	if(!IsValidThemeId(id))
	{
		if(error) *error = ThemeString(L"fbe.theme.error.invalid_id", L"Invalid theme id.");
		return false;
	}
	if(!ReadJsonStringMember(json, 0, L"name", name) || name.empty() || name.size() > 100)
	{
		if(error) *error = ThemeString(L"fbe.theme.error.invalid_name", L"Missing or invalid theme name.");
		return false;
	}
	if(!FbeRuntimeLocalization::JsonFindObjectMember(json, 0, L"colors", colorsStart))
	{
		if(error) *error = ThemeString(L"fbe.theme.error.missing_colors", L"Missing required field: colors.");
		return false;
	}
	for(int i = 0; i < XML_SRC_STYLE_TOKEN_COUNT; ++i)
	{
		size_t colorStart = 0;
		std::wstring colorText;
		const bool hasColor = FbeRuntimeLocalization::JsonFindObjectMember(json, colorsStart,
			kStyleTokenNames[i], colorStart);
		if(!hasColor && i == XML_SRC_STYLE_XML_COMMENT)
		{
			record.colors[i] = record.colors[XML_SRC_STYLE_XML_TEXT];
			continue;
		}
		if(!hasColor || !FbeRuntimeLocalization::JsonParseString(json, colorStart, colorText) ||
			!ParseHexColor(colorText, record.colors[i]))
		{
			if(error) error->Format(ThemeString(L"fbe.theme.error.invalid_color", L"Invalid required color: %s."), kStyleTokenNames[i]);
			return false;
		}
	}
	record.info.id = id.c_str();
	record.info.name = name.c_str();
	record.info.isDark = isDark;
	record.metadata.isDark = isDark;
	record.metadata.recalculateIsDark = false;
	if(!ReadOptionalJsonStringMember(json, 0, L"baseThemeId", 64, record.metadata.baseThemeId, error) ||
		!ReadOptionalJsonStringMember(json, 0, L"author", 100, record.metadata.author, error) ||
		!ReadOptionalJsonStringMember(json, 0, L"description", 2000, record.metadata.description, error) ||
		!ReadOptionalJsonStringMember(json, 0, L"source", 500, record.metadata.source, error) ||
		!ReadOptionalJsonStringMember(json, 0, L"license", 500, record.metadata.license, error))
		return false;
	if(!record.metadata.baseThemeId.IsEmpty() && !IsValidThemeId(std::wstring(record.metadata.baseThemeId)))
	{
		if(error) *error = ThemeString(L"fbe.theme.error.invalid_base_theme_id", L"Invalid baseThemeId.");
		return false;
	}
	return true;
}

CString GetThemeDirectory()
{
	wchar_t modulePath[MAX_PATH] = {};
	const DWORD length = ::GetModuleFileNameW(NULL, modulePath, _countof(modulePath));
	if(length == 0 || length >= _countof(modulePath) ||
		!FbeRuntimeLocalization::RemoveFileSpec(modulePath) ||
		!FbeRuntimeLocalization::AppendPath(modulePath, _countof(modulePath), L"Themes"))
		return CString();
	return CString(modulePath);
}

bool HasThemeId(const std::vector<ThemeRecord>& themes, const CString& id)
{
	for(size_t i = 0; i < themes.size(); ++i)
		if(themes[i].info.id.CompareNoCase(id) == 0) return true;
	return false;
}

CString GetUserThemeDirectory()
{
	CString directory = U::GetSettingsDir();
	if(directory.IsEmpty()) return CString();
	if(directory.Right(1) != L"\\") directory += L"\\";
	return directory + L"Themes";
}

CString MakeAvailableUserThemeId(const CString& requestedId)
{
	CString base = requestedId;
	// A theme ID is global across both Themes directories.  Imported files with
	// the same ID as a shipped theme must receive a new ID; otherwise the
	// shipped record shadows the user file and makes it impossible to delete.
	if(IsBuiltInThemeId(base) || FindExternalTheme(base) != NULL)
		base = CString(L"user-") + base;
	if(base.IsEmpty()) base = L"user-theme";
	const CString directory = GetUserThemeDirectory();
	for(int suffix = 1; ; ++suffix)
	{
		CString candidate = base;
		if(suffix > 1) candidate.Format(L"%s-%d", base, suffix);
		const CString path = directory + L"\\" + candidate + L".fbetheme";
		if(::GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES && FindExternalTheme(candidate) == NULL)
			return candidate;
	}
}

void LoadThemesFromDirectory(const CString& directory, bool isUser, std::vector<ThemeRecord>& themes)
{
	if(directory.IsEmpty()) return;
	const CString mask = directory + L"\\*.fbetheme";
	WIN32_FIND_DATAW findData = {};
	HANDLE found = ::FindFirstFileW(mask, &findData);
	if(found == INVALID_HANDLE_VALUE) return;
	do
	{
		if((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
		ThemeRecord record = {};
		const CString path = directory + L"\\" + findData.cFileName;
		CString parseError;
		const bool parsed = ParseThemeFile(path, record, false, &parseError);
		if(parsed && !IsBuiltInThemeId(record.info.id) && !HasThemeId(themes, record.info.id))
		{
			record.info.isUser = isUser;
			themes.push_back(record);
		}
		else if(!parsed)
		{
			CString trace = L"FBE XML theme skipped: " + path + L": " + parseError + L"\r\n";
			::OutputDebugStringW(trace);
		}
	}
	while(::FindNextFileW(found, &findData));
	::FindClose(found);
}

std::vector<ThemeRecord> g_externalThemes;
bool g_externalThemesInitialized = false;
std::vector<XmlSourceThemeInfo> g_availableThemes;
bool g_availableThemesInitialized = false;

const std::vector<ThemeRecord>& GetExternalThemes()
{
	if(g_externalThemesInitialized) return g_externalThemes;
	g_externalThemesInitialized = true;
	LoadThemesFromDirectory(GetThemeDirectory(), false, g_externalThemes);
	const CString userDirectory = GetUserThemeDirectory();
	if(userDirectory.CompareNoCase(GetThemeDirectory()) != 0)
		LoadThemesFromDirectory(userDirectory, true, g_externalThemes);
	return g_externalThemes;
}
const ThemeRecord* FindExternalTheme(const CString& id)
{
	const std::vector<ThemeRecord>& themes = GetExternalThemes();
	for(size_t i = 0; i < themes.size(); ++i)
		if(themes[i].info.id.CompareNoCase(id) == 0) return &themes[i];
	return NULL;
}
}

namespace XmlSourceThemes
{
const CString& GetThemeIdForPalette(DWORD palette)
{
	static const CString system(kThemeSystem);
	static const CString light(kThemeFbeLight);
	static const CString dark(kThemeFbeDark);
	static const CString historical(kThemeHistorical);
	switch(palette)
	{
	case XML_SRC_COLOR_PALETTE_SYSTEM: return system;
	case XML_SRC_COLOR_PALETTE_HISTORICAL: return historical;
	case XML_SRC_COLOR_PALETTE_FBE_DARK: return dark;
	// Контрастные схемы были экспериментальными. Старые настройки сохраняем,
	// но прозрачно переводим к поддерживаемым FBE Light/FBE Dark.
	case XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_LIGHT: return light;
	case XML_SRC_COLOR_PALETTE_FBE_HIGH_CONTRAST_DARK: return dark;
	default: return light;
	}
}

DWORD GetPaletteForThemeId(const CString& id)
{
	if(id.CompareNoCase(kThemeSystem) == 0) return XML_SRC_COLOR_PALETTE_SYSTEM;
	if(id.CompareNoCase(kThemeHistorical) == 0) return XML_SRC_COLOR_PALETTE_HISTORICAL;
	if(id.CompareNoCase(kThemeFbeDark) == 0) return XML_SRC_COLOR_PALETTE_FBE_DARK;
	if(id.CompareNoCase(kThemeFbeHighContrastLight) == 0) return XML_SRC_COLOR_PALETTE_FBE_LIGHT;
	if(id.CompareNoCase(kThemeFbeHighContrastDark) == 0) return XML_SRC_COLOR_PALETTE_FBE_DARK;
	return XML_SRC_COLOR_PALETTE_FBE_LIGHT;
}

CString NormalizeThemeId(const CString& id)
{
	if(id.CompareNoCase(kThemeFbeHighContrastLight) == 0)
		return CString(kThemeFbeLight);
	if(id.CompareNoCase(kThemeFbeHighContrastDark) == 0)
		return CString(kThemeFbeDark);
	if(id.CompareNoCase(L"system") == 0) return CString(kThemeSystem);
	if(IsBuiltInThemeId(id) || FindExternalTheme(id) != NULL) return id;
	return CString(kThemeFbeLight);
}

const std::vector<XmlSourceThemeInfo>& GetAvailableThemes()
{
	if(g_availableThemesInitialized) return g_availableThemes;
	g_availableThemesInitialized = true;
	g_availableThemes.push_back({ CString(kThemeSystem), L"Automatic — follow Windows theme", false, false });
	g_availableThemes.push_back({ CString(kThemeFbeLight), L"FBE Light", false, false });
	g_availableThemes.push_back({ CString(kThemeFbeDark), L"FBE Dark", false, true });
	g_availableThemes.push_back({ CString(kThemeHistorical), L"Historical FBE", false, false });
	const std::vector<ThemeRecord>& externalThemes = GetExternalThemes();
	std::vector<XmlSourceThemeInfo> builtInThemes;
	std::vector<XmlSourceThemeInfo> userThemes;
	for(size_t i = 0; i < externalThemes.size(); ++i)
	{
		if(externalThemes[i].info.isUser)
			userThemes.push_back(externalThemes[i].info);
		else
			builtInThemes.push_back(externalThemes[i].info);
	}
	const auto byName = [](const XmlSourceThemeInfo& left, const XmlSourceThemeInfo& right)
	{
		return left.name.CompareNoCase(right.name) < 0;
	};
	std::sort(builtInThemes.begin(), builtInThemes.end(), byName);
	std::sort(userThemes.begin(), userThemes.end(), byName);
	g_availableThemes.insert(g_availableThemes.end(), builtInThemes.begin(), builtInThemes.end());
	for(size_t i = 0; i < userThemes.size(); ++i)
	{
		g_availableThemes.push_back(userThemes[i]);
	}
	return g_availableThemes;
}

bool GetImportThemeId(const CString& sourcePath, CString& id, CString& error)
{
	ThemeRecord record = {};
	id.Empty();
	error.Empty();
	if(!ParseThemeFile(sourcePath, record, true, &error)) return false;
	id = record.info.id;
	return true;
}

bool IsUserTheme(const CString& id)
{
	const ThemeRecord* record = FindExternalTheme(id);
	return record != NULL && record->info.isUser;
}

bool GetThemeMetadata(const CString& id, XmlSourceThemeMetadata& metadata)
{
	const ThemeRecord* record = FindExternalTheme(NormalizeThemeId(id));
	if(record == NULL) return false;
	metadata = record->metadata;
	return true;
}

void ReloadThemes()
{
	// Re-read both directories after import, save, or deletion in this process.
	// Settings dialogs may therefore show the changed list immediately.
	if(g_externalThemesInitialized) g_externalThemes.clear();
	if(g_availableThemesInitialized) g_availableThemes.clear();
	 g_externalThemesInitialized = false;
	 g_availableThemesInitialized = false;
}

bool GetThemeColor(const CString& id, XmlSrcStyleToken token, DWORD& color)
{
	if(token >= XML_SRC_STYLE_TOKEN_COUNT) return false;
	const CString normalized = NormalizeThemeId(id);
	if(normalized.CompareNoCase(kThemeSystem) == 0) return false;
	const ThemeRecord* externalTheme = FindExternalTheme(normalized);
	if(externalTheme != NULL)
	{
		color = externalTheme->colors[token];
		return true;
	}
	color = kBuiltInThemeColors[GetBuiltInThemeIndex(normalized)][token];
	return true;
}

bool ImportThemeFile(const CString& sourcePath, CString& importedId, CString& error, ImportThemeConflictMode conflictMode)
{
	importedId.Empty();
	error.Empty();
	ThemeRecord record = {};
	if(sourcePath.IsEmpty() || !ParseThemeFile(sourcePath, record, true, &error))
	{
			if(error.IsEmpty()) error = ThemeString(L"fbe.theme.error.invalid_file", L"Invalid .fbetheme file.");
		return false;
	}
	const CString directory = GetUserThemeDirectory();
	if(directory.IsEmpty() || (!::CreateDirectoryW(directory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS))
	{
		error = ThemeString(L"fbe.theme.error.create_directory", L"Cannot create the user Themes directory.");
		return false;
	}
	CString importedThemeId = MakeAvailableUserThemeId(record.info.id);
	const ThemeRecord* existing = FindExternalTheme(record.info.id);
	if(conflictMode == IMPORT_THEME_REPLACE_USER && existing != NULL && existing->info.isUser)
		importedThemeId = record.info.id;
	const bool copied = importedThemeId.CompareNoCase(record.info.id) != 0;
	const CString destination = directory + L"\\" + importedThemeId + L".fbetheme";
	CString importedName(record.info.name);
	if(copied) importedName += ThemeString(L"fbe.theme.import.copy_suffix", L" (imported)");
	// Always write a validated UTF-8 file. This also normalizes a manually
	// created Windows-1251 theme instead of leaving an unreadable user copy.
	if(!ExportThemeFile(importedThemeId, importedName, record.colors, destination, error, &record.metadata)) return false;
	const DWORD importedAttributes = ::GetFileAttributesW(destination);
	if(importedAttributes != INVALID_FILE_ATTRIBUTES && (importedAttributes & FILE_ATTRIBUTE_READONLY) != 0)
		::SetFileAttributesW(destination, importedAttributes & ~FILE_ATTRIBUTE_READONLY);
	importedId = importedThemeId;
	ReloadThemes();
	return true;
}

bool DeleteUserTheme(const CString& id, CString& error)
{
	error.Empty();
	const ThemeRecord* record = FindExternalTheme(id);
	const CString userDirectory = GetUserThemeDirectory();
	// Versions prior to unique imported IDs could leave a hidden user copy with
	// the same ID as a shipped theme.  Let deletion remove that legacy copy.
	CString path = userDirectory + L"\\" + id + L".fbetheme";
	if(record == NULL || !record->info.isUser)
	{
		if(::GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
		{
			error = ThemeString(L"fbe.theme.error.delete_only_user", L"Only a user theme can be deleted.");
			return false;
		}
	}
	else
	{
		path = userDirectory + L"\\" + record->info.id + L".fbetheme";
	}
	const DWORD attributes = ::GetFileAttributesW(path);
	if(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0)
		::SetFileAttributesW(path, attributes & ~FILE_ATTRIBUTE_READONLY);
	if(!::DeleteFileW(path))
	{
		error.Format(ThemeString(L"fbe.theme.error.delete_failed", L"Cannot delete the user theme (error %lu)."), ::GetLastError());
		return false;
	}
	ReloadThemes();
	return true;
}

bool ExportThemeFile(const CString& id, const CString& name, const DWORD* colors,
	const CString& destinationPath, CString& error, const XmlSourceThemeMetadata* metadata)
{
	error.Empty();
	if(id.IsEmpty() || name.IsEmpty() || colors == NULL || destinationPath.IsEmpty())
	{
		error = ThemeString(L"fbe.theme.error.incomplete_data", L"Theme data is incomplete.");
		return false;
	}
	const CString escapedName = EscapeJsonString(name);
	CString content;
	const bool calculatedIsDark = (54 * GetRValue(colors[XML_SRC_STYLE_EDITOR_BACKGROUND]) +
		183 * GetGValue(colors[XML_SRC_STYLE_EDITOR_BACKGROUND]) +
		19 * GetBValue(colors[XML_SRC_STYLE_EDITOR_BACKGROUND])) / 256 < 128;
	const bool isDark = metadata != NULL && !metadata->recalculateIsDark ? metadata->isDark : calculatedIsDark;
	content.Format(L"{\r\n  \"format\": \"FictionBookEditorNext.CodeTheme\",\r\n  \"formatVersion\": 1,\r\n  \"id\": \"%s\",\r\n  \"name\": \"%s\",\r\n  \"isDark\": %s,\r\n",
		id, escapedName, isDark ? L"true" : L"false");
	if(metadata != NULL)
	{
		const struct { const wchar_t* key; const CString* value; } fields[] = {
			{ L"baseThemeId", &metadata->baseThemeId }, { L"author", &metadata->author },
			{ L"description", &metadata->description }, { L"source", &metadata->source },
			{ L"license", &metadata->license },
		};
		for(int i = 0; i < _countof(fields); ++i)
			if(!fields[i].value->IsEmpty())
				content.AppendFormat(L"  \"%s\": \"%s\",\r\n", fields[i].key, EscapeJsonString(*fields[i].value));
	}
	content += L"  \"colors\": {\r\n";
	for(int i = 0; i < XML_SRC_STYLE_TOKEN_COUNT; ++i)
	{
		CString color;
		color.Format(L"#%02X%02X%02X", GetRValue(colors[i]), GetGValue(colors[i]), GetBValue(colors[i]));
		content.AppendFormat(L"    \"%s\": \"%s\"%s\r\n", kStyleTokenNames[i], color,
			i + 1 == XML_SRC_STYLE_TOKEN_COUNT ? L"" : L",");
	}
	content += L"  }\r\n}\r\n";
	const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, content, content.GetLength(), NULL, 0, NULL, NULL);
	if(bytes <= 0) { error = ThemeString(L"fbe.theme.error.encode", L"Cannot encode theme."); return false; }
	std::vector<char> utf8(bytes);
	::WideCharToMultiByte(CP_UTF8, 0, content, content.GetLength(), &utf8[0], bytes, NULL, NULL);
	const CString temporary = destinationPath + L".tmp";
	HANDLE file = ::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) { error = ThemeString(L"fbe.theme.error.create_temporary", L"Cannot create temporary theme file."); return false; }
	DWORD written = 0;
	const BOOL saved = ::WriteFile(file, &utf8[0], static_cast<DWORD>(utf8.size()), &written, NULL);
	::CloseHandle(file);
	if(!saved || written != utf8.size() || !::MoveFileExW(temporary, destinationPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		::DeleteFileW(temporary);
		error = ThemeString(L"fbe.theme.error.save", L"Cannot save exported theme.");
		return false;
	}
	return true;
}

bool SaveThemeAsUser(const CString& name, const DWORD* colors, CString& savedId, CString& error,
	const XmlSourceThemeMetadata* metadata)
{
	savedId.Empty();
	error.Empty();
	CString displayName(name);
	displayName.Trim();
	if(displayName.IsEmpty() || displayName.GetLength() > 100 ||
		displayName.Find(L'\r') >= 0 || displayName.Find(L'\n') >= 0)
	{
		error = ThemeString(L"fbe.theme.error.invalid_name", L"Missing or invalid theme name.");
		return false;
	}
	const CString directory = GetUserThemeDirectory();
	if(directory.IsEmpty() || (!::CreateDirectoryW(directory, NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS))
	{
		error = ThemeString(L"fbe.theme.error.create_directory", L"Cannot create the user Themes directory.");
		return false;
	}
	CString id = L"user-theme";
	for(int suffix = 2; ; ++suffix)
	{
		const CString path = directory + L"\\" + id + L".fbetheme";
		if(::GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) break;
		id.Format(L"user-theme-%d", suffix);
	}
	const CString path = directory + L"\\" + id + L".fbetheme";
	if(!ExportThemeFile(id, displayName, colors, path, error, metadata)) return false;
	savedId = id;
	ReloadThemes();
	return true;
}
}
