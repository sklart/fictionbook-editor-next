#pragma once

#include <windows.h>
#include <atlstr.h>
#include <comdef.h>

namespace FbeBackupFileCommit {

inline void CommitSavedFile(const CString& temporaryFile, const CString& destinationFile, bool createBackupFile,
	BOOL (WINAPI* replaceFile)(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID) = ::ReplaceFileW,
	bool* preserveTemporaryFileOnFailure = NULL)
{
	// Once this helper owns a completed temporary save, keep it on every
	// failure path. ReplaceFile may report a partial state (1176/1177), and
	// the temporary file can then be the only copy of the newly saved book.
	if (preserveTemporaryFileOnFailure) *preserveTemporaryFileOnFailure = true;
	HANDLE temporaryHandle = ::CreateFileW(temporaryFile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (temporaryHandle == INVALID_HANDLE_VALUE)
		throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
	const BOOL flushed = ::FlushFileBuffers(temporaryHandle);
	const DWORD flushError = flushed ? ERROR_SUCCESS : ::GetLastError();
	::CloseHandle(temporaryHandle);
	if (!flushed)
		throw _com_error(HRESULT_FROM_WIN32(flushError));

	const DWORD attributes = ::GetFileAttributesW(destinationFile);
	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		const DWORD attributesError = ::GetLastError();
		if (attributesError != ERROR_FILE_NOT_FOUND && attributesError != ERROR_PATH_NOT_FOUND)
			throw _com_error(HRESULT_FROM_WIN32(attributesError));
		if (!::MoveFileExW(temporaryFile, destinationFile, MOVEFILE_WRITE_THROUGH))
			throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
		if (preserveTemporaryFileOnFailure) *preserveTemporaryFileOnFailure = false;
		return;
	}

	const CString backupFile = destinationFile + L".bak";
	const LPCWSTR backupFilePath = createBackupFile ? static_cast<LPCWSTR>(backupFile) : NULL;
	// ReplaceFile atomically replaces an existing backup itself.  Do not delete
	// it first: a failed replacement must leave the last known good backup intact.
	if (!replaceFile(destinationFile, temporaryFile, backupFilePath, 0, NULL, NULL))
		throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
	if (preserveTemporaryFileOnFailure) *preserveTemporaryFileOnFailure = false;
}

}
