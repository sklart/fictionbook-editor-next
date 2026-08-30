#include "../../src/fbe/BackupFileCommit.h"
#include <fstream>
#include <string>

static std::wstring Read(const std::wstring& path) { std::wifstream file(path.c_str()); std::wstring value; std::getline(file, value); return value; }
static void Write(const std::wstring& path, const wchar_t* value) { std::wofstream file(path.c_str()); file << value; }
static void Require(bool value) { if (!value) ExitProcess(1); }
static bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }
static BOOL WINAPI FailReplace(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
static std::wstring g_replacedFile;
static BOOL WINAPI FailUnableToMoveReplacement(LPCWSTR destination, LPCWSTR, LPCWSTR backup, DWORD, LPVOID, LPVOID) {
    // 1176: with no backup, the old destination has already gone; with a
    // backup it remains in place. In both cases the replacement stays put.
    if (!backup) DeleteFileW(destination);
    SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT); return FALSE;
}
static BOOL WINAPI FailUnableToMoveReplacement2(LPCWSTR destination, LPCWSTR, LPCWSTR backup, DWORD, LPVOID, LPVOID) {
    // 1177: the old destination was moved away, but the replacement remains.
    if (backup) {
        DeleteFileW(backup);
        MoveFileExW(destination, backup, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    } else MoveFileExW(destination, g_replacedFile.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2); return FALSE;
}
static std::wstring Temp(const wchar_t* name) { wchar_t path[MAX_PATH] = {}; GetTempPath(MAX_PATH, path); return std::wstring(path) + name; }
static void Remove(const std::wstring& path) { DeleteFileW(path.c_str()); DeleteFileW((path + L".bak").c_str()); DeleteFileW((path + L".tmp").c_str()); DeleteFileW((path + L".replaced").c_str()); }

int wmain()
{
    const std::wstring file = Temp(L"fbe-backup-file-commit-test.fb2");
    const std::wstring temp = file + L".tmp";
    g_replacedFile = file + L".replaced";
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
    Remove(file);
    Write(file, L"old1176-no-backup"); Write(temp, L"new1176-no-backup"); bool keep = false;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, FailUnableToMoveReplacement, &keep); Require(false); } catch (const _com_error&) { }
    Require(keep && !Exists(file) && !Exists(file + L".bak") && Read(temp) == L"new1176-no-backup" && !Exists(g_replacedFile)); Remove(file);
    Write(file, L"old1176-backup"); Write(file + L".bak", L"backup1176"); Write(temp, L"new1176-backup"); keep = false;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true, FailUnableToMoveReplacement, &keep); Require(false); } catch (const _com_error&) { }
    Require(keep && Read(file) == L"old1176-backup" && Read(file + L".bak") == L"backup1176" && Read(temp) == L"new1176-backup" && !Exists(g_replacedFile)); Remove(file);
    Write(file, L"old1177-backup"); Write(file + L".bak", L"older1177"); Write(temp, L"new1177-backup"); keep = false;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true, FailUnableToMoveReplacement2, &keep); Require(false); } catch (const _com_error&) { }
    Require(keep && !Exists(file) && Read(file + L".bak") == L"old1177-backup" && Read(temp) == L"new1177-backup" && !Exists(g_replacedFile)); Remove(file);
    Write(file, L"old1177-no-backup"); Write(temp, L"new1177-no-backup"); keep = false;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, FailUnableToMoveReplacement2, &keep); Require(false); } catch (const _com_error&) { }
    Require(keep && !Exists(file) && !Exists(file + L".bak") && Read(temp) == L"new1177-no-backup" && Read(g_replacedFile) == L"old1177-no-backup");
    Remove(file); return 0;
}
