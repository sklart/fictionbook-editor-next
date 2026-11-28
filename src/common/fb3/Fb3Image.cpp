#include "Fb3Image.h"
#include "Fb3Package.h"
#include "../fb2/Fb2BinaryInspector.h"

#include <zlib.h>
#include <cstring>

namespace FbeFb3 {
namespace {

using Bytes = std::vector<std::uint8_t>;

std::uint32_t BigEndian(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) |
        (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

std::uint32_t LittleEndian(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

bool Png(const Bytes& image) {
    bool data = false;
    for (size_t offset = 8; offset + 12 <= image.size();) {
        const auto* chunk = image.data() + offset;
        const auto count = BigEndian(chunk);
        if (count > image.size() - offset - 12) return false;
        const auto* name = chunk + 4;
        const auto* payload = name + 4;
        if (::crc32(0, name, count + 4) != BigEndian(payload + count)) return false;
        if (offset == 8 && (std::memcmp(name, "IHDR", 4) != 0 || count != 13 || !BigEndian(payload) || !BigEndian(payload + 4))) return false;
        offset += static_cast<size_t>(count) + 12;
        if (std::memcmp(name, "IDAT", 4) == 0 && count) data = true;
        if (std::memcmp(name, "IEND", 4) == 0) return data && !count && offset == image.size();
    }
    return false;
}

bool WebP(const Bytes& image) {
    const auto declared = LittleEndian(image.data() + 4);
    if (declared != image.size() - 8) return false;
    bool data = false;
    size_t offset = 12;
    for (; offset + 8 <= image.size();) {
        const auto* chunk = image.data() + offset;
        const auto count = LittleEndian(chunk + 4);
        const auto padded = static_cast<std::uint64_t>(count) + (count & 1u);
        if (padded > image.size() - offset - 8) return false;
        if ((std::memcmp(chunk, "VP8 ", 4) == 0 && count >= 10) ||
            (std::memcmp(chunk, "VP8L", 4) == 0 && count >= 5) ||
            (std::memcmp(chunk, "ANMF", 4) == 0 && count > 16)) data = true;
        offset += static_cast<size_t>(padded) + 8;
    }
    return data && offset == image.size();
}

bool Jpeg(const Bytes& image) {
    // The start of a scan must precede an end marker. Ignore data past EOI,
    // as digital cameras and editors can append metadata to otherwise intact JPEGs.
    size_t offset = 2;
    while (offset + 4 <= image.size() && image[offset++] == 0xff) {
        while (offset < image.size() && image[offset] == 0xff) ++offset;
        if (offset + 3 > image.size()) return false;
        const auto marker = image[offset++];
        const size_t length = (static_cast<size_t>(image[offset]) << 8) | image[offset + 1];
        if (length < 2 || length > image.size() - offset) return false;
        offset += length;
        if (marker == 0xda) {
            while (offset + 1 < image.size()) {
                if (image[offset] == 0xff && image[offset + 1] == 0xd9) return true;
                ++offset;
            }
            return false;
        }
    }
    return false;
}

}

void ValidateRasterImage(const std::wstring& name, const std::wstring& type, const Bytes& image) {
    if (type != FbeFb2Binary::DetectMimeType(image)) throw Error{L"Image contents do not match the declared type for " + name};
    const bool valid = type == L"image/png" ? Png(image) : type == L"image/jpeg" ? Jpeg(image) :
        type == L"image/webp" ? WebP(image) : type == L"image/gif" ? image.size() >= 14 && image.back() == 0x3b :
        type == L"image/bmp" && image.size() >= 14 && LittleEndian(image.data() + 2) <= image.size();
    if (!valid) throw Error{L"Incomplete or damaged bitmap data in " + name};
}

}
