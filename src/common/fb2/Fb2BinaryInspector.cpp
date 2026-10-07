#include "Fb2BinaryInspector.h"

#include <algorithm>
#include <cctype>
#include <cwctype>

namespace
{
bool IsXmlWhitespace(wchar_t ch)
{
    return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
}

int Base64Value(wchar_t ch)
{
    if (ch >= L'A' && ch <= L'Z') return ch - L'A';
    if (ch >= L'a' && ch <= L'z') return ch - L'a' + 26;
    if (ch >= L'0' && ch <= L'9') return ch - L'0' + 52;
    if (ch == L'+') return 62;
    if (ch == L'/') return 63;
    return -1;
}

std::wstring TrimAndLower(const std::wstring& value)
{
    size_t first = 0, last = value.size();
    while (first < last && std::iswspace(value[first])) ++first;
    while (last > first && std::iswspace(value[last - 1])) --last;
    std::wstring result(value.substr(first, last - first));
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return result;
}

bool IsKnownMimeAlias(const std::wstring& mime)
{
    const std::wstring normalized = TrimAndLower(mime);
    return normalized == L"image/jpg" || normalized == L"image/pjpeg";
}

std::uint16_t ReadLittleEndian16(const std::vector<std::uint8_t>& bytes, size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

std::uint32_t ReadLittleEndian32(const std::vector<std::uint8_t>& bytes, size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
        static_cast<std::uint32_t>(bytes[offset + 1]) << 8 |
        static_cast<std::uint32_t>(bytes[offset + 2]) << 16 |
        static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

bool IsBmp(const std::vector<std::uint8_t>& bytes)
{
    constexpr size_t kFileHeaderSize = 14;
    if (bytes.size() < kFileHeaderSize + sizeof(std::uint32_t) || bytes[0] != 'B' || bytes[1] != 'M') return false;
    if (ReadLittleEndian16(bytes, 6) != 0 || ReadLittleEndian16(bytes, 8) != 0) return false;

    const std::uint32_t pixelOffset = ReadLittleEndian32(bytes, 10);
    const std::uint32_t dibHeaderSize = ReadLittleEndian32(bytes, kFileHeaderSize);
    const bool knownDibHeader = dibHeaderSize == 12 || dibHeaderSize == 40 || dibHeaderSize == 52 ||
        dibHeaderSize == 56 || dibHeaderSize == 108 || dibHeaderSize == 124;
    if (!knownDibHeader || bytes.size() < kFileHeaderSize + dibHeaderSize) return false;
    return pixelOffset >= kFileHeaderSize + dibHeaderSize;
}
}

bool FbeFb2Binary::DecodeBase64(const std::wstring& text, std::vector<std::uint8_t>& bytes)
{
    bytes.clear();
    bytes.reserve(text.size() / 4 * 3);
    int quartet[4] = { 0, 0, 0, 0 };
    int count = 0;
    bool complete = false;
    for (size_t index = 0; index < text.size(); ++index)
    {
        const wchar_t ch = text[index];
        if (IsXmlWhitespace(ch)) continue;
        if (complete) { bytes.clear(); return false; }
        if (ch == L'=') quartet[count++] = -2;
        else
        {
            const int value = Base64Value(ch);
            if (value < 0 || count >= 4) { bytes.clear(); return false; }
            quartet[count++] = value;
        }
        if (count != 4) continue;
        if (quartet[0] < 0 || quartet[1] < 0 || (quartet[2] == -2 && quartet[3] != -2) ||
            (quartet[2] == -2 && (quartet[1] & 0x0F) != 0) ||
            (quartet[3] == -2 && quartet[2] >= 0 && (quartet[2] & 0x03) != 0)) { bytes.clear(); return false; }
        bytes.push_back(static_cast<std::uint8_t>((quartet[0] << 2) | (quartet[1] >> 4)));
        if (quartet[2] != -2)
        {
            bytes.push_back(static_cast<std::uint8_t>(((quartet[1] & 0x0F) << 4) | (quartet[2] >> 2)));
            if (quartet[3] != -2) bytes.push_back(static_cast<std::uint8_t>(((quartet[2] & 0x03) << 6) | quartet[3]));
            else complete = true;
        }
        else complete = true;
        count = 0;
    }
    if (count != 0) { bytes.clear(); return false; }
    return true;
}

std::wstring FbeFb2Binary::NormalizeMimeType(const std::wstring& mime)
{
    std::wstring normalized = TrimAndLower(mime);
    if (IsKnownMimeAlias(mime)) return L"image/jpeg";
    return normalized;
}

std::wstring FbeFb2Binary::DetectMimeType(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47 && bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A) return L"image/png";
    if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) return L"image/jpeg";
    if (bytes.size() >= 6 && bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8' && (bytes[4] == '7' || bytes[4] == '9') && bytes[5] == 'a') return L"image/gif";
    if (bytes.size() >= 12 && bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' && bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P') return L"image/webp";
    if (IsBmp(bytes)) return L"image/bmp";
    if (bytes.size() >= 4 && ((bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == 0x2A && bytes[3] == 0x00) || (bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0x00 && bytes[3] == 0x2A))) return L"image/tiff";
    const size_t probeLength = (std::min)(bytes.size(), static_cast<size_t>(512));
    size_t offset = 0;
    if (probeLength >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) offset = 3;
    while (offset < probeLength && (bytes[offset] == ' ' || bytes[offset] == '\t' || bytes[offset] == '\r' || bytes[offset] == '\n')) ++offset;
    std::string probe(reinterpret_cast<const char*>(bytes.data() + offset), probeLength - offset);
    std::transform(probe.begin(), probe.end(), probe.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (probe.find("<svg") != std::string::npos) return L"image/svg+xml";
    return std::wstring();
}

FbeFb2Binary::Inspection FbeFb2Binary::InspectBinary(const std::wstring& declaredMime, const std::wstring& base64)
{
    Inspection inspection;
    inspection.declaredMime = declaredMime;
    inspection.normalizedDeclaredMime = NormalizeMimeType(declaredMime);
    if (!DecodeBase64(base64, inspection.bytes)) { inspection.status = BinaryStatus::InvalidBase64; return inspection; }
    if (inspection.bytes.empty()) { inspection.status = BinaryStatus::Empty; return inspection; }
    inspection.detectedMime = DetectMimeType(inspection.bytes);
    if (inspection.detectedMime.empty()) { inspection.status = BinaryStatus::UnknownFormat; return inspection; }
    if (inspection.normalizedDeclaredMime.empty()) { inspection.status = BinaryStatus::MissingMime; return inspection; }
    if (inspection.normalizedDeclaredMime != inspection.detectedMime) { inspection.status = BinaryStatus::MimeMismatch; return inspection; }
    inspection.status = IsKnownMimeAlias(inspection.declaredMime) ? BinaryStatus::MimeAlias : BinaryStatus::Ok;
    return inspection;
}
