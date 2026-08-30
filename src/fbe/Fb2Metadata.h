#pragma once

#include <atlstr.h>
#include <objidl.h>
#include <vector>

namespace FB2Metadata {

struct Metadata {
    ATL::CString title;
    ATL::CString authors;
    std::vector<ATL::CString> authorValues;
    ATL::CString genres;
    ATL::CString keywords;
    ATL::CString language;
    ATL::CString sourceLanguage;
    ATL::CString sequence;
    ATL::CString documentAuthors;
    std::vector<ATL::CString> documentAuthorValues;
    ATL::CString documentDate;
    ATL::CString documentDateValue;
    ATL::CString documentId;
    ATL::CString documentVersion;

    void Clear();
};

bool TryRead(const wchar_t* filePath, Metadata& metadata, ATL::CString* errorMessage = nullptr);
// Reads directly from an already-open stream.  Shell extensions use this entry
// point so Explorer never needs a temporary copy of the FB2 file.
bool TryReadStream(IStream* stream, Metadata& metadata, ATL::CString* errorMessage = nullptr);

} // namespace FB2Metadata
