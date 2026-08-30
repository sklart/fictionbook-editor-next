#pragma once

#include <atlstr.h>
#include <objidl.h>
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
bool TryReadStream(IStream* stream, CoverImage& coverImage, size_t maximumDecodedBytes, ATL::CString* errorMessage = nullptr);

} // namespace FB2CoverImage
