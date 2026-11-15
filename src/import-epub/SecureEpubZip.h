#pragma once

// Extract a ZIP archive into a newly created, private directory. No archive
// entry is allowed to replace an existing file or traverse a reparse point.
bool ExtractSecureEpubZip(const CStringW& source, const CStringW& destination, CStringW& error);
