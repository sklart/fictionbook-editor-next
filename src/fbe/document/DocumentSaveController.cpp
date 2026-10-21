#include "stdafx.h"
#include "DocumentSaveController.h"
#include "../StartupTrace.h"
#include "DocumentSession.h"
#include "..\\apputils.h"
#include "..\\FBDoc.h"
#include "../archive/ArchiveDocumentWriter.h"
#include "../BackupFileCommit.h"

void DocumentSaveController::WriteSnapshot(const std::vector<unsigned char>& bytes, const CString& path, bool overwrite, bool backup)
{
	const int slash = path.ReverseFind(L'\\');
	if (slash < 0 || bytes.empty()) _com_issue_error(E_INVALIDARG);
	wchar_t temporary[MAX_PATH] = {};
	if (!::GetTempFileNameW(path.Left(slash + 1), L"fbs", 0, temporary))
		_com_issue_error(HRESULT_FROM_WIN32(::GetLastError()));
	bool preserve = false;
	HANDLE file = INVALID_HANDLE_VALUE;
	try {
		file = ::CreateFileW(temporary, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE) _com_issue_error(HRESULT_FROM_WIN32(::GetLastError()));
		size_t offset = 0;
		while (offset < bytes.size()) {
			const DWORD requested = static_cast<DWORD>(min(bytes.size() - offset, static_cast<size_t>(1024 * 1024)));
			DWORD written = 0;
			if (!::WriteFile(file, bytes.data() + offset, requested, &written, NULL)) _com_issue_error(HRESULT_FROM_WIN32(::GetLastError()));
			if (written != requested) _com_issue_error(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT));
			offset += written;
		}
		::CloseHandle(file);file = INVALID_HANDLE_VALUE;
		FbeBackupFileCommit::CommitSavedFile(temporary, path, backup, ::ReplaceFileW, &preserve, overwrite);
	} catch (...) {
		if (file != INVALID_HANDLE_VALUE) ::CloseHandle(file);
		if (!preserve) ::DeleteFileW(temporary);
		throw;
	}
}

DocumentSaveResult DocumentSaveController::SaveCurrentSnapshot(DocumentSession& session, const std::vector<unsigned char>& bytes)
{
	const DocumentLocation location = session.Location();
	if (location.storagePath.IsEmpty()) _com_issue_error(HRESULT_FROM_WIN32(ERROR_INVALID_NAME));
	if (location.containerKind == DocumentContainerKind::Rar) _com_issue_error(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	if (IsDocumentLocationModified(location)) _com_issue_error(HRESULT_FROM_WIN32(ERROR_FILE_INVALID));
	DocumentLocation saved = location;
	if (location.IsArchive()) {
		FbeArchive::Error error;
		if (!FbeArchive::SaveDocument(location, bytes, saved, error))
			return {DocumentSaveStatus::Failed, DocumentSaveFailureKind::ArchiveWrite, location, error};
		if (!UpdateDocumentLocationFingerprint(saved)) _com_issue_error(HRESULT_FROM_WIN32(::GetLastError()));
		session.SavedArchive(saved);
	} else {
		// Deny concurrent writers while checking and committing the normal file.
		// Sharing DELETE permits the atomic replacement of this directory entry.
		HANDLE source = ::CreateFileW(location.storagePath, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (source == INVALID_HANDLE_VALUE) _com_issue_error(HRESULT_FROM_WIN32(::GetLastError()));
		try {
			if (IsDocumentLocationModified(location)) _com_issue_error(HRESULT_FROM_WIN32(ERROR_FILE_INVALID));
			WriteSnapshot(bytes, location.storagePath, true, _Settings.GetCreateBackupFile());
		} catch (...) {::CloseHandle(source);throw;}
		::CloseHandle(source);
		if (!UpdateDocumentLocationFingerprint(saved)) _com_issue_error(HRESULT_FROM_WIN32(::GetLastError()));
		session.OpenNormal(saved.storagePath, saved.documentType);
	}
	return {DocumentSaveStatus::Success, DocumentSaveFailureKind::None, saved, FbeArchive::Error()};
}

void DocumentSaveController::SaveAsSnapshot(DocumentSession& session, const CString& path, FictionBookFileType type,
	const std::vector<unsigned char>& bytes, bool overwrite)
{
	WriteSnapshot(bytes, path, overwrite);
	DocumentLocation saved = CreateNormalDocumentLocation(path, type);
	if (!UpdateDocumentLocationFingerprint(saved)) _com_issue_error(HRESULT_FROM_WIN32(::GetLastError()));
	session.SaveAsNormal(path, type);
}

DocumentSaveResult DocumentSaveController::SaveCurrent(FB::Doc& document, DocumentSession& session, const DocumentLocation& location)
{
	StartupTrace::ScopedOperation operation(StartupTrace::Operation::Save);
	if (!location.IsArchive())
	{
		if (!document.Save()) return { DocumentSaveStatus::Failed, DocumentSaveFailureKind::NormalWrite, location, FbeArchive::Error() };
		session.Saved();
		operation.Succeeded();
		return { DocumentSaveStatus::Success, DocumentSaveFailureKind::None, location, FbeArchive::Error() };
	}

	std::vector<unsigned char> serialized;
	if (!document.SerializeToMemory(serialized, location.documentType)) return { DocumentSaveStatus::Failed, DocumentSaveFailureKind::Serialization, location, FbeArchive::Error() };
	FbeArchive::Error error;
	DocumentLocation savedLocation;
	if (!FbeArchive::SaveDocument(location, serialized, savedLocation, error)) return { DocumentSaveStatus::Failed, DocumentSaveFailureKind::ArchiveWrite, location, error };
	session.SavedArchive(savedLocation);
	operation.Succeeded();
	return { DocumentSaveStatus::Success, DocumentSaveFailureKind::None, savedLocation, FbeArchive::Error() };
}

DocumentSaveResult DocumentSaveController::SaveAsNormal(FB::Doc& document, DocumentSession& session, const DocumentSaveAsRequest& request)
{
	StartupTrace::ScopedOperation operation(StartupTrace::Operation::Save);
	const CString previousEncoding = document.m_encoding;
	document.m_encoding = request.encoding;
	if (!document.Save(request.filename))
	{
		document.m_encoding = previousEncoding;
		return { DocumentSaveStatus::Failed, DocumentSaveFailureKind::NormalWrite, session.Location(), FbeArchive::Error() };
	}
	session.SaveAsNormal(request.filename, document.GetDocumentFileType());
	operation.Succeeded();
	return { DocumentSaveStatus::Success, DocumentSaveFailureKind::None, session.Location(), FbeArchive::Error() };
}
