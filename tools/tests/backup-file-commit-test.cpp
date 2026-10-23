#include "../../src/fbe/BackupFileCommit.h"
#include <fstream>
#include <string>
#include <thread>
#include <iterator>

static std::wstring Read(const std::wstring& path) { std::wifstream file(path.c_str()); std::wstring value; std::getline(file, value); return value; }
static void Write(const std::wstring& path, const wchar_t* value) { std::wofstream file(path.c_str()); file << value; }
static void Require(bool value) { if (!value) ExitProcess(1); }
static bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }
static std::string ReadBytes(const std::wstring& path) { std::ifstream file(path.c_str(), std::ios::binary); return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()); }
static std::string g_oldBytes, g_newBytes, g_backupBytes;
static unsigned g_replaceCalls = 0;
static BOOL WINAPI FailRemoveReplaced(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID) {
    ++g_replaceCalls; SetLastError(ERROR_UNABLE_TO_REMOVE_REPLACED); return FALSE;
}
static BOOL WINAPI TransientRemoveReplaced(LPCWSTR destination, LPCWSTR temporary, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved) {
    ++g_replaceCalls;
    Require(ReadBytes(destination) == g_oldBytes && ReadBytes(temporary) == g_newBytes && backup == NULL &&
        ReadBytes(std::wstring(destination) + L".bak") == g_backupBytes);
    if (g_replaceCalls <= 2) { SetLastError(ERROR_UNABLE_TO_REMOVE_REPLACED); return FALSE; }
    return ::ReplaceFileW(destination, temporary, backup, flags, exclude, reserved);
}
static BOOL WINAPI FailReplace(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
static std::wstring g_replacedFile;
static BOOL WINAPI FailUnableToMoveReplacement(LPCWSTR destination, LPCWSTR, LPCWSTR backup, DWORD, LPVOID, LPVOID) {
    ++g_replaceCalls;
    // 1176: with no backup, the old destination has already gone; with a
    // backup it remains in place. In both cases the replacement stays put.
    if (!backup) DeleteFileW(destination);
    SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT); return FALSE;
}
static BOOL WINAPI FailUnableToMoveReplacement2(LPCWSTR destination, LPCWSTR, LPCWSTR backup, DWORD, LPVOID, LPVOID) {
    ++g_replaceCalls;
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
    Write(file, L"old1175"); Write(temp, L"new1175");
    Write(file + L".bak", L"previous1175"); g_replaceCalls = 0; bool keep1175 = true;
    g_oldBytes = ReadBytes(file); g_newBytes = ReadBytes(temp); g_backupBytes = ReadBytes(file + L".bak");
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, TransientRemoveReplaced, &keep1175); }
    catch (const _com_error&) { Require(false); }
    Require(g_replaceCalls >= 3 && g_replaceCalls <= 6 && !keep1175 && !Exists(temp) &&
        ReadBytes(file) == g_newBytes && ReadBytes(file + L".bak") == g_backupBytes);
    Remove(file);
    Write(file, L"permanent1175"); Write(temp, L"unused1175"); Write(file + L".bak", L"backup1175");
    g_oldBytes = ReadBytes(file); g_newBytes = ReadBytes(temp); g_backupBytes = ReadBytes(file + L".bak");
    g_replaceCalls = 0; keep1175 = true; HRESULT failure1175 = S_OK;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, FailRemoveReplaced, &keep1175); Require(false); }
    catch (const _com_error& error) { failure1175 = error.Error(); }
    Require(failure1175 == HRESULT_FROM_WIN32(ERROR_UNABLE_TO_REMOVE_REPLACED) && g_replaceCalls == 6 && !keep1175 &&
        ReadBytes(file) == g_oldBytes && ReadBytes(temp) == g_newBytes && ReadBytes(file + L".bak") == g_backupBytes);
    g_replaceCalls = 0; keep1175 = true; failure1175 = S_OK;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true, FailRemoveReplaced, &keep1175); Require(false); }
    catch (const _com_error& error) { failure1175 = error.Error(); }
    Require(failure1175 == HRESULT_FROM_WIN32(ERROR_UNABLE_TO_REMOVE_REPLACED) && g_replaceCalls == 1 && !keep1175 &&
        ReadBytes(file) == g_oldBytes && ReadBytes(temp) == g_newBytes && ReadBytes(file + L".bak") == g_backupBytes);
    Require(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY) != FALSE);
    keep1175 = true; failure1175 = S_OK;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, ::ReplaceFileW, &keep1175); Require(false); }
    catch (const _com_error& error) { failure1175 = error.Error(); }
    Require(FAILED(failure1175) && !keep1175 && ReadBytes(file) == g_oldBytes &&
        ReadBytes(temp) == g_newBytes && ReadBytes(file + L".bak") == g_backupBytes);
    Require(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE);
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
    Write(file, L"old-no-backup-existing"); Write(file + L".bak", L"preserved-backup"); Write(temp, L"new-no-backup-existing");
    FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false);
    Require(Read(file) == L"new-no-backup-existing" && Read(file + L".bak") == L"preserved-backup");
    Write(file, L"old2"); Write(file + L".bak", L"older"); Write(temp, L"new2"); FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true);
    Require(Read(file) == L"new2" && Read(file + L".bak") == L"old2");
    Write(file, L"old3"); Write(file + L".bak", L"previous-backup"); Write(temp, L"new3");
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true, FailReplace); Require(false); }
    catch (const _com_error&) { }
    Require(Read(file) == L"old3" && Read(file + L".bak") == L"previous-backup");
    Remove(file);
    Write(file, L"old1176-no-backup"); Write(temp, L"new1176-no-backup"); bool keep = false; g_replaceCalls = 0;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, FailUnableToMoveReplacement, &keep); Require(false); } catch (const _com_error&) { }
    Require(g_replaceCalls == 1 && keep && !Exists(file) && !Exists(file + L".bak") && Read(temp) == L"new1176-no-backup" && !Exists(g_replacedFile)); Remove(file);
    Write(file, L"old1176-backup"); Write(file + L".bak", L"backup1176"); Write(temp, L"new1176-backup"); keep = false; g_replaceCalls = 0;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true, FailUnableToMoveReplacement, &keep); Require(false); } catch (const _com_error&) { }
    Require(g_replaceCalls == 1 && keep && Read(file) == L"old1176-backup" && Read(file + L".bak") == L"backup1176" && Read(temp) == L"new1176-backup" && !Exists(g_replacedFile)); Remove(file);
    Write(file, L"old1177-backup"); Write(file + L".bak", L"older1177"); Write(temp, L"new1177-backup"); keep = false; g_replaceCalls = 0;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), true, FailUnableToMoveReplacement2, &keep); Require(false); } catch (const _com_error&) { }
    Require(g_replaceCalls == 1 && keep && !Exists(file) && Read(file + L".bak") == L"old1177-backup" && Read(temp) == L"new1177-backup" && !Exists(g_replacedFile)); Remove(file);
    Write(file, L"old1177-no-backup"); Write(temp, L"new1177-no-backup"); keep = false; g_replaceCalls = 0;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, FailUnableToMoveReplacement2, &keep); Require(false); } catch (const _com_error&) { }
    Require(g_replaceCalls == 1 && keep && !Exists(file) && !Exists(file + L".bak") && Read(temp) == L"new1177-no-backup" && Read(g_replacedFile) == L"old1177-no-backup");
    Remove(file);
    Write(temp, L"create-only");keep = false;
    FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, ::ReplaceFileW, &keep, false);
    Require(!keep && Read(file) == L"create-only");
    Write(temp, L"competing-result");keep = false;
    try { FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, ::ReplaceFileW, &keep, false); Require(false); }
    catch (const _com_error&) { }
    Require(!keep && Read(file) == L"create-only" && Read(temp) == L"competing-result");
    if (!keep) DeleteFileW(temp.c_str());
    Require(!Exists(temp));
    Remove(file);
    // A real CREATE_NEW competitor races the single atomic move. Exactly one
    // creator wins; a failed move must retain the competitor's complete bytes.
    Write(temp, L"move-winner");HANDLE start = CreateEventW(NULL, TRUE, FALSE, NULL);
    Require(start != NULL);bool competitor = false;
    std::thread writer([&]() {
        WaitForSingleObject(start, INFINITE);
        HANDLE output = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (output != INVALID_HANDLE_VALUE) {
            const char text[] = "create-winner";DWORD written = 0;
            competitor = WriteFile(output, text, sizeof(text)-1, &written, NULL) && written == sizeof(text)-1;
            CloseHandle(output);
        }
    });
    SetEvent(start);bool moved = false;keep = false;
    try {FbeBackupFileCommit::CommitSavedFile(temp.c_str(), file.c_str(), false, ::ReplaceFileW, &keep, false);moved = true;}
    catch (const _com_error&) { }
    writer.join();CloseHandle(start);
    Require(moved != competitor && !keep && Read(file) == (moved ? L"move-winner" : L"create-winner"));
    if (!keep) DeleteFileW(temp.c_str());Require(!Exists(temp));
    Remove(file);
    Write(file, L"safe-original");Write(temp, L"safe-unused");keep = true;
    try {FbeBackupFileCommit::CommitSavedFile(temp.c_str(),file.c_str(),false,FailReplace,&keep);Require(false);}
    catch (const _com_error&) { }
    Require(!keep && Read(file) == L"safe-original");
    if (!keep) DeleteFileW(temp.c_str());Require(!Exists(temp));
    Remove(file); return 0;
}
