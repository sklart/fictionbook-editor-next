#pragma once

#include "..\ArchiveDocumentResolver.h"
struct DocumentLocation;
namespace FbeArchiveUi { bool ResolveOpenRequest(const CString& storagePath, FbeArchive::ResolvedDocument& resolved, const DocumentLocation* preferredLocation = NULL, FbeArchive::Error* failure = NULL); void ShowError(HWND owner, const FbeArchive::Error& error); }