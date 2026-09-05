#pragma once

#include <atlstr.h>
#include <vector>

namespace FB2CoverThumbnail {

struct DecodedImage {
    HBITMAP bitmap;
    int width;
    int height;

    DecodedImage();
    ~DecodedImage();

    void Reset();
    bool IsEmpty() const;
};

bool TryDecode(const std::vector<unsigned char>& bytes, DecodedImage& image, ATL::CString* errorMessage = nullptr);
bool TryResizeToFit(const DecodedImage& sourceImage, unsigned int maxEdge, DecodedImage& resizedImage, ATL::CString* errorMessage = nullptr);

} // namespace FB2CoverThumbnail
