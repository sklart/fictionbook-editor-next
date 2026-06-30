#pragma once

#include <atlstr.h>

namespace FB2Metadata {

struct Metadata {
    ATL::CString title;
    ATL::CString authors;
    ATL::CString genres;
    ATL::CString keywords;
    ATL::CString language;
    ATL::CString sourceLanguage;
    ATL::CString sequence;
    ATL::CString documentAuthors;
    ATL::CString documentDate;
    ATL::CString documentDateValue;
    ATL::CString documentId;
    ATL::CString documentVersion;

    void Clear();
};

bool TryRead(const wchar_t* filePath, Metadata& metadata, ATL::CString* errorMessage = nullptr);

} // namespace FB2Metadata
