#pragma once

#include "..\\DocumentLocation.h"
#include <vector>

namespace FbeArchiveRecentDocuments
{
struct Record
{
	DocumentLocation location;
};

bool ParseUnsigned(const CString& text, unsigned int& value);
bool IsValidRecord(const Record& record);
void SplitFields(const CString& line, std::vector<CString>& fields);
CString CompactCaptionPart(const CString& value, int limit);
CString DisplayName(const std::vector<Record>& records, size_t target);
bool SameIdentity(const DocumentLocation& left, const DocumentLocation& right);
CString Key(const DocumentLocation& location);
bool ParseKey(const CString& key, DocumentLocation& location);
}
