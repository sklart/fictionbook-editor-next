#include "stdafx.h"
#include "ScriptCardParser.h"
#include "ScriptCardBlock.h"
#include "..\..\common\RuntimeLocalizationCommon.h"
#include "..\..\common\DeploymentContext.h"
#include "..\utils\utils.h"
#include <cwctype>
#include <mutex>
#include <set>

namespace ScriptCards {
namespace {
constexpr wchar_t kOpen[] = L"/*<FBE-SCRIPT-CARD version=\"1\">";
constexpr wchar_t kClose[] = L"</FBE-SCRIPT-CARD>*/";
constexpr wchar_t kLegacyOpen[] = L"/*<FBE-NEXT-CARD version=\"1\">";
constexpr wchar_t kLegacyClose[] = L"</FBE-NEXT-CARD>*/";

bool ExtractJson(const std::wstring& text, std::wstring& json)
{
	const bool current = text.compare(0, _countof(kOpen) - 1, kOpen) == 0;
	const wchar_t* open = current ? kOpen : kLegacyOpen;
	const wchar_t* closeMarker = current ? kClose : kLegacyClose;
	const size_t openLength = wcslen(open);
	if(text.compare(0, openLength, open) != 0) return false;
	const size_t close = text.find(closeMarker, openLength);
	if(close == std::wstring::npos) return false;
	json = text.substr(openLength, close - openLength);
	return true;
}

bool ValidateJson(const std::wstring& json, size_t& cursor, int depth)
{
	if(depth > 32) return false;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	if(cursor >= json.size()) return false;
	if(json[cursor] == L'{')
	{
		++cursor;
		std::set<std::wstring> keys;
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
		if(cursor < json.size() && json[cursor] == L'}') { ++cursor; return true; }
		for(;;)
		{
			std::wstring key;
			if(!FbeRuntimeLocalization::JsonParseString(json, cursor, key) || !keys.insert(key).second) return false;
			FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
			if(cursor >= json.size() || json[cursor++] != L':') return false;
			if(!ValidateJson(json, cursor, depth + 1)) return false;
			FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
			if(cursor >= json.size()) return false;
			const wchar_t delimiter = json[cursor++];
			if(delimiter == L'}') return true;
			if(delimiter != L',') return false;
		}
	}
	if(json[cursor] == L'[')
	{
		++cursor;
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
		if(cursor < json.size() && json[cursor] == L']') { ++cursor; return true; }
		for(;;)
		{
			if(!ValidateJson(json, cursor, depth + 1)) return false;
			FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
			if(cursor >= json.size()) return false;
			const wchar_t delimiter = json[cursor++];
			if(delimiter == L']') return true;
			if(delimiter != L',') return false;
		}
	}
	return FbeRuntimeLocalization::JsonSkipValue(json, cursor);
}

bool Member(const std::wstring& json, size_t object, const wchar_t* key, size_t& value)
{
	return FbeRuntimeLocalization::JsonFindObjectMember(json, object, key, value);
}

bool StringMember(const std::wstring& json, size_t object, const wchar_t* key, CString& target, bool allowEmpty = false)
{
	size_t value = 0;
	std::wstring parsed;
	if(!Member(json, object, key, value) || !FbeRuntimeLocalization::JsonParseString(json, value, parsed) ||
		(!allowEmpty && parsed.empty())) return false;
	target = parsed.c_str();
	return true;
}

bool TriMember(const std::wstring& json, size_t object, const wchar_t* key, int& result)
{
	size_t value = 0;
	if(!Member(json, object, key, value)) return false;
	if(json.compare(value, 4, L"true") == 0) result = 1;
	else if(json.compare(value, 5, L"false") == 0) result = 0;
	else if(json.compare(value, 4, L"null") == 0) result = -1;
	else return false;
	return true;
}

bool StringArray(const std::wstring& json, size_t object, const wchar_t* key, std::vector<CString>& result)
{
	size_t cursor = 0;
	if(!Member(json, object, key, cursor) || json[cursor++] != L'[') return false;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	if(cursor < json.size() && json[cursor] == L']') return true;
	for(;;)
	{
		std::wstring value;
		if(!FbeRuntimeLocalization::JsonParseString(json, cursor, value)) return false;
		result.push_back(value.c_str());
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
		if(cursor >= json.size()) return false;
		const wchar_t delimiter = json[cursor++];
		if(delimiter == L']') return true;
		if(delimiter != L',') return false;
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	}
}

bool Authors(const std::wstring& json, size_t object, std::vector<Author>& result)
{
	size_t cursor = 0;
	if(!Member(json, object, L"authors", cursor) || json[cursor++] != L'[') return false;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	if(cursor < json.size() && json[cursor] == L']') return false;
	for(;;)
	{
		const size_t author = cursor;
		Author entry;
		if(!StringMember(json, author, L"role", entry.role)) return false;
		if(entry.role != L"author" && entry.role != L"original_author" && entry.role != L"coauthor" &&
			entry.role != L"modifier" && entry.role != L"contributor" &&
			entry.role != L"ai_assistance" && entry.role != L"unknown") return false;
		if(entry.role != L"unknown" && !StringMember(json, author, L"name", entry.name)) return false;
		if(entry.role == L"unknown") StringMember(json, author, L"name", entry.name, true);
		if(!FbeRuntimeLocalization::JsonSkipValue(json, cursor)) return false;
		result.push_back(entry);
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
		if(cursor >= json.size()) return false;
		const wchar_t delimiter = json[cursor++];
		if(delimiter == L']') return true;
		if(delimiter != L',') return false;
	}
}

CString ValueText(const std::wstring& json, size_t cursor)
{
	std::wstring parsed;
	if(FbeRuntimeLocalization::JsonParseString(json, cursor, parsed)) return parsed.c_str();
	const size_t start = cursor;
	if(!FbeRuntimeLocalization::JsonSkipValue(json, cursor)) return CString();
	return CString(json.substr(start, cursor - start).c_str());
}

CString ObjectField(const std::wstring& json, size_t object, const wchar_t* key)
{
	size_t value = 0;
	return Member(json, object, key, value) ? ValueText(json, value) : CString();
}

bool OptionalDisplayArray(const std::wstring& json, size_t object, const wchar_t* key,
	std::vector<CString>& result)
{
	size_t cursor = 0;
	if(!Member(json, object, key, cursor)) return true;
	if(json[cursor++] != L'[') return false;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	if(cursor < json.size() && json[cursor] == L']') return true;
	for(;;)
	{
		CString display;
		if(json[cursor] == L'{')
		{
			if(wcscmp(key, L"settings") == 0)
			{
				display = ObjectField(json, cursor, L"name");
				const CString value = ObjectField(json, cursor, L"value");
				if(!value.IsEmpty()) display += L" = " + value;
				CString note = ObjectField(json, cursor, L"description");
				if(!note.IsEmpty()) display += L" — " + note;
			}
			else if(wcscmp(key, L"examples") == 0)
			{
				const CString before = ObjectField(json, cursor, L"before");
				const CString after = ObjectField(json, cursor, L"after");
				if(!before.IsEmpty() || !after.IsEmpty()) display = before + L" → " + after;
				const CString note = ObjectField(json, cursor, L"note");
				if(!note.IsEmpty()) display += L"\r\n" + note;
			}
			else if(wcscmp(key, L"links") == 0)
			{
				display = ObjectField(json, cursor, L"title");
				const CString url = ObjectField(json, cursor, L"url");
				if(!url.IsEmpty()) display += (display.IsEmpty() ? CString() : CString(L": ")) + url;
			}
			else if(wcscmp(key, L"changelog") == 0)
			{
				display = ObjectField(json, cursor, L"version");
				const CString description = ObjectField(json, cursor, L"description");
				if(!description.IsEmpty()) display += (display.IsEmpty() ? CString() : CString(L": ")) + description;
			}
		}
		else display = ValueText(json, cursor);
		if(display.IsEmpty()) display = ValueText(json, cursor);
		if(!FbeRuntimeLocalization::JsonSkipValue(json, cursor)) return false;
		result.push_back(display);
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
		if(cursor >= json.size()) return false;
		const wchar_t delimiter = json[cursor++];
		if(delimiter == L']') return true;
		if(delimiter != L',') return false;
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	}
}

bool OptionalSettings(const std::wstring& json, std::vector<Setting>& result)
{
	size_t cursor = 0;
	if(!Member(json, 0, L"settings", cursor)) return true;
	if(json[cursor++] != L'[') return false;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	while(cursor < json.size() && json[cursor] != L']')
	{
		Setting row;
		if(json[cursor] == L'{')
		{
			row.title = ObjectField(json, cursor, L"title");
			row.name = ObjectField(json, cursor, L"name");
			row.expression = ObjectField(json, cursor, L"expression");
			row.valueMeaning = ObjectField(json, cursor, L"valueMeaning");
			row.description = ObjectField(json, cursor, L"description");
			size_t value = 0;
			if(Member(json, cursor, L"value", value))
			{
				std::wstring parsed;
				if(FbeRuntimeLocalization::JsonParseString(json, value, parsed))
				{
					row.value = parsed.c_str();
					row.valueKind = Setting::ValueKind::String;
				}
				else if(json.compare(value, 4, L"true") == 0 || json.compare(value, 5, L"false") == 0)
				{
					row.value = json[value] == L't' ? L"true" : L"false";
					row.valueKind = Setting::ValueKind::Boolean;
				}
				else if(json[value] == L'-' || (json[value] >= L'0' && json[value] <= L'9'))
				{
					row.value = ValueText(json, value);
					row.valueKind = Setting::ValueKind::Number;
				}
			}
			row.sharedSetting = row.expression.Find(L"extracting(") >= 0;
			CString meaning = row.valueMeaning;
			meaning.MakeLower();
			row.defaultValue = row.sharedSetting && row.valueKind != Setting::ValueKind::Missing &&
				(meaning.Find(L"резерв") >= 0 || meaning.Find(L"по умолчанию") >= 0 || meaning.Find(L"default") >= 0 ||
					meaning.Find(L"fallback") >= 0);
			if(row.title.IsEmpty())
			{
				int separator = row.description.Find(L": ");
				int separatorLength = 2;
				if(separator < 0)
				{
					separator = row.description.Find(L" — ");
					separatorLength = 3;
				}
				if(separator > 3 && separator < 80)
				{
					CString candidate = row.description.Left(separator);
					CString lower = candidate;
					lower.MakeLower();
					bool meaningful = lower.Find(L"true") < 0 && lower.Find(L"false") < 0;
					bool hasLetter = false;
					for(int index = 0; index < candidate.GetLength(); ++index)
					{
						if(iswdigit(candidate[index])) meaningful = false;
						if(iswalpha(candidate[index])) hasLetter = true;
					}
					if(meaningful && hasLetter)
					{
						row.title = candidate;
						row.description = row.description.Mid(separator + separatorLength);
					}
				}
			}
			if(row.valueKind == Setting::ValueKind::Boolean)
			{
				CString legend = row.description;
				legend.MakeLower();
				legend.Remove(L' ');
				legend.TrimRight(L".。 ");
				if(legend == L"false—нет,true—да" || legend == L"true—да,false—нет" ||
					legend == L"false-нет,true-да" || legend == L"true-да,false-нет")
					row.description.Empty();
			}
		}
		else row.name = ValueText(json, cursor);
		if(!FbeRuntimeLocalization::JsonSkipValue(json, cursor)) return false;
		if(!row.name.IsEmpty() || !row.title.IsEmpty()) result.push_back(row);
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
		if(cursor >= json.size()) return false;
		if(json[cursor] == L',') { ++cursor; FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor); }
		else if(json[cursor] != L']') return false;
	}
	return cursor < json.size() && json[cursor] == L']';
}

bool DecodeCard(const BYTE* bytes, const ScriptCardBlock::Span& span, std::wstring& text)
{
	const BYTE* start = bytes + span.begin;
	const size_t length = span.end - span.begin;
	if(length >= 4 && start[0] == '/' && start[1] == 0 && start[2] == '*' && start[3] == 0)
	{
		if(length % 2 != 0) return false;
		text.resize(length / 2);
		for(size_t i = 0; i < text.size(); ++i) text[i] = static_cast<wchar_t>(start[2 * i] | (start[2 * i + 1] << 8));
		return true;
	}
	if(length >= 4 && start[0] == 0 && start[1] == '/' && start[2] == 0 && start[3] == '*')
	{
		if(length % 2 != 0) return false;
		text.resize(length / 2);
		for(size_t i = 0; i < text.size(); ++i) text[i] = static_cast<wchar_t>((start[2 * i] << 8) | start[2 * i + 1]);
		return true;
	}
	if(length > INT_MAX) return false;
	int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<LPCCH>(start), static_cast<int>(length), NULL, 0);
	const UINT codepage = count > 0 ? CP_UTF8 : 1251;
	if(count == 0) count = ::MultiByteToWideChar(codepage, 0, reinterpret_cast<LPCCH>(start), static_cast<int>(length), NULL, 0);
	if(count <= 0) return false;
	text.resize(count);
	return ::MultiByteToWideChar(codepage, codepage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
		reinterpret_cast<LPCCH>(start), static_cast<int>(length), &text[0], count) == count;
}

bool ValidDigest(const CString& digest)
{
	if(digest.GetLength() != 64) return false;
	for(int i = 0; i < digest.GetLength(); ++i)
		if(!((digest[i] >= L'0' && digest[i] <= L'9') || (digest[i] >= L'a' && digest[i] <= L'f'))) return false;
	return true;
}

bool ValidRelativePath(const std::wstring& path)
{
	if(path.empty() || path[0] == L'/' || path.find_first_of(L"\\:\0", 0, 3) != std::wstring::npos)
		return false;
	size_t start = 0;
	while(start < path.size())
	{
		const size_t separator = path.find(L'/', start);
		const std::wstring part = path.substr(start, separator == std::wstring::npos ? separator : separator - start);
		if(part.empty() || part == L"." || part == L"..") return false;
		if(separator == std::wstring::npos) return true;
		start = separator + 1;
	}
	return false;
}

bool Dependencies(const std::wstring& json, size_t object, std::vector<Dependency>& result)
{
	size_t cursor = 0;
	if(!Member(json, object, L"dependencies", cursor) || json[cursor++] != L'[') return false;
	FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	if(cursor < json.size() && json[cursor] == L']') return true;
	for(;;)
	{
		const size_t item = cursor;
		Dependency entry;
		std::wstring path;
		size_t value = 0;
		if(!Member(json, item, L"path", value) || !FbeRuntimeLocalization::JsonParseString(json, value, path) ||
			!ValidRelativePath(path) || !Member(json, item, L"optional", value)) return false;
		entry.path = path.c_str();
		if(json.compare(value, 4, L"true") == 0) entry.optional = true;
		else if(json.compare(value, 5, L"false") != 0) return false;
		if(!Member(json, item, L"sha256", value)) return false;
		if(json.compare(value, 4, L"null") != 0)
		{
			std::wstring digest;
			if(!FbeRuntimeLocalization::JsonParseString(json, value, digest)) return false;
			entry.expectedSha256 = digest.c_str();
			if(!ValidDigest(entry.expectedSha256)) return false;
		}
		if(!FbeRuntimeLocalization::JsonSkipValue(json, cursor)) return false;
		result.push_back(entry);
		FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
		if(cursor >= json.size()) return false;
		const wchar_t delimiter = json[cursor++];
		if(delimiter == L']') return true;
		if(delimiter != L',') return false;
	}
}
}

static Card ParseJsonDocument(const std::wstring& json, const BYTE* bytes, size_t length, bool external, bool hashSource = true)
{
	Card card;
	card.state = State::Invalid;
	if(!external && json.find(L"*/") != std::wstring::npos) { card.error = L"Unsafe comment terminator"; return card; }
	size_t cursor = 0;
	if(!ValidateJson(json, cursor, 0)) { card.error = L"Card JSON is invalid or has duplicate keys"; return card; }
	FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	if(cursor != json.size() || json.find_first_not_of(L" \t\r\n") == std::wstring::npos ||
		json[json.find_first_not_of(L" \t\r\n")] != L'{')
		{ card.error = L"Card JSON must be one object"; return card; }
	card.json = json.c_str();
	size_t schema = 0, revision = 0, digest = 0;
	size_t schemaEnd = 0;
	const bool validSchema = Member(json, 0, L"schema", schema) &&
		(schemaEnd = schema, FbeRuntimeLocalization::JsonSkipValue(json, schemaEnd)) &&
		json.substr(schema, schemaEnd - schema) == L"1";
	CString algorithm;
	if(!validSchema ||
		!StringMember(json, 0, L"title", card.title) || !StringMember(json, 0, L"version", card.version) ||
		!StringMember(json, 0, L"summary", card.summary) || !StringMember(json, 0, L"description", card.description) ||
		!StringMember(json, 0, L"scope", card.scope) || !TriMember(json, 0, L"modifiesDocument", card.modifiesDocument) ||
		!TriMember(json, 0, L"requiresSelection", card.requiresSelection) ||
		!StringArray(json, 0, L"usage", card.usage) || !StringArray(json, 0, L"warnings", card.warnings) ||
		!Authors(json, 0, card.authors) || !Dependencies(json, 0, card.dependencies) ||
		!Member(json, 0, L"sourceRevision", revision) || json[revision] != L'{' ||
		!StringMember(json, revision, L"algorithm", algorithm) ||
		algorithm != (external ? L"sha256-raw-js-v1" : L"sha256-exclude-fbe-card-v1") ||
		!StringMember(json, revision, L"reviewStatus", card.reviewStatus) ||
		(card.reviewStatus != L"generated" && card.reviewStatus != L"reviewed" && card.reviewStatus != L"unknown") ||
		!Member(json, revision, L"sha256", digest))
		{ card.error = L"Card schema is invalid"; return card; }
	size_t complete = 0;
	if(Member(json, 0, L"dependenciesComplete", complete))
	{
		if(json.compare(complete, 4, L"true") == 0) card.dependenciesComplete = true;
		else if(json.compare(complete, 5, L"false") != 0)
			{ card.error = L"Card dependency completeness is invalid"; return card; }
	}
	if(Member(json, 0, L"minFbeVersion", complete) &&
		!StringMember(json, 0, L"minFbeVersion", card.minFbeVersion))
		{ card.error = L"Minimum FBE version is invalid"; return card; }
	if(Member(json, 0, L"undo", complete))
	{
		if(json[complete] != L'{') { card.error = L"Undo description is invalid"; return card; }
		card.undoStatus = ObjectField(json, complete, L"status");
		card.undo = ObjectField(json, complete, L"description");
	}
	if(!OptionalDisplayArray(json, 0, L"settings", card.settings) ||
		!OptionalSettings(json, card.settingRows) ||
		!OptionalDisplayArray(json, 0, L"examples", card.examples) ||
		!OptionalDisplayArray(json, 0, L"changelog", card.changelog) ||
		!OptionalDisplayArray(json, 0, L"links", card.links) ||
		(Member(json, 0, L"tags", complete) && !StringArray(json, 0, L"tags", card.tags)))
		{ card.error = L"Optional card fields are invalid"; return card; }
	card.authorshipStatus = ObjectField(json, 0, L"authorshipStatus");
	if(hashSource)
		card.currentSha256 = external ? ScriptCardBlock::Sha256Raw(bytes, length) : ScriptCardBlock::Sha256WithoutCard(bytes, length);
	if(json.compare(digest, 4, L"null") == 0)
	{
		card.state = State::Unchecked;
		return card;
	}
	std::wstring expected;
	if(!FbeRuntimeLocalization::JsonParseString(json, digest, expected))
		{ card.error = L"Expected SHA-256 is invalid"; return card; }
	card.expectedSha256 = expected.c_str();
	if(!ValidDigest(card.expectedSha256)) { card.error = L"Expected SHA-256 is invalid"; return card; }
	if(!hashSource) { card.state = State::Unchecked; return card; }
	card.state = card.currentSha256.IsEmpty() ? State::Unchecked :
		card.currentSha256 == card.expectedSha256 ? State::Current : State::Changed;
	return card;
}

Card Parse(const BYTE* bytes, size_t length)
{
	Card card;
	ScriptCardBlock::Span span;
	const ScriptCardBlock::Status block = ScriptCardBlock::Find(bytes, length, span);
	if(block == ScriptCardBlock::Status::Missing) return card;
	card.state = State::Invalid;
	if(block == ScriptCardBlock::Status::Malformed) { card.error = L"Card markers or size are invalid"; return card; }
	std::wstring text, json;
	if(!DecodeCard(bytes, span, text) || !ExtractJson(text, json))
		{ card.error = L"Card encoding or markers are invalid"; return card; }
	return ParseJsonDocument(json, bytes, length, false);
}

namespace {
CString LongPath(const CString& path)
{
	CString normalized(path);
	normalized.Replace(L'/', L'\\');
	if(normalized.Left(4) == L"\\\\?\\") return normalized;
	if(normalized.Left(2) == L"\\\\") return CString(L"\\\\?\\UNC\\") + normalized.Mid(2);
	if(normalized.GetLength() >= 3 && normalized[1] == L':' && normalized[2] == L'\\') return CString(L"\\\\?\\") + normalized;
	return normalized;
}

bool ReadBytes(const CString& path, std::vector<BYTE>& bytes, size_t limit)
{
	HANDLE file = ::CreateFileW(LongPath(path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	if(file == INVALID_HANDLE_VALUE) return false;
	BY_HANDLE_FILE_INFORMATION info = {}; LARGE_INTEGER size = {};
	bool ok = ::GetFileInformationByHandle(file, &info) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
		::GetFileSizeEx(file, &size) && size.QuadPart >= 0 && static_cast<ULONGLONG>(size.QuadPart) <= limit;
	if(ok) bytes.resize(static_cast<size_t>(size.QuadPart));
	for(size_t offset = 0; ok && offset < bytes.size(); )
	{
		DWORD read = 0;
		ok = ::ReadFile(file, bytes.data() + offset, static_cast<DWORD>((std::min)(bytes.size() - offset, size_t(65536))), &read, NULL) && read > 0;
		offset += read;
	}
	::CloseHandle(file);
	return ok;
}

bool Utf8Json(const std::vector<BYTE>& bytes, std::wstring& json)
{
	size_t start = bytes.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf ? 3 : 0;
	if(bytes.size() <= start || bytes.size() - start > INT_MAX) return false;
	const char* input = reinterpret_cast<const char*>(bytes.data() + start);
	const int length = static_cast<int>(bytes.size() - start);
	const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, length, NULL, 0);
	if(count <= 0) return false;
	json.resize(count);
	return ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, length, &json[0], count) == count;
}

bool EnsureCardDirectories(const CString& root, const CString& relative)
{
	if(::SHCreateDirectoryExW(NULL, root, NULL) != ERROR_SUCCESS && ::GetFileAttributesW(LongPath(root)) == INVALID_FILE_ATTRIBUTES)
		return false;
	CString rootEntry(root); rootEntry.TrimRight(L"\\/");
	const DWORD rootAttributes = ::GetFileAttributesW(LongPath(rootEntry));
	if(rootAttributes == INVALID_FILE_ATTRIBUTES || !(rootAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
		(rootAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
	CString directory(root);
	int begin = 0;
	for(;;)
	{
		const int slash = relative.Find(L'\\', begin);
		if(slash < 0) return true;
		const CString part = relative.Mid(begin, slash - begin);
		if(part.IsEmpty() || part == L"." || part == L".." || part.Find(L':') >= 0) return false;
		directory += part + L"\\";
		if(!::CreateDirectoryW(LongPath(directory), NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) return false;
		CString entry(directory); entry.TrimRight(L"\\/");
		const DWORD attributes = ::GetFileAttributesW(LongPath(entry));
		if(attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
		begin = slash + 1;
	}
}

CString RawDigest(const std::vector<BYTE>& data)
{
	return ScriptCardBlock::Sha256Raw(data.empty() ? reinterpret_cast<const BYTE*>("") : data.data(), data.size());
}

// Prepare a complete sibling first; the final rename never exposes a partial JSON.
bool PlaceCardAtomically(const CString& destination, const std::vector<BYTE>& bytes, const CString* expectedHash)
{
	static volatile LONG sequence = 0;
	const int separator = destination.ReverseFind(L'\\');
	if(separator < 0) return false;
	CString temporary;
	HANDLE file = INVALID_HANDLE_VALUE;
	for(int attempt = 0; attempt < 16 && file == INVALID_HANDLE_VALUE; ++attempt)
	{
		temporary.Format(L"%s.fbe-card-tmp-%lu-%ld", static_cast<LPCWSTR>(destination.Left(separator + 1)),
			static_cast<unsigned long>(::GetCurrentProcessId()), ::InterlockedIncrement(&sequence));
		file = ::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if(file == INVALID_HANDLE_VALUE && ::GetLastError() != ERROR_FILE_EXISTS) return false;
	}
	if(file == INVALID_HANDLE_VALUE) return false;
	bool ok = true;
	for(size_t offset = 0; ok && offset < bytes.size();)
	{
		DWORD written = 0;
		ok = ::WriteFile(file, bytes.data() + offset, static_cast<DWORD>((std::min)(bytes.size() - offset, size_t(65536))), &written, NULL) && written > 0;
		offset += written;
	}
	if(ok) ok = ::FlushFileBuffers(file) != FALSE;
	::CloseHandle(file);
	// Failure injection is restricted to the existing isolated test mode.
	wchar_t testMode[2] = {};
	if(ok && ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) == 1 && testMode[0] == L'1')
	{
		const DWORD needed = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_CARD_FAIL_BEFORE_PUBLISH", nullptr, 0);
		if(needed > 1 && needed <= 32768)
		{
			std::vector<wchar_t> requested(needed);
			if(::GetEnvironmentVariableW(L"FBE_NEXT_TEST_CARD_FAIL_BEFORE_PUBLISH", requested.data(), needed) == needed - 1 &&
				LongPath(requested.data()).CompareNoCase(destination) == 0) ok = false;
		}
	}
	HANDLE locked = INVALID_HANDLE_VALUE;
	if(ok && expectedHash)
	{
		// Deny concurrent writes while re-reading the installed version and replacing it.
		locked = ::CreateFileW(destination, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
		BY_HANDLE_FILE_INFORMATION info = {}; LARGE_INTEGER size = {};
		ok = locked != INVALID_HANDLE_VALUE && ::GetFileInformationByHandle(locked, &info) &&
			!(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && ::GetFileSizeEx(locked, &size) &&
			size.QuadPart >= 0 && size.QuadPart <= 64 * 1024;
		std::vector<BYTE> old;
		if(ok) old.resize(static_cast<size_t>(size.QuadPart));
		for(size_t offset = 0; ok && offset < old.size();)
		{
			DWORD read = 0;
			ok = ::ReadFile(locked, old.data() + offset, static_cast<DWORD>(old.size() - offset), &read, NULL) && read > 0;
			offset += read;
		}
		if(ok) ok = RawDigest(old) == *expectedHash;
	}
	if(ok && expectedHash)
	{
		// ReplaceFile preserves atomicity while the old file is held against
		// writers; MoveFileEx cannot replace it with that sharing mode.
		ok = ::ReplaceFileW(destination, temporary, nullptr, 0, nullptr, nullptr) != FALSE;
	}
	else if(ok) ok = ::MoveFileExW(temporary, destination, MOVEFILE_WRITE_THROUGH) != FALSE;
	if(locked != INVALID_HANDLE_VALUE) ::CloseHandle(locked);
	if(!ok) ::DeleteFileW(temporary);
	return ok;
}
}

CString ExternalCardPath(const CString& scriptPath)
{
	static std::mutex syncMutex;
	const std::lock_guard<std::mutex> guard(syncMutex);
	const CString settings(DeploymentContext::SettingsDirectory().c_str());
	if(settings.IsEmpty()) return CString();
	CString root(settings); root.TrimRight(L"\\/"); root += L"\\ScriptCards\\";
	CString normalizedScript(scriptPath);
	normalizedScript.Replace(L'/', L'\\');
	const CString script = U::GetFullPathName(normalizedScript);
	CString stock = U::GetFullPathName(U::GetProgDir() + L"Scripts\\");
	stock.TrimRight(L"\\/"); stock += L"\\";
	CString relative;
	const bool bundled = script.GetLength() > stock.GetLength() && _wcsnicmp(script, stock, stock.GetLength()) == 0;
	if(bundled)
	{
		relative = script.Mid(stock.GetLength());
		if(relative.GetLength() < 4 || relative.Right(3).CompareNoCase(L".js") != 0) return CString();
		relative = relative.Left(relative.GetLength() - 3) + L".json";
	}
	else
	{
		CString canonical(script); canonical.MakeLower();
		const CString digest = ScriptCardBlock::Sha256Raw(reinterpret_cast<const BYTE*>(canonical.GetString()), canonical.GetLength() * sizeof(wchar_t));
		if(digest.IsEmpty()) return CString();
		// The distinct suffix cannot collide with a bundled *.json card,
		// even if a bundled category happens to be named _external.
		relative = L"_external\\" + digest + L".json.fbe-external";
	}
	if(relative.Find(L'/') >= 0 || relative.Find(L':') >= 0 || !EnsureCardDirectories(root, relative)) return CString();
	const CString destination = LongPath(root + relative);
	if(!bundled) return destination;
	// Portable copies may share one DataPath across processes or sessions.
	// Serialize synchronization for each target, including its manifest update.
	CString lockPath(destination); lockPath.MakeLower();
	const CString lockHash = ScriptCardBlock::Sha256Raw(reinterpret_cast<const BYTE*>(lockPath.GetString()),
		static_cast<size_t>(lockPath.GetLength()) * sizeof(wchar_t));
	if(lockHash.IsEmpty()) return destination;
	const CString mutexName = CString(L"Global\\FBE-ScriptCard-") + lockHash;
	HANDLE namedMutex = ::CreateMutexW(nullptr, FALSE, mutexName);
	if(!namedMutex) return destination;
	const DWORD wait = ::WaitForSingleObject(namedMutex, 2500);
	if(wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) { ::CloseHandle(namedMutex); return destination; }
	struct MutexRelease {
		HANDLE handle;
		~MutexRelease() { ::ReleaseMutex(handle); ::CloseHandle(handle); }
	} releaseMutex = { namedMutex };
	const CString source = U::GetProgDir() + L"ScriptCards\\" + relative;
	std::vector<BYTE> incoming;
	if(!ReadBytes(source, incoming, 64 * 1024)) return destination;
	const CString incomingHash = RawDigest(incoming);
	if(incomingHash.IsEmpty()) return destination;
	CString key(relative); key.MakeLower();
	key = ScriptCardBlock::Sha256Raw(reinterpret_cast<const BYTE*>(key.GetString()), key.GetLength() * sizeof(wchar_t));
	const CString manifest = root + L".fbe-defaults.ini";
	const DWORD manifestAttributes = ::GetFileAttributesW(manifest);
	if(manifestAttributes != INVALID_FILE_ATTRIBUTES && (manifestAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return destination;
	if(::GetFileAttributesW(destination) == INVALID_FILE_ATTRIBUTES)
	{
		if(PlaceCardAtomically(destination, incoming, nullptr)) ::WritePrivateProfileStringW(L"cards", key, incomingHash, manifest);
		return destination;
	}
	std::vector<BYTE> current;
	if(!ReadBytes(destination, current, 64 * 1024))
	{
		const CString pending = destination + L".fbe-new-" + incomingHash.Left(12);
		PlaceCardAtomically(pending, incoming, nullptr);
		::WritePrivateProfileStringW(L"conflicts", key, incomingHash, manifest);
		return destination;
	}
	const CString currentHash = RawDigest(current);
	wchar_t baseline[80] = {};
	::GetPrivateProfileStringW(L"cards", key, L"", baseline, _countof(baseline), manifest);
	if(currentHash == incomingHash)
	{
		// An existing file without a recorded baseline has unknown provenance.
		wchar_t conflict[80] = {};
		if(::GetPrivateProfileStringW(L"conflicts", key, L"", conflict, _countof(conflict), manifest) != 0)
			::WritePrivateProfileStringW(L"conflicts", key, NULL, manifest);
		if(::GetPrivateProfileStringW(L"updates", key, L"", conflict, _countof(conflict), manifest) != 0)
			::WritePrivateProfileStringW(L"updates", key, NULL, manifest);
	}
	else if(CString(baseline) != incomingHash)
	{
		if(baseline[0] != 0 && currentHash == baseline && PlaceCardAtomically(destination, incoming, &currentHash))
		{
			::WritePrivateProfileStringW(L"cards", key, incomingHash, manifest);
			::WritePrivateProfileStringW(L"updates", key, NULL, manifest);
			::WritePrivateProfileStringW(L"conflicts", key, NULL, manifest);
			return destination;
		}
		const CString pending = destination + L".fbe-new-" + incomingHash.Left(12);
		PlaceCardAtomically(pending, incoming, nullptr);
		::WritePrivateProfileStringW(L"conflicts", key, incomingHash, manifest);
	}
	return destination;
}

void SynchronizeBundledCards()
{
	static std::once_flag once;
	std::call_once(once, []() {
		const CString defaults = U::GetProgDir() + L"ScriptCards\\";
		std::vector<CString> directories(1, CString());
		while(!directories.empty())
		{
			const CString relative = directories.back(); directories.pop_back();
			WIN32_FIND_DATAW entry = {};
			HANDLE find = ::FindFirstFileW(LongPath(defaults + relative + L"*"), &entry);
			if(find == INVALID_HANDLE_VALUE) continue;
			do {
				if(wcscmp(entry.cFileName, L".") == 0 || wcscmp(entry.cFileName, L"..") == 0 ||
					(entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
				CString name(relative + entry.cFileName);
				if(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) directories.push_back(name + L"\\");
				else if(name.GetLength() > 5 && name.Right(5).CompareNoCase(L".json") == 0)
					ExternalCardPath(U::GetProgDir() + L"Scripts\\" + name.Left(name.GetLength() - 5) + L".js");
			} while(::FindNextFileW(find, &entry));
			::FindClose(find);
		}
	});
}

Card LoadFile(const CString& path)
{
	Card card;
	HANDLE file = ::CreateFileW(LongPath(path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) { card.state = State::Invalid; card.error = L"Script file cannot be opened"; return card; }
	LARGE_INTEGER size = {};
	if(!::GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > INT_MAX)
		{ ::CloseHandle(file); card.state = State::Invalid; card.error = L"Script file is too large"; return card; }
	std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
	size_t position = 0;
	while(position < bytes.size())
	{
		DWORD read = 0;
		if(!::ReadFile(file, bytes.data() + position, static_cast<DWORD>((std::min)(bytes.size() - position, size_t(65536))), &read, NULL) || read == 0)
			{ ::CloseHandle(file); card.state = State::Invalid; card.error = L"Script file cannot be read"; return card; }
		position += read;
	}
	::CloseHandle(file);
	const CString externalPath = ExternalCardPath(path);
	if(!externalPath.IsEmpty())
	{
		const DWORD attributes = ::GetFileAttributesW(externalPath);
		const DWORD error = attributes == INVALID_FILE_ATTRIBUTES ? ::GetLastError() : ERROR_SUCCESS;
		if(attributes != INVALID_FILE_ATTRIBUTES || (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND))
		{
			std::vector<BYTE> external; std::wstring json;
			if(!ReadBytes(externalPath, external, 64 * 1024) || !Utf8Json(external, json))
				{ card.state = State::Invalid; card.error = L"External card cannot be read as UTF-8 JSON"; return card; }
			return ParseJsonDocument(json, bytes.empty() ? reinterpret_cast<const BYTE*>("") : bytes.data(), bytes.size(), true);
		}
	}
	return Parse(bytes.empty() ? reinterpret_cast<const BYTE*>("") : bytes.data(), bytes.size());
}

CString ReadPreviewSummary(const CString& path)
{
	const CString externalPath = ExternalCardPath(path);
	if(!externalPath.IsEmpty())
	{
		const DWORD attributes = ::GetFileAttributesW(externalPath);
		const DWORD error = attributes == INVALID_FILE_ATTRIBUTES ? ::GetLastError() : ERROR_SUCCESS;
		if(attributes != INVALID_FILE_ATTRIBUTES || (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND))
		{
			std::vector<BYTE> bytes; std::wstring json;
			if(!ReadBytes(externalPath, bytes, 64 * 1024) || !Utf8Json(bytes, json)) return CString();
			const Card preview = ParseJsonDocument(json, nullptr, 0, true, false);
			return preview.state == State::Invalid ? CString() : preview.summary;
		}
	}
	HANDLE file = ::CreateFileW(LongPath(path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) return CString();
	// The card format permits at most 64 KiB, including its markers and BOM.
	std::vector<BYTE> bytes(64 * 1024 + 3);
	DWORD read = 0;
	const bool loaded = ::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, NULL) != FALSE;
	::CloseHandle(file);
	if(!loaded || read == 0) return CString();
	bytes.resize(read);
	ScriptCardBlock::Span span;
	if(ScriptCardBlock::Find(bytes.data(), bytes.size(), span) != ScriptCardBlock::Status::Present)
		return CString();
	std::wstring text;
	std::wstring json;
	if(!DecodeCard(bytes.data(), span, text) || !ExtractJson(text, json)) return CString();
	const Card preview = ParseJsonDocument(json, nullptr, 0, false, false);
	return preview.state == State::Invalid ? CString() : preview.summary;
}
}
