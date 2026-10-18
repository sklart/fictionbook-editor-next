#include <windows.h>
#include <string>

static bool failRestore = false;
static BOOL WINAPI PartialReplace(LPCWSTR target, LPCWSTR, LPCWSTR backup, DWORD, LPVOID, LPVOID)
{
    if (!::MoveFileExW(target, backup, MOVEFILE_REPLACE_EXISTING)) return FALSE;
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
