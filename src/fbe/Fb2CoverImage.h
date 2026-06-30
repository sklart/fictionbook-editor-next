#pragma once

#include <atlstr.h>
#include <vector>

namespace FB2CoverImage {

struct CoverImage {
    ATL::CString href;
    ATL::CString binaryId;
    ATL::CString contentType;
    std::vector<unsigned char> bytes;

    void Clear();
    bool IsEmpty() const;
};

bool TryRead(const wchar_t* filePath, CoverImage& coverImage, ATL::CString* errorMessage = nullptr);

} // namespace FB2CoverImage
