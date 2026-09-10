#include "stdafx.h"
#include "RecentDocumentsManager.h"
#include "../ArchiveRecentDocuments.h"
#include "RecentDocumentsStore.h"

namespace
{

typedef FbeArchiveRecentDocuments::Record ArchiveMruRecord;

static bool ParseArchiveMruUnsigned(const CString& text, unsigned int& value)
{
	return FbeArchiveRecentDocuments::ParseUnsigned(text, value);
}

static void ReadArchiveMruRecords(std::vector<ArchiveMruRecord>& records)
{
	FbeRecentDocuments::Store().ReadArchiveRecords(records);
}

static CString ArchiveMruDisplayName(const std::vector<ArchiveMruRecord>& records, size_t target)
{
	return FbeArchiveRecentDocuments::DisplayName(records, target);
}

static bool SameArchiveMruIdentity(const DocumentLocation& left, const DocumentLocation& right)
{
	return FbeArchiveRecentDocuments::SameIdentity(left, right);
}

// CRecentDocumentList stores this opaque key, never a caption.  Captions are
// deliberately rebuilt for the menu, so they cannot become document identity.
static CString ArchiveMruKey(const DocumentLocation& location)
{
	return FbeArchiveRecentDocuments::Key(location);
}

static bool ParseArchiveMruKey(const CString& key, DocumentLocation& location)
{
	return FbeArchiveRecentDocuments::ParseKey(key, location);
}

static void ReadMruOrder(std::vector<CString>& order)
{
	FbeRecentDocuments::Store().ReadOrder(order);
}

static void WriteMruOrder(const std::vector<CString>& order)
{
	FbeRecentDocuments::Store().WriteOrder(order);
}

static void TouchMruOrder(const CString& key)
{
	std::vector<CString> order; ReadMruOrder(order);
	order.erase(std::remove_if(order.begin(), order.end(), [&key](const CString& item) { return item.CompareNoCase(key) == 0; }), order.end()); order.insert(order.begin(), key); WriteMruOrder(order);
}

static void ApplyMruOrder(CRecentDocumentList& list)
{
	std::vector<CString> order; ReadMruOrder(order);
	std::vector<CString> existing;
	for (int i = 0; i < list.m_arrDocs.GetSize(); ++i)
	{
		const CString value(list.m_arrDocs[i].szDocName);
		bool duplicate = false;
		for (size_t previous = 0; previous < existing.size(); ++previous)
			if (existing[previous].CompareNoCase(value) == 0) { duplicate = true; break; }
		if (!duplicate) existing.push_back(value);
	}

	// Build oldest-to-newest first.  Unknown legacy records retain their old
	// relative position; only entries explicitly present in MRUOrder move.
	std::vector<CString> unified;
	for (size_t i = 0; i < existing.size(); ++i)
	{
		bool present = false;
		for (size_t oi = 0; oi < order.size(); ++oi)
			if (existing[i].CompareNoCase(order[oi]) == 0) { present = true; break; }
		if (!present) unified.push_back(existing[i]);
	}
	for (size_t oi = order.size(); oi > 0; --oi)
		for (size_t i = 0; i < existing.size(); ++i)
			if (existing[i].CompareNoCase(order[oi - 1]) == 0) { unified.push_back(existing[i]); break; }

	// The backing array is oldest-to-newest.  Keep the newest ten by selecting
	// the tail before repopulating it; do not mutate the front while merging.
	if (unified.size() > 10) unified.erase(unified.begin(), unified.end() - 10);
	list.m_arrDocs.RemoveAll();
	const int previousMax = list.GetMaxEntries();
	list.SetMaxEntries(list.m_nMaxEntries_Max - 1);
	for (size_t i = 0; i < unified.size(); ++i) list.AddToList(unified[i]);
	list.SetMaxEntries(min(previousMax, 10));
}

static bool FindArchiveMruRecord(const CString& key, DocumentLocation& location)
{
	DocumentLocation requested;
	if (!ParseArchiveMruKey(key, requested)) return false;
	location = DocumentLocation(); std::vector<ArchiveMruRecord> records; ReadArchiveMruRecords(records);
	for (size_t i = 0; i < records.size(); ++i)
		if (SameArchiveMruIdentity(records[i].location, requested)) { location = records[i].location; return true; }
	return false;
}

static CString ArchiveMruCaption(const CString& key)
{
	DocumentLocation location; if (!FindArchiveMruRecord(key, location)) return key;
	std::vector<ArchiveMruRecord> records; ReadArchiveMruRecords(records);
	for (size_t i = 0; i < records.size(); ++i) if (SameArchiveMruIdentity(records[i].location, location)) return ArchiveMruDisplayName(records, i);
	return key;
}

// WTL's UpdateMenu is convenient for locating the MRU insertion point, but it
// always prefixes items with "&1", "&2", etc.  Build the dynamic section
// ourselves so captions stay presentation-only and stale dynamic entries never
// survive a refresh.
static void RebuildMruMenu(CRecentDocumentList& list)
{
	HMENU menu = list.GetMenuHandle(); if (menu == NULL) return;
	for (int index = list.m_arrDocs.GetSize() - 1; index >= 0; --index)
	{
		DocumentLocation location;
		const CString key(list.m_arrDocs[index].szDocName);
		if (ParseArchiveMruKey(key, location) && !FindArchiveMruRecord(key, location)) list.m_arrDocs.RemoveAt(index);
	}
	list.UpdateMenu();
	int insertionPoint = -1;
	for (int index = 0; index < ::GetMenuItemCount(menu); ++index)
	{
		MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
		if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID == ID_FILE_MRU_FIRST) { insertionPoint = index; break; }
	}
	if (insertionPoint < 0) return;
	// UpdateMenu can temporarily leave both the resource placeholder and a
	// generated item with ID_FILE_MRU_FIRST.  Delete every MRU-range item by
	// position, not merely the first matching command ID.
	for (int index = ::GetMenuItemCount(menu) - 1; index >= 0; --index)
	{
		MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
		if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ::DeleteMenu(menu, index, MF_BYPOSITION);
	}

	const int count = min(list.m_arrDocs.GetSize(), 10);
	if (count == 0)
	{
		::InsertMenu(menu, insertionPoint, MF_BYPOSITION | MF_STRING, ID_FILE_MRU_FIRST, list.m_szNoEntries);
		::EnableMenuItem(menu, ID_FILE_MRU_FIRST, MF_BYCOMMAND | MF_GRAYED);
		return;
	}
	for (int offset = 0; offset < count; ++offset)
	{
		const UINT id = ID_FILE_MRU_FIRST + offset; CString key;
		if (!list.GetFromList(id, key)) continue;
		DocumentLocation archiveLocation;
		CString caption = ParseArchiveMruKey(key, archiveLocation) ? ArchiveMruCaption(key) : key;
		// A menu caption is presentation only, but it must still distinguish every
		// visible command.  Preserve any minimal folder context chosen above and
		// append a compact ordinal only as the final collision fallback.
		unsigned int duplicate = 1;
		for (int previous = 0; previous < offset; ++previous)
		{
			CString previousKey; if (!list.GetFromList(ID_FILE_MRU_FIRST + previous, previousKey)) continue;
			DocumentLocation previousLocation; const CString previousCaption = ParseArchiveMruKey(previousKey, previousLocation) ? ArchiveMruCaption(previousKey) : previousKey;
			if (previousCaption.CompareNoCase(caption) == 0) ++duplicate;
		}
		if (duplicate > 1) { CString discriminator; discriminator.Format(L" (%u)", duplicate); caption = FbeArchiveRecentDocuments::CompactCaptionPart(caption, 96 - discriminator.GetLength()) + discriminator; }
		if (caption.GetLength() > 96) caption = FbeArchiveRecentDocuments::CompactCaptionPart(caption, 96);
		::InsertMenu(menu, insertionPoint + offset, MF_BYPOSITION | MF_STRING, id, caption);
	}
}

static void RememberArchiveMruRecord(CRecentDocumentList& list, const DocumentLocation& location)
{
	if (!location.IsArchive()) { list.AddToList(location.storagePath); return; }
	std::vector<ArchiveMruRecord> retained; ReadArchiveMruRecords(retained);
	retained.erase(std::remove_if(retained.begin(), retained.end(), [&location](const ArchiveMruRecord& record) { return SameArchiveMruIdentity(record.location, location); }), retained.end());
	ArchiveMruRecord current; current.location = location; retained.insert(retained.begin(), current);
	if (FbeRecentDocuments::Store().WriteArchiveRecords(retained))
	{
		list.AddToList(ArchiveMruKey(location));
		TouchMruOrder(ArchiveMruKey(location));
		for (int i = list.m_arrDocs.GetSize() - 1; i >= 0; --i) if (CString(list.m_arrDocs[i].szDocName).CompareNoCase(location.storagePath) == 0) list.m_arrDocs.RemoveAt(i);
		RebuildMruMenu(list);
	}
}

static void RememberNormalMruRecord(CRecentDocumentList& list, const CString& path)
{
	list.AddToList(path); TouchMruOrder(path); RebuildMruMenu(list);
}

static void RemoveArchiveMruRecord(CRecentDocumentList& list, const DocumentLocation& location)
{
	std::vector<ArchiveMruRecord> records; ReadArchiveMruRecords(records);
	records.erase(std::remove_if(records.begin(), records.end(), [&location](const ArchiveMruRecord& item) { return SameArchiveMruIdentity(item.location, location); }), records.end());
	FbeRecentDocuments::Store().WriteArchiveRecords(records);
	const CString key = ArchiveMruKey(location); for (int i = list.m_arrDocs.GetSize() - 1; i >= 0; --i) if (CString(list.m_arrDocs[i].szDocName) == key) list.m_arrDocs.RemoveAt(i);
	std::vector<CString> order; ReadMruOrder(order); order.erase(std::remove_if(order.begin(), order.end(), [&key](const CString& item) { return item == key; }), order.end()); WriteMruOrder(order);
	RebuildMruMenu(list);
}

static void AddArchiveMruRecordsToList(CRecentDocumentList& list)
{
	std::vector<ArchiveMruRecord> records; ReadArchiveMruRecords(records);
	for (size_t i = 0; i < records.size(); ++i) if (::GetFileAttributes(records[i].location.storagePath) == INVALID_FILE_ATTRIBUTES) RemoveArchiveMruRecord(list, records[i].location);
	ReadArchiveMruRecords(records);
	for (size_t i = records.size(); i > 0; --i) list.AddToList(ArchiveMruKey(records[i - 1].location));
	ApplyMruOrder(list);
	list.SetMaxEntries(10);
	RebuildMruMenu(list);
}

static void RemoveLegacyArchiveMruEntries(CRecentDocumentList& list)
{
	std::vector<ArchiveMruRecord> records; ReadArchiveMruRecords(records);
	for (int index = list.m_arrDocs.GetSize() - 1; index >= 0; --index)
	{
		const CString value(list.m_arrDocs[index].szDocName);
		DocumentLocation parsed;
		bool remove = ParseArchiveMruKey(value, parsed) || DetectDocumentContainerKind(value) != DocumentContainerKind::None ||
			(value.Find(L" \x2014 ") >= 0 && (value.Right(4).CompareNoCase(L".zip") == 0 || value.Right(4).CompareNoCase(L".rar") == 0));
		for (size_t record = 0; !remove && record < records.size(); ++record)
			remove = value.CompareNoCase(records[record].location.storagePath) == 0 || value.CompareNoCase(ArchiveMruDisplayName(records, record)) == 0;
		if (remove) list.m_arrDocs.RemoveAt(index);
	}
	list.UpdateMenu();
}

static void ReadPortableMru(CRecentDocumentList& list)
{
	std::vector<CString> entries;
	FbeRecentDocuments::Store().ReadPortable(entries);
	for (size_t i = 0; i < entries.size(); ++i) list.AddToList(entries[i]);
}

static void WritePortableMru(const CRecentDocumentList& list)
{
	std::vector<CString> entries;
	for (int index = 0; index < list.m_arrDocs.GetSize(); ++index)
	{
		const CString value(list.m_arrDocs[index].szDocName);
		DocumentLocation archive;
		if (!ParseArchiveMruKey(value, archive)) entries.push_back(value);
	}
	FbeRecentDocuments::Store().WritePortable(entries);
}

static void WriteRegistryMruWithoutArchive(CRecentDocumentList& list, LPCTSTR settingsKey)
{
	ATL::CSimpleArray<CRecentDocumentList::_DocEntry> saved = list.m_arrDocs;
	for (int index = list.m_arrDocs.GetSize() - 1; index >= 0; --index) { DocumentLocation archive; if (ParseArchiveMruKey(CString(list.m_arrDocs[index].szDocName), archive)) list.m_arrDocs.RemoveAt(index); }
	list.WriteToRegistry(settingsKey);
	list.m_arrDocs = saved; RebuildMruMenu(list);
}

}

namespace FbeRecentDocuments
{
void ReadArchiveMruRecords(std::vector<FbeArchiveRecentDocuments::Record>& records) { ::ReadArchiveMruRecords(records); }
bool ParseArchiveMruUnsigned(const CString& text, unsigned int& value) { return ::ParseArchiveMruUnsigned(text, value); }
CString ArchiveMruKey(const DocumentLocation& location) { return ::ArchiveMruKey(location); }
bool ParseArchiveMruKey(const CString& key, DocumentLocation& location) { return ::ParseArchiveMruKey(key, location); }
bool FindArchiveMruRecord(const CString& key, DocumentLocation& location) { return ::FindArchiveMruRecord(key, location); }
bool SameArchiveMruIdentity(const DocumentLocation& left, const DocumentLocation& right) { return ::SameArchiveMruIdentity(left, right); }
CString ArchiveMruCaption(const CString& key) { return ::ArchiveMruCaption(key); }
void TouchMruOrder(const CString& key) { ::TouchMruOrder(key); }
void ReadMruOrder(std::vector<CString>& order) { ::ReadMruOrder(order); }
void ReadPortableMru(WTL::CRecentDocumentList& list) { ::ReadPortableMru(list); }
void RememberArchiveMruRecord(CRecentDocumentList& list, const DocumentLocation& location) { ::RememberArchiveMruRecord(list, location); }
void RememberNormalMruRecord(CRecentDocumentList& list, const CString& path) { ::RememberNormalMruRecord(list, path); }
void RemoveArchiveMruRecord(CRecentDocumentList& list, const DocumentLocation& location) { ::RemoveArchiveMruRecord(list, location); }
void AddArchiveMruRecordsToList(CRecentDocumentList& list) { ::AddArchiveMruRecordsToList(list); }
void RemoveLegacyArchiveMruEntries(CRecentDocumentList& list) { ::RemoveLegacyArchiveMruEntries(list); }
void WritePortableMru(const CRecentDocumentList& list) { ::WritePortableMru(list); }
void WriteRegistryMruWithoutArchive(CRecentDocumentList& list, LPCTSTR settingsKey) { ::WriteRegistryMruWithoutArchive(list, settingsKey); }
void RebuildMruMenu(CRecentDocumentList& list) { ::RebuildMruMenu(list); }
}
