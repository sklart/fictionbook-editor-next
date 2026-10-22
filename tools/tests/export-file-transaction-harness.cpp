#include <windows.h>
#include <string>

static bool failRestore = false;
static BOOL WINAPI PartialReplace(LPCWSTR target, LPCWSTR, LPCWSTR backup, DWORD, LPVOID, LPVOID)
{
    // Retry fixture setup only while both file identities and contents' metadata
    // are unchanged. Never retry the injected partial failure or its rollback.
    const auto snapshot = [](LPCWSTR path, BY_HANDLE_FILE_INFORMATION& info) {
        HANDLE file = ::CreateFileW(path, FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
        if (file == INVALID_HANDLE_VALUE) return false;
        const BOOL read = ::GetFileInformationByHandle(file, &info);
        ::CloseHandle(file);
        return read != FALSE;
    };
    const auto same = [](const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b) {
        return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber && a.nFileIndexHigh == b.nFileIndexHigh &&
            a.nFileIndexLow == b.nFileIndexLow && a.dwFileAttributes == b.dwFileAttributes &&
            a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow &&
            ::CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) == 0 &&
            ::CompareFileTime(&a.ftCreationTime, &b.ftCreationTime) == 0;
    };
    BY_HANDLE_FILE_INFORMATION original = {}, reserved = {};
    const bool captured = snapshot(target, original) && snapshot(backup, reserved);
    for (unsigned attempt = 0;; ++attempt) {
        if (::MoveFileExW(target, backup, MOVEFILE_REPLACE_EXISTING)) break;
        const DWORD error = ::GetLastError();
        BY_HANDLE_FILE_INFORMATION currentOriginal = {}, currentReserved = {};
        const DWORD forbidden = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT;
        if (attempt == 4 || (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) ||
            !captured || !snapshot(target, currentOriginal) || !snapshot(backup, currentReserved) ||
            !same(original, currentOriginal) || !same(reserved, currentReserved) ||
            (original.dwFileAttributes & forbidden) || (reserved.dwFileAttributes & forbidden)) {
            ::SetLastError(error); return FALSE;
        }
        ::Sleep(10u << attempt); // Total delay: 10 + 20 + 40 + 80 = 150 ms.
    }
    ::SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2);
    return FALSE;
}
static BOOL WINAPI RestoreMove(LPCWSTR from, LPCWSTR to, DWORD flags)
{
    if (failRestore) { ::SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return ::MoveFileExW(from, to, flags);
}
#define ReplaceFileW PartialReplace
#define MoveFileExW RestoreMove
#include "../../src/common/ExportFileTransaction.h"
#undef ReplaceFileW
#undef MoveFileExW

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) return 1;
    failRestore = std::wstring(argv[1]) == L"restore-fails";
    FbeExport::FileTransaction transaction;
    std::wstring staged;
    if (FAILED(transaction.Stage(argv[2], staged))) return 2;
    HANDLE file = ::CreateFileW(staged.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return 3;
    DWORD written = 0;
    const BOOL saved = ::WriteFile(file, "replacement", 11, &written, NULL);
    ::CloseHandle(file);
    if (!saved || written != 11) return 4;
    return transaction.Commit() == HRESULT_FROM_WIN32(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2) ? 0 : 5;
}
