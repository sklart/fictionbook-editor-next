#pragma once

#include "..\\ArchiveRecentDocuments.h"
#include <vector>

namespace FbeRecentDocuments
{
class Store
{
public:
	void ReadArchiveRecords(std::vector<FbeArchiveRecentDocuments::Record>& records) const;
	bool WriteArchiveRecords(const std::vector<FbeArchiveRecentDocuments::Record>& records) const;

	void ReadOrder(std::vector<CString>& order) const;
	bool WriteOrder(const std::vector<CString>& order) const;

	void ReadPortable(std::vector<CString>& entries) const;
	bool WritePortable(const std::vector<CString>& entries) const;
};
}
