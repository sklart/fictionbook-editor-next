#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FbeFb2Binary
{
enum class BinaryStatus
{
    Ok,
    MissingMime,
    MimeAlias,
    MimeMismatch,
    UnknownFormat,
    InvalidBase64,
    Empty
};

struct Inspection
{
    BinaryStatus status = BinaryStatus::InvalidBase64;
    std::wstring declaredMime;
    std::wstring normalizedDeclaredMime;
    std::wstring detectedMime;
    std::vector<std::uint8_t> bytes;
};

bool DecodeBase64(const std::wstring& text, std::vector<std::uint8_t>& bytes);
std::wstring NormalizeMimeType(const std::wstring& mime);
std::wstring DetectMimeType(const std::vector<std::uint8_t>& bytes);
Inspection InspectBinary(const std::wstring& declaredMime, const std::wstring& base64);
}
