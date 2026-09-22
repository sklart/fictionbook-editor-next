#include "stdafx.h"
#include "EditorBackgrounds.h"
#include "..\\ThemeManager.h"
#include "..\\RuntimeLocalization.h"
#include "..\\utils\\utils.h"
#include "..\\..\\common\\RuntimeLocalizationCommon.h"
#include <string>

namespace {
bool ReadString(const std::wstring& json, size_t object, const wchar_t* name, CString& value)
{
	size_t start = 0; std::wstring result;
	return FbeRuntimeLocalization::JsonFindObjectMember(json, object, name, start) &&
		FbeRuntimeLocalization::JsonParseString(json, start, result) && !(value = result.c_str()).IsEmpty();
}

bool IsSafeFileName(const CString& value)
{
	return !value.IsEmpty() && value.Find(L"..") < 0 && value.FindOneOf(L"\\/:?#%") < 0 &&
		value.Right(4).CompareNoCase(L".png") == 0;
}

bool IsSafeLocalizationKey(const CString& value)
{
	return value.Left(38) == L"fbe.settings.editor_background.preset." &&
		value.SpanIncluding(L"abcdefghijklmnopqrstuvwxyz0123456789._").GetLength() == value.GetLength();
}

bool ParseCssColor(const CString& value, COLORREF& color)
{
	if(value.GetLength() != 7 || value[0] != L'#') return false;
	unsigned int red = 0, green = 0, blue = 0;
	if(swscanf_s(value, L"#%2x%2x%2x", &red, &green, &blue) != 3) return false;
	color = RGB(red, green, blue);
	return true;
}

bool IsRegularFile(const CString& path)
{
	const DWORD attributes = ::GetFileAttributes(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IsSchemaVersionOne(const std::wstring& json, size_t valueStart)
{
	if(valueStart >= json.size() || json[valueStart] != L'1') return false;
	size_t end = valueStart + 1; FbeRuntimeLocalization::JsonSkipWhitespace(json, end);
	return end < json.size() && (json[end] == L',' || json[end] == L'}');
}
}

void EditorBackgrounds::Load(std::vector<EditorBackgroundDescriptor>& backgrounds)
{
	backgrounds.clear();
	std::wstring json; const CString manifest = U::GetProgDirFile(L"EditorBackgrounds\\backgrounds.json");
	if(!FbeRuntimeLocalization::ReadUtf8TextFile(manifest, json)) return;
	size_t schema = 0; FbeRuntimeLocalization::JsonSkipWhitespace(json, schema);
	if(!FbeRuntimeLocalization::JsonFindObjectMember(json, schema, L"schemaVersion", schema) || !IsSchemaVersionOne(json, schema)) return;
	size_t array = 0;
	if(!FbeRuntimeLocalization::JsonFindObjectMember(json, 0, L"backgrounds", array)) return;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
	if(array >= json.size() || json[array++] != L'[') return;
	for(;;)
	{
		FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
		if(array >= json.size() || json[array] == L']') break;
		const size_t object = array;
		if(!FbeRuntimeLocalization::JsonSkipValue(json, array)) { backgrounds.clear(); return; }
		EditorBackgroundDescriptor entry;
		if(!ReadString(json, object, L"id", entry.id) || !ReadString(json, object, L"name", entry.name) ||
			!ReadString(json, object, L"localizationKey", entry.localizationKey) ||
			!ReadString(json, object, L"file", entry.fileName) || !ReadString(json, object, L"theme", entry.theme) ||
			!ReadString(json, object, L"fallbackColor", entry.fallbackColor) || !ReadString(json, object, L"recommendedTextColor", entry.recommendedTextColor) ||
			!IsSafeFileName(entry.fileName) || !IsSafeLocalizationKey(entry.localizationKey) ||
			(entry.theme != L"light" && entry.theme != L"dark")) { backgrounds.clear(); return; }
		COLORREF fallback = 0, text = 0;
		if(!ParseCssColor(entry.fallbackColor, fallback) || !ParseCssColor(entry.recommendedTextColor, text)) { backgrounds.clear(); return; }
		bool duplicate = false;
		for(size_t i = 0; i < backgrounds.size(); ++i) duplicate |= backgrounds[i].id == entry.id;
		if(duplicate) { backgrounds.clear(); return; }
		backgrounds.push_back(entry);
		FbeRuntimeLocalization::JsonSkipWhitespace(json, array);
		if(array < json.size() && json[array] == L',') { ++array; continue; }
		if(array < json.size() && json[array] == L']') break;
		backgrounds.clear(); return;
	}
}

bool EditorBackgrounds::ResolveBuiltIn(const CString& id, CString& filePath)
{
	std::vector<EditorBackgroundDescriptor> backgrounds; Load(backgrounds);
	for(size_t i = 0; i < backgrounds.size(); ++i) if(backgrounds[i].id == id)
	{
		filePath = U::GetProgDirFile(L"EditorBackgrounds\\") + backgrounds[i].fileName;
		return IsRegularFile(filePath);
	}
	return false;
}

bool EditorBackgrounds::GetBuiltInRecommendedColors(const CString& id, COLORREF& fallbackColor, COLORREF& textColor)
{
	std::vector<EditorBackgroundDescriptor> backgrounds; Load(backgrounds);
	for(size_t i = 0; i < backgrounds.size(); ++i)
		if(backgrounds[i].id == id)
			return ParseCssColor(backgrounds[i].fallbackColor, fallbackColor) &&
				ParseCssColor(backgrounds[i].recommendedTextColor, textColor);
	return false;
}

EditorBackgroundColors EditorBackgrounds::ResolveBodyColors(DWORD configuredForeground, DWORD configuredBackground,
	const CString& backgroundKind, const CString& backgroundId, bool highContrast)
{
	EditorBackgroundColors colors = {
		configuredForeground == CLR_DEFAULT ? ::GetSysColor(COLOR_WINDOWTEXT) : static_cast<COLORREF>(configuredForeground),
		configuredBackground == CLR_DEFAULT ? ::GetSysColor(COLOR_WINDOW) : static_cast<COLORREF>(configuredBackground)
	};
	if(highContrast) return colors;
	if(backgroundKind == L"none")
	{
		if(configuredForeground == CLR_DEFAULT && configuredBackground == CLR_DEFAULT && ThemeManager::IsDark())
		{
			colors.foreground = ThemeManager::TextColor();
			colors.background = ThemeManager::WindowColor();
		}
		return colors;
	}
	if(backgroundKind == L"builtin")
	{
		COLORREF fallback = 0, text = 0;
		if(GetBuiltInRecommendedColors(backgroundId, fallback, text))
		{
			if(configuredForeground == CLR_DEFAULT) colors.foreground = text;
			if(configuredBackground == CLR_DEFAULT) colors.background = fallback;
		}
	}
	return colors;
}

bool EditorBackgrounds::IsSupportedLocalImage(const CString& source)
{
	CString path(source); path.Trim();
	if(path.IsEmpty() || ::PathIsRelative(path) || ::PathIsURL(path) || !IsRegularFile(path)) return false;
	return path.Right(4).CompareNoCase(L".png") == 0 || path.Right(4).CompareNoCase(L".jpg") == 0 || path.Right(5).CompareNoCase(L".jpeg") == 0;
}
