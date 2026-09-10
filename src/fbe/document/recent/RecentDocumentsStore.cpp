#include "stdafx.h"
#include "RecentDocumentsStore.h"
#include "..\\..\\..\\common\\DeploymentContext.h"

namespace
{
const wchar_t* const kArchiveMruVersion = L"FBE-ARCHIVE-MRU\t2";

CString SettingsPath(const wchar_t* name)
{
	return CString(DeploymentContext::SettingsDirectory().c_str()) + name;
}

bool EnsureSettingsDirectory()
{
	const CString directory(DeploymentContext::SettingsDirectory().c_str());
	return ::CreateDirectory(directory, NULL) || ::GetLastError() == ERROR_ALREADY_EXISTS;
}

bool ReadTextFile(const CString& path, CString& content, bool requireEvenLength)
{
	content.Empty();
	HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	const DWORD length = ::GetFileSize(file, NULL);
	if (length == INVALID_FILE_SIZE || length > 64 * 1024 || (requireEvenLength && length % sizeof(wchar_t) != 0)) { ::CloseHandle(file); return false; }
	std::vector<wchar_t> text(length / sizeof(wchar_t) + 1, 0);
	DWORD read = 0;
	const BOOL ok = ::ReadFile(file, &text[0], length, &read, NULL);
	::CloseHandle(file);
	if (!ok || read != length || read % sizeof(wchar_t) != 0) return false;
	content = CString(&text[0]);
	return true;
}

bool WriteTextFile(const CString& path, const std::vector<CString>& lines)
{
	if (!EnsureSettingsDirectory()) return false;
	const CString temporary = path + L".tmp";
	HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	bool ok = true;
	for (size_t i = 0; ok && i < lines.size(); ++i)
	{
		const CString line = lines[i] + L"\r\n";
		DWORD written = 0;
		ok = ::WriteFile(file, line.GetString(), line.GetLength() * sizeof(wchar_t), &written, NULL) &&
			written == static_cast<DWORD>(line.GetLength() * sizeof(wchar_t));
	}
	::FlushFileBuffers(file);
	::CloseHandle(file);
	if (ok && ::MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
	::DeleteFile(temporary);
	return false;
}
}

namespace FbeRecentDocuments
{
void Store::ReadArchiveRecords(std::vector<FbeArchiveRecentDocuments::Record>& records) const
{
	records.clear();
	CString content;
	if (!ReadTextFile(SettingsPath(L"ArchiveMRU.txt"), content, true)) return;
	int position = 0;
	bool versioned = false;
	while (position >= 0)
	{
		CString line = content.Tokenize(L"\n", position);
		line.TrimRight(L"\r");
		if (!versioned && line == kArchiveMruVersion) { versioned = true; continue; }
		std::vector<CString> fields;
		FbeArchiveRecentDocuments::SplitFields(line, fields);
		FbeArchiveRecentDocuments::Record record;
		unsigned int occurrence = 0;
		if (versioned)
		{
			unsigned int kind = 0;
			if (fields.size() != 4 || !FbeArchiveRecentDocuments::ParseUnsigned(fields[0], kind) || !FbeArchiveRecentDocuments::ParseUnsigned(fields[1], occurrence) ||
				(kind != static_cast<unsigned int>(DocumentContainerKind::Zip) && kind != static_cast<unsigned int>(DocumentContainerKind::Rar))) continue;
			record.location.containerKind = static_cast<DocumentContainerKind>(kind);
			record.location.storagePath = fields[2];
			record.location.entryPath = fields[3];
		}
		else
		{
			if (fields.size() != 3 || !FbeArchiveRecentDocuments::ParseUnsigned(fields[1], occurrence)) continue;
			record.location.storagePath = fields[0];
			record.location.entryPath = fields[2];
			record.location.containerKind = DetectDocumentContainerKind(record.location.storagePath);
		}
		record.location.entryOccurrence = occurrence;
		record.location.documentType = DetectFictionBookFileType(record.location.entryPath);
		if (FbeArchiveRecentDocuments::IsValidRecord(record)) records.push_back(record);
	}
}

bool Store::WriteArchiveRecords(const std::vector<FbeArchiveRecentDocuments::Record>& records) const
{
	std::vector<CString> lines;
	lines.push_back(kArchiveMruVersion);
	for (size_t i = 0; i < records.size() && i < 16; ++i)
	{
		const DocumentLocation& location = records[i].location;
		CString line;
		line.Format(L"%u\t%u\t%s\t%s", static_cast<unsigned int>(location.containerKind), location.entryOccurrence,
			static_cast<LPCWSTR>(location.storagePath), static_cast<LPCWSTR>(location.entryPath));
		lines.push_back(line);
	}
	return WriteTextFile(SettingsPath(L"ArchiveMRU.txt"), lines);
}

void Store::ReadOrder(std::vector<CString>& order) const
{
	order.clear();
	CString content;
	if (!ReadTextFile(SettingsPath(L"MRUOrder.txt"), content, true)) return;
	int position = 0;
	while (position >= 0)
	{
		CString item = content.Tokenize(L"\n", position);
		item.TrimRight(L"\r");
		if (!item.IsEmpty() && item.FindOneOf(L"\r\n") < 0) order.push_back(item);
	}
}

bool Store::WriteOrder(const std::vector<CString>& order) const
{
	std::vector<CString> limited(order.begin(), order.begin() + min(static_cast<size_t>(10), order.size()));
	return WriteTextFile(SettingsPath(L"MRUOrder.txt"), limited);
}

void Store::ReadPortable(std::vector<CString>& entries) const
{
	entries.clear();
	CString content;
	if (!ReadTextFile(SettingsPath(L"MRU.xml"), content, false)) return;
	int position = 0;
	while (position >= 0)
	{
		CString line = content.Tokenize(L"\n", position);
		line.Trim();
		if (!line.IsEmpty()) entries.push_back(line);
	}
}

bool Store::WritePortable(const std::vector<CString>& entries) const
{
	const CString path = SettingsPath(L"MRU.xml");
	if (entries.empty())
	{
		::DeleteFile(path + L".tmp");
		return ::DeleteFile(path) || ::GetLastError() == ERROR_FILE_NOT_FOUND;
	}
	return WriteTextFile(path, entries);
}
}
