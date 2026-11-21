#include "stdafx.h"
#include "RecoveryStore.h"
#include "../utils/UnsignedDecimal.h"
#include "..\\..\\common\\DeploymentContext.h"
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace {
bool TestFailure(const wchar_t* point)
{
	wchar_t mode[4] = {}, fault[40] = {};
	if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) != 1 || mode[0] != L'1' ||
		!::GetEnvironmentVariableW(L"FBE_NEXT_TEST_RECOVERY_FAILURE", fault, _countof(fault)) || wcscmp(fault, point) != 0) return false;
	::SetEnvironmentVariableW(L"FBE_NEXT_TEST_RECOVERY_FAILURE", nullptr);
	return true;
}
bool SnapshotHash(const CString& path, CString& result)
{
	result.Empty();
	HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	BCRYPT_ALG_HANDLE algorithm = NULL; BCRYPT_HASH_HANDLE hash = NULL;
	bool ok = BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0)) &&
		BCRYPT_SUCCESS(::BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0));
	BYTE buffer[65536], digest[32]; DWORD count = 0;
	while (ok) {
		if (!::ReadFile(file, buffer, sizeof(buffer), &count, NULL)) { ok = false; break; }
		if (!count) break;
		ok = BCRYPT_SUCCESS(::BCryptHashData(hash, buffer, count, 0));
	}
	if (ok) ok = BCRYPT_SUCCESS(::BCryptFinishHash(hash, digest, sizeof(digest), 0));
	if (hash) ::BCryptDestroyHash(hash);
	if (algorithm) ::BCryptCloseAlgorithmProvider(algorithm, 0);
	::CloseHandle(file);
	if (!ok) return false;
	const wchar_t* hex = L"0123456789abcdef";
	for (BYTE value : digest) { result.AppendChar(hex[value >> 4]); result.AppendChar(hex[value & 15]); }
	return true;
}
}

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

bool RecoveryStore::InvalidateArchiveLocation() const
{
	if (::DeleteFileW(ArchiveSidecarPath())) return true;
	const DWORD error = ::GetLastError();
	return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

bool RecoveryStore::WriteArchiveLocation(const DocumentLocation& location) const
{
	CString digest;
	if (!SnapshotHash(SnapshotPath(), digest)) return false;
	if (TestFailure(L"sidecar-create")) return false;
	const CString path = ArchiveSidecarPath(), temporary = path + L".tmp";
	HANDLE file = ::CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	CString text;
	text.Format(L"3\r\n%u\r\n%u\r\n%u\r\n%I64u\r\n%I64u\r\n%s\r\n%s\r\n%s\r\n", static_cast<unsigned int>(location.containerKind), location.entryOccurrence,
		static_cast<unsigned int>(location.documentType), location.containerLastWriteTime, location.containerFileSize,
		static_cast<LPCWSTR>(location.storagePath), static_cast<LPCWSTR>(location.entryPath), static_cast<LPCWSTR>(digest));
	DWORD written = 0;
	bool ok = !TestFailure(L"sidecar-write") && ::WriteFile(file, text.GetString(), text.GetLength() * sizeof(wchar_t), &written, NULL) && written == static_cast<DWORD>(text.GetLength() * sizeof(wchar_t));
	if (ok && (TestFailure(L"sidecar-flush") || !::FlushFileBuffers(file)))
		ok = false;
	::CloseHandle(file);
	if (ok && !TestFailure(L"sidecar-rename") && ::MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
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
	while (!fields.empty() && fields.back().IsEmpty()) fields.pop_back();
	if (fields.size() != 9 || fields[0] != L"3") return false;
	CString digest;
	if (fields[8].GetLength() != 64 || !SnapshotHash(SnapshotPath(), digest) || fields[8] != digest) return false;
	unsigned int kind = 0, occurrence = 0, type = 0;
	unsigned __int64 lastWriteTime = 0, fileSize = 0;
	if (!FbeNumbers::ParseUnsignedDecimal(fields[1], kind) || !FbeNumbers::ParseUnsignedDecimal(fields[2], occurrence) ||
		!FbeNumbers::ParseUnsignedDecimal(fields[3], type) || !FbeNumbers::ParseUnsignedDecimal(fields[4], lastWriteTime) ||
		!FbeNumbers::ParseUnsignedDecimal(fields[5], fileSize)) return false;
	if (kind != static_cast<unsigned int>(DocumentContainerKind::Zip) && kind != static_cast<unsigned int>(DocumentContainerKind::Rar)) return false;
	if (type > static_cast<unsigned int>(FictionBookFileType::Fbd) || fields[6].IsEmpty() || fields[7].IsEmpty()) return false;
	location.containerKind = static_cast<DocumentContainerKind>(kind); location.entryOccurrence = occurrence;
	location.documentType = static_cast<FictionBookFileType>(type); location.containerLastWriteTime = lastWriteTime; location.containerFileSize = fileSize;
	location.storagePath = fields[6]; location.entryPath = fields[7];
	return location.documentType != FictionBookFileType::Unknown;
}
}
