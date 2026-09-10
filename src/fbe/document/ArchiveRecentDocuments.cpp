#include "stdafx.h"
#include "ArchiveRecentDocuments.h"

namespace
{
const wchar_t* const kKeyPrefix = L"\x1Earchive\t";

bool IsSafeField(const CString& field) { return !field.IsEmpty() && field.FindOneOf(L"\t\r\n") < 0; }

CString FileName(const CString& path)
{
	const int slash = max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
	return slash < 0 ? path : path.Mid(slash + 1);
}

CString DirectoryName(const CString& path)
{
	const int slash = max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
	return slash < 0 ? CString() : path.Left(slash + 1);
}

CString DirectoryContext(const CString& path, int components)
{
	CString directory = DirectoryName(path);
	while (!directory.IsEmpty() && (directory.Right(1) == L"\\" || directory.Right(1) == L"/")) directory = directory.Left(directory.GetLength() - 1);
	if (directory.IsEmpty() || components <= 0) return CString();
	int start = directory.GetLength();
	while (components-- > 0)
	{
		const int slash = max(directory.Left(max(0, start - 1)).ReverseFind(L'\\'), directory.Left(max(0, start - 1)).ReverseFind(L'/'));
		if (slash < 0) { start = 0; break; }
		start = slash + 1;
		if (slash == 0) { start = 0; break; }
	}
	return directory.Mid(start) + L"\\";
}

CString CompactCaptionPartImpl(const CString& value, int limit)
{
	if (value.GetLength() <= limit) return value;
	const int suffixLength = min(max(8, limit / 2), limit - 2);
	const int prefixLength = max(1, limit - suffixLength - 1);
	return value.Left(prefixLength) + L"\x2026" + value.Right(suffixLength);
}
}

namespace FbeArchiveRecentDocuments
{
CString CompactCaptionPart(const CString& value, int limit)
{
	return CompactCaptionPartImpl(value, limit);
}

bool ParseUnsigned(const CString& text, unsigned int& value)
{
	if (text.IsEmpty()) return false;
	wchar_t* end = NULL;
	const unsigned long parsed = wcstoul(text, &end, 10);
	if (end == text.GetString() || *end != L'\0' || parsed > UINT_MAX) return false;
	value = static_cast<unsigned int>(parsed);
	return true;
}

bool IsValidRecord(const Record& record)
{
	const DocumentLocation& location = record.location;
	return location.IsArchive() && location.documentType != FictionBookFileType::Unknown && IsSafeField(location.storagePath) && IsSafeField(location.entryPath);
}

void SplitFields(const CString& line, std::vector<CString>& fields)
{
	fields.clear(); int start = 0;
	for (;;) { const int tab = line.Find(L'\t', start); if (tab < 0) { fields.push_back(line.Mid(start)); return; } fields.push_back(line.Mid(start, tab - start)); start = tab + 1; }
}

CString DisplayName(const std::vector<Record>& records, size_t target)
{
	const DocumentLocation& location = records[target].location;
	CString book = FileName(location.entryPath), archive = FileName(location.storagePath);
	bool sameBook = false;
	for (size_t i = 0; i < records.size(); ++i)
		if (i != target && records[i].location.storagePath.CompareNoCase(location.storagePath) == 0 && FileName(records[i].location.entryPath).CompareNoCase(book) == 0) { sameBook = true; break; }
	if (sameBook && !DirectoryName(location.entryPath).IsEmpty()) book = DirectoryName(location.entryPath) + book;
	CString archiveContext = archive;
	CString result = book + L" \x2014 " + archiveContext;
	unsigned int equal = 0;
	for (size_t i = 0; i < records.size(); ++i)
	{
		CString candidate = FileName(records[i].location.entryPath);
		if (sameBook && !DirectoryName(records[i].location.entryPath).IsEmpty()) candidate = DirectoryName(records[i].location.entryPath) + candidate;
		if ((candidate + L" \x2014 " + FileName(records[i].location.storagePath)).CompareNoCase(result) == 0) ++equal;
	}
	if (equal > 1)
	{
		for (int depth = 1; depth < 32; ++depth)
		{
			const CString candidate = DirectoryContext(location.storagePath, depth);
			bool unique = true;
			for (size_t i = 0; i < records.size(); ++i)
				if (i != target && FileName(records[i].location.entryPath).CompareNoCase(FileName(location.entryPath)) == 0 && FileName(records[i].location.storagePath).CompareNoCase(archive) == 0 && records[i].location.storagePath.CompareNoCase(location.storagePath) != 0 && DirectoryContext(records[i].location.storagePath, depth).CompareNoCase(candidate) == 0) { unique = false; break; }
			archiveContext = candidate + archive;
			if (unique) break;
		}
	}
	result = book + L" \x2014 " + archiveContext;
	CString occurrence;
	if (equal > 1) { occurrence.Format(L" (%u)", location.entryOccurrence + 1); result += occurrence; }
	if (result.GetLength() > 96)
	{
		const int available = max(32, 96 - 3 - occurrence.GetLength());
		int archiveLimit = max(16, available / 3);
		archiveLimit = min(archiveLimit, available - 16);
		const int bookLimit = max(16, available - archiveLimit);
		result = CompactCaptionPartImpl(book, bookLimit) + L" \x2014 " + CompactCaptionPartImpl(archiveContext, archiveLimit) + occurrence;
		if (result.GetLength() > 96) result = CompactCaptionPartImpl(book, 16) + L" \x2014 " + CompactCaptionPartImpl(archiveContext, max(8, 77 - occurrence.GetLength())) + occurrence;
	}
	return result;
}

bool SameIdentity(const DocumentLocation& left, const DocumentLocation& right)
{
	return left.containerKind == right.containerKind && left.storagePath.CompareNoCase(right.storagePath) == 0 && left.entryPath == right.entryPath && left.entryOccurrence == right.entryOccurrence;
}

CString Key(const DocumentLocation& location)
{
	CString key; key.Format(L"%s%u\t%u\t%s\t%s", kKeyPrefix, static_cast<unsigned int>(location.containerKind), location.entryOccurrence, static_cast<LPCWSTR>(location.storagePath), static_cast<LPCWSTR>(location.entryPath));
	return key;
}

bool ParseKey(const CString& key, DocumentLocation& location)
{
	location = DocumentLocation();
	if (key.Left(wcslen(kKeyPrefix)) != kKeyPrefix) return false;
	std::vector<CString> fields; SplitFields(key.Mid(wcslen(kKeyPrefix)), fields);
	unsigned int kind = 0, occurrence = 0;
	if (fields.size() != 4 || !ParseUnsigned(fields[0], kind) || !ParseUnsigned(fields[1], occurrence) || (kind != static_cast<unsigned int>(DocumentContainerKind::Zip) && kind != static_cast<unsigned int>(DocumentContainerKind::Rar))) return false;
	location.containerKind = static_cast<DocumentContainerKind>(kind); location.entryOccurrence = occurrence;
	location.storagePath = fields[2]; location.entryPath = fields[3]; location.documentType = DetectFictionBookFileType(location.entryPath);
	return location.IsArchive() && location.documentType != FictionBookFileType::Unknown;
}
}
