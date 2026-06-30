#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fbe::epub {

enum class EpubVersion {
    Epub2,
    Epub3
};

struct EpubResource {
    // id must be unique inside OPF manifest, e.g. "img001".
    std::wstring id;

    // Relative href inside OEBPS, e.g. "images/cover.jpg".
    // Use forward slashes only.
    std::wstring href;

    // MIME type, e.g. "image/jpeg", "image/png".
    std::string mediaType;

    // Space-separated OPF item properties for EPUB 3, e.g. "cover-image svg".
    // EPUB 2 ignores this field.
    std::wstring properties;

    std::vector<std::uint8_t> data;
};

struct EpubNavPoint {
    std::wstring title;
    std::wstring href; // Relative href from OEBPS root, e.g. "text/chapter_002.xhtml#section_id".
    std::wstring epubType; // EPUB 3 semantic, e.g. "section". EPUB 2 ignores this field.
    std::vector<EpubNavPoint> children;
};

struct EpubCollection {
    std::wstring name;
    std::wstring number;
};

struct EpubChapter {
    std::wstring id;       // Optional. If empty, generated as chap001.
    std::wstring title;    // Used in TOC/nav.

    // EPUB 3 structural semantic, e.g. "bodymatter", "frontmatter", "endnotes".
    // EPUB 2 ignores this field.
    std::wstring epubType;

    // Include this chapter in NCX/nav.xhtml.
    bool includeInToc = true;

    // Nested TOC entries derived from nested FB2 sections.
    std::vector<EpubNavPoint> navChildren;

    // Valid XHTML fragment for the chapter body.
    // Example: L"<p>Text</p><p><em>Italic</em></p>".
    // The exporter wraps it into a full XHTML document.
    std::wstring bodyXhtml;
};

struct EpubBook {
    // identifier should preferably be stable, e.g. "urn:uuid:...".
    // If empty, a deterministic temporary id is generated from title/author.
    std::wstring identifier;
    std::wstring title;
    std::wstring language = L"ru";
    // author is kept for backward compatibility. Prefer authors for multiple creators.
    std::wstring author;
    std::vector<std::wstring> authors;
    std::vector<std::wstring> translators;
    std::vector<std::wstring> subjects;
    std::vector<std::wstring> keywords;

    std::wstring publisher;
    std::wstring description;
    std::wstring date;
    std::wstring rights;
    std::wstring source;
    std::wstring isbn;
    std::wstring seriesName;
    std::wstring seriesNumber;
    std::vector<EpubCollection> collections;

    // OPF manifest id of cover image resource, e.g. "img001".
    std::wstring coverImageId;

    // Optional custom CSS. If empty, a small default stylesheet is used.
    std::string cssUtf8;

    std::vector<EpubChapter> chapters;
    std::vector<EpubResource> resources;
};

struct EpubExportOptions {
    EpubVersion version = EpubVersion::Epub3;

    // Adds toc.ncx even for EPUB 3 for compatibility with older readers.
    bool includeNcxFallbackInEpub3 = true;

    // Creates a separate cover.xhtml page and puts it into the spine/navigation
    // when coverImageId points to an existing image. The cover image itself can
    // still be marked as cover-image in metadata when this option is disabled.
    bool includeCoverPage = true;

    // Adds the cover page to NCX/nav.xhtml TOC. The guide/landmarks entry can
    // still exist even when the cover is hidden from the visible TOC.
    bool includeCoverInToc = true;

    // Keep all generated files under OEBPS/.
    std::wstring packageRoot = L"OEBPS";

    // Pretty XML output. Disable only for debugging size-sensitive cases.
    bool pretty = true;
};

class EpubExporter {
public:
    bool Export(const EpubBook& book,
                const std::filesystem::path& outputFile,
                const EpubExportOptions& options,
                std::wstring* errorMessage = nullptr) const;
};

// Helpers intended for adapters from FBE/FB2 DOM.
std::string Utf8FromWide(const std::wstring& value);
std::wstring WideFromUtf8(const std::string& value);
std::wstring XmlEscape(const std::wstring& value);
std::wstring MakeSimpleParagraphsXhtml(const std::wstring& plainText);

} // namespace fbe::epub
