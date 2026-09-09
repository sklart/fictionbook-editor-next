#include "stdafx.h"
#include "ArchiveReader.h"

#define LIBARCHIVE_STATIC
#include <archive.h>
#include <archive_entry.h>
#include <string>

namespace
{
class Reader { public: Reader() : value(archive_read_new()) {} ~Reader() { if(value) archive_read_free(value); } archive* value; };
class Writer { public: Writer() : value(archive_write_new()) {} ~Writer() { if(value) archive_write_free(value); } archive* value; };

bool OpenZipReader(archive* reader, const CString& path)
{
	return reader && archive_read_support_filter_none(reader) == ARCHIVE_OK &&
		archive_read_support_format_zip(reader) == ARCHIVE_OK &&
		archive_read_open_filename_w(reader, path, 10240) == ARCHIVE_OK;
}

CString PathOf(archive_entry* entry)
{
	const wchar_t* wide = archive_entry_pathname_w(entry);
	return wide ? CString(wide) : CString();
}

bool CopyPayload(archive* reader, archive* writer)
{
	unsigned char buffer[64 * 1024];
	for (;;)
	{
		const la_ssize_t read = archive_read_data(reader, buffer, sizeof(buffer));
		if (read == 0) return true;
		if (read < 0) return false;
		size_t offset = 0;
		while (offset < static_cast<size_t>(read))
		{
			const la_ssize_t written = archive_write_data(writer, buffer + offset, static_cast<size_t>(read) - offset);
			if (written <= 0 || static_cast<size_t>(written) > static_cast<size_t>(read) - offset) return false;
			offset += static_cast<size_t>(written);
		}
	}
}

std::string Utf8Path(const CString& path)
{
	const int length = ::WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
	if (length <= 1) return std::string();
	std::vector<char> buffer(static_cast<size_t>(length));
	if (::WideCharToMultiByte(CP_UTF8, 0, path, -1, &buffer[0], length, NULL, NULL) != length) return std::string();
	return std::string(&buffer[0]);
}

bool WritePayload(archive* writer, const std::vector<unsigned char>& payload)
{
	size_t offset = 0;
	while (offset < payload.size())
	{
		const la_ssize_t written = archive_write_data(writer, &payload[offset], payload.size() - offset);
		if (written <= 0 || static_cast<size_t>(written) > payload.size() - offset) return false;
		offset += static_cast<size_t>(written);
	}
	return true;
}
}

namespace FbeArchive
{
bool RewriteZipEntry(const CString& storagePath, const Entry& target,
	const std::vector<unsigned char>& replacement, Error& error)
{
	error = Error();
	if (replacement.size() > kMaximumDocumentBytes) { error.code = ErrorCode::EntryTooLarge; return false; }
	CString directory(storagePath); const int slash = directory.ReverseFind(L'\\');
	if (slash < 0) directory = L"."; else directory = directory.Left(slash);
	wchar_t temporaryPath[MAX_PATH] = {};
	if (::GetTempFileNameW(directory, L"fza", 0, temporaryPath) == 0) { error.code = ErrorCode::WriteFailed; error.systemError = ::GetLastError(); return false; }
	const CString temporary(temporaryPath);
	// GetTempFileName creates the placeholder.  libarchive opens a new output
	// archive, so remove that placeholder before handing the path to it.
	if (!::DeleteFileW(temporary)) { error.code = ErrorCode::WriteFailed; error.systemError = ::GetLastError(); return false; }
	bool completed = false;
	Reader reader; Writer writer;
	if (!OpenZipReader(reader.value, storagePath) || !writer.value ||
		archive_write_set_format_zip(writer.value) != ARCHIVE_OK ||
		archive_write_set_options(writer.value, "hdrcharset=UTF-8") != ARCHIVE_OK ||
		archive_write_open_filename_w(writer.value, temporary) != ARCHIVE_OK)
	{
		error.code = ErrorCode::WriteFailed; error.systemError = static_cast<DWORD>(archive_errno(writer.value)); goto cleanup;
	}
	archive_entry* header = NULL; unsigned int ordinal = 0; bool replaced = false;
	for (;;)
	{
		const int next = archive_read_next_header(reader.value, &header);
		if (next == ARCHIVE_EOF) break;
		if (next != ARCHIVE_OK && next != ARCHIVE_WARN) { error.code = ErrorCode::Corrupted; goto cleanup; }
		archive_entry* copy = archive_entry_clone(header);
		if (copy == NULL) { error.code = ErrorCode::WriteFailed; goto cleanup; }
		// Keep all cloned metadata, but normalize the name into the ZIP writer's
		// UTF-8 header path so Unicode entries remain writable.
		const CString entryPath = PathOf(header);
		const std::string utf8Path = Utf8Path(entryPath);
		if (utf8Path.empty()) { archive_entry_free(copy); error.code = ErrorCode::WriteFailed; goto cleanup; }
		archive_entry_set_pathname_utf8(copy, utf8Path.c_str());
		const bool match = ordinal == target.occurrence && archive_entry_filetype(header) == AE_IFREG && PathOf(header) == target.path;
		if (match) archive_entry_set_size(copy, static_cast<la_int64_t>(replacement.size()));
		const int writeHeader = archive_write_header(writer.value, copy);
		archive_entry_free(copy);
		if (writeHeader < ARCHIVE_WARN) { error.code = ErrorCode::WriteFailed; goto cleanup; }
		if (match)
		{
			if (!WritePayload(writer.value, replacement)) { error.code = ErrorCode::WriteFailed; goto cleanup; }
			if (archive_write_finish_entry(writer.value) < ARCHIVE_WARN) { error.code = ErrorCode::WriteFailed; goto cleanup; }
			archive_read_data_skip(reader.value); replaced = true;
		}
		else if (!CopyPayload(reader.value, writer.value) || archive_write_finish_entry(writer.value) < ARCHIVE_WARN) { error.code = ErrorCode::WriteFailed; goto cleanup; }
		++ordinal;
	}
	if (!replaced) { error.code = ErrorCode::EntryNotFound; goto cleanup; }
	if (archive_write_close(writer.value) < ARCHIVE_WARN) { error.code = ErrorCode::WriteFailed; goto cleanup; }
	// ReplaceFile cannot atomically replace an archive while our reader still
	// owns its source handle on Windows.
	if (archive_read_close(reader.value) != ARCHIVE_OK) { error.code = ErrorCode::Corrupted; goto cleanup; }
	if (!::ReplaceFileW(storagePath, temporary, NULL, REPLACEFILE_WRITE_THROUGH, NULL, NULL)) { error.code = ErrorCode::ReplaceFailed; error.systemError = ::GetLastError(); goto cleanup; }
	completed = true;
cleanup:
	if (!completed) ::DeleteFileW(temporary);
	return completed;
}
}
