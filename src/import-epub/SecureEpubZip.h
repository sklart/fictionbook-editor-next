#pragma once

#include <atlstr.h>
#include <cstddef>

struct SecureZipLimits {
    unsigned long long entryBytes;
    unsigned long long totalBytes;
    std::size_t entries;
};

bool ExtractSecureZip(const CStringW& source, const CStringW& destination, const SecureZipLimits& limits,
    CStringW& error);

// Extract a ZIP archive into a newly created, private directory. No archive
// entry is allowed to replace an existing file or traverse a reparse point.
bool ExtractSecureEpubZip(const CStringW& source, const CStringW& destination, CStringW& error);
