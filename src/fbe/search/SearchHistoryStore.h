#pragma once

#include <vector>
#include "../settings/SettingsFileTransaction.h"

// Per-profile, length-prefixed UTF-16 data; no registry is required in portable mode.
namespace FbeSearchHistory {
constexpr DWORD MaxEntries = 100;
constexpr DWORD MaxCharacters = 32768;
constexpr DWORD MaxBytes = 4 * 1024 * 1024;

inline bool Load(const CString& path, std::vector<CString>& entries)
{
	entries.clear();
	CAtlFile file;
	if (FAILED(file.Create(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING))) return false;
	ULONGLONG size = 0;
	if (FAILED(file.GetSize(size)) || size < 12 || size > MaxBytes) return false;
	std::vector<BYTE> data(static_cast<size_t>(size));
	DWORD read = 0;
	if (FAILED(file.Read(data.data(), static_cast<DWORD>(size), read)) || read != size) return false;
	const BYTE magic[] = { 'F','B','E','H','I','S','T','1' };
	if (memcmp(data.data(), magic, sizeof(magic)) != 0) return false;
	size_t offset = sizeof(magic);
	auto nextDword = [&](DWORD& value) {
		if (offset + sizeof(value) > data.size()) return false;
		memcpy(&value, data.data() + offset, sizeof(value)); offset += sizeof(value); return true;
	};
	DWORD count = 0;
	if (!nextDword(count) || count > MaxEntries) return false;
	std::vector<CString> parsed;
	for (DWORD i = 0; i < count; ++i)
	{
		DWORD chars = 0;
		if (!nextDword(chars) || chars == 0 || chars > MaxCharacters || offset + static_cast<size_t>(chars) * sizeof(wchar_t) > data.size()) return false;
		CString value;
		wchar_t* buffer = value.GetBuffer(static_cast<int>(chars));
		memcpy(buffer, data.data() + offset, chars * sizeof(wchar_t));
		value.ReleaseBufferSetLength(static_cast<int>(chars));
		offset += chars * sizeof(wchar_t);
		for (int index = 0; index < value.GetLength(); ++index) if (value[index] == L'\0') return false;
		parsed.push_back(value);
	}
	if (offset != data.size()) return false;
	entries.swap(parsed); return true;
}

inline bool Save(const CString& path, const std::vector<CString>& entries)
{
	if (entries.size() > MaxEntries) return false;
	std::vector<BYTE> data = { 'F','B','E','H','I','S','T','1' };
	auto append = [&](const void* bytes, size_t size) {
		const BYTE* first = static_cast<const BYTE*>(bytes); data.insert(data.end(), first, first + size);
	};
	const DWORD count = static_cast<DWORD>(entries.size()); append(&count, sizeof(count));
	for (const CString& entry : entries)
	{
		const DWORD chars = static_cast<DWORD>(entry.GetLength());
		if (!chars || chars > MaxCharacters || data.size() + sizeof(chars) + static_cast<size_t>(chars) * sizeof(wchar_t) > MaxBytes) return false;
		append(&chars, sizeof(chars)); append(static_cast<LPCWSTR>(entry), chars * sizeof(wchar_t));
	}
	const CString temporary = SettingsFileTransaction::Temporary(path);
	if (temporary.IsEmpty()) return false;
	SettingsFileTransaction transaction; transaction.Add(path, temporary);
	CAtlFile file;
	if (FAILED(file.Create(temporary, GENERIC_WRITE, 0, CREATE_ALWAYS))) return false;
	if (FAILED(file.Write(data.data(), static_cast<DWORD>(data.size()))) || FAILED(file.Flush())) return false;
	file.Close();
	return transaction.Commit();
}
}
