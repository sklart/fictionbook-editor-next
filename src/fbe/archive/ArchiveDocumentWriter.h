#pragma once

#include "ArchiveReader.h"

namespace FbeArchive
{
// Saves an already serialized document into its selected ZIP entry.  UI and
// document serialization remain the responsibility of the caller.
bool SaveDocument(DocumentLocation& location, const std::vector<unsigned char>& serialized, Error& error);
}
