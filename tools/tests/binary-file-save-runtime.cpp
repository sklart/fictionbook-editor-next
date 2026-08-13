#include <windows.h>
#include <atlstr.h>
#include <vector>
#include <stdio.h>

#include "BinaryFileSave.h"

static bool Fail(const wchar_t* message)
{
	fwprintf(stderr, L"%s\n", message);
	return false;
}

static bool ReadBytes(const CString& path, const std::vector<BYTE>& expected)
{
	HANDLE file = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;
	std::vector<BYTE> actual(expected.size());
	DWORD size = ::GetFileSize(file, NULL), read = 0;
	const BOOL readOk = size == expected.size() && (actual.empty() || ::ReadFile(file, &actual[0], size, &read, NULL));
	::CloseHandle(file);
	return readOk && read == size && actual == expected;
}

static bool NoTemporaryFiles(const CString& directory)
{
	WIN32_FIND_DATA findData = {};
	HANDLE find = ::FindFirstFile(directory + L"\\fbe*.tmp", &findData);
	if (find == INVALID_HANDLE_VALUE)
		return ::GetLastError() == ERROR_FILE_NOT_FOUND;
	::FindClose(find);
	return false;
}

int wmain()
{
	wchar_t tempPath[MAX_PATH] = {};
	if (!::GetTempPath(_countof(tempPath), tempPath)) return Fail(L"GetTempPath failed") ? 0 : 1;
	wchar_t uniquePath[MAX_PATH] = {};
	if (!::GetTempFileName(tempPath, L"bfs", 0, uniquePath)) return Fail(L"GetTempFileName failed") ? 0 : 1;
	::DeleteFile(uniquePath);
	if (!::CreateDirectory(uniquePath, NULL)) return Fail(L"CreateDirectory failed") ? 0 : 1;
	const CString directory(uniquePath);
	const CString target = directory + L"\\target.bin";
	const std::vector<BYTE> first = { 0x10, 0x20, 0x30, 0x40 };
	const std::vector<BYTE> second = { 0xFF, 0x00, 0xEE, 0xDD, 0xCC, 0xBB };
	DWORD error = ERROR_SUCCESS;
	bool ok = true;

	// Test A: create and byte-for-byte verification.
	ok = BinaryFileSave::WriteAtomically(target, &first[0], static_cast<DWORD>(first.size()),
		BinaryFileSave::ExistingFilePolicy::ReplaceExisting, &error) &&
		error == ERROR_SUCCESS && ReadBytes(target, first);
	if (!ok) Fail(L"Create test failed");

	// Test B: overwrite must replace every byte, not merely the file size.
	if (ok) ok = BinaryFileSave::WriteAtomically(target, &second[0], static_cast<DWORD>(second.size()),
		BinaryFileSave::ExistingFilePolicy::ReplaceExisting, &error) &&
		error == ERROR_SUCCESS && ReadBytes(target, second);
	if (!ok) Fail(L"Overwrite test failed");

	// Test C: lock the destination so MoveFileEx fails after writing the temp file.
	HANDLE lock = INVALID_HANDLE_VALUE;
	if (ok) lock = ::CreateFile(target, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (ok && lock == INVALID_HANDLE_VALUE) { Fail(L"Could not lock target"); ok = false; }
	if (ok) {
		error = ERROR_SUCCESS;
		const bool writeFailed = !BinaryFileSave::WriteAtomically(target, &first[0], static_cast<DWORD>(first.size()),
			BinaryFileSave::ExistingFilePolicy::ReplaceExisting, &error);
		::CloseHandle(lock);
		lock = INVALID_HANDLE_VALUE;
		ok = writeFailed && error != ERROR_SUCCESS && ReadBytes(target, second) && NoTemporaryFiles(directory);
		if (!ok) Fail(L"Replacement-failure preservation test failed");
	}
	if (lock != INVALID_HANDLE_VALUE) ::CloseHandle(lock);

	// Test D: a batch export must not replace an existing external file.
	if (ok) {
		::DeleteFile(target);
		error = ERROR_SUCCESS;
		ok = BinaryFileSave::WriteAtomically(target, &first[0], static_cast<DWORD>(first.size()),
			BinaryFileSave::ExistingFilePolicy::ReplaceExisting, &error) && error == ERROR_SUCCESS;
		if (ok) {
			error = ERROR_SUCCESS;
			const bool writeFailed = !BinaryFileSave::WriteAtomically(target, &second[0], static_cast<DWORD>(second.size()),
				BinaryFileSave::ExistingFilePolicy::FailIfExists, &error);
			ok = writeFailed && (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) &&
				ReadBytes(target, first) && NoTemporaryFiles(directory);
		}
		if (!ok) Fail(L"Fail-if-exists preservation test failed");
	}

	// Test E: FailIfExists still creates an absent destination.
	if (ok) {
		const CString absent = directory + L"\\absent.bin";
		error = ERROR_SUCCESS;
		ok = BinaryFileSave::WriteAtomically(absent, &second[0], static_cast<DWORD>(second.size()),
			BinaryFileSave::ExistingFilePolicy::FailIfExists, &error) &&
			error == ERROR_SUCCESS && ReadBytes(absent, second);
		if (!ok) Fail(L"Fail-if-exists create test failed");
	}

	// Zero bytes are valid; invalid data and an empty destination are not.
	if (ok) {
		const CString empty = directory + L"\\empty.bin";
		error = ERROR_SUCCESS;
		ok = BinaryFileSave::WriteAtomically(empty, NULL, 0,
			BinaryFileSave::ExistingFilePolicy::ReplaceExisting, &error) && error == ERROR_SUCCESS && ReadBytes(empty, std::vector<BYTE>());
		if (!ok) Fail(L"Zero-byte test failed");
	}
	if (ok) {
		error = ERROR_SUCCESS;
		ok = !BinaryFileSave::WriteAtomically(target, NULL, 1,
			BinaryFileSave::ExistingFilePolicy::ReplaceExisting, &error) && error == ERROR_INVALID_PARAMETER;
		if (!ok) Fail(L"Invalid-data test failed");
	}
	if (ok) {
		error = ERROR_SUCCESS;
		ok = !BinaryFileSave::WriteAtomically(CString(), NULL, 0,
			BinaryFileSave::ExistingFilePolicy::ReplaceExisting, &error) && error == ERROR_INVALID_PARAMETER;
		if (!ok) Fail(L"Empty-destination test failed");
	}

	::DeleteFile(directory + L"\\empty.bin");
	::DeleteFile(directory + L"\\absent.bin");
	::DeleteFile(target);
	::RemoveDirectory(directory);
	return ok ? 0 : 1;
}
