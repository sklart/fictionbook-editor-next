#pragma once

#include "DocumentLocation.h"
#include <vector>

struct DocumentOpenSource
{
	DocumentLocation location;
	std::vector<unsigned char> rawBytes;

	static DocumentOpenSource Normal(const CString& path);
	bool IsArchive() const { return location.IsArchive(); }
};
