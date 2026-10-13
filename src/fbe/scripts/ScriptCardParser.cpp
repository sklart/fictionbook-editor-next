#include "stdafx.h"
#include "ScriptCardParser.h"
#include "ScriptCardBlock.h"
#include "..\..\common\RuntimeLocalizationCommon.h"
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
				CString value = ObjectField(json, cursor, L"expression");
				if(value.IsEmpty()) value = ObjectField(json, cursor, L"value");
				if(!value.IsEmpty()) display += L" = " + value;
				CString note = ObjectField(json, cursor, L"description");
				if(!note.IsEmpty()) display += L" — " + note;
				CString meaning = ObjectField(json, cursor, L"valueMeaning");
				if(!meaning.IsEmpty()) display += L"\r\n" + meaning;
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

Card Parse(const BYTE* bytes, size_t length)
{
	Card card;
	ScriptCardBlock::Span span;
	const ScriptCardBlock::Status block = ScriptCardBlock::Find(bytes, length, span);
	if(block == ScriptCardBlock::Status::Missing) return card;
	card.state = State::Invalid;
	if(block == ScriptCardBlock::Status::Malformed) { card.error = L"Card markers or size are invalid"; return card; }
	std::wstring text;
	std::wstring json;
	if(!DecodeCard(bytes, span, text) || !ExtractJson(text, json))
		{ card.error = L"Card encoding or markers are invalid"; return card; }
	if(json.find(L"*/") != std::wstring::npos) { card.error = L"Unsafe comment terminator"; return card; }
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
		!StringMember(json, revision, L"algorithm", algorithm) || algorithm != L"sha256-exclude-fbe-card-v1" ||
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
		card.undo = ObjectField(json, complete, L"description");
		if(card.undo.IsEmpty()) card.undo = ObjectField(json, complete, L"status");
	}
	if(!OptionalDisplayArray(json, 0, L"settings", card.settings) ||
		!OptionalDisplayArray(json, 0, L"examples", card.examples) ||
		!OptionalDisplayArray(json, 0, L"changelog", card.changelog) ||
		!OptionalDisplayArray(json, 0, L"links", card.links) ||
		(Member(json, 0, L"tags", complete) && !StringArray(json, 0, L"tags", card.tags)))
		{ card.error = L"Optional card fields are invalid"; return card; }
	card.currentSha256 = ScriptCardBlock::Sha256WithoutCard(bytes, length);
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
	card.state = card.currentSha256.IsEmpty() ? State::Unchecked :
		card.currentSha256 == card.expectedSha256 ? State::Current : State::Changed;
	return card;
}

Card LoadFile(const CString& path)
{
	Card card;
	HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) { card.state = State::Invalid; card.error = L"Script file cannot be opened"; return card; }
	LARGE_INTEGER size = {};
	if(!::GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > INT_MAX)
		{ ::CloseHandle(file); card.state = State::Invalid; card.error = L"Script file is too large"; return card; }
	std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
	if(bytes.empty()) { ::CloseHandle(file); return card; }
	size_t position = 0;
	while(position < bytes.size())
	{
		DWORD read = 0;
		if(!::ReadFile(file, bytes.data() + position, static_cast<DWORD>((std::min)(bytes.size() - position, size_t(65536))), &read, NULL) || read == 0)
			{ ::CloseHandle(file); card.state = State::Invalid; card.error = L"Script file cannot be read"; return card; }
		position += read;
	}
	::CloseHandle(file);
	return Parse(bytes.data(), bytes.size());
}

CString ReadPreviewSummary(const CString& path)
{
	HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
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
	size_t cursor = 0;
	if(!ValidateJson(json, cursor, 0)) return CString();
	FbeRuntimeLocalization::JsonSkipWhitespace(json, cursor);
	if(cursor != json.size()) return CString();
	CString summary;
	return StringMember(json, 0, L"summary", summary) ? summary : CString();
}
}
