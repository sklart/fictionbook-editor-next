#include "stdafx.h"
#include "EditorBackgrounds.h"
#include "RuntimeLocalization.h"
#include "utils.h"
#include "..\\common\\RuntimeLocalizationCommon.h"
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
			!ReadString(json, object, L"file", entry.fileName) || !ReadString(json, object, L"theme", entry.theme) ||
			!IsSafeFileName(entry.fileName) || (entry.theme != L"light" && entry.theme != L"dark")) { backgrounds.clear(); return; }
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

bool EditorBackgrounds::IsSupportedLocalImage(const CString& source)
{
	CString path(source); path.Trim();
	if(path.IsEmpty() || ::PathIsRelative(path) || ::PathIsURL(path) || !IsRegularFile(path)) return false;
	return path.Right(4).CompareNoCase(L".png") == 0 || path.Right(4).CompareNoCase(L".jpg") == 0 || path.Right(5).CompareNoCase(L".jpeg") == 0;
}
