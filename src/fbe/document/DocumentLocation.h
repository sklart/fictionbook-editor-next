#pragma once

#include <atlstr.h>
#include "..\\FictionBookFileType.h"

// The physical storage path is deliberately kept separate from an entry name.
// Legacy document and scripting APIs remain filesystem-path APIs.
enum class DocumentContainerKind
{
    None,
    Zip,
    Rar
};

struct DocumentLocation
{
    DocumentContainerKind containerKind = DocumentContainerKind::None;
    CString storagePath;
    CString entryPath;
    FictionBookFileType documentType = FictionBookFileType::Unknown;
    unsigned int entryOccurrence = 0;
	unsigned __int64 containerLastWriteTime = static_cast<unsigned __int64>(-1);
	unsigned __int64 containerFileSize = static_cast<unsigned __int64>(-1);

    bool IsArchive() const { return containerKind != DocumentContainerKind::None; }
};

inline DocumentContainerKind DetectDocumentContainerKind(const CString& path)
{
    const int dot = path.ReverseFind(L'.');
    if (dot < 0) return DocumentContainerKind::None;
    const CString extension = path.Mid(dot);
    if (extension.CompareNoCase(L".zip") == 0) return DocumentContainerKind::Zip;
    if (extension.CompareNoCase(L".rar") == 0) return DocumentContainerKind::Rar;
    return DocumentContainerKind::None;
}
