#include "../../src/common/fb2/Fb2BinaryInspector.h"

#include <iostream>

namespace
{
int failures = 0;
void Check(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << std::endl; ++failures; }
}

std::wstring Base64(const std::vector<std::uint8_t>& bytes)
{
    static const wchar_t alphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::wstring text;
    for (size_t index = 0; index < bytes.size(); index += 3)
    {
        const unsigned value = static_cast<unsigned>(bytes[index]) << 16 | (index + 1 < bytes.size() ? static_cast<unsigned>(bytes[index + 1]) << 8 : 0) | (index + 2 < bytes.size() ? bytes[index + 2] : 0);
        text += alphabet[(value >> 18) & 63]; text += alphabet[(value >> 12) & 63];
        text += index + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : L'=';
        text += index + 2 < bytes.size() ? alphabet[value & 63] : L'=';
    }
    return text;
}

void CheckMime(const std::vector<std::uint8_t>& bytes, const wchar_t* mime, const char* name)
{
    Check(FbeFb2Binary::DetectMimeType(bytes) == mime, name);
}
}

int main()
{
    CheckMime({ 0xFF, 0xD8, 0xFF }, L"image/jpeg", "JPEG");
    CheckMime({ 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A }, L"image/png", "PNG");
    CheckMime({ 'G', 'I', 'F', '8', '7', 'a' }, L"image/gif", "GIF87a");
    CheckMime({ 'G', 'I', 'F', '8', '9', 'a' }, L"image/gif", "GIF89a");
    CheckMime({ 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P' }, L"image/webp", "WebP");
    CheckMime({ '<', 's', 'v', 'g', ' ' }, L"image/svg+xml", "SVG");
    const std::vector<std::uint8_t> bmp = {
        'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0,
        12, 0, 0, 0, 1, 0, 1, 0, 1, 0, 24, 0
    };
    CheckMime(bmp, L"image/bmp", "BMP");
    Check(FbeFb2Binary::DetectMimeType({ 'B' }).empty(), "BMP short signature");
    Check(FbeFb2Binary::DetectMimeType({ 'B', 'M' }).empty(), "BMP short header");
    std::vector<std::uint8_t> truncatedBmp = bmp;
    truncatedBmp.resize(17);
    Check(FbeFb2Binary::DetectMimeType(truncatedBmp).empty(), "BMP incomplete DIB size");
    std::vector<std::uint8_t> invalidBmpOffset = bmp;
    invalidBmpOffset[10] = 13;
    Check(FbeFb2Binary::DetectMimeType(invalidBmpOffset).empty(), "BMP invalid pixel offset");
    std::vector<std::uint8_t> outOfRangeBmpOffset = bmp;
    outOfRangeBmpOffset[10] = 64;
    Check(FbeFb2Binary::DetectMimeType(outOfRangeBmpOffset).empty(), "BMP pixel offset beyond binary");
    std::vector<std::uint8_t> invalidBmpDib = bmp;
    invalidBmpDib[14] = 13;
    Check(FbeFb2Binary::DetectMimeType(invalidBmpDib).empty(), "BMP invalid DIB header");
    CheckMime({ 'I', 'I', 0x2A, 0 }, L"image/tiff", "TIFF");
    Check(FbeFb2Binary::DetectMimeType({ 1, 2, 3 }).empty(), "unknown");

    const std::wstring jpeg = Base64({ 0xFF, 0xD8, 0xFF });
    Check(FbeFb2Binary::InspectBinary(L"image/jpeg", jpeg).status == FbeFb2Binary::BinaryStatus::Ok, "jpeg ok");
    Check(FbeFb2Binary::InspectBinary(L"image/jpg", jpeg).status == FbeFb2Binary::BinaryStatus::MimeAlias, "jpg alias");
    Check(FbeFb2Binary::InspectBinary(L"image/pjpeg", jpeg).status == FbeFb2Binary::BinaryStatus::MimeAlias, "pjpeg alias");
    Check(FbeFb2Binary::InspectBinary(L"IMAGE/JPEG", jpeg).status == FbeFb2Binary::BinaryStatus::Ok, "jpeg case normalization");
    Check(FbeFb2Binary::InspectBinary(L" image/jpeg ", jpeg).status == FbeFb2Binary::BinaryStatus::Ok, "jpeg whitespace normalization");
    Check(FbeFb2Binary::InspectBinary(L"image/jpeg ", jpeg).status == FbeFb2Binary::BinaryStatus::Ok, "jpeg trailing whitespace normalization");
    Check(FbeFb2Binary::InspectBinary(L"image/png", jpeg).status == FbeFb2Binary::BinaryStatus::MimeMismatch, "mismatch");
    Check(FbeFb2Binary::InspectBinary(L"", Base64({ 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A })).status == FbeFb2Binary::BinaryStatus::MissingMime, "missing mime");
    const FbeFb2Binary::Inspection octet = FbeFb2Binary::InspectBinary(L"application/octet-stream", Base64({ 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A }));
    Check(octet.status == FbeFb2Binary::BinaryStatus::MimeMismatch && octet.detectedMime == L"image/png", "octet png detection");
    Check(FbeFb2Binary::InspectBinary(L"application/octet-stream", Base64({ 1, 2, 3 })).status == FbeFb2Binary::BinaryStatus::UnknownFormat, "unknown format");
    Check(FbeFb2Binary::InspectBinary(L"image/png", L"AA=A").status == FbeFb2Binary::BinaryStatus::InvalidBase64, "padding");
    Check(FbeFb2Binary::InspectBinary(L"image/png", L"AAAA=").status == FbeFb2Binary::BinaryStatus::InvalidBase64, "incomplete group");
    Check(FbeFb2Binary::InspectBinary(L"image/png", L"TQ==AAAA").status == FbeFb2Binary::BinaryStatus::InvalidBase64, "data after padding");
    Check(FbeFb2Binary::InspectBinary(L"image/png", L"AB==").status == FbeFb2Binary::BinaryStatus::InvalidBase64, "non-canonical padding bits");
    Check(FbeFb2Binary::InspectBinary(L"image/png", L"AAAA!===").status == FbeFb2Binary::BinaryStatus::InvalidBase64, "invalid character");
    Check(FbeFb2Binary::InspectBinary(L"image/png", L"").status == FbeFb2Binary::BinaryStatus::Empty, "empty");
    const std::vector<std::uint8_t> block(64 * 1024, 0x5A);
    const std::wstring block64 = Base64(block);
    for (int index = 0; index < 100; ++index)
        Check(FbeFb2Binary::InspectBinary(L"application/octet-stream", block64).bytes.size() == block.size(), "64 KiB batch");
    for (int index = 0; index < 500; ++index)
        Check(FbeFb2Binary::InspectBinary(L"application/octet-stream", L"AQID").bytes.size() == 3, "small batch");
    const std::vector<std::uint8_t> large(25 * 1024 * 1024, 0x33);
    Check(FbeFb2Binary::InspectBinary(L"application/octet-stream", Base64(large)).bytes.size() == large.size(), "25 MiB");
    return failures == 0 ? 0 : 1;
}
