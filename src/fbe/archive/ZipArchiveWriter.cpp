#include "stdafx.h"
#include "ArchiveReader.h"

#define LIBARCHIVE_STATIC
#include <archive.h>
#include <archive_entry.h>

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
	const void* block = NULL; size_t size = 0; la_int64_t offset = 0;
	for (;;)
	{
		const int result = archive_read_data_block(reader, &block, &size, &offset);
		if (result == ARCHIVE_EOF) return true;
		if (result != ARCHIVE_OK || archive_write_data_block(writer, block, size, offset) != ARCHIVE_OK) return false;
	}
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
	bool completed = false;
	Reader reader; Writer writer;
	if (!OpenZipReader(reader.value, storagePath) || !writer.value ||
		archive_write_set_format_zip(writer.value) != ARCHIVE_OK ||
		archive_write_open_filename_w(writer.value, temporary) != ARCHIVE_OK)
	{
		error.code = ErrorCode::WriteFailed; goto cleanup;
	}
	archive_entry* header = NULL; unsigned int ordinal = 0; bool replaced = false;
	for (;;)
	{
		const int next = archive_read_next_header(reader.value, &header);
		if (next == ARCHIVE_EOF) break;
		if (next != ARCHIVE_OK && next != ARCHIVE_WARN) { error.code = ErrorCode::Corrupted; goto cleanup; }
		archive_entry* copy = archive_entry_clone(header);
		if (copy == NULL) { error.code = ErrorCode::WriteFailed; goto cleanup; }
		const bool match = ordinal == target.occurrence && archive_entry_filetype(header) == AE_IFREG && PathOf(header) == target.path;
		if (match) archive_entry_set_size(copy, static_cast<la_int64_t>(replacement.size()));
		const int writeHeader = archive_write_header(writer.value, copy);
		archive_entry_free(copy);
		if (writeHeader != ARCHIVE_OK) { error.code = ErrorCode::WriteFailed; goto cleanup; }
		if (match)
		{
			if (!replacement.empty() && archive_write_data(writer.value, &replacement[0], replacement.size()) != static_cast<la_ssize_t>(replacement.size())) { error.code = ErrorCode::WriteFailed; goto cleanup; }
			if (archive_write_finish_entry(writer.value) != ARCHIVE_OK) { error.code = ErrorCode::WriteFailed; goto cleanup; }
			archive_read_data_skip(reader.value); replaced = true;
		}
		else if (!CopyPayload(reader.value, writer.value)) { error.code = ErrorCode::WriteFailed; goto cleanup; }
		++ordinal;
	}
	if (!replaced) { error.code = ErrorCode::EntryNotFound; goto cleanup; }
	if (archive_write_close(writer.value) != ARCHIVE_OK) { error.code = ErrorCode::WriteFailed; goto cleanup; }
	if (!::ReplaceFileW(storagePath, temporary, NULL, REPLACEFILE_WRITE_THROUGH, NULL, NULL)) { error.code = ErrorCode::ReplaceFailed; error.systemError = ::GetLastError(); goto cleanup; }
	completed = true;
cleanup:
	if (!completed) ::DeleteFileW(temporary);
	return completed;
}
}
