#include "../../src/fbe/BackupFileCommit.h"
#include <fstream>
#include <string>

static std::wstring Read(const std::wstring& path) { std::wifstream file(path.c_str()); std::wstring value; std::getline(file, value); return value; }
static void Write(const std::wstring& path, const wchar_t* value) { std::wofstream file(path.c_str()); file << value; }
static void Require(bool value) { if (!value) ExitProcess(1); }
static BOOL WINAPI FailReplace(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
static BOOL WINAPI FailUnableToMoveReplacement(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID) { SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT); return FALSE; }
static BOOL WINAPI FailUnableToMoveReplacement2(LPCWSTR destination, LPCWSTR, LPCWSTR backup, DWORD, LPVOID, LPVOID) {
    DeleteFileW(destination);
    if (backup) MoveFileExW(backup, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2); return FALSE;
}
static std::wstring Temp(const wchar_t* name) { wchar_t path[MAX_PATH] = {}; GetTempPath(MAX_PATH, path); return std::wstring(path) + name; }
static void Remove(const std::wstring& path) { DeleteFile(path.c_str()); DeleteFile((path + L".bak").c_str()); }

int wmain()
{
    const std::wstring file = Temp(L"fbe-backup-file-commit-test.fb2");
    const std::wstring temp = file + L".tmp";
    Remove(file);
    Write(temp, L"new"); FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true);
    Require(Read(file) == L"new" && GetFileAttributes((file + L".bak").c_str()) == INVALID_FILE_ATTRIBUTES);
    Remove(file);
    Write(temp, L"new"); FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false);
    Require(Read(file) == L"new" && GetFileAttributes((file + L".bak").c_str()) == INVALID_FILE_ATTRIBUTES);
    Write(file, L"old"); Write(temp, L"new"); FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true);
    Require(Read(file) == L"new" && Read(file + L".bak") == L"old");
    DeleteFile((file + L".bak").c_str()); Write(file, L"old"); Write(temp, L"new"); FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false);
    Require(Read(file) == L"new" && GetFileAttributes((file + L".bak").c_str()) == INVALID_FILE_ATTRIBUTES);
    Write(file, L"old2"); Write(file + L".bak", L"older"); Write(temp, L"new2"); FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true);
    Require(Read(file) == L"new2" && Read(file + L".bak") == L"old2");
    Write(file, L"old3"); Write(file + L".bak", L"previous-backup"); Write(temp, L"new3");
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true, FailReplace); Require(false); }
    catch (const _com_error&) { }
    Require(Read(file) == L"old3" && Read(file + L".bak") == L"previous-backup");
    for (int backup = 0; backup != 2; ++backup) {
        Write(file, L"old1176"); if (backup) Write(file + L".bak", L"backup1176"); else DeleteFileW((file + L".bak").c_str()); Write(temp, L"new1176"); bool keep = false;
        try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), backup != 0, FailUnableToMoveReplacement, &keep); Require(false); } catch (const _com_error&) { }
        Require(keep && Read(file) == L"old1176" && Read(temp) == L"new1176" && (backup ? Read(file + L".bak") == L"backup1176" : GetFileAttributesW((file + L".bak").c_str()) == INVALID_FILE_ATTRIBUTES));
        Write(file, L"old1177"); if (backup) Write(file + L".bak", L"backup1177"); else DeleteFileW((file + L".bak").c_str()); Write(temp, L"new1177"); keep = false;
        try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), backup != 0, FailUnableToMoveReplacement2, &keep); Require(false); } catch (const _com_error&) { }
        Require(keep && Read(temp) == L"new1177" && (backup ? Read(file) == L"backup1177" : GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) && GetFileAttributesW((file + L".bak").c_str()) == INVALID_FILE_ATTRIBUTES);
    }
    Remove(file); DeleteFile(temp.c_str()); return 0;
}
