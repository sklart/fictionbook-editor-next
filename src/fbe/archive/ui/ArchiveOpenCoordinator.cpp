#include "stdafx.h"
#include "ArchiveOpenCoordinator.h"
#include "..\..\ArchiveEntryPicker.h"
#include "..\..\document\DocumentLocation.h"
#include "..\..\RuntimeLocalization.h"
static bool IsArchiveTestScenario(const wchar_t* expected) { wchar_t mode[4] = {}, scenario[64] = {}; return ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", mode, _countof(mode)) == 1 && mode[0] == L'1' && ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SCENARIO", scenario, _countof(scenario)) == wcslen(expected) && wcscmp(scenario, expected) == 0; }
typedef FbeArchive::ResolvedDocument ResolvedOpenDocument;

bool FbeArchiveUi::ResolveOpenRequest(const CString& storagePath, FbeArchive::ResolvedDocument& resolved,
	const DocumentLocation* preferredLocation, FbeArchive::Error* failure)
{
	std::vector<FbeArchive::Entry> entries;
	FbeArchive::Error error;
	if (!FbeArchive::EnumerateFictionBookEntries(storagePath, entries, error)) { if (failure) *failure = error; return false; }
	int selected = 0;
	const bool hasPreferredLocation = preferredLocation != NULL;
	if (preferredLocation != NULL)
	{
		selected = -1;
		for (size_t index = 0; index < entries.size(); ++index)
		{
			if (entries[index].path == preferredLocation->entryPath &&
				entries[index].occurrence == preferredLocation->entryOccurrence)
			{
				selected = static_cast<int>(index);
				break;
			}
		}
		if (selected < 0) { error.code = FbeArchive::ErrorCode::EntryNotFound; if (failure) *failure = error; return false; }
	}
	if (!hasPreferredLocation && entries.size() > 1)
	{
		// The native picker remains the production path.  Runtime integration
		// tests may select an exact internal name without automating a dialog.
		wchar_t testMode[4] = {}, requestedEntry[MAX_PATH] = {}, requestedOccurrence[16] = {};
		const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode));
		const DWORD requestedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_ENTRY", requestedEntry, _countof(requestedEntry));
		const DWORD occurrenceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_OCCURRENCE", requestedOccurrence, _countof(requestedOccurrence));
		if (testModeLength == 1 && testMode[0] == L'1' && requestedLength && requestedLength < _countof(requestedEntry))
		{
			unsigned int requestedIndex = 0; wchar_t* occurrenceEnd = NULL;
			const unsigned long parsedOccurrence = occurrenceLength ? wcstoul(requestedOccurrence, &occurrenceEnd, 10) : 0;
			if (occurrenceLength && (occurrenceEnd == requestedOccurrence || *occurrenceEnd != L'\0' || parsedOccurrence > UINT_MAX)) { error.code = FbeArchive::ErrorCode::EntryNotFound; if (failure) *failure = error; return false; }
			requestedIndex = static_cast<unsigned int>(parsedOccurrence);
			selected = -1;
			for (size_t index = 0; index < entries.size(); ++index)
				if (entries[index].path == requestedEntry && (!occurrenceLength || entries[index].occurrence == requestedIndex)) { selected = static_cast<int>(index); break; }
			if (selected < 0) { error.code = FbeArchive::ErrorCode::EntryNotFound; if (failure) *failure = error; return false; }
		}
		else
		{
		CArchiveEntryPicker picker(entries);
		if (picker.DoModal() != IDOK) return false;
		selected = picker.SelectedIndex();
		if (selected < 0 || static_cast<size_t>(selected) >= entries.size()) return false;
		}
	}
	if (!FbeArchive::ResolveDocument(storagePath, entries[selected], resolved, error)) { if (failure) *failure = error; return false; }
	return true;
}

void FbeArchiveUi::ShowError(HWND owner, const FbeArchive::Error& error)
{
	if (IsArchiveTestScenario(L"archive-runtime") || IsArchiveTestScenario(L"archive-open-runtime") || IsArchiveTestScenario(L"archive-rar-save-runtime") || IsArchiveTestScenario(L"archive-mru-runtime") ||
		IsArchiveTestScenario(L"archive-two-phase-runtime") || IsArchiveTestScenario(L"archive-recovery-external-verify")) return;
	LPCWSTR key = L"fbe.archive.error.corrupted", fallback = L"The archive is corrupted or cannot be read.";
	switch (error.code)
	{
	case FbeArchive::ErrorCode::NoFictionBookEntries: key = L"fbe.archive.error.no_documents"; fallback = L"No FictionBook documents were found in the archive."; break;
	case FbeArchive::ErrorCode::Encrypted: key = L"fbe.archive.error.encrypted"; fallback = L"The archive is password protected. Opening protected archives is not supported yet."; break;
	case FbeArchive::ErrorCode::EntryTooLarge: key = L"fbe.archive.error.too_large"; fallback = L"The FictionBook document in the archive is too large to open."; break;
	case FbeArchive::ErrorCode::EntryNotFound: key = L"fbe.archive.error.entry_missing"; fallback = L"The selected document no longer exists in the archive."; break;
	case FbeArchive::ErrorCode::UnsupportedFormat: key = L"fbe.archive.error.unsupported"; fallback = L"The archive format is not supported."; break;
	case FbeArchive::ErrorCode::ModifiedExternally: key = L"fbe.archive.error.modified"; fallback = L"The archive was modified by another program."; break;
	case FbeArchive::ErrorCode::WriteFailed:
	case FbeArchive::ErrorCode::ReplaceFailed: key = L"fbe.archive.error.update_failed"; fallback = L"Failed to update the ZIP archive."; break;
	default: break;
	}
	const CString text = FbeLoadRuntimeStringByKey(key, fallback);
	const CString caption = FbeLoadRuntimeStringByKey(L"fbe.archive.error.caption", L"Archive");
	::MessageBox(owner, text, caption, MB_OK | MB_ICONEXCLAMATION);
}
