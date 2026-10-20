#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>
#include <shlwapi.h>
static CString g_delayedSource;
static unsigned int g_delayedFailures = 0;
static DWORD g_delayedError = ERROR_SUCCESS;
static BOOL WINAPI MoveWithDelayedReader(LPCWSTR source, LPCWSTR destination, DWORD flags)
{
    if(g_delayedSource == source && g_delayedFailures)
    {
        --g_delayedFailures;
        ::SetLastError(g_delayedError);
        return FALSE;
    }
    return ::MoveFileExW(source, destination, flags);
}
#define MoveFileExW MoveWithDelayedReader
#include "../../src/fbe/settings/SettingsFileTransaction.h"
#undef MoveFileExW

static bool Write(const CString& path, const char* text)
{
    HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const DWORD size = static_cast<DWORD>(strlen(text));
    bool ok = ::WriteFile(file, text, size, &written, NULL) && written == size;
    ::CloseHandle(file); return ok;
}
static bool Equals(const CString& path, const char* text)
{
    HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(file == INVALID_HANDLE_VALUE) return false;
    char bytes[64] = {}; DWORD read = 0;
    const bool ok = ::ReadFile(file, bytes, sizeof(bytes) - 1, &read, NULL) && read == strlen(text) && strcmp(bytes, text) == 0;
    ::CloseHandle(file); return ok;
}
int wmain(int argc, wchar_t** argv)
{
    if(argc != 2) return 1;
    const CString first = CString(argv[1]) + L"\\first.xml", second = CString(argv[1]) + L"\\second.xml";
    if(!Write(first, "old-first") || !Write(second, "old-second")) return 2;
    // Model the observed transient rename denials while publishing real files.
    for(DWORD code : { ERROR_ACCESS_DENIED, ERROR_SHARING_VIOLATION })
    {
        const CString staged = SettingsFileTransaction::Temporary(first);
        if(staged.IsEmpty() || !Write(staged, "reader-released")) return 19;
        g_delayedSource = staged; g_delayedFailures = 2; g_delayedError = code;
        if(!SettingsFileTransaction::MoveStagedFile(staged, first, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ||
            !Equals(first, "reader-released") || ::GetFileAttributesW(staged) != INVALID_FILE_ATTRIBUTES) return 20;
        g_delayedSource.Empty();
        if(!Write(first, "old-first")) return 21;
    }
    // A real persistent reader lock remains an error and preserves both files.
    {
        const CString staged = SettingsFileTransaction::Temporary(first);
        if(staged.IsEmpty() || !Write(staged, "still-locked")) return 22;
        HANDLE reader = ::CreateFileW(staged, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(reader == INVALID_HANDLE_VALUE) return 23;
        const bool moved = SettingsFileTransaction::MoveStagedFile(staged, first, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        const DWORD error = ::GetLastError();
        ::CloseHandle(reader);
        if(moved || (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION) ||
            !Equals(first, "old-first") || !Equals(staged, "still-locked")) return 24;
        ::DeleteFileW(staged);
    }
    // Failure after the first replacement rolls back all previously installed files.
    HANDLE locked = ::CreateFileW(second, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(locked == INVALID_HANDLE_VALUE) return 3;
    bool rejected = false;
    {
        SettingsFileTransaction files;
        CString one = SettingsFileTransaction::Temporary(first), two = SettingsFileTransaction::Temporary(second);
        if(one.IsEmpty() || two.IsEmpty() || !Write(one, "new-first") || !Write(two, "new-second")) return 4;
        files.Add(first, one); files.Add(second, two);
        rejected = !files.Commit();
        if(!Equals(first, "old-first")) return 12;
    }
    ::CloseHandle(locked);
    if(!rejected || !Equals(first, "old-first") || !Equals(second, "old-second")) return 5;
    // Abandoned preparation has no observable effect on the original file.
    {
        SettingsFileTransaction files;
        CString temporary = SettingsFileTransaction::Temporary(first);
        if(temporary.IsEmpty() || !Write(temporary, "cancelled")) return 6;
        files.Add(first, temporary);
    }
    if(!Equals(first, "old-first")) return 7;
    // Read-only files are rejected without moving or changing the original.
    if(!::SetFileAttributesW(first, FILE_ATTRIBUTE_READONLY)) return 13;
    {
        SettingsFileTransaction files;
        CString temporary = SettingsFileTransaction::Temporary(first);
        if(temporary.IsEmpty() || !Write(temporary, "readonly")) return 14;
        files.Add(first, temporary);
        if(files.Commit() || !Equals(first, "old-first")) return 15;
    }
    if(!::SetFileAttributesW(first, FILE_ATTRIBUTE_NORMAL)) return 16;
    // A dictionary destination appearing after staging must never be replaced.
    {
        SettingsFileTransaction files;
        CString temporary = SettingsFileTransaction::Temporary(first);
        if(temporary.IsEmpty() || !Write(temporary, "migrated")) return 17;
        files.Add(first, temporary, true);
        if(files.Commit() || files.Error() != ERROR_FILE_EXISTS || !Equals(first, "old-first")) return 18;
    }
    // Replacement and deletion form one successful batch.
    {
        SettingsFileTransaction files;
        CString temporary = SettingsFileTransaction::Temporary(first);
        if(temporary.IsEmpty() || !Write(temporary, "new-first")) return 8;
        files.Add(first, temporary); files.Add(second);
        if(!files.Commit()) return 9;
    }
    if(!Equals(first, "new-first") || ::GetFileAttributesW(second) != INVALID_FILE_ATTRIBUTES) return 10;
    WIN32_FIND_DATAW found = {}; HANDLE search = ::FindFirstFileW(CString(argv[1]) + L"\\fbe*.tmp", &found);
    if(search != INVALID_HANDLE_VALUE) { ::FindClose(search); return 11; }
    return 0;
}
