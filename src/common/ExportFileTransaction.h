#pragma once
#include <windows.h>
#include <string>
#include <vector>

// Shared by HTML, DOCX and EPUB. All output is staged beside its destination;
// existing results remain intact until every writer has finished successfully.
namespace FbeExport {
class FileTransaction {
    struct File {
        std::wstring target, temporary, backup;
        bool applied = false, existed = false;
    };
    std::vector<File> files;
    static HRESULT Error() { const DWORD code = ::GetLastError(); return HRESULT_FROM_WIN32(code ? code : ERROR_WRITE_FAULT); }
    static HRESULT Temporary(const std::wstring& target, std::wstring& temporary) {
        wchar_t absolute[MAX_PATH] = {}, name[MAX_PATH] = {};
        const DWORD length = ::GetFullPathNameW(target.c_str(), MAX_PATH, absolute, NULL);
        if (!length || length >= MAX_PATH) return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
        std::wstring directory(absolute);
        directory.erase(directory.find_last_of(L"\\/") + 1);
        if (!::GetTempFileNameW(directory.c_str(), L"fbe", 0, name)) return Error();
        temporary = name; return S_OK;
    }
    void Rollback() {
        for (auto it = files.rbegin(); it != files.rend(); ++it) {
            if (!it->applied) continue;
            const bool restored = it->existed
                ? (::GetFileAttributesW(it->target.c_str()) != INVALID_FILE_ATTRIBUTES
                    ? ::ReplaceFileW(it->target.c_str(), it->backup.c_str(), NULL, 0, NULL, NULL) != FALSE
                    : ::MoveFileExW(it->backup.c_str(), it->target.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE)
                : ::DeleteFileW(it->target.c_str()) != FALSE;
            if (restored) it->applied = false;
            // On rollback failure retain the backup for recovery, never erase it.
        }
    }
public:
    FileTransaction() = default;
    FileTransaction(const FileTransaction&) = delete;
    FileTransaction& operator=(const FileTransaction&) = delete;
    ~FileTransaction() {
        for (const auto& file : files) {
            if (!file.temporary.empty()) ::DeleteFileW(file.temporary.c_str());
            if (!file.applied && !file.backup.empty()) ::DeleteFileW(file.backup.c_str());
        }
    }
    HRESULT Stage(const std::wstring& target, std::wstring& temporary) {
        for (const auto& file : files) if (::CompareStringOrdinal(file.target.c_str(), -1, target.c_str(), -1, TRUE) == CSTR_EQUAL) return E_INVALIDARG;
        File file; file.target = target;
        HRESULT hr = Temporary(target, file.temporary); if (FAILED(hr)) return hr;
        temporary = file.temporary;
        try { files.push_back(file); } catch (...) { ::DeleteFileW(file.temporary.c_str()); throw; }
        return S_OK;
    }
    HRESULT Commit() {
        // Flush and validate the complete staging set before replacing anything.
        for (auto& file : files) {
            HANDLE handle = ::CreateFileW(file.temporary.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (handle == INVALID_HANDLE_VALUE) return Error();
            const BOOL flushed = ::FlushFileBuffers(handle); const HRESULT hr = flushed ? S_OK : Error();
            const BOOL closed = ::CloseHandle(handle);
            if (FAILED(hr)) return hr;
            if (!closed) return Error();
            const DWORD attributes = ::GetFileAttributesW(file.target.c_str());
            file.existed = attributes != INVALID_FILE_ATTRIBUTES;
            if (file.existed && (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
            if (file.existed) { const HRESULT backup = Temporary(file.target, file.backup); if (FAILED(backup)) return backup; }
        }
        for (auto& file : files) {
            BOOL replaced = FALSE;
            DWORD error = ERROR_SUCCESS;
            for (unsigned attempt = 0; attempt < 4; ++attempt) {
                replaced = file.existed
                    ? ::ReplaceFileW(file.target.c_str(), file.temporary.c_str(), file.backup.c_str(), 0, NULL, NULL)
                    : ::MoveFileExW(file.temporary.c_str(), file.target.c_str(), MOVEFILE_WRITE_THROUGH);
                if (replaced) break;
                error = ::GetLastError();
                // This failure retains both original names. Retry only that
                // unchanged state, for at most 70 ms; partial moves roll back.
                if (!file.existed || error != ERROR_UNABLE_TO_REMOVE_REPLACED || attempt == 3) break;
                const DWORD target = ::GetFileAttributesW(file.target.c_str());
                const DWORD temporary = ::GetFileAttributesW(file.temporary.c_str());
                if (target == INVALID_FILE_ATTRIBUTES || temporary == INVALID_FILE_ATTRIBUTES ||
                    (target & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
                    (temporary & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) break;
                ::Sleep(10u << attempt);
            }
            if (!replaced) {
                // ReplaceFile can fail after moving the original to the backup.
                // Mark that state before rollback so cleanup cannot erase it.
                if (file.existed && error == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2) file.applied = true;
                Rollback(); return HRESULT_FROM_WIN32(error ? error : ERROR_WRITE_FAULT);
            }
            file.applied = true;
        }
        for (auto& file : files) { if (!file.backup.empty()) ::DeleteFileW(file.backup.c_str()); file.applied = false; }
        files.clear(); return S_OK;
    }
};
}
