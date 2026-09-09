#include "../../src/fbe/stdafx.h"
#include "../../src/fbe/archive/ArchiveReader.h"
#define LIBARCHIVE_STATIC
#include <archive.h>
#include <archive_entry.h>
#include <map>
#include <string>

static void Check(bool value, const char* message) { if (!value) { fprintf(stderr, "%s\n", message); exit(1); } }
static std::vector<unsigned char> Bytes(const char* text) { return std::vector<unsigned char>(text, text + strlen(text)); }
static std::string Utf8(const wchar_t* text) { int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL); std::vector<char> buffer(length); if (length) WideCharToMultiByte(CP_UTF8, 0, text, -1, &buffer[0], length, NULL, NULL); return length ? std::string(&buffer[0]) : std::string(); }
static void Add(archive* writer, const wchar_t* name, const std::vector<unsigned char>& bytes)
{
    archive_entry* entry = archive_entry_new(); Check(entry != NULL, "entry allocation");
    const std::string utf8 = Utf8(name); archive_entry_set_pathname_utf8(entry, utf8.c_str()); archive_entry_set_filetype(entry, AE_IFREG); archive_entry_set_perm(entry, 0644); archive_entry_set_size(entry, bytes.size());
    Check(archive_write_header(writer, entry) == ARCHIVE_OK, "zip header");
    size_t offset = 0; while (offset < bytes.size()) { la_ssize_t wrote = archive_write_data(writer, &bytes[offset], bytes.size() - offset); Check(wrote > 0, "zip write"); offset += static_cast<size_t>(wrote); }
    Check(archive_write_finish_entry(writer) == ARCHIVE_OK, "zip finish"); archive_entry_free(entry);
}
static void CreateZip(const CString& path, bool fbdOnly = false, bool empty = false)
{
    archive* writer = archive_write_new(); Check(writer != NULL, "writer allocation"); Check(archive_write_set_format_zip(writer) == ARCHIVE_OK, "zip format"); Check(archive_write_set_options(writer, "hdrcharset=UTF-8") == ARCHIVE_OK, "zip charset"); Check(archive_write_open_filename_w(writer, path) == ARCHIVE_OK, "zip open");
    if (!empty) {
        if (fbdOnly) Add(writer, L"book.fbd", Bytes("<FictionBook><binary/></FictionBook>"));
        else { Add(writer, L"book.fb2", Bytes("old-book")); Add(writer, L"second.fb2", Bytes("second")); Add(writer, L"cover.jpg", Bytes("cover-bytes")); Add(writer, L"readme.txt", Bytes("readme")); Add(writer, L"папка/книга.fb2", Bytes("unicode")); Add(writer, L"duplicate.fb2", Bytes("first")); Add(writer, L"duplicate.fb2", Bytes("second-duplicate")); Add(writer, L"nested/../unsafe.fb2", Bytes("unsafe")); }
    }
    Check(archive_write_close(writer) == ARCHIVE_OK, "zip close"); archive_write_free(writer);
}
static std::map<CString, std::vector<unsigned char> > ReadAll(const CString& path)
{
    std::map<CString, std::vector<unsigned char> > result; archive* reader = archive_read_new(); Check(archive_read_support_filter_all(reader) == ARCHIVE_OK, "reader filter"); Check(archive_read_support_format_zip(reader) == ARCHIVE_OK, "reader format"); Check(archive_read_open_filename_w(reader, path, 10240) == ARCHIVE_OK, "reader open"); archive_entry* entry = NULL;
    while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) { const wchar_t* name = archive_entry_pathname_w(entry); std::vector<unsigned char> bytes; unsigned char buffer[256]; for (;;) { la_ssize_t got = archive_read_data(reader, buffer, sizeof(buffer)); Check(got >= 0, "reader data"); if (!got) break; bytes.insert(bytes.end(), buffer, buffer + got); } result[CString(name)] = bytes; }
    archive_read_free(reader); return result;
}
int main()
{
    wchar_t temp[MAX_PATH] = {}; GetTempPathW(_countof(temp), temp); CString path(temp); path += L"fbe-archive-functional.zip"; DeleteFileW(path); CreateZip(path);
    std::map<CString, std::vector<unsigned char> > before = ReadAll(path); std::vector<FbeArchive::Entry> entries; FbeArchive::Error error; Check(FbeArchive::EnumerateFictionBookEntries(path, entries, error), "enumerate fixture");
    FbeArchive::Entry target; bool found = false, unicode = false; unsigned int duplicates = 0; for (size_t i = 0; i < entries.size(); ++i) { if (entries[i].path == L"book.fb2") { target = entries[i]; found = true; } if (entries[i].path == L"папка/книга.fb2") unicode = true; if (entries[i].path == L"duplicate.fb2") ++duplicates; Check(entries[i].path.Find(L"..") < 0, "unsafe path leaked"); }
    Check(found && unicode && duplicates == 2, "entry enumeration"); if (!FbeArchive::RewriteZipEntry(path, target, Bytes("new-book"), error)) { fprintf(stderr, "rewrite error %d/%lu\n", static_cast<int>(error.code), error.systemError); return 1; } std::map<CString, std::vector<unsigned char> > after = ReadAll(path); Check(after[L"book.fb2"] == Bytes("new-book"), "replacement contents"); Check(after[L"second.fb2"] == before[L"second.fb2"] && after[L"cover.jpg"] == before[L"cover.jpg"] && after[L"readme.txt"] == before[L"readme.txt"], "unchanged payloads");
    std::vector<unsigned char> snapshot = ReadAll(path)[L"book.fb2"]; FbeArchive::Entry missing = target; missing.occurrence += 1000; Check(!FbeArchive::RewriteZipEntry(path, missing, Bytes("broken"), error) && error.code == FbeArchive::ErrorCode::EntryNotFound, "rewrite failure"); Check(ReadAll(path)[L"book.fb2"] == snapshot, "failed rewrite changed archive");
    CString fbd(temp); fbd += L"fbe-archive-fbd.zip"; DeleteFileW(fbd); CreateZip(fbd, true); entries.clear(); Check(FbeArchive::EnumerateFictionBookEntries(fbd, entries, error) && entries.size() == 1 && entries[0].documentType == FictionBookFileType::Fbd, "fbd archive");
    CString empty(temp); empty += L"fbe-archive-empty.zip"; DeleteFileW(empty); CreateZip(empty, false, true); entries.clear(); Check(!FbeArchive::EnumerateFictionBookEntries(empty, entries, error) && error.code == FbeArchive::ErrorCode::NoFictionBookEntries, "empty archive");
    CString corrupt(temp); corrupt += L"fbe-archive-corrupt.zip"; HANDLE file = CreateFileW(corrupt, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); DWORD written = 0; WriteFile(file, "not zip", 7, &written, NULL); CloseHandle(file); entries.clear(); Check(!FbeArchive::EnumerateFictionBookEntries(corrupt, entries, error), "corrupt archive"); DeleteFileW(path); DeleteFileW(fbd); DeleteFileW(empty); DeleteFileW(corrupt); return 0;
}
