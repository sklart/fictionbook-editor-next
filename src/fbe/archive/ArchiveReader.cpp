#include "stdafx.h"
#include "ArchiveReader.h"

#define LIBARCHIVE_STATIC
#include <archive.h>
#include <archive_entry.h>
#include <limits>

namespace
{
class ArchiveHandle
{
public:
    ArchiveHandle() : m_archive(archive_read_new()) {}
    ~ArchiveHandle() { if (m_archive != NULL) archive_read_free(m_archive); }
    archive* Get() const { return m_archive; }
private:
    archive* m_archive;
};

CString EntryPath(archive_entry* entry)
{
    const wchar_t* widePath = archive_entry_pathname_w(entry);
    if (widePath != NULL) return CString(widePath);
    const char* narrowPath = archive_entry_pathname(entry);
    if (narrowPath == NULL) return CString();
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrowPath, -1, NULL, 0);
    if (length <= 0) return CString(L"<invalid archive entry name>");
    CString result;
    LPWSTR buffer = result.GetBuffer(length);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrowPath, -1, buffer, length);
    result.ReleaseBuffer();
    return result;
}

bool IsSafeEntryPath(const CString& path)
{
    // Entries are never extracted, but rejecting paths that cannot be a
    // document identity also keeps persisted MRU/recovery metadata bounded.
    if (path.IsEmpty() || path.GetLength() >= 32768) return false;
    if (path[0] == L'/' || path[0] == L'\\') return false;
    if (path.GetLength() >= 2 && path[1] == L':') return false;
    for (int index = 0; index < path.GetLength(); ++index)
        if (path[index] < L' ') return false;
	int segmentStart = 0;
	for (int index = 0; index <= path.GetLength(); ++index)
	{
		if (index != path.GetLength() && path[index] != L'/' && path[index] != L'\\') continue;
		if (index - segmentStart == 2 && path.Mid(segmentStart, 2) == L"..") return false;
		segmentStart = index + 1;
	}
    return true;
}

bool PrepareReader(archive* reader, const CString& storagePath, FbeArchive::Error& error)
{
    if (reader == NULL) { error.code = FbeArchive::ErrorCode::OpenFailed; return false; }
    archive_read_support_filter_none(reader);
    archive_read_support_format_zip(reader);
    archive_read_support_format_rar(reader);
    archive_read_support_format_rar5(reader);
    if (archive_read_open_filename_w(reader, storagePath, 10240) != ARCHIVE_OK)
    {
        error.code = FbeArchive::ErrorCode::OpenFailed;
        return false;
    }
    return true;
}

FbeArchive::ErrorCode ReadError(const archive* reader)
{
    UNREFERENCED_PARAMETER(reader);
    return FbeArchive::ErrorCode::Corrupted;
}

bool IsEncrypted(const archive* reader, archive_entry* entry)
{
    UNREFERENCED_PARAMETER(reader);
    return archive_entry_is_encrypted(entry) == 1;
}
}

namespace FbeArchive
{
bool EnumerateFictionBookEntries(const CString& storagePath, std::vector<Entry>& entries, Error& error)
{
    entries.clear(); error = Error();
    ArchiveHandle handle;
    if (!PrepareReader(handle.Get(), storagePath, error)) return false;

    archive_entry* header = NULL;
    unsigned int ordinal = 0;
    while (true)
    {
        const int result = archive_read_next_header(handle.Get(), &header);
        if (result == ARCHIVE_EOF) break;
        if (result != ARCHIVE_OK && result != ARCHIVE_WARN) { error.code = ReadError(handle.Get()); return false; }
        if (archive_entry_filetype(header) == AE_IFREG)
        {
            const CString path = EntryPath(header);
            const FictionBookFileType type = DetectFictionBookFileType(path);
            if (type != FictionBookFileType::Unknown && IsSafeEntryPath(path))
            {
                const la_int64_t size = archive_entry_size_is_set(header) ? archive_entry_size(header) : -1;
                if (size < 0 || static_cast<unsigned __int64>(size) > kMaximumDocumentBytes) { error.code = ErrorCode::EntryTooLarge; return false; }
                Entry entry; entry.path = path; entry.uncompressedSize = static_cast<unsigned __int64>(size); entry.documentType = type; entry.occurrence = ordinal;
                entries.push_back(entry);
            }
        }
        ++ordinal;
        archive_read_data_skip(handle.Get());
    }
    if (entries.empty()) { error.code = ErrorCode::NoFictionBookEntries; return false; }
    return true;
}

bool ReadEntry(const CString& storagePath, const Entry& entry, std::vector<unsigned char>& bytes, Error& error)
{
    bytes.clear(); error = Error();
    ArchiveHandle handle;
    if (!PrepareReader(handle.Get(), storagePath, error)) return false;
    archive_entry* header = NULL;
    unsigned int ordinal = 0;
    while (true)
    {
        const int result = archive_read_next_header(handle.Get(), &header);
        if (result == ARCHIVE_EOF) { error.code = ErrorCode::EntryNotFound; return false; }
        if (result != ARCHIVE_OK && result != ARCHIVE_WARN) { error.code = ReadError(handle.Get()); return false; }
        if (ordinal++ != entry.occurrence) { archive_read_data_skip(handle.Get()); continue; }
        if (archive_entry_filetype(header) != AE_IFREG || !IsSafeEntryPath(entry.path) || EntryPath(header) != entry.path) { error.code = ErrorCode::EntryNotFound; return false; }
        if (IsEncrypted(handle.Get(), header)) { error.code = ErrorCode::Encrypted; return false; }
        const la_int64_t declaredSize = archive_entry_size_is_set(header) ? archive_entry_size(header) : -1;
        if (declaredSize < 0 || static_cast<unsigned __int64>(declaredSize) > kMaximumDocumentBytes) { error.code = ErrorCode::EntryTooLarge; return false; }
        try { bytes.reserve(static_cast<size_t>(declaredSize)); }
        catch (const std::bad_alloc&) { error.code = ErrorCode::EntryTooLarge; return false; }
        unsigned char buffer[64 * 1024];
        for (;;)
        {
            const la_ssize_t read = archive_read_data(handle.Get(), buffer, sizeof(buffer));
            if (read == 0) return true;
            if (read < 0) { error.code = ReadError(handle.Get()); bytes.clear(); return false; }
            if (bytes.size() > kMaximumDocumentBytes - static_cast<size_t>(read)) { error.code = ErrorCode::EntryTooLarge; bytes.clear(); return false; }
            try { bytes.insert(bytes.end(), buffer, buffer + read); }
            catch (const std::bad_alloc&) { error.code = ErrorCode::EntryTooLarge; bytes.clear(); return false; }
        }
    }
}
}
