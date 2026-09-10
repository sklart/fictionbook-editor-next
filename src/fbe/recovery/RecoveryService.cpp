#include "stdafx.h"
#include "RecoveryService.h"
#include "..\\apputils.h"
#include "..\\FBDoc.h"
#include "..\\StartupTrace.h"

namespace FbeRecovery
{
RecoveryService::RecoveryService() : m_written(false) {}

bool RecoveryService::SaveSourceSnapshot(const CString& filename, const char* sourceText, size_t sourceTextLength) const
{
	CString temporaryFile;
	HANDLE file = INVALID_HANDLE_VALUE;
	StartupTrace::Event(L"recovery", L"R100", L"source recovery started");
	try
	{
		CString directory(filename);
		const int separator = directory.ReverseFind(L'\\');
		if (separator < 0) directory = L".\\";
		else directory.Delete(separator, directory.GetLength() - separator);
		wchar_t temporaryBuffer[MAX_PATH] = {};
		if (::GetTempFileName(directory, L"fbs", 0, temporaryBuffer) == 0) return false;
		temporaryFile = temporaryBuffer;
		file = ::CreateFile(temporaryFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE) throw ::GetLastError();
		DWORD written = 0;
		if (sourceTextLength > 0 && (!::WriteFile(file, sourceText, static_cast<DWORD>(sourceTextLength), &written, NULL) || written != static_cast<DWORD>(sourceTextLength))) throw ::GetLastError();
		if (!::FlushFileBuffers(file)) throw ::GetLastError();
		::CloseHandle(file); file = INVALID_HANDLE_VALUE;
		if (!::MoveFileEx(temporaryFile, filename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw ::GetLastError();
		StartupTrace::Event(L"recovery", L"R110", L"source recovery completed");
		return true;
	}
	catch (...)
	{
		if (file != INVALID_HANDLE_VALUE) ::CloseHandle(file);
		if (!temporaryFile.IsEmpty()) ::DeleteFile(temporaryFile);
		StartupTrace::Error(L"recovery", L"R120", L"source recovery failed");
		return false;
	}
}

bool RecoveryService::Save(FB::Doc* document, bool documentChanged, bool sourceActive, bool sourceXmlInvalid,
	const char* sourceText, size_t sourceTextLength, const DocumentLocation& location)
{
	if (!document || !documentChanged) return false;
	const CString snapshotPath(m_store.SnapshotPath());
	if (!m_written && ::GetFileAttributes(snapshotPath) != INVALID_FILE_ATTRIBUTES) return false;
	const bool saved = sourceActive ? SaveSourceSnapshot(snapshotPath, sourceText, sourceTextLength) : (!sourceXmlInvalid && document->SaveRecoveryCopy(snapshotPath));
	if (!saved && sourceXmlInvalid) StartupTrace::Warning(L"recovery", L"R130", L"source recovery skipped because source XML is invalid");
	if (saved && location.IsArchive()) m_store.WriteArchiveLocation(location);
	else if (saved) ::DeleteFile(m_store.ArchiveSidecarPath());
	m_written = saved || m_written;
	return saved;
}

void RecoveryService::DeleteIfWritten()
{
	if (!m_written) return;
	m_store.DeleteFiles();
	m_written = false;
}

bool RecoveryService::GetRestoreCandidate(bool hasCommandLineArguments, RestoreCandidate& candidate) const
{
	if (hasCommandLineArguments || !m_store.HasSnapshot()) return false;
	candidate.snapshotPath = m_store.SnapshotPath();
	candidate.archiveBacked = m_store.ReadArchiveLocation(candidate.archiveLocation);
	return true;
}

void RecoveryService::CompleteRestore()
{
	m_store.DeleteFiles();
	m_written = false;
}

CString RecoveryService::SnapshotPath() const { return m_store.SnapshotPath(); }
}
