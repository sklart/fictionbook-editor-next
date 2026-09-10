#pragma once

#include "../DocumentLocation.h"
#include "../ArchiveRecentDocuments.h"

namespace WTL { class CRecentDocumentList; }

namespace FbeRecentDocuments
{
	typedef FbeArchiveRecentDocuments::Record ArchiveMruRecord;
	void ReadArchiveMruRecords(std::vector<FbeArchiveRecentDocuments::Record>& records);
	bool ParseArchiveMruUnsigned(const CString& text, unsigned int& value);
	CString ArchiveMruKey(const DocumentLocation& location);
	bool ParseArchiveMruKey(const CString& key, DocumentLocation& location);
	bool FindArchiveMruRecord(const CString& key, DocumentLocation& location);
	bool SameArchiveMruIdentity(const DocumentLocation& left, const DocumentLocation& right);
	CString ArchiveMruCaption(const CString& key);
	void TouchMruOrder(const CString& key);
	void ReadMruOrder(std::vector<CString>& order);
	void ReadPortableMru(WTL::CRecentDocumentList& list);
void RememberArchiveMruRecord(WTL::CRecentDocumentList& list, const DocumentLocation& location);
void RememberNormalMruRecord(WTL::CRecentDocumentList& list, const CString& path);
void RemoveArchiveMruRecord(WTL::CRecentDocumentList& list, const DocumentLocation& location);
void AddArchiveMruRecordsToList(WTL::CRecentDocumentList& list);
void RemoveLegacyArchiveMruEntries(WTL::CRecentDocumentList& list);
void WritePortableMru(const WTL::CRecentDocumentList& list);
void WriteRegistryMruWithoutArchive(WTL::CRecentDocumentList& list, LPCTSTR settingsKey);
void RebuildMruMenu(WTL::CRecentDocumentList& list);
}
