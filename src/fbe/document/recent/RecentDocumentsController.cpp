#include "stdafx.h"
#include "RecentDocumentsController.h"
#include "RecentDocumentsManager.h"
#include "../../archive/ui/ArchiveOpenCoordinator.h"

bool FbeRecentDocuments::Controller::Resolve(WORD command, CString& normalPath, DocumentLocation& archiveLocation, bool& archive)
{
	normalPath.Empty(); archiveLocation = DocumentLocation(); archive = false;
	if(!m_list.GetFromList(command, normalPath)) return false;
	archive = FindArchiveMruRecord(normalPath, archiveLocation);
	return true;
}

void FbeRecentDocuments::Controller::OnOpened(WORD command, const CString& normalPath, const DocumentLocation& archiveLocation, bool archive)
{
	m_list.MoveToTop(command);
	if(archive) RememberArchiveMruRecord(m_list, archiveLocation);
	else TouchMruOrder(normalPath);
	RebuildMruMenu(m_list);
}

void FbeRecentDocuments::Controller::OnFailed(WORD command)
{
	CString key; DocumentLocation archive;
	if (m_list.GetFromList(command, key) && !ParseArchiveMruKey(key, archive)) RemoveMruOrder(key);
	m_list.RemoveFromList(command);
	RebuildMruMenu(m_list);
}

void FbeRecentDocuments::Controller::OnCancelledArchive(const DocumentLocation& archiveLocation)
{
	FbeArchive::ResolvedDocument probe; FbeArchive::Error error;
	if (!FbeArchiveUi::ResolveOpenRequest(archiveLocation.storagePath, probe, &archiveLocation, &error) &&
		(error.code == FbeArchive::ErrorCode::EntryNotFound || error.code == FbeArchive::ErrorCode::OpenFailed))
		RemoveArchiveMruRecord(m_list, archiveLocation);
}

void FbeRecentDocuments::Controller::OnSavedAsNormal(const CString& path)
{
	RememberNormalMruRecord(m_list, path);
}
