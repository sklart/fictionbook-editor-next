#include "stdafx.h"
#include "RecoveryStore.h"
#include "..\\..\\common\\DeploymentContext.h"

namespace FbeRecovery
{
CString RecoveryStore::SnapshotPath() const
{
	CString directory(DeploymentContext::RecoveryDirectory().c_str());
	::CreateDirectory(directory, NULL);
	return directory + L"Recovery.fb2";
}

CString RecoveryStore::ArchiveSidecarPath() const
{
	return CString(DeploymentContext::RecoveryDirectory().c_str()) + L"Recovery.archive.txt";
}

bool RecoveryStore::HasSnapshot() const
{
	const DWORD attributes = ::GetFileAttributes(SnapshotPath());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void RecoveryStore::DeleteFiles() const
{
	::DeleteFile(SnapshotPath());
	::DeleteFile(ArchiveSidecarPath());
}

bool RecoveryStore::WriteArchiveLocation(const DocumentLocation& location) const
{
	const CString path = ArchiveSidecarPath(), temporary = path + L".tmp";
	HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	CString text;
	text.Format(L"2\r\n%u\r\n%u\r\n%u\r\n%I64u\r\n%I64u\r\n%s\r\n%s\r\n", static_cast<unsigned int>(location.containerKind), location.entryOccurrence,
		static_cast<unsigned int>(location.documentType), location.containerLastWriteTime, location.containerFileSize,
		static_cast<LPCWSTR>(location.storagePath), static_cast<LPCWSTR>(location.entryPath));
	DWORD written = 0;
	const bool ok = ::WriteFile(file, text.GetString(), text.GetLength() * sizeof(wchar_t), &written, NULL) && written == static_cast<DWORD>(text.GetLength() * sizeof(wchar_t));
	::FlushFileBuffers(file); ::CloseHandle(file);
	if (ok && ::MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
	::DeleteFile(temporary); return false;
}

bool RecoveryStore::ReadArchiveLocation(DocumentLocation& location) const
{
	location = DocumentLocation();
	HANDLE file = ::CreateFile(ArchiveSidecarPath(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	const DWORD length = ::GetFileSize(file, NULL);
	if (length == INVALID_FILE_SIZE || length > 16 * 1024 || (length % sizeof(wchar_t)) != 0) { ::CloseHandle(file); return false; }
	std::vector<wchar_t> text(length / sizeof(wchar_t) + 1, 0); DWORD read = 0;
	const BOOL ok = ::ReadFile(file, &text[0], length, &read, NULL); ::CloseHandle(file);
	if (!ok || read != length) return false;
	std::vector<CString> fields; int position = 0;
	while (position >= 0) { CString field = CString(&text[0]).Tokenize(L"\n", position); field.TrimRight(L"\r"); fields.push_back(field); }
	if (fields.size() < 8 || fields[0] != L"2") return false;
	const unsigned long kind = wcstoul(fields[1], NULL, 10), occurrence = wcstoul(fields[2], NULL, 10), type = wcstoul(fields[3], NULL, 10);
	if (kind != static_cast<unsigned long>(DocumentContainerKind::Zip) && kind != static_cast<unsigned long>(DocumentContainerKind::Rar)) return false;
	if (occurrence > UINT_MAX || type > static_cast<unsigned long>(FictionBookFileType::Fbd) || fields[6].IsEmpty() || fields[7].IsEmpty()) return false;
	location.containerKind = static_cast<DocumentContainerKind>(kind); location.entryOccurrence = static_cast<unsigned int>(occurrence);
	location.documentType = static_cast<FictionBookFileType>(type); location.containerLastWriteTime = _wcstoui64(fields[4], NULL, 10); location.containerFileSize = _wcstoui64(fields[5], NULL, 10);
	location.storagePath = fields[6]; location.entryPath = fields[7];
	return location.documentType != FictionBookFileType::Unknown;
}
}
