#pragma once

#include <windows.h>
#include <atlstr.h>
#include <comdef.h>

namespace FbeBackupFileCommit {

inline void CommitSavedFile(const CString& temporaryFile, const CString& destinationFile, bool createBackupFile,
	BOOL (WINAPI* replaceFile)(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID) = ::ReplaceFileW,
	bool* preserveTemporaryFileOnFailure = NULL, bool overwrite = true)
{
	// Before replacement, failures cannot have partially replaced an existing
	// destination. Let the owner remove its completed but unused temporary.
	if (preserveTemporaryFileOnFailure) *preserveTemporaryFileOnFailure = false;
	HANDLE temporaryHandle = ::CreateFileW(temporaryFile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (temporaryHandle == INVALID_HANDLE_VALUE)
		throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
	const BOOL flushed = ::FlushFileBuffers(temporaryHandle);
	const DWORD flushError = flushed ? ERROR_SUCCESS : ::GetLastError();
	::CloseHandle(temporaryHandle);
	if (!flushed)
		throw _com_error(HRESULT_FROM_WIN32(flushError));
	if (!overwrite) {
		// No existence-check race: MoveFileEx without REPLACE_EXISTING never
		// replaces a concurrently created destination.
		if (!::MoveFileExW(temporaryFile, destinationFile, MOVEFILE_WRITE_THROUGH))
			throw _com_error(HRESULT_FROM_WIN32(::GetLastError()));
		if (preserveTemporaryFileOnFailure) *preserveTemporaryFileOnFailure = false;
		return;
	}

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
	// From this point a partial ReplaceFile failure (notably 1176/1177) may
	// leave this temporary as the only recoverable copy. Keep unknown failures.
	if (preserveTemporaryFileOnFailure) *preserveTemporaryFileOnFailure = true;
	DWORD retryBudget = 150, retryPause = 10;
	while (!replaceFile(destinationFile, temporaryFile, backupFilePath, 0, NULL, NULL)) {
		const DWORD error = ::GetLastError();
		// 1175 leaves both original names in place (ReplaceFile's documented
		// contract). A short-lived delete denial can resolve after a completed
		// copy. Retry only this non-partial, no-backup case; never 1176/1177.
		if (!createBackupFile && error == ERROR_UNABLE_TO_REMOVE_REPLACED && retryBudget) {
			const DWORD pause = retryPause < retryBudget ? retryPause : retryBudget;
			::Sleep(pause);
			retryBudget -= pause; retryPause += 10;
			continue;
		}
		if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION ||
			error == ERROR_LOCK_VIOLATION || error == ERROR_UNABLE_TO_REMOVE_REPLACED ||
			error == ERROR_INVALID_PARAMETER || error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			if (preserveTemporaryFileOnFailure) *preserveTemporaryFileOnFailure = false;
		throw _com_error(HRESULT_FROM_WIN32(error));
	}
	if (preserveTemporaryFileOnFailure) *preserveTemporaryFileOnFailure = false;
}

}
