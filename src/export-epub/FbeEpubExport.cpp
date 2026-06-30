#include "FbeEpubExport.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <codecvt>
#endif

namespace fbe::epub {
namespace {

std::string NarrowAscii(const char* s) { return std::string(s); }

std::vector<std::uint8_t> Bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

std::string JoinPathUtf8(const std::wstring& a, const std::wstring& b) {
    std::wstring result = a;
    if (!result.empty() && result.back() != L'/') {
        result += L'/';
    }
    result += b;
    std::replace(result.begin(), result.end(), L'\\', L'/');
    return Utf8FromWide(result);
}

std::uint64_t Fnv1a64(const std::wstring& seed, std::uint64_t basis) {
    std::uint64_t h = basis;
    for (wchar_t ch : seed) {
        const std::uint32_t value = static_cast<std::uint32_t>(ch);
        h ^= (value & 0xFFu);
        h *= 1099511628211ull;
        h ^= ((value >> 8u) & 0xFFu);
        h *= 1099511628211ull;
        h ^= ((value >> 16u) & 0xFFu);
        h *= 1099511628211ull;
        h ^= ((value >> 24u) & 0xFFu);
        h *= 1099511628211ull;
    }
    return h;
}

std::wstring HexByte(std::uint8_t value) {
    const wchar_t* digits = L"0123456789abcdef";
    std::wstring out;
    out.push_back(digits[(value >> 4u) & 0x0Fu]);
    out.push_back(digits[value & 0x0Fu]);
    return out;
}

std::wstring DeterministicUuidUrn(const std::wstring& seed) {
    std::array<std::uint8_t, 16> bytes{};
    const std::uint64_t h1 = Fnv1a64(seed, 14695981039346656037ull);
    const std::uint64_t h2 = Fnv1a64(L"ExportEPUB|" + seed, 1099511628211ull);

    for (int i = 0; i < 8; ++i) {
        bytes[static_cast<size_t>(i)] = static_cast<std::uint8_t>((h1 >> ((7 - i) * 8)) & 0xFFu);
        bytes[static_cast<size_t>(i + 8)] = static_cast<std::uint8_t>((h2 >> ((7 - i) * 8)) & 0xFFu);
    }

    // RFC 4122 variant + version 5-like deterministic UUID.
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0Fu) | 0x50u);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3Fu) | 0x80u);

    std::wstring out = L"urn:uuid:";
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back(L'-');
        out += HexByte(bytes[i]);
    }
    return out;
}

std::wstring EnsureIdentifier(const EpubBook& book) {
    if (!book.identifier.empty()) {
        return book.identifier;
    }

    // EPUBCheck treats urn:uuid:* as a real UUID, so the fallback must use
    // the canonical 8-4-4-4-12 hexadecimal UUID form.
    const std::wstring seed = book.title + L"|" + book.author + L"|" + book.language;
    return DeterministicUuidUrn(seed.empty() ? L"ExportEPUB" : seed);
}

std::wstring EnsureTitle(const EpubBook& book) {
    return book.title.empty() ? L"Untitled" : book.title;
}

std::wstring EnsureLanguage(const EpubBook& book) {
    return book.language.empty() ? L"ru" : book.language;
}


std::vector<std::wstring> EffectiveAuthors(const EpubBook& book) {
    if (!book.authors.empty()) {
        return book.authors;
    }
    if (!book.author.empty()) {
        return { book.author };
    }
    return {};
}

bool HasCover(const EpubBook& book) {
    if (book.coverImageId.empty()) return false;
    for (const auto& r : book.resources) {
        if (r.id == book.coverImageId) return true;
    }
    return false;
}

const EpubResource* FindResourceById(const EpubBook& book, const std::wstring& id) {
    for (const auto& r : book.resources) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

bool ResourceIsSvg(const EpubResource& resource) {
    return resource.mediaType == "image/svg+xml";
}

std::wstring ManifestPropertiesAttr(const std::wstring& props) {
    if (props.empty()) return {};
    return L" properties=\"" + XmlEscape(props) + L"\"";
}

std::wstring NormalizeSubject(const std::wstring& value) {
    std::wstring s = value;
    while (!s.empty() && (s.front() == L' ' || s.front() == L'\t' || s.front() == L'\r' || s.front() == L'\n')) s.erase(s.begin());
    while (!s.empty() && (s.back() == L' ' || s.back() == L'\t' || s.back() == L'\r' || s.back() == L'\n')) s.pop_back();
    return s;
}

std::wstring MakeModifiedTimestampUtc() {
    using clock = std::chrono::system_clock;
    std::time_t now = clock::to_time_t(clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    std::wstringstream ss;
    ss << std::put_time(&tm, L"%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}


bool IsAsciiDigit(wchar_t ch) {
    return ch >= L'0' && ch <= L'9';
}

bool IsAllDigits(const std::wstring& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), IsAsciiDigit);
}

std::wstring TrimDateText(std::wstring s) {
    while (!s.empty() && (s.front() == L' ' || s.front() == L'\t' || s.front() == L'\r' || s.front() == L'\n')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == L' ' || s.back() == L'\t' || s.back() == L'\r' || s.back() == L'\n')) {
        s.pop_back();
    }
    return s;
}

bool LooksLikeW3cDate(const std::wstring& s) {
    // W3CDTF forms commonly accepted by EPUBCheck: YYYY, YYYY-MM,
    // YYYY-MM-DD, and date-time forms beginning with YYYY-MM-DDT.
    if (s.size() == 4) {
        return IsAllDigits(s);
    }
    if (s.size() == 7 && s[4] == L'-') {
        return IsAllDigits(s.substr(0, 4)) && IsAllDigits(s.substr(5, 2));
    }
    if (s.size() >= 10 && s[4] == L'-' && s[7] == L'-') {
        return IsAllDigits(s.substr(0, 4)) && IsAllDigits(s.substr(5, 2)) && IsAllDigits(s.substr(8, 2));
    }
    return false;
}

std::wstring TwoDigits(const std::wstring& s) {
    return s.size() == 1 ? (L"0" + s) : s;
}

std::vector<std::wstring> SplitByChar(const std::wstring& s, wchar_t delim) {
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t ch : s) {
        if (ch == delim) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    parts.push_back(cur);
    return parts;
}

std::wstring NormalizeOpfDate(const std::wstring& value) {
    std::wstring s = TrimDateText(value);
    if (s.empty()) return {};

    // Remove common FB2/free-text wrappers while keeping the first date token.
    const std::wstring delimiters = L" \t\r\n,;()[]{}";
    const std::size_t firstDelim = s.find_first_of(delimiters);
    if (firstDelim != std::wstring::npos) {
        const std::wstring first = s.substr(0, firstDelim);
        if (LooksLikeW3cDate(first) || first.find(L'.') != std::wstring::npos || first.find(L'/') != std::wstring::npos) {
            s = first;
        }
    }

    if (LooksLikeW3cDate(s)) {
        return s;
    }

    // FB2 files often store a localized visible date like 25.06.2026 even
    // when date/@value has a normalized value. EPUB OPF requires W3CDTF.
    const wchar_t delim = (s.find(L'.') != std::wstring::npos) ? L'.' : ((s.find(L'/') != std::wstring::npos) ? L'/' : L'\0');
    if (delim != L'\0') {
        const auto parts = SplitByChar(s, delim);
        if (parts.size() == 3 && IsAllDigits(parts[0]) && IsAllDigits(parts[1]) && IsAllDigits(parts[2])) {
            // dd.mm.yyyy or dd/mm/yyyy -> yyyy-mm-dd.
            if (parts[2].size() == 4) {
                return parts[2] + L"-" + TwoDigits(parts[1]) + L"-" + TwoDigits(parts[0]);
            }
            // yyyy.mm.dd or yyyy/mm/dd -> yyyy-mm-dd.
            if (parts[0].size() == 4) {
                return parts[0] + L"-" + TwoDigits(parts[1]) + L"-" + TwoDigits(parts[2]);
            }
        }
    }

    if (IsAllDigits(s) && s.size() == 8) {
        // yyyyMMdd -> yyyy-mm-dd.
        return s.substr(0, 4) + L"-" + s.substr(4, 2) + L"-" + s.substr(6, 2);
    }

    if (IsAllDigits(s) && s.size() >= 4) {
        // Last-resort fallback for strings such as "2026 год" after tokenizing.
        return s.substr(0, 4);
    }

    // Unknown free-form date would make EPUB 2 invalid. Omit it from OPF;
    // the visible FB2 date remains in body XHTML where relevant.
    return {};
}

std::wstring IdOrGenerated(const EpubChapter& chapter, std::size_t index) {
    if (!chapter.id.empty()) {
        return chapter.id;
    }
    std::wstringstream ss;
    ss << L"chap" << std::setw(3) << std::setfill(L'0') << (index + 1);
    return ss.str();
}

std::wstring ChapterFileName(std::size_t index) {
    std::wstringstream ss;
    ss << L"text/chapter_" << std::setw(3) << std::setfill(L'0') << (index + 1) << L".xhtml";
    return ss.str();
}

std::string NormalizeHrefUtf8(const std::wstring& href) {
    std::wstring tmp = href;
    std::replace(tmp.begin(), tmp.end(), L'\\', L'/');
    while (!tmp.empty() && tmp.front() == L'/') {
        tmp.erase(tmp.begin());
    }
    return Utf8FromWide(tmp);
}

std::wstring DefaultCssWide() {
    return LR"CSS(
body {
  margin: 5%;
  line-height: 1.35;
  font-family: serif;
}
p {
  margin: 0 0 0.75em 0;
  text-align: justify;
}
h1, h2, h3 {
  line-height: 1.2;
  text-align: center;
  text-indent: 0;
  page-break-after: avoid;
}
.title, .subtitle {
  text-align: center;
  text-indent: 0;
}
img {
  max-width: 100%;
  height: auto;
}
.cover {
  text-align: center;
  page-break-after: always;
}
.image,
.annotation p,
.epigraph p,
.cite p,
.poem p,
.stanza p {
  text-indent: 0;
}
.empty-line {
  height: 1em;
  margin: 0;
  padding: 0;
  line-height: 1em;
}
.poem {
  margin: 1em 0 1em 2em;
  text-align: left;
}
.verse {
  margin: 0;
  text-indent: 0;
  text-align: left;
}
.epigraph, .cite {
  margin: 1.2em 2em;
  text-indent: 0;
}
.text-author {
  text-align: right;
  font-style: normal;
  text-indent: 0;
}
code, .code {
  font-family: monospace;
  white-space: pre-wrap;
}
.note {
  font-size: 0.95em;
}
.noteref {
  vertical-align: super;
  font-size: 0.8em;
  text-decoration: none;
  white-space: nowrap;
}
.backlinks {
  text-align: right;
  font-size: 0.9em;
  text-indent: 0;
}
.backlink {
  margin-left: 0.75em;
  text-decoration: none;
}
table {
  border-collapse: collapse;
  margin: 1em auto;
}
td, th {
  padding: 0.2em 0.4em;
  vertical-align: top;
}
)CSS";
}

class Crc32 {
public:
    Crc32() { Init(); }

    std::uint32_t Compute(const std::vector<std::uint8_t>& data) const {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (std::uint8_t b : data) {
            crc = table_[(crc ^ b) & 0xFFu] ^ (crc >> 8u);
        }
        return crc ^ 0xFFFFFFFFu;
    }

private:
    void Init() {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1u)) : (c >> 1u);
            }
            table_[i] = c;
        }
    }

    std::array<std::uint32_t, 256> table_{};
};

class ZipStoredWriter {
public:
    bool Open(const std::filesystem::path& path, std::wstring* error) {
        out_.open(path, std::ios::binary | std::ios::trunc);
        if (!out_) {
            SetError(error, L"Cannot create output EPUB file: " + path.wstring());
            return false;
        }
        return true;
    }

    bool AddFile(const std::string& pathUtf8, const std::vector<std::uint8_t>& data, std::wstring* error) {
        if (!out_) {
            SetError(error, L"ZIP writer is not open");
            return false;
        }
        if (pathUtf8.empty()) {
            SetError(error, L"ZIP entry path is empty");
            return false;
        }
        if constexpr (sizeof(std::vector<std::uint8_t>::size_type) > sizeof(std::uint32_t)) {
            if (data.size() > 0xFFFFFFFFull) {
                SetError(error, L"ZIP64 is not implemented in the built-in writer");
                return false;
            }
        }
        if (pathUtf8.size() > 0xFFFFu) {
            SetError(error, L"ZIP entry path is too long");
            return false;
        }

        Entry e;
        e.path = pathUtf8;
        e.crc = crc_.Compute(data);
        e.size = static_cast<std::uint32_t>(data.size());
        e.offset = Tell32(error);
        if (error && !error->empty()) return false;

        // Local file header.
        Write32(0x04034b50u);
        Write16(10);          // version needed: 1.0 because method is Store.
        Write16(0x0800);      // UTF-8 file names.
        Write16(0);           // compression method: Store.
        Write16(0);           // file time.
        Write16(0);           // file date.
        Write32(e.crc);
        Write32(e.size);
        Write32(e.size);
        Write16(static_cast<std::uint16_t>(e.path.size()));
        Write16(0);           // extra field length. Important for EPUB mimetype.
        out_.write(e.path.data(), static_cast<std::streamsize>(e.path.size()));
        if (!data.empty()) {
            out_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
        if (!out_) {
            SetError(error, L"Failed while writing ZIP local entry");
            return false;
        }

        entries_.push_back(e);
        return true;
    }

    bool Close(std::wstring* error) {
        if (!out_) return true;

        const std::uint32_t centralOffset = Tell32(error);
        if (error && !error->empty()) return false;

        for (const Entry& e : entries_) {
            Write32(0x02014b50u);
            Write16(0x0314);       // version made by: Unix-ish + 2.0.
            Write16(10);           // version needed.
            Write16(0x0800);       // UTF-8 file names.
            Write16(0);            // Store.
            Write16(0);
            Write16(0);
            Write32(e.crc);
            Write32(e.size);
            Write32(e.size);
            Write16(static_cast<std::uint16_t>(e.path.size()));
            Write16(0);            // extra length.
            Write16(0);            // comment length.
            Write16(0);            // disk number start.
            Write16(0);            // internal attributes.
            Write32(0);            // external attributes.
            Write32(e.offset);
            out_.write(e.path.data(), static_cast<std::streamsize>(e.path.size()));
        }

        const std::uint32_t centralSize = Tell32(error) - centralOffset;
        if (error && !error->empty()) return false;

        if (entries_.size() > 0xFFFFu) {
            SetError(error, L"Too many ZIP entries for non-ZIP64 writer");
            return false;
        }

        WriteEndOfCentralDirectory(
            static_cast<std::uint16_t>(entries_.size()),
            centralSize,
            centralOffset);

        out_.close();
        if (!out_) {
            SetError(error, L"Failed to finalize ZIP/EPUB file");
            return false;
        }
        return true;
    }

private:
    struct Entry {
        std::string path;
        std::uint32_t crc = 0;
        std::uint32_t size = 0;
        std::uint32_t offset = 0;
    };

    void SetError(std::wstring* error, const std::wstring& message) const {
        if (error) *error = message;
    }

    std::uint32_t Tell32(std::wstring* error) {
        const auto pos = out_.tellp();
        if (pos < 0 || static_cast<std::uint64_t>(pos) > 0xFFFFFFFFull) {
            SetError(error, L"Built-in ZIP writer does not support ZIP64 output");
            return 0;
        }
        return static_cast<std::uint32_t>(pos);
    }

    static void Append16(std::vector<char>& bytes, std::uint16_t v) {
        bytes.push_back(static_cast<char>(v & 0xFFu));
        bytes.push_back(static_cast<char>((v >> 8u) & 0xFFu));
    }

    static void Append32(std::vector<char>& bytes, std::uint32_t v) {
        bytes.push_back(static_cast<char>(v & 0xFFu));
        bytes.push_back(static_cast<char>((v >> 8u) & 0xFFu));
        bytes.push_back(static_cast<char>((v >> 16u) & 0xFFu));
        bytes.push_back(static_cast<char>((v >> 24u) & 0xFFu));
    }

    void WriteEndOfCentralDirectory(std::uint16_t entryCount,
                                    std::uint32_t centralSize,
                                    std::uint32_t centralOffset) {
        std::vector<char> eocd;
        eocd.reserve(22);
        Append32(eocd, 0x06054b50u);
        Append16(eocd, 0); // disk number
        Append16(eocd, 0); // central directory disk
        Append16(eocd, entryCount);
        Append16(eocd, entryCount);
        Append32(eocd, centralSize);
        Append32(eocd, centralOffset);
        Append16(eocd, 0); // comment length
        out_.write(eocd.data(), static_cast<std::streamsize>(eocd.size()));
    }

    void Write16(std::uint16_t v) {
        char b[2] = {
            static_cast<char>(v & 0xFFu),
            static_cast<char>((v >> 8u) & 0xFFu)
        };
        out_.write(b, 2);
    }

    void Write32(std::uint32_t v) {
        char b[4] = {
            static_cast<char>(v & 0xFFu),
            static_cast<char>((v >> 8u) & 0xFFu),
            static_cast<char>((v >> 16u) & 0xFFu),
            static_cast<char>((v >> 24u) & 0xFFu)
        };
        out_.write(b, 4);
    }

    std::ofstream out_;
    std::vector<Entry> entries_;
    Crc32 crc_;
};

std::wstring BuildContainerXml(const EpubExportOptions& options) {
    std::wstringstream x;
    x << LR"(<?xml version="1.0" encoding="UTF-8"?>)" << L"\n"
      << LR"(<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">)" << L"\n"
      << L"  <rootfiles>\n"
      << L"    <rootfile full-path=\"" << XmlEscape(options.packageRoot) << L"/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
      << L"  </rootfiles>\n"
      << L"</container>\n";
    return x.str();
}

std::wstring BuildChapterXhtml(const EpubBook& book,
                               const EpubChapter& chapter,
                               EpubVersion version) {
    const bool isEpub3 = version == EpubVersion::Epub3;
    const std::wstring lang = XmlEscape(EnsureLanguage(book));
    const std::wstring title = XmlEscape(chapter.title.empty() ? EnsureTitle(book) : chapter.title);
    const std::wstring body = chapter.bodyXhtml.empty()
        ? (L"<p>" + XmlEscape(title) + L"</p>")
        : chapter.bodyXhtml;
    const bool bodyAlreadyHasHeading =
        body.find(L"class=\"title\"") != std::wstring::npos ||
        body.find(L"<h1") != std::wstring::npos ||
        body.find(L"<h2") != std::wstring::npos;

    std::wstringstream x;
    x << LR"(<?xml version="1.0" encoding="UTF-8"?>)" << L"\n";
    if (version == EpubVersion::Epub2) {
        x << LR"(<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">)" << L"\n";
    } else {
        x << LR"(<!DOCTYPE html>)" << L"\n";
    }
    x << LR"X(<html xmlns="http://www.w3.org/1999/xhtml")X";
    if (isEpub3) {
        x << LR"X( xmlns:epub="http://www.idpf.org/2007/ops")X";
    }
    x << LR"X( xml:lang=")X" << lang << LR"X(" lang=")X" << lang << LR"X(">
)X"
      << L"<head>\n"
      << L"  <title>" << title << L"</title>\n"
      << LR"X(  <link rel="stylesheet" type="text/css" href="../styles/style.css"/>
)X"
      << L"</head>\n"
      << L"<body";
    if (isEpub3 && !chapter.epubType.empty()) {
        x << LR"X( epub:type=")X" << XmlEscape(chapter.epubType) << LR"X(")X";
    }
    x << L">\n";
    if (!bodyAlreadyHasHeading) {
        x << L"  <h1>" << title << L"</h1>\n";
    }
    x << body << L"\n"
      << L"</body>\n"
      << L"</html>\n";
    return x.str();
}

std::wstring BuildCoverXhtml(const EpubBook& book, EpubVersion version) {
    const EpubResource* cover = FindResourceById(book, book.coverImageId);
    if (cover == nullptr) return {};

    const bool isEpub3 = version == EpubVersion::Epub3;
    const std::wstring lang = XmlEscape(EnsureLanguage(book));
    const std::wstring title = XmlEscape(EnsureTitle(book));
    const std::wstring src = XmlEscape(cover->href);

    std::wstringstream x;
    x << LR"(<?xml version="1.0" encoding="UTF-8"?>)" << L"\n";
    if (version == EpubVersion::Epub2) {
        x << LR"(<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">)" << L"\n";
    } else {
        x << LR"(<!DOCTYPE html>)" << L"\n";
    }
    x << LR"X(<html xmlns="http://www.w3.org/1999/xhtml")X";
    if (isEpub3) {
        x << LR"X( xmlns:epub="http://www.idpf.org/2007/ops")X";
    }
    x << LR"X( xml:lang=")X" << lang << LR"X(" lang=")X" << lang << LR"X(">
)X"
      << L"<head>\n"
      << L"  <title>Обложка — " << title << L"</title>\n"
      << LR"X(  <link rel="stylesheet" type="text/css" href="styles/style.css"/>
)X"
      << L"</head>\n"
      << L"<body" << (isEpub3 ? LR"X( epub:type="cover")X" : L"") << L">\n"
      << LR"X(  <div class="cover">
)X";

    if (ResourceIsSvg(*cover)) {
        x << LR"X(    <object type="image/svg+xml" data=")X" << src << LR"X(">)X" << title << L"</object>\n";
    } else {
        x << LR"X(    <img alt=")X" << title << LR"X(" src=")X" << src << LR"X("/>
)X";
    }

    x << L"  </div>\n"
      << L"</body>\n"
      << L"</html>\n";
    return x.str();
}

std::size_t NavDepth(const std::vector<EpubNavPoint>& points) {
    std::size_t depth = 0;
    for (const auto& p : points) {
        depth = std::max<std::size_t>(depth, 1 + NavDepth(p.children));
    }
    return depth;
}

std::size_t TocDepth(const EpubBook& book, bool includeCoverPage) {
    std::size_t depth = includeCoverPage && HasCover(book) ? 1 : 0;
    for (const auto& c : book.chapters) {
        if (!c.includeInToc) continue;
        depth = std::max<std::size_t>(depth, 1 + NavDepth(c.navChildren));
    }
    return std::max<std::size_t>(depth, 1);
}

void WriteNcxNavPoints(std::wstringstream& x,
                       const std::vector<EpubNavPoint>& points,
                       int& playOrder,
                       int indent)
{
    const std::wstring pad(static_cast<std::size_t>(indent), L' ');
    for (const auto& p : points) {
        if (p.title.empty() || p.href.empty()) continue;
        const int thisOrder = playOrder++;
        x << pad << LR"X(<navPoint id="navPoint-)X" << thisOrder << LR"X(" playOrder=")X" << thisOrder << LR"X(">)X" << L"\n"
          << pad << L"  <navLabel><text>" << XmlEscape(p.title) << L"</text></navLabel>\n"
          << pad << L"  <content src=\"" << XmlEscape(p.href) << L"\"/>\n";
        WriteNcxNavPoints(x, p.children, playOrder, indent + 2);
        x << pad << L"</navPoint>\n";
    }
}

void WriteNavHtmlPoints(std::wstringstream& x,
                        const std::vector<EpubNavPoint>& points,
                        int indent)
{
    if (points.empty()) return;
    const std::wstring pad(static_cast<std::size_t>(indent), L' ');
    x << pad << L"<ol>\n";
    for (const auto& p : points) {
        if (p.title.empty() || p.href.empty()) continue;
        x << pad << L"  <li><a";
        if (!p.epubType.empty()) {
            x << L" epub:type=\"" << XmlEscape(p.epubType) << L"\"";
        }
        x << L" href=\"" << XmlEscape(p.href) << L"\">" << XmlEscape(p.title) << L"</a>";
        if (!p.children.empty()) {
            x << L"\n";
            WriteNavHtmlPoints(x, p.children, indent + 4);
            x << pad << L"  ";
        }
        x << L"</li>\n";
    }
    x << pad << L"</ol>\n";
}

std::wstring BuildNcx(const EpubBook& book, bool includeCoverPage, bool includeCoverInToc) {
    const std::wstring uid = XmlEscape(EnsureIdentifier(book));
    const std::wstring title = XmlEscape(EnsureTitle(book));
    const bool hasCover = includeCoverPage && includeCoverInToc && HasCover(book);

    std::wstringstream x;
    x << LR"(<?xml version="1.0" encoding="UTF-8"?>)" << L"\n"
      << LR"(<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">)" << L"\n"
      << L"<head>\n"
      << LR"X(  <meta name="dtb:uid" content=")X" << uid << LR"X("/>
)X"
      << L"  <meta name=\"dtb:depth\" content=\"" << TocDepth(book, includeCoverPage) << L"\"/>\n"
      << LR"X(  <meta name="dtb:totalPageCount" content="0"/>
  <meta name="dtb:maxPageNumber" content="0"/>
)X"
      << L"</head>\n"
      << L"<docTitle><text>" << title << L"</text></docTitle>\n"
      << L"<navMap>\n";

    int playOrder = 1;
    if (hasCover) {
        x << LR"X(  <navPoint id="navPoint-cover" playOrder=")X" << playOrder++ << LR"X(">
    <navLabel><text>Обложка</text></navLabel>
    <content src="cover.xhtml"/>
  </navPoint>
)X";
    }

    for (std::size_t i = 0; i < book.chapters.size(); ++i) {
        const auto& c = book.chapters[i];
        if (!c.includeInToc) continue;
        const int thisOrder = playOrder++;
        const std::wstring chapterTitle = XmlEscape(c.title.empty() ? L"Chapter" : c.title);
        x << LR"X(  <navPoint id="navPoint-)X" << thisOrder << LR"X(" playOrder=")X" << thisOrder << LR"X(">
    <navLabel><text>)X" << chapterTitle << LR"X(</text></navLabel>
    <content src=")X" << XmlEscape(ChapterFileName(i)) << LR"X("/>
)X";
        WriteNcxNavPoints(x, c.navChildren, playOrder, 4);
        x << L"  </navPoint>\n";
    }

    x << L"</navMap>\n</ncx>\n";
    return x.str();
}


int FirstChapterIndexByEpubType(const EpubBook& book, const std::wstring& epubType) {
    for (std::size_t i = 0; i < book.chapters.size(); ++i) {
        if (book.chapters[i].epubType == epubType) return static_cast<int>(i);
    }
    return -1;
}

std::wstring BuildNavXhtml(const EpubBook& book, bool includeCoverPage, bool includeCoverInToc) {
    const std::wstring lang = XmlEscape(EnsureLanguage(book));
    const std::wstring title = XmlEscape(EnsureTitle(book));
    const bool hasCover = includeCoverPage && includeCoverInToc && HasCover(book);

    std::wstringstream x;
    x << LR"(<?xml version="1.0" encoding="UTF-8"?>)" << L"\n"
      << LR"(<!DOCTYPE html>)" << L"\n"
      << LR"X(<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" xml:lang=")X" << lang << LR"X(" lang=")X" << lang << LR"X(">
<head>
  <title>)X" << title << LR"X(</title>
  <link rel="stylesheet" type="text/css" href="styles/style.css"/>
</head>
<body>
<nav epub:type="toc" id="toc">
  <h1>)X" << title << LR"X(</h1>
  <ol>
)X";

    if (hasCover) {
        x << LR"X(    <li><a href="cover.xhtml">Обложка</a></li>
)X";
    }

    for (std::size_t i = 0; i < book.chapters.size(); ++i) {
        const auto& c = book.chapters[i];
        if (!c.includeInToc) continue;
        x << LR"X(    <li><a href=")X" << XmlEscape(ChapterFileName(i)) << LR"X(">)X"
          << XmlEscape(c.title.empty() ? L"Chapter" : c.title) << LR"X(</a>)X";
        if (!c.navChildren.empty()) {
            x << L"\n";
            WriteNavHtmlPoints(x, c.navChildren, 6);
            x << L"    ";
        }
        x << L"</li>\n";
    }

    x << LR"X(  </ol>
</nav>
<nav epub:type="landmarks" id="landmarks">
  <h2>Навигация</h2>
  <ol>
)X";
    if (hasCover) {
        x << LR"X(    <li><a epub:type="cover" href="cover.xhtml">Обложка</a></li>
)X";
    }
    const int frontmatterIndex = FirstChapterIndexByEpubType(book, L"frontmatter");
    if (frontmatterIndex >= 0) {
        x << LR"X(    <li><a epub:type="frontmatter" href=")X" << XmlEscape(ChapterFileName(static_cast<std::size_t>(frontmatterIndex))) << LR"X(">Аннотация</a></li>
)X";
    }
    const int bodymatterIndex = FirstChapterIndexByEpubType(book, L"bodymatter");
    if (bodymatterIndex >= 0) {
        x << LR"X(    <li><a epub:type="bodymatter" href=")X" << XmlEscape(ChapterFileName(static_cast<std::size_t>(bodymatterIndex))) << LR"X(">Начало текста</a></li>
)X";
    }
    const int endnotesIndex = FirstChapterIndexByEpubType(book, L"endnotes");
    if (endnotesIndex >= 0) {
        x << LR"X(    <li><a epub:type="endnotes" href=")X" << XmlEscape(ChapterFileName(static_cast<std::size_t>(endnotesIndex))) << LR"X(">Примечания</a></li>
)X";
    }
    x << LR"X(  </ol>
</nav>
</body>
</html>
)X";
    return x.str();
}


std::wstring BuildOpf(const EpubBook& book, const EpubExportOptions& options) {
    const bool isEpub3 = options.version == EpubVersion::Epub3;
    const bool hasCoverResource = HasCover(book);
    const bool hasCoverPage = options.includeCoverPage && hasCoverResource;
    const std::wstring identifier = XmlEscape(EnsureIdentifier(book));
    const std::wstring title = XmlEscape(EnsureTitle(book));
    const std::wstring language = XmlEscape(EnsureLanguage(book));
    const std::wstring opfDate = NormalizeOpfDate(book.date);
    const std::vector<std::wstring> authors = EffectiveAuthors(book);

    std::wstringstream x;
    x << LR"(<?xml version="1.0" encoding="UTF-8"?>)" << L"\n";
    if (isEpub3) {
        x << LR"(<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang=")"
          << language << LR"(" prefix="rendition: http://www.idpf.org/vocab/rendition/# schema: http://schema.org/">)" << L"\n";
    } else {
        x << LR"(<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="2.0">)" << L"\n";
    }

    if (isEpub3) {
        x << L"<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
          << L"  <dc:identifier id=\"bookid\">" << identifier << L"</dc:identifier>\n";
        if (!book.isbn.empty()) {
            x << L"  <dc:identifier id=\"isbn\">" << XmlEscape(book.isbn) << L"</dc:identifier>\n";
        }
        x << L"  <dc:title>" << title << L"</dc:title>\n"
          << L"  <dc:language>" << language << L"</dc:language>\n"
          << L"  <meta property=\"dcterms:modified\">" << MakeModifiedTimestampUtc() << L"</meta>\n"
          << L"  <meta property=\"rendition:layout\">reflowable</meta>\n"
          << L"  <meta property=\"rendition:orientation\">auto</meta>\n"
          << L"  <meta property=\"rendition:spread\">auto</meta>\n"
          << L"  <meta property=\"schema:accessMode\">textual</meta>\n"
          << L"  <meta property=\"schema:accessModeSufficient\">textual</meta>\n"
          << L"  <meta property=\"schema:accessibilityFeature\">readingOrder</meta>\n"
          << L"  <meta property=\"schema:accessibilityFeature\">tableOfContents</meta>\n"
          << L"  <meta property=\"schema:accessibilityHazard\">none</meta>\n";
        for (std::size_t i = 0; i < authors.size(); ++i) {
            if (authors[i].empty()) continue;
            x << L"  <dc:creator id=\"creator" << (i + 1) << L"\">" << XmlEscape(authors[i]) << L"</dc:creator>\n";
            x << L"  <meta refines=\"#creator" << (i + 1) << L"\" property=\"role\" scheme=\"marc:relators\">aut</meta>\n";
        }
        for (std::size_t i = 0; i < book.translators.size(); ++i) {
            if (book.translators[i].empty()) continue;
            x << L"  <dc:contributor id=\"translator" << (i + 1) << L"\">" << XmlEscape(book.translators[i]) << L"</dc:contributor>\n";
            x << L"  <meta refines=\"#translator" << (i + 1) << L"\" property=\"role\" scheme=\"marc:relators\">trl</meta>\n";
        }
        if (!book.publisher.empty()) {
            x << L"  <dc:publisher>" << XmlEscape(book.publisher) << L"</dc:publisher>\n";
        }
        if (!book.description.empty()) {
            x << L"  <dc:description>" << XmlEscape(book.description) << L"</dc:description>\n";
        }
        if (!opfDate.empty()) {
            x << L"  <dc:date>" << XmlEscape(opfDate) << L"</dc:date>\n";
        }
        if (!book.rights.empty()) {
            x << L"  <dc:rights>" << XmlEscape(book.rights) << L"</dc:rights>\n";
        }
        if (!book.source.empty()) {
            x << L"  <dc:source>" << XmlEscape(book.source) << L"</dc:source>\n";
        }
        for (const auto& s0 : book.subjects) {
            const auto subject = NormalizeSubject(s0);
            if (!subject.empty()) x << L"  <dc:subject>" << XmlEscape(subject) << L"</dc:subject>\n";
        }
        for (const auto& k0 : book.keywords) {
            const auto keyword = NormalizeSubject(k0);
            if (!keyword.empty()) x << L"  <dc:subject>" << XmlEscape(keyword) << L"</dc:subject>\n";
        }
        if (!book.collections.empty()) {
            for (std::size_t i = 0; i < book.collections.size(); ++i) {
                const auto& col = book.collections[i];
                if (col.name.empty()) continue;
                x << L"  <meta property=\"belongs-to-collection\" id=\"series" << (i + 1) << L"\">" << XmlEscape(col.name) << L"</meta>\n"
                  << L"  <meta refines=\"#series" << (i + 1) << L"\" property=\"collection-type\">series</meta>\n";
                if (!col.number.empty()) {
                    x << L"  <meta refines=\"#series" << (i + 1) << L"\" property=\"group-position\">" << XmlEscape(col.number) << L"</meta>\n";
                }
            }
        } else if (!book.seriesName.empty()) {
            x << L"  <meta property=\"belongs-to-collection\" id=\"series1\">" << XmlEscape(book.seriesName) << L"</meta>\n"
              << L"  <meta refines=\"#series1\" property=\"collection-type\">series</meta>\n";
            if (!book.seriesNumber.empty()) {
                x << L"  <meta refines=\"#series1\" property=\"group-position\">" << XmlEscape(book.seriesNumber) << L"</meta>\n";
            }
        }
        x << L"</metadata>\n";
    } else {
        x << L"<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:opf=\"http://www.idpf.org/2007/opf\">\n"
          << L"  <dc:identifier id=\"bookid\">" << identifier << L"</dc:identifier>\n";
        if (!book.isbn.empty()) {
            x << L"  <dc:identifier opf:scheme=\"ISBN\">" << XmlEscape(book.isbn) << L"</dc:identifier>\n";
        }
        x << L"  <dc:title>" << title << L"</dc:title>\n"
          << L"  <dc:language>" << language << L"</dc:language>\n";
        for (const auto& author : authors) {
            if (!author.empty()) x << L"  <dc:creator opf:role=\"aut\">" << XmlEscape(author) << L"</dc:creator>\n";
        }
        for (const auto& translator : book.translators) {
            if (!translator.empty()) x << L"  <dc:contributor opf:role=\"trl\">" << XmlEscape(translator) << L"</dc:contributor>\n";
        }
        if (!book.publisher.empty()) {
            x << L"  <dc:publisher>" << XmlEscape(book.publisher) << L"</dc:publisher>\n";
        }
        if (!book.description.empty()) {
            x << L"  <dc:description>" << XmlEscape(book.description) << L"</dc:description>\n";
        }
        if (!opfDate.empty()) {
            x << L"  <dc:date>" << XmlEscape(opfDate) << L"</dc:date>\n";
        }
        if (!book.rights.empty()) {
            x << L"  <dc:rights>" << XmlEscape(book.rights) << L"</dc:rights>\n";
        }
        if (!book.source.empty()) {
            x << L"  <dc:source>" << XmlEscape(book.source) << L"</dc:source>\n";
        }
        for (const auto& s0 : book.subjects) {
            const auto subject = NormalizeSubject(s0);
            if (!subject.empty()) x << L"  <dc:subject>" << XmlEscape(subject) << L"</dc:subject>\n";
        }
        for (const auto& k0 : book.keywords) {
            const auto keyword = NormalizeSubject(k0);
            if (!keyword.empty()) x << L"  <dc:subject>" << XmlEscape(keyword) << L"</dc:subject>\n";
        }
        if (hasCoverResource) {
            x << L"  <meta name=\"cover\" content=\"" << XmlEscape(book.coverImageId) << L"\"/>\n";
        }
        if (!book.seriesName.empty()) {
            x << L"  <meta name=\"calibre:series\" content=\"" << XmlEscape(book.seriesName) << L"\"/>\n";
            if (!book.seriesNumber.empty()) {
                x << L"  <meta name=\"calibre:series_index\" content=\"" << XmlEscape(book.seriesNumber) << L"\"/>\n";
            }
        }
        x << L"</metadata>\n";
    }

    x << L"<manifest>\n";
    x << L"  <item id=\"style\" href=\"styles/style.css\" media-type=\"text/css\"/>\n";

    if (hasCoverPage) {
        x << L"  <item id=\"cover\" href=\"cover.xhtml\" media-type=\"application/xhtml+xml\"/>\n";
    }

    if (isEpub3) {
        x << L"  <item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n";
        if (options.includeNcxFallbackInEpub3) {
            x << L"  <item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\n";
        }
    } else {
        x << L"  <item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\n";
    }

    for (std::size_t i = 0; i < book.chapters.size(); ++i) {
        x << L"  <item id=\"" << XmlEscape(IdOrGenerated(book.chapters[i], i))
          << L"\" href=\"" << XmlEscape(ChapterFileName(i))
          << L"\" media-type=\"application/xhtml+xml\"/>\n";
    }

    for (const auto& r : book.resources) {
        std::wstring props = r.properties;
        if (isEpub3 && r.id == book.coverImageId) {
            if (!props.empty()) props += L" ";
            props += L"cover-image";
        }
        x << L"  <item id=\"" << XmlEscape(r.id) << L"\" href=\"" << XmlEscape(r.href)
          << L"\" media-type=\"" << WideFromUtf8(r.mediaType) << L"\"";
        if (isEpub3) x << ManifestPropertiesAttr(props);
        x << L"/>\n";
    }

    x << L"</manifest>\n";

    if (isEpub3) {
        x << L"<spine" << (options.includeNcxFallbackInEpub3 ? L" toc=\"ncx\"" : L"") << L">\n";
    } else {
        x << L"<spine toc=\"ncx\">\n";
    }
    if (hasCoverPage) {
        x << L"  <itemref idref=\"cover\"/>\n";
    }
    for (std::size_t i = 0; i < book.chapters.size(); ++i) {
        x << L"  <itemref idref=\"" << XmlEscape(IdOrGenerated(book.chapters[i], i)) << L"\"/>\n";
    }
    x << L"</spine>\n";

    if (!isEpub3 && !book.chapters.empty()) {
        x << L"<guide>\n";
        if (hasCoverPage) {
            x << L"  <reference type=\"cover\" title=\"Обложка\" href=\"cover.xhtml\"/>\n";
        }
        x << L"  <reference type=\"text\" title=\"Start\" href=\"" << XmlEscape(ChapterFileName(0)) << L"\"/>\n"
          << L"</guide>\n";
    }

    x << L"</package>\n";
    return x.str();
}

bool ValidateBook(const EpubBook& book, std::wstring* error) {
    if (book.chapters.empty()) {
        if (error) *error = L"EPUB export requires at least one chapter";
        return false;
    }
    if (book.title.empty()) {
        if (error) *error = L"EPUB export requires book title";
        return false;
    }

    std::unordered_set<std::wstring> ids;
    ids.insert(L"style");
    ids.insert(L"nav");
    ids.insert(L"ncx");
    if (HasCover(book)) {
        ids.insert(L"cover");
    }

    for (std::size_t i = 0; i < book.chapters.size(); ++i) {
        const auto id = IdOrGenerated(book.chapters[i], i);
        if (!ids.insert(id).second) {
            if (error) *error = L"Duplicate manifest id in chapter list: " + id;
            return false;
        }
    }

    for (const auto& r : book.resources) {
        if (r.id.empty() || r.href.empty() || r.mediaType.empty()) {
            if (error) *error = L"EPUB resource requires id, href and mediaType";
            return false;
        }
        if (!ids.insert(r.id).second) {
            if (error) *error = L"Duplicate manifest id in resource list: " + r.id;
            return false;
        }
    }

    if (!book.coverImageId.empty() && !HasCover(book)) {
        if (error) *error = L"EPUB coverImageId does not reference an existing resource: " + book.coverImageId;
        return false;
    }

    return true;
}

} // namespace

std::string Utf8FromWide(const std::wstring& value) {
    if (value.empty()) return {};
#ifdef _WIN32
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(value);
#endif
}

std::wstring WideFromUtf8(const std::string& value) {
    if (value.empty()) return {};
#ifdef _WIN32
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(value);
#endif
}

std::wstring XmlEscape(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size());
    for (wchar_t ch : value) {
        switch (ch) {
        case L'&': out += L"&amp;"; break;
        case L'<': out += L"&lt;"; break;
        case L'>': out += L"&gt;"; break;
        case L'\"': out += L"&quot;"; break;
        case L'\'': out += L"&apos;"; break;
        default: out += ch; break;
        }
    }
    return out;
}

std::wstring MakeSimpleParagraphsXhtml(const std::wstring& plainText) {
    std::wstringstream in(plainText);
    std::wstring line;
    std::wstring out;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        out += L"<p>" + XmlEscape(line) + L"</p>\n";
    }
    if (out.empty()) out = L"<p></p>\n";
    return out;
}

bool EpubExporter::Export(const EpubBook& book,
                          const std::filesystem::path& outputFile,
                          const EpubExportOptions& options,
                          std::wstring* errorMessage) const {
    if (errorMessage) errorMessage->clear();
    if (!ValidateBook(book, errorMessage)) {
        return false;
    }

    ZipStoredWriter zip;
    if (!zip.Open(outputFile, errorMessage)) return false;

    // EPUB OCF requirement: mimetype must be first, uncompressed, and without extra field.
    if (!zip.AddFile("mimetype", Bytes("application/epub+zip"), errorMessage)) return false;

    if (!zip.AddFile("META-INF/container.xml", Bytes(Utf8FromWide(BuildContainerXml(options))), errorMessage)) return false;

    const std::string root = Utf8FromWide(options.packageRoot);
    auto insideRoot = [&](const std::wstring& relative) -> std::string {
        return root + "/" + NormalizeHrefUtf8(relative);
    };

    const std::string css = book.cssUtf8.empty() ? Utf8FromWide(DefaultCssWide()) : book.cssUtf8;
    if (!zip.AddFile(insideRoot(L"styles/style.css"), Bytes(css), errorMessage)) return false;

    if (!zip.AddFile(insideRoot(L"content.opf"), Bytes(Utf8FromWide(BuildOpf(book, options))), errorMessage)) return false;

    if (options.includeCoverPage && HasCover(book)) {
        if (!zip.AddFile(insideRoot(L"cover.xhtml"), Bytes(Utf8FromWide(BuildCoverXhtml(book, options.version))), errorMessage)) return false;
    }

    if (options.version == EpubVersion::Epub3) {
        if (!zip.AddFile(insideRoot(L"nav.xhtml"), Bytes(Utf8FromWide(BuildNavXhtml(book, options.includeCoverPage, options.includeCoverInToc))), errorMessage)) return false;
        if (options.includeNcxFallbackInEpub3) {
            if (!zip.AddFile(insideRoot(L"toc.ncx"), Bytes(Utf8FromWide(BuildNcx(book, options.includeCoverPage, options.includeCoverInToc))), errorMessage)) return false;
        }
    } else {
        if (!zip.AddFile(insideRoot(L"toc.ncx"), Bytes(Utf8FromWide(BuildNcx(book, options.includeCoverPage, options.includeCoverInToc))), errorMessage)) return false;
    }

    for (std::size_t i = 0; i < book.chapters.size(); ++i) {
        const auto chapterXml = BuildChapterXhtml(book, book.chapters[i], options.version);
        if (!zip.AddFile(insideRoot(ChapterFileName(i)), Bytes(Utf8FromWide(chapterXml)), errorMessage)) return false;
    }

    for (const auto& r : book.resources) {
        if (!zip.AddFile(insideRoot(r.href), r.data, errorMessage)) return false;
    }

    return zip.Close(errorMessage);
}

} // namespace fbe::epub
