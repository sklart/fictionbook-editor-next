#pragma once

#include <atlstr.h>
#include <vector>

#include "..\\DocumentLocation.h"

namespace FbeArchive
{
enum class ErrorCode
{
    None,
    OpenFailed,
    UnsupportedFormat,
    Corrupted,
    Encrypted,
    NoFictionBookEntries,
    EntryNotFound,
    EntryAmbiguous,
    EntryReadFailed,
    EntryTooLarge,
    WriteFailed,
    ReplaceFailed,
    ModifiedExternally
};

struct Error
{
    ErrorCode code = ErrorCode::None;
    DWORD systemError = ERROR_SUCCESS;
};

struct Entry
{
    CString path;
    unsigned __int64 uncompressedSize = 0;
    FictionBookFileType documentType = FictionBookFileType::Unknown;
    unsigned int occurrence = 0;
};

// A 512 MiB input cap leaves headroom for MSXML, DOM and editor views in the
// 32-bit process while still allowing image-heavy FictionBook files.
const unsigned __int64 kMaximumDocumentBytes = 512ULL * 1024ULL * 1024ULL;

bool EnumerateFictionBookEntries(const CString& storagePath, std::vector<Entry>& entries, Error& error);
bool ReadEntry(const CString& storagePath, const Entry& entry, std::vector<unsigned char>& bytes, Error& error);
}
