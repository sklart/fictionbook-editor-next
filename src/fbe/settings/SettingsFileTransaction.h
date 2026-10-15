#pragma once
#include <vector>

// Same-directory staging; originals remain recoverable until the batch succeeds.
class SettingsFileTransaction
{
    struct Entry { CString destination, temporary, backup; bool hadOriginal = false, installed = false; };
    std::vector<Entry> m_entries;
    void Rollback()
    {
        for(auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
        {
            if(it->installed && ::DeleteFileW(it->destination)) it->installed = false;
            if(it->hadOriginal && ::MoveFileExW(it->backup, it->destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                it->hadOriginal = false; it->installed = false; it->backup.Empty();
            }
        }
    }
    bool Fail() { Rollback(); return false; }

public:
    static CString Temporary(const CString& destination)
    {
        CString directory(destination);
        ::PathRemoveFileSpecW(directory.GetBuffer()); directory.ReleaseBuffer();
        wchar_t path[MAX_PATH] = {};
        return ::GetTempFileNameW(directory, L"fbe", 0, path) ? CString(path) : CString();
    }
    void Add(const CString& destination, const CString& temporary = CString())
    {
        Entry entry; entry.destination = destination; entry.temporary = temporary;
        m_entries.push_back(entry);
    }
    bool Commit()
    {
        for(Entry& entry : m_entries)
        {
            const DWORD attributes = ::GetFileAttributesW(entry.destination);
            if(attributes != INVALID_FILE_ATTRIBUTES)
            {
                if(attributes & FILE_ATTRIBUTE_READONLY) return Fail();
                entry.backup = Temporary(entry.destination);
                if(entry.backup.IsEmpty() || !::MoveFileExW(entry.destination, entry.backup, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return Fail();
                entry.hadOriginal = true;
            }
            else if(::GetLastError() != ERROR_FILE_NOT_FOUND) return Fail();
            if(!entry.temporary.IsEmpty())
            {
                if(!::MoveFileExW(entry.temporary, entry.destination, MOVEFILE_WRITE_THROUGH)) return Fail();
                entry.temporary.Empty(); entry.installed = true;
            }
        }
        for(Entry& entry : m_entries)
        {
            if(entry.hadOriginal) ::DeleteFileW(entry.backup);
            entry.hadOriginal = false; entry.installed = false;
        }
        return true;
    }
    ~SettingsFileTransaction()
    {
        Rollback();
        for(const auto& entry : m_entries)
        {
            // Keep an original backup if its restoration was blocked externally.
            if(!entry.hadOriginal && !entry.backup.IsEmpty()) ::DeleteFileW(entry.backup);
            if(!entry.temporary.IsEmpty()) ::DeleteFileW(entry.temporary);
        }
    }
};
