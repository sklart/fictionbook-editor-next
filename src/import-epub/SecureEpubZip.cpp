#include "stdafx.h"
#include "SecureEpubZip.h"

#define LIBARCHIVE_STATIC
#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace {
struct ArchiveOwner {
    archive* value = archive_read_new();
    ~ArchiveOwner() { if (value) archive_read_free(value); }
};
struct Handles {
    std::vector<HANDLE> values;
    ~Handles() { for (HANDLE value : values) ::CloseHandle(value); }
};
unsigned short U16(const unsigned char* p) { return static_cast<unsigned short>(p[0] | (p[1] << 8)); }
unsigned long U32(const unsigned char* p) { return static_cast<unsigned long>(U16(p)) | (static_cast<unsigned long>(U16(p + 2)) << 16); }
unsigned long long U64(const unsigned char* p) { return U32(p) | (static_cast<unsigned long long>(U32(p + 4)) << 32); }

// Inspect directory framing and raw names: libarchive deliberately tolerates
// missing end records and exposes names as NUL-terminated strings.
bool ValidateZipDirectory(const CStringW& source) {
    HANDLE file = ::CreateFileW(source, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    Handles owner; owner.values.push_back(file);
    LARGE_INTEGER fileSize = {};
    if (!::GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 22) return false;
    const auto readAt = [file, &fileSize](unsigned long long offset, void* dest, DWORD bytes) {
        if (offset > static_cast<unsigned long long>(fileSize.QuadPart) || bytes > static_cast<unsigned long long>(fileSize.QuadPart) - offset) return false;
        LARGE_INTEGER pos; pos.QuadPart = static_cast<LONGLONG>(offset); DWORD got = 0;
        return ::SetFilePointerEx(file, pos, nullptr, FILE_BEGIN) && ::ReadFile(file, dest, bytes, &got, nullptr) && got == bytes;
    };
    const size_t tailSize = static_cast<size_t>((std::min)(fileSize.QuadPart, 65557LL));
    const auto tailStart = static_cast<unsigned long long>(fileSize.QuadPart) - tailSize;
    std::vector<unsigned char> tail(tailSize);
    if (!readAt(tailStart, tail.data(), static_cast<DWORD>(tail.size()))) return false;
    size_t eocd = tail.size();
    for (size_t i = tail.size() - 22;; --i) {
        if (U32(&tail[i]) == 0x06054b50 && i + 22 + U16(&tail[i + 20]) == tail.size()) { eocd = i; break; }
        if (!i) break;
    }
    if (eocd == tail.size()) return false;
    const unsigned char* end = &tail[eocd];
    if (U16(end + 4) != 0 || U16(end + 6) != 0 || U16(end + 8) != U16(end + 10)) return false;
    unsigned long long count = U16(end + 10), size = U32(end + 12), offset = U32(end + 16);
    unsigned long long endOffset = tailStart + eocd;
    if (count == 0xffff || size == 0xffffffffUL || offset == 0xffffffffUL) {
        unsigned char locator[20] = {}, zip64[56] = {};
        if (endOffset < sizeof(locator) || !readAt(endOffset - sizeof(locator), locator, sizeof(locator)) ||
            U32(locator) != 0x07064b50 || U32(locator + 4) != 0 || U32(locator + 16) != 1 ||
            !readAt(U64(locator + 8), zip64, sizeof(zip64)) || U32(zip64) != 0x06064b50 ||
            U64(zip64 + 4) < 44 || U64(locator + 8) > endOffset - sizeof(locator) ||
            endOffset - sizeof(locator) - U64(locator + 8) < 12 ||
            U64(zip64 + 4) > endOffset - sizeof(locator) - U64(locator + 8) - 12 ||
            U32(zip64 + 16) != 0 || U32(zip64 + 20) != 0 || U64(zip64 + 24) != U64(zip64 + 32)) return false;
        count = U64(zip64 + 32); size = U64(zip64 + 40); offset = U64(zip64 + 48);
        endOffset = U64(locator + 8);
    }
    if (!count || count > 100000 || offset > endOffset || size > endOffset - offset) return false;
    const unsigned long long directoryStart = offset, directoryEnd = offset + size;
    for (unsigned long long n = 0; n < count; ++n) {
        unsigned char header[46] = {};
        if (offset > directoryEnd || directoryEnd - offset < sizeof(header) || !readAt(offset, header, sizeof(header)) || U32(header) != 0x02014b50) return false;
        const DWORD nameSize = U16(header + 28), extraSize = U16(header + 30), commentSize = U16(header + 32);
        const unsigned long long recordSize = sizeof(header) + nameSize + extraSize + commentSize;
        if (!nameSize || recordSize > directoryEnd - offset) return false;
        std::vector<unsigned char> name(nameSize);
        if (!readAt(offset + sizeof(header), name.data(), nameSize) || std::find(name.begin(), name.end(), 0) != name.end()) return false;
        unsigned long long localOffset = U32(header + 42);
        if (U16(header + 34) != 0 && U16(header + 34) != 0xffff) return false;
        if (localOffset == 0xffffffffUL || U16(header + 34) == 0xffff) {
            std::vector<unsigned char> extra(extraSize);
            if (!readAt(offset + sizeof(header) + nameSize, extra.data(), extraSize)) return false;
            size_t pos = 0; bool found = false;
            while (pos + 4 <= extra.size()) {
                const auto id = U16(&extra[pos]), length = U16(&extra[pos + 2]); pos += 4;
                if (length > extra.size() - pos) return false;
                if (id == 1) {
                    size_t cursor = pos, limit = pos + length;
                    if (U32(header + 24) == 0xffffffffUL) cursor += 8;
                    if (U32(header + 20) == 0xffffffffUL) cursor += 8;
                    if (cursor > limit) return false;
                    if (localOffset == 0xffffffffUL) {
                        if (limit - cursor < 8) return false;
                        localOffset = U64(&extra[cursor]); cursor += 8;
                    }
                    if (U16(header + 34) == 0xffff && (limit - cursor < 4 || U32(&extra[cursor]) != 0)) return false;
                    found = true; break;
                }
                pos += length;
            }
            if (!found) return false;
        }
        unsigned char local[30] = {};
        if (localOffset > directoryStart || directoryStart - localOffset < sizeof(local) + nameSize ||
            !readAt(localOffset, local, sizeof(local)) || U32(local) != 0x04034b50 || U16(local + 26) != nameSize ||
            U16(local + 6) != U16(header + 8) || U16(local + 8) != U16(header + 10) ||
            directoryStart - localOffset - sizeof(local) - nameSize < U16(local + 28)) return false;
        if (!(U16(local + 6) & 8) &&
            (U32(local + 14) != U32(header + 16) ||
             (U32(local + 18) != 0xffffffffUL && U32(header + 20) != 0xffffffffUL && U32(local + 18) != U32(header + 20)) ||
             (U32(local + 22) != 0xffffffffUL && U32(header + 24) != 0xffffffffUL && U32(local + 22) != U32(header + 24)))) return false;
        std::vector<unsigned char> localName(nameSize);
        if (!readAt(localOffset + sizeof(local), localName.data(), nameSize) || localName != name) return false;
        offset += recordSize;
    }
    return offset == directoryEnd;
}
struct CaseInsensitiveLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const {
        return ::CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_LESS_THAN;
    }
};

bool IsReservedName(const CStringW& name) {
    CStringW stem(name); const int dot = stem.Find(L'.'); if (dot >= 0) stem = stem.Left(dot);
    stem.MakeUpper();
    if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" || stem == L"CONIN$" || stem == L"CONOUT$") return true;
    if (stem.GetLength() == 4 && (stem.Left(3) == L"COM" || stem.Left(3) == L"LPT")) {
        const wchar_t digit = stem[3];
        if ((digit >= L'1' && digit <= L'9') || digit == 0x00b9 || digit == 0x00b2 || digit == 0x00b3) return true;
    }
    return false;
}

bool SplitSafePath(const wchar_t* raw, std::vector<CStringW>& components, CStringW& normalized) {
    if (!raw || !*raw) return false;
    CStringW path(raw); path.Replace(L'/', L'\\');
    if (path.GetLength() >= 32767 || path[0] == L'\\') return false;
    path.TrimRight(L"\\");
    if (path.IsEmpty()) return false;
    int start = 0;
    while (start < path.GetLength()) {
        int end = path.Find(L'\\', start); if (end < 0) end = path.GetLength();
        CStringW component(path.Mid(start, end - start));
        if (component.IsEmpty() || component == L"." || component == L".." ||
            component.Right(1) == L"." || component.Right(1) == L" " ||
            component.FindOneOf(L"<>:\"|?*") >= 0 || IsReservedName(component)) return false;
        for (int i = 0; i < component.GetLength(); ++i) if (component[i] < 32) return false;
        if (!normalized.IsEmpty()) normalized += L"\\";
        normalized += component; components.push_back(component);
        start = end + 1;
    }
    return !components.empty() && components.size() <= 64;
}

bool OpenDirectoryNoReparse(const CStringW& path, Handles& handles) {
    HANDLE handle = ::CreateFileW(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!::GetFileInformationByHandle(handle, &info) || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) { ::CloseHandle(handle); return false; }
    handles.values.push_back(handle);
    return true;
}
}

static bool ExtractZip(const CStringW& source, const CStringW& destination, const SecureZipLimits& limits,
    bool epub, CStringW& error) {
    error.Empty();
    Handles sourceLock;
    HANDLE sourceHandle = ::CreateFileW(source, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (sourceHandle == INVALID_HANDLE_VALUE) { error = L"Cannot open EPUB ZIP archive."; return false; }
    sourceLock.values.push_back(sourceHandle);
    if (!ValidateZipDirectory(source)) { error = L"Invalid or incomplete EPUB ZIP directory."; return false; }
    const auto maxEntryBytes = limits.entryBytes;
    const auto maxTotalBytes = limits.totalBytes;
    const auto maxEntries = limits.entries;
    if (!::CreateDirectoryW(destination, nullptr)) { error = L"Cannot create EPUB extraction directory."; return false; }
    Handles rootLock;
    if (!OpenDirectoryNoReparse(destination, rootLock)) { error = L"Unsafe EPUB extraction directory."; return false; }
    ULARGE_INTEGER freeBytes = {};
    const unsigned long long reserveBytes = 256ULL * 1024 * 1024;
    if (!::GetDiskFreeSpaceExW(destination, &freeBytes, nullptr, nullptr) || freeBytes.QuadPart <= reserveBytes) {
        error = L"Insufficient free space for EPUB extraction."; return false;
    }
    const unsigned long long availableBudget = (std::min)(maxTotalBytes, freeBytes.QuadPart - reserveBytes);
    ArchiveOwner reader;
    if (!reader.value || archive_read_support_filter_none(reader.value) != ARCHIVE_OK ||
        archive_read_support_format_zip(reader.value) != ARCHIVE_OK ||
        archive_read_set_format_option(reader.value, "zip", "hdrcharset", "CP437") != ARCHIVE_OK ||
        archive_read_open_filename_w(reader.value, source, 65536) != ARCHIVE_OK) {
        error = L"Cannot open EPUB ZIP archive."; return false;
    }
    std::set<std::wstring, CaseInsensitiveLess> names;
    unsigned long long total = 0, nextSpaceCheck = 16ULL * 1024 * 1024; size_t count = 0; archive_entry* entry = nullptr;
    wchar_t testMode[4] = {}, delayedLeaf[MAX_PATH] = {}, rootReport[MAX_PATH] = {};
    const bool delay = epub && ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) == 1 && testMode[0] == L'1' &&
        ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_EPUB_DELAY_ENTRY", delayedLeaf, _countof(delayedLeaf)) > 0;
    bool delayed = false;
    for (;;) {
        const int status = archive_read_next_header(reader.value, &entry);
        if (status == ARCHIVE_EOF) break;
        if (status != ARCHIVE_OK || !entry || ++count > maxEntries) { error = L"Invalid EPUB ZIP entry or entry limit exceeded."; return false; }
        const wchar_t* raw = archive_entry_pathname_w(entry);
        std::vector<CStringW> components; CStringW relative;
        if (!SplitSafePath(raw, components, relative) || !names.emplace(relative.GetString()).second) {
            error = L"Unsafe or duplicate EPUB ZIP entry: "; if (raw) error += raw; return false;
        }
        if (delay && !delayed && components.back() == delayedLeaf) {
            if (::GetEnvironmentVariableW(L"FBE_NEXT_TEST_EPUB_ROOT_REPORT", rootReport, _countof(rootReport)) > 0) {
                const int size = ::WideCharToMultiByte(CP_UTF8, 0, destination, destination.GetLength(), nullptr, 0, nullptr, nullptr);
                std::vector<char> bytes(size > 0 ? static_cast<size_t>(size) : 0);
                if (!size || ::WideCharToMultiByte(CP_UTF8, 0, destination, destination.GetLength(), bytes.data(), size, nullptr, nullptr) != size) return false;
                HANDLE report = ::CreateFileW(rootReport, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (report == INVALID_HANDLE_VALUE) return false;
                DWORD written = 0; const bool ok = ::WriteFile(report, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
                ::CloseHandle(report); if (!ok) return false;
            }
            delayed = true; ::Sleep(1200);
        }
        const auto type = archive_entry_filetype(entry);
        if ((type != AE_IFDIR && type != AE_IFREG) || archive_entry_symlink(entry) || archive_entry_hardlink(entry) || archive_entry_is_encrypted(entry) == 1) {
            error = L"Unsupported EPUB ZIP entry type or encryption: " + relative; return false;
        }
        const bool directory = type == AE_IFDIR;
        const la_int64_t declared = archive_entry_size_is_set(entry) ? archive_entry_size(entry) : -1;
        if (!directory && (declared < 0 || static_cast<unsigned long long>(declared) > maxEntryBytes ||
            static_cast<unsigned long long>(declared) > availableBudget - total)) { error = L"EPUB ZIP entry exceeds extraction limits or available space: " + relative; return false; }
        Handles parents; CStringW path(destination);
        for (size_t i = 0; i < components.size() - (directory ? 0 : 1); ++i) {
            path += L"\\" + components[i];
            if (path.GetLength() >= 32767 || (!::CreateDirectoryW(path, nullptr) && ::GetLastError() != ERROR_ALREADY_EXISTS) || !OpenDirectoryNoReparse(path, parents)) {
                error = L"Cannot safely create EPUB ZIP directory: " + relative; return false;
            }
        }
        if (directory) { if (archive_read_data_skip(reader.value) != ARCHIVE_OK) { error = L"Invalid ZIP directory: " + relative; return false; } continue; }
        path += L"\\" + components.back();
        if (path.GetLength() >= 32767) { error = L"EPUB ZIP path is too long: " + relative; return false; }
        HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) { error = L"Cannot safely create EPUB ZIP resource: " + relative; return false; }
        unsigned long long writtenTotal = 0; bool ok = true; unsigned char buffer[65536];
        for (;;) {
            const la_ssize_t size = archive_read_data(reader.value, buffer, sizeof(buffer));
            if (size == 0) break;
            if (size < 0 || static_cast<unsigned long long>(size) > maxEntryBytes - writtenTotal ||
                static_cast<unsigned long long>(size) > availableBudget - total - writtenTotal) { ok = false; break; }
            DWORD written = 0;
            if (!::WriteFile(file, buffer, static_cast<DWORD>(size), &written, nullptr) || written != static_cast<DWORD>(size)) { ok = false; break; }
            writtenTotal += static_cast<unsigned long long>(size);
            if (total + writtenTotal >= nextSpaceCheck) {
                if (!::GetDiskFreeSpaceExW(destination, &freeBytes, nullptr, nullptr) || freeBytes.QuadPart <= reserveBytes) { ok = false; break; }
                nextSpaceCheck = total + writtenTotal + 16ULL * 1024 * 1024;
            }
        }
        if (!::CloseHandle(file)) ok = false;
        if (!ok || writtenTotal != static_cast<unsigned long long>(declared)) { error = L"Cannot fully extract EPUB ZIP resource: " + relative; return false; }
        total += writtenTotal;
    }
    if (!count || (delay && !delayed) || archive_read_close(reader.value) != ARCHIVE_OK) { error = L"EPUB ZIP archive is empty or incomplete."; return false; }
    return true;
}

bool ExtractSecureEpubZip(const CStringW& source, const CStringW& destination, CStringW& error) {
    return ExtractZip(source, destination, {8ULL << 30, 32ULL << 30, 100000}, true, error);
}

bool ExtractSecureZip(const CStringW& source, const CStringW& destination, const SecureZipLimits& limits,
    CStringW& error) {
    const bool ok = ExtractZip(source, destination, limits, false, error);
    if (!ok) {
        const int detail = error.Find(L":");
        if (detail >= 0) {
            CStringW context = error.Left(detail);
            context.Replace(L"EPUB ", L"");
            error = context + error.Mid(detail);
        } else {
            error.Replace(L"EPUB ", L"");
        }
    }
    return ok;
}
