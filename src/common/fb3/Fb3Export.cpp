#include "Fb3Export.h"
#include "Fb3Batch.h"
#include "Fb3Image.h"
#include "Fb3Package.h"
#include "Fb3Schema.h"
#include "../fb2/Fb2BinaryInspector.h"

#define LIBARCHIVE_STATIC
#include <archive.h>
#include <archive_entry.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <algorithm>
#include <deque>
#include <regex>
#include <set>
#include <string_view>

namespace FbeFb3 {
namespace {

namespace fs = std::filesystem;
using Text = std::wstring;
using View = std::wstring_view;
using Node = CComPtr<IXMLDOMNode>;
using Nodes = std::vector<Node>;
constexpr wchar_t Fb2Namespace[] = L"http://www.gribuser.ru/xml/fictionbook/2.0";
constexpr wchar_t Xlink[] = L"http://www.w3.org/1999/xlink";
constexpr wchar_t ZipTypes[] = L"http://schemas.openxmlformats.org/package/2006/content-types";
constexpr wchar_t ZipRels[] = L"http://schemas.openxmlformats.org/package/2006/relationships";
constexpr wchar_t Fb3Rels[] = L"http://www.fictionbook.org/FictionBook3/relationships/";

[[noreturn]] void Fail(Text message, const wchar_t* code = L"invalid-document", Text element = {}, Text location = {}) {
    throw Error{std::move(message), code, std::move(element), std::move(location)};
}
Text FromBstr(const CComBSTR& value) { return value ? Text(value, value.Length()) : Text(); }
Text Tag(const Node& node) { CComBSTR name; if (node) node->get_baseName(&name); return FromBstr(name); }
Text Content(const Node& node) { CComBSTR text; if (node) node->get_text(&text); return FromBstr(text); }
Text Attr(const Node& node, const wchar_t* name, const wchar_t* ns = L"") {
    CComPtr<IXMLDOMNamedNodeMap> attributes;
    if (node) node->get_attributes(&attributes);
    Node value;
    if (attributes) attributes->getQualifiedItem(CComBSTR(name), CComBSTR(ns), &value);
    return Content(value);
}
template<class F> void Visit(const Node& parent, F process) {
    Node child;
    if (parent) parent->get_firstChild(&child);
    while (child) {
        DOMNodeType type = NODE_INVALID;
        child->get_nodeType(&type);
        process(child, type);
        Node next;
        child->get_nextSibling(&next);
        child = next;
    }
}
Nodes Children(const Node& parent, View name = {}) {
    Nodes elements;
    Visit(parent, [&](const Node& node, DOMNodeType type) {
        if (type == NODE_ELEMENT && (name.empty() || Tag(node) == name)) elements.push_back(node);
    });
    return elements;
}
Node Child(const Node& parent, View name) {
    const auto nodes = Children(parent, name);
    return nodes.empty() ? Node() : nodes.front();
}
Text Field(const Node& parent, View name) { return Content(Child(parent, name)); }

struct Xml {
    Text value;
    void Escape(View input) {
        for (const auto character : input) {
            switch (character) {
            case L'&': value += L"&amp;"; break;
            case L'<': value += L"&lt;"; break;
            case L'>': value += L"&gt;"; break;
            case L'\"': value += L"&quot;"; break;
            case L'\r': value += L"&#13;"; break;
            default: value += character; break;
            }
        }
    }
    void Start(View name) { value += L'<'; value += name; }
    void Attribute(View name, View data) {
        if (data.empty()) return;
        value += L' '; value += name; value += L"=\""; Escape(data); value += L'\"';
    }
    void Open(View name) { Start(name); value += L'>'; }
    void End(View name) { value += L"</"; value += name; value += L'>'; }
    void Element(View name, View data) { Open(name); Escape(data); End(name); }
};

bool IsUuid(View value) {
    if (value.size() != 36) return false;
    for (size_t index = 0; index < value.size(); ++index) {
        const auto ch = value[index];
        if (index == 8 || index == 13 || index == 18 || index == 23) { if (ch != L'-') return false; }
        else if (View(L"0123456789abcdefABCDEF").find(ch) == View::npos) return false;
    }
    return true;
}

Text StableUuid(const Text& key) {
    // UUIDv5, with a fixed namespace unique to this converter.
    constexpr std::uint8_t ns[]{0x5a,0x28,0x9b,0xd2,0x10,0xe2,0x4e,0x36,0x98,0xef,0xc4,0xad,0x7a,0xba,0xa9,0x14};
    std::string input(reinterpret_cast<const char*>(ns), sizeof(ns));
    input += Utf8(key);
    BYTE digest[20]{};
    DWORD count = sizeof(digest);
    if (!::CryptHashCertificate(0, CALG_SHA1, 0, reinterpret_cast<const BYTE*>(input.data()), static_cast<DWORD>(input.size()), digest, &count) || count != sizeof(digest))
        Fail(L"Cannot generate a stable FB3 identifier.");
    digest[6] = (digest[6] & 0x0f) | 0x50;
    digest[8] = (digest[8] & 0x3f) | 0x80;
    constexpr wchar_t digits[] = L"0123456789abcdef";
    Text uuid;
    for (size_t index = 0; index < 16; ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10) uuid += L'-';
        uuid += digits[digest[index] >> 4]; uuid += digits[digest[index] & 0xf];
    }
    return uuid;
}

Text SchemaIsbn(View input) {
    if (input.empty()) return {};
    Text digits;
    size_t separators = 0;
    bool separated = false, normalized = true;
    for (size_t index = 0; index < input.size(); ++index) {
        const auto ch = input[index];
        if ((ch >= L'0' && ch <= L'9') || ((ch == L'x' || ch == L'X') && index + 1 == input.size())) {
            digits += ch;
            separated = false;
        } else if (ch == L'-' || ch == L' ' || ch == L'\t') {
            if (index == 0 || index + 1 == input.size() || separated) normalized = false;
            separated = true;
            ++separators;
        } else return {};
    }
    if (normalized && separators >= 3 && separators <= 6) return Text(input);
    if (digits.size() == 10) return digits.substr(0, 1) + L" " + digits.substr(1, 3) + L" " + digits.substr(4, 5) + L" " + digits.back();
    if (digits.size() == 13) return digits.substr(0, 3) + L" " + digits.substr(3, 1) + L" " + digits.substr(4, 8) + L" " + digits.back();
    return {};
}

void ValidateInput(const CComPtr<IXMLDOMDocument2>& document, const fs::path& schema) {
    CComPtr<IXMLDOMSchemaCollection2> schemas;
    if (FAILED(schemas.CoCreateInstance(__uuidof(XMLSchemaCache60))) ||
        FAILED(schemas->add(CComBSTR(Fb2Namespace), CComVariant(fs::absolute(schema).c_str()))) ||
        FAILED(document->putref_schemas(CComVariant(static_cast<IDispatch*>(schemas))))) Fail(L"Cannot load the installed FictionBook 2 schema.");
    CComPtr<IXMLDOMParseError> error;
    long code = 0;
    if (FAILED(document->validate(&error)) || !error || FAILED(error->get_errorCode(&code))) Fail(L"Cannot validate the source FB2 document.");
    if (code) {
        CComBSTR reason;
        error->get_reason(&reason);
        Fail(L"Source FB2 schema validation failed: " + FromBstr(reason));
    }
}

class ZipWriter {
public:
    explicit ZipWriter(const fs::path& file) : m_archive(archive_write_new()) {
        if (!m_archive) Fail(L"Cannot start the FB3 ZIP writer.");
        if (archive_write_set_format_zip(m_archive) != ARCHIVE_OK || archive_write_open_filename_w(m_archive, file.c_str()) != ARCHIVE_OK) {
            archive_write_free(m_archive);
            Fail(L"Cannot open the temporary FB3 ZIP file.");
        }
    }
    ~ZipWriter() { if (m_archive) archive_write_free(m_archive); }
    ZipWriter(const ZipWriter&) = delete;
    void Put(const std::string& path, const void* bytes, size_t size) {
        struct Header { archive_entry* value = archive_entry_new(); ~Header() { archive_entry_free(value); } } header;
        if (!header.value) Fail(L"Cannot create an FB3 ZIP entry.");
        archive_entry_set_pathname_utf8(header.value, path.c_str());
        archive_entry_set_filetype(header.value, AE_IFREG);
        archive_entry_set_perm(header.value, 0644);
        archive_entry_set_size(header.value, static_cast<la_int64_t>(size));
        if (archive_write_header(m_archive, header.value) != ARCHIVE_OK) Fail(L"Cannot write an FB3 ZIP entry header.");
        auto* remaining = static_cast<const char*>(bytes);
        while (size) {
            const auto done = archive_write_data(m_archive, remaining, size);
            if (done <= 0 || static_cast<size_t>(done) > size) Fail(L"Cannot write an FB3 ZIP entry.");
            remaining += done;
            size -= static_cast<size_t>(done);
        }
        if (archive_write_finish_entry(m_archive) != ARCHIVE_OK) Fail(L"Cannot finish an FB3 ZIP entry.");
    }
    void Put(const std::string& path, const Text& xml) { const auto encoded = Utf8(xml); Put(path, encoded.data(), encoded.size()); }
    void Close() {
        if (archive_write_close(m_archive) != ARCHIVE_OK) Fail(L"Cannot complete the FB3 ZIP directory.");
        const auto status = archive_write_free(m_archive);
        m_archive = nullptr;
        if (status != ARCHIVE_OK) Fail(L"Cannot close the FB3 package.");
    }
private:
    archive* m_archive;
};

class Output {
public:
    explicit Output(const fs::path& final) {
        GUID guid{};
        wchar_t name[40]{};
        if (FAILED(::CoCreateGuid(&guid)) || !::StringFromGUID2(guid, name, _countof(name))) Fail(L"Cannot name the temporary FB3 package.");
        m_file = final.parent_path() / (Text(L".fb3-export-") + name + L".tmp");
        const auto handle = ::CreateFileW(m_file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) Fail(L"Cannot create an FB3 temporary package in " + final.parent_path().wstring());
        ::CloseHandle(handle);
    }
    ~Output() { if (!m_file.empty()) ::DeleteFileW(m_file.c_str()); }
    const fs::path& Path() const { return m_file; }
    void Publish(const fs::path& final, bool replace) {
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!::MoveFileExW(m_file.c_str(), final.c_str(), flags))
            Fail(L"Cannot publish FB3 to " + final.wstring() + L" (Windows error " + std::to_wstring(::GetLastError()) + L").");
        m_file.clear();
    }
private:
    fs::path m_file;
};

struct Resource { Text path, mime, relationship; std::vector<std::uint8_t> bytes; };
enum class Context { Section, Note, Reduced, Annotation, Description };

class Exporter {
public:
    explicit Exporter(const Node& root) : m_root(root) {}
    Diagnostics Convert(ZipWriter& zip);
private:
    void Warn(Text message, const wchar_t* code = L"layout", Text element = {}) {
        if (m_messages.insert(message).second) m_diagnostics.push_back({Severity::Warning, code, std::move(message), std::move(element), {}});
    }
    void Info(Text message, const wchar_t* code, Text element) {
        if (m_messages.insert(message).second) m_diagnostics.push_back({Severity::Info, code, std::move(message), std::move(element), {}});
    }
    void ReadResources();
    const Resource& Image(const Node& image);
    Text Identify(const Node& node, bool uuid = false, bool note = false);
    void Inline(Xml& xml, const Node& node, Context context);
    void Paragraph(Xml& xml, const Node& node, View tag, Context context);
    void Block(Xml& xml, const Node& node, Context context);
    void Title(Xml& xml, const Node& node, Context context);
    void Quote(Xml& xml, const Node& node, bool epigraph);
    void Poem(Xml& xml, const Node& node);
    void Table(Xml& xml, const Node& node);
    void Section(Xml& xml, const Node& node, bool note = false);
    void NoteChildren(Xml& xml, const Node& node, Nodes& headers, bool& groupOpen);
    void OpenNoteGroup(Xml& xml, Nodes& headers, bool& groupOpen);
    void Person(Xml& xml, const Node& node, View role);
    void Sequence(Xml& xml, const Node& node, bool paper = false);
    void Metadata(Xml& xml, const Node& description);
    void Body(Xml& xml, const Nodes& bodies);
    Node m_root;
    Text m_bookId, m_cover;
    size_t m_ordinal = 0;
    std::map<Text, Resource, std::less<>> m_resources;
    std::map<Text, Text, std::less<>> m_ids;
    std::map<Text, std::deque<Text>, std::less<>> m_seriesIdentities;
    std::set<Text, std::less<>> m_emitted, m_notes, m_messages;
    struct Reference { Text id; size_t offset; bool note; };
    std::vector<Reference> m_references;
    Diagnostics m_diagnostics;
};

void Exporter::ReadResources() {
    for (const auto& node : Children(m_root, L"binary")) {
        const auto id = Attr(node, L"id");
        auto checked = FbeFb2Binary::InspectBinary(Attr(node, L"content-type"), Content(node));
        if (checked.status != FbeFb2Binary::BinaryStatus::Ok && checked.status != FbeFb2Binary::BinaryStatus::MimeAlias)
            Fail(L"Invalid FB2 binary data or MIME type for " + id);
        auto extension = checked.detectedMime == L"image/jpeg" ? L"jpg" : checked.detectedMime == L"image/png" ? L"png" :
            checked.detectedMime == L"image/gif" ? L"gif" : checked.detectedMime == L"image/svg+xml" ? L"svg" :
            checked.detectedMime == L"image/bmp" ? L"bmp" : checked.detectedMime == L"image/webp" ? L"webp" : nullptr;
        if (!extension || checked.bytes.empty() || checked.bytes.size() > (64ULL << 20)) Fail(L"Unsupported FB2 binary resource " + id);
        if (checked.detectedMime == L"image/svg+xml") {
            const auto svg = LoadXmlBytes(L"FB2 SVG " + id, checked.bytes);
            CComPtr<IXMLDOMElement> root;
            CComBSTR ns;
            if (FAILED(svg->get_documentElement(&root)) || !root || FAILED(root->get_namespaceURI(&ns)) ||
                ns != L"http://www.w3.org/2000/svg" || Tag(Node(root)) != L"svg") Fail(L"Invalid FB2 SVG root in " + id);
        } else ValidateRasterImage(id, checked.detectedMime, checked.bytes);
        const auto number = std::to_wstring(m_resources.size() + 1);
        if (id.empty() || !m_resources.emplace(id, Resource{L"img/image-" + number + L"." + extension,
            checked.detectedMime, L"image-" + number, std::move(checked.bytes)}).second) Fail(L"Duplicate or empty FB2 binary ID " + id);
    }
}

const Resource& Exporter::Image(const Node& image) {
    const auto href = Attr(image, L"href", Xlink);
    const auto found = href.size() > 1 && href.front() == L'#' ? m_resources.find(href.substr(1)) : m_resources.end();
    if (found == m_resources.end()) Fail(L"Missing or external FB2 image " + href, L"missing-image", L"image", href);
    if (!Attr(image, L"title").empty()) Warn(L"FB2 image title was retained as its alternative text when available.");
    return found->second;
}

Text Exporter::Identify(const Node& node, bool uuid, bool note) {
    const auto input = Attr(node, L"id");
    if (input.empty() && !uuid && !note) return {};
    const auto key = m_bookId + (input.empty() ? L"/generated/" + std::to_wstring(++m_ordinal) : L"/anchor/" + input);
    const auto result = uuid ? StableUuid(key) : (note ? L"note-" : L"id-") + StableUuid(key);
    if (!m_emitted.insert(result).second || (!input.empty() && !m_ids.emplace(input, result).second)) Fail(L"Duplicate FB2 ID " + input);
    if (note) m_notes.insert(result);
    if (input.empty()) Info(L"An identifier was generated for an FB3 section or note without a source ID.",
        L"anchor-generated", note ? L"notebody" : L"section");
    return result;
}

void Exporter::Inline(Xml& xml, const Node& node, Context context) {
    Visit(node, [&](const Node& child, DOMNodeType type) {
        if (type == NODE_TEXT || type == NODE_CDATA_SECTION) { xml.Escape(Content(child)); return; }
        if (type != NODE_ELEMENT) return;
        auto tag = Tag(child);
        if (tag == L"emphasis") tag = L"em";
        if (tag == L"image" && context != Context::Description) {
            const auto& image = Image(child);
            xml.Start(L"img"); xml.Attribute(L"src", image.relationship); xml.Attribute(L"id", Identify(child));
            const auto alt = Attr(child, L"alt");
            xml.Attribute(L"alt", alt.empty() ? Attr(child, L"title") : alt);
            xml.value += L"/>";
        } else if (tag == L"a") {
            const auto href = Attr(child, L"href", Xlink);
            if (href.size() > 1 && href.front() == L'#') {
                if (context == Context::Description) {
                    Warn(L"An FB2 description link was retained as its text in FB3.");
                    Inline(xml, child, context);
                    return;
                }
                const bool note = Attr(child, L"type") == L"note";
                xml.Start(note ? L"note" : L"a");
                if (note) xml.Attribute(L"autotext", L"keep");
                xml.value += note ? L" href=\"" : L" l:href=\"#";
                m_references.push_back({href.substr(1), xml.value.size(), note});
                xml.value += L"\">";
                Inline(xml, child, context);
                xml.End(note ? L"note" : L"a");
            } else {
                const auto colon = href.find(L':');
                if (colon == Text::npos || href.find_first_of(L"\r\n\t\\") != Text::npos) Fail(L"Unsafe FB2 hyperlink " + href);
                const auto scheme = href.substr(0, colon);
                if (::CompareStringOrdinal(scheme.c_str(), -1, L"http", -1, TRUE) != CSTR_EQUAL &&
                    ::CompareStringOrdinal(scheme.c_str(), -1, L"https", -1, TRUE) != CSTR_EQUAL &&
                    ::CompareStringOrdinal(scheme.c_str(), -1, L"mailto", -1, TRUE) != CSTR_EQUAL) Fail(L"Unsafe FB2 hyperlink " + href);
                xml.Start(L"a"); xml.Attribute(context == Context::Description ? L"href" : L"l:href", href); xml.value += L'>';
                Inline(xml, child, context); xml.End(L"a");
            }
        } else if ((context == Context::Description && tag != L"strong" && tag != L"em") || tag == L"style") {
            if (tag == L"style" && context != Context::Description) {
                xml.Start(L"span"); xml.Attribute(L"class", Attr(child, L"name")); xml.value += L'>';
                Inline(xml, child, context); xml.End(L"span");
            } else {
                if (tag == L"image") Fail(L"The FB3 description cannot represent an inline FB2 image.");
                Warn(L"FB2 description formatting <" + tag + L"> was reduced to its textual content.");
                Inline(xml, child, context);
            }
        } else if (tag == L"strong" || tag == L"em" || tag == L"code" || tag == L"strikethrough" || tag == L"sub" || tag == L"sup") {
            xml.Open(tag); Inline(xml, child, context); xml.End(tag);
        } else Fail(L"Unsupported inline FB2 element <" + tag + L">.");
    });
}

void Exporter::Paragraph(Xml& xml, const Node& node, View tag, Context context) {
    xml.Start(tag);
    if (context != Context::Description) xml.Attribute(L"id", Identify(node));
    xml.value += L'>';
    Inline(xml, node, context);
    xml.End(tag);
}

void Exporter::Title(Xml& xml, const Node& node, Context context) {
    if (!node) return;
    xml.Open(L"title");
    bool first = true;
    for (const auto& child : Children(node)) {
        if (Tag(child) == L"empty-line" && first) { xml.value += L"<p/>"; first = false; }
        Block(xml, child, context);
        first = false;
    }
    if (first) xml.value += L"<p/>";
    xml.End(L"title");
}

void Exporter::Quote(Xml& xml, const Node& node, bool epigraph) {
    const auto tag = epigraph ? L"epigraph" : L"blockquote";
    xml.Open(tag);
    const auto id = Identify(node);
    bool first = true;
    for (const auto& child : Children(node)) {
        if (Tag(child) == L"text-author") continue;
        if (first && (!id.empty() || (Tag(child) != L"p" && !(epigraph && Tag(child) == L"poem")))) {
            xml.Start(L"p"); xml.Attribute(L"id", id); xml.value += L"/>";
        }
        if (epigraph && Tag(child) == L"poem") Poem(xml, child);
        else Block(xml, child, Context::Reduced);
        first = false;
    }
    if (first) { xml.Start(L"p"); xml.Attribute(L"id", id); xml.value += L"/>"; }
    const auto authors = Children(node, L"text-author");
    if (!authors.empty()) {
        xml.Open(L"subscription");
        for (const auto& author : authors) Paragraph(xml, author, L"p", Context::Reduced);
        xml.End(L"subscription");
    }
    xml.End(tag);
}

void Exporter::Poem(Xml& xml, const Node& node) {
    xml.Start(L"poem"); xml.Attribute(L"id", Identify(node)); xml.value += L'>';
    Title(xml, Child(node, L"title"), Context::Reduced);
    for (const auto& epigraph : Children(node, L"epigraph")) Quote(xml, epigraph, true);
    bool stanza = false;
    for (const auto& item : Children(node)) {
        const auto tag = Tag(item);
        if (tag != L"stanza" && tag != L"subtitle") continue;
        xml.Open(L"stanza");
        if (tag == L"subtitle") {
            Warn(L"A standalone FB2 poem subtitle was represented in an FB3 stanza.");
            Paragraph(xml, item, L"p", Context::Reduced);
        } else {
            Title(xml, Child(item, L"title"), Context::Reduced);
            if (const auto sub = Child(item, L"subtitle")) Paragraph(xml, sub, L"p", Context::Reduced);
            for (const auto& verse : Children(item, L"v")) Paragraph(xml, verse, L"p", Context::Reduced);
        }
        xml.End(L"stanza"); stanza = true;
    }
    if (!stanza) xml.value += L"<stanza><p/></stanza>";
    const auto authors = Children(node, L"text-author");
    const auto date = Child(node, L"date");
    if (!authors.empty() || date) {
        xml.Open(L"subscription");
        for (const auto& author : authors) Paragraph(xml, author, L"p", Context::Reduced);
        if (date) { Warn(L"FB2 poem date was retained as an FB3 attribution line."); xml.Element(L"p", Content(date)); }
        xml.End(L"subscription");
    }
    xml.End(L"poem");
}

void Exporter::Table(Xml& xml, const Node& node) {
    xml.Start(L"table"); xml.Attribute(L"id", Identify(node)); xml.value += L'>';
    for (const auto& row : Children(node, L"tr")) {
        xml.Start(L"tr"); xml.Attribute(L"align", Attr(row, L"align")); xml.value += L'>';
        for (const auto& cell : Children(row)) {
            const auto tag = Tag(cell);
            xml.Start(tag);
            for (const auto* name : {L"colspan", L"rowspan", L"align", L"valign"}) xml.Attribute(name, Attr(cell, name));
            xml.value += L'>'; Paragraph(xml, cell, L"p", Context::Section); xml.End(tag);
        }
        xml.End(L"tr");
    }
    xml.End(L"table");
}

void Exporter::Section(Xml& xml, const Node& node, bool note) {
    const auto tag = note ? L"notebody" : L"section";
    xml.Start(tag); xml.Attribute(L"id", Identify(node, !note, note)); xml.value += L'>';
    Title(xml, Child(node, L"title"), Context::Reduced);
    for (const auto& epigraph : Children(node, L"epigraph")) {
        if (note) Block(xml, epigraph, Context::Reduced);
        else Quote(xml, epigraph, true);
    }
    const auto annotation = Child(node, L"annotation");
    if (annotation && !note) {
        if (Child(node, L"image")) Warn(L"FB2 section annotation was placed before its leading illustration in FB3.");
        xml.Start(L"annotation"); xml.Attribute(L"id", Identify(annotation)); xml.value += L'>';
        const auto blocks = Children(annotation);
        if (blocks.empty() || Tag(blocks.front()) == L"empty-line") xml.value += L"<p/>";
        for (const auto& element : blocks) Block(xml, element, Context::Annotation);
        xml.End(L"annotation");
    }
    const auto elements = Children(node);
    const bool nested = !note && std::any_of(elements.begin(), elements.end(), [](const Node& child) { return Tag(child) == L"section"; });
    bool written = false;
    bool wrapper = false;
    for (const auto& child : elements) {
        const auto kind = Tag(child);
        if (kind == L"title" || kind == L"epigraph" || (kind == L"annotation" && !note)) continue;
        if (kind == L"section") {
            if (note) continue;
            if (wrapper) { xml.End(L"section"); wrapper = false; }
            Section(xml, child);
        } else {
            if (nested && !wrapper) {
                Warn(L"FB2 section content was wrapped in an FB3 child section to preserve order.");
                xml.Start(L"section"); xml.Attribute(L"id", Identify(Node(), true)); xml.value += L'>';
                wrapper = true;
            }
            const auto before = xml.value.size();
            Block(xml, child, note ? Context::Note : Context::Section);
            if (before == xml.value.size() && wrapper) xml.value += L"<p/>";
        }
        written = true;
    }
    if (wrapper) xml.End(L"section");
    if (!written) xml.value += L"<p/>";
    xml.End(tag);
}

void Exporter::OpenNoteGroup(Xml& xml, Nodes& headers, bool& groupOpen) {
    if (groupOpen) return;
    xml.Open(L"notes");
    groupOpen = true;
    Nodes titles;
    for (const auto& header : headers) {
        if (const auto title = Child(header, L"title")) titles.push_back(title);
    }
    if (titles.size() == 1) Title(xml, titles.front(), Context::Reduced);
    else if (!titles.empty()) {
        xml.Open(L"title");
        for (const auto& title : titles) {
            for (const auto& block : Children(title)) Block(xml, block, Context::Reduced);
        }
        xml.End(L"title");
    }
    for (const auto& header : headers) {
        for (const auto& quote : Children(header, L"epigraph")) Quote(xml, quote, true);
    }
    headers.clear();
}

void Exporter::NoteChildren(Xml& xml, const Node& node, Nodes& headers, bool& groupOpen) {
    for (const auto& note : Children(node, L"section")) {
        const bool nested = static_cast<bool>(Child(note, L"section"));
        const auto children = Children(note);
        const bool grouping = nested &&
            std::all_of(children.begin(), children.end(), [](const Node& child) {
                const auto tag = Tag(child);
                return tag == L"section" || tag == L"title" || tag == L"epigraph";
            }) && (Attr(note, L"id").empty() ||
                (Content(Child(note, L"title")).empty() && !Child(note, L"epigraph")));
        if (grouping) {
            if (groupOpen) { xml.End(L"notes"); groupOpen = false; }
            headers.push_back(note);
        } else {
            OpenNoteGroup(xml, headers, groupOpen);
            Section(xml, note, true);
        }
        if (nested) {
            Warn(L"Nested FB2 note groups were exported as consecutive FB3 notes.", L"note-groups", L"section");
            NoteChildren(xml, note, headers, groupOpen);
        }
        if (grouping && groupOpen) { xml.End(L"notes"); groupOpen = false; }
    }
}

void Exporter::Body(Xml& xml, const Nodes& bodies) {
    xml.value = L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    xml.Start(L"fb3-body"); xml.Attribute(L"xmlns", BodyNamespace); xml.Attribute(L"xmlns:l", Xlink); xml.Attribute(L"id", m_bookId); xml.value += L'>';
    const auto first = bodies.front();
    Title(xml, Child(first, L"title"), Context::Reduced);
    for (const auto& quote : Children(first, L"epigraph")) Quote(xml, quote, true);
    if (const auto image = Child(first, L"image")) {
        xml.Start(L"section"); xml.Attribute(L"id", Identify(Node(), true)); xml.value += L'>';
        Block(xml, image, Context::Section); xml.End(L"section");
    }
    const auto sections = Children(first, L"section");
    // The FB2 body itself cannot carry the imported FB3 book anchor.
    const auto wrapper = sections.size() == 1 ? sections.front() : Node();
    const auto bookAnchor = L"fb3-anchor-" + m_bookId;
    if (Attr(wrapper, L"id") == bookAnchor && Child(wrapper, L"section")) {
        const auto contents = Children(wrapper);
        if (std::all_of(contents.begin(), contents.end(), [](const Node& child) { return Tag(child) == L"section"; })) {
            m_ids.emplace(bookAnchor, m_bookId);
            for (const auto& section : contents) Section(xml, section);
        } else Section(xml, wrapper);
    } else {
        for (const auto& section : sections) Section(xml, section);
    }
    const auto isNotes = [](const Node& body) { const auto name = Attr(body, L"name"); return name == L"notes" || name == L"comments"; };
    for (size_t index = 1; index < bodies.size(); ++index) {
        if (!isNotes(bodies[index])) {
            Warn(L"An additional FB2 text body was represented as an FB3 section.");
            Section(xml, bodies[index]);
        }
    }
    for (size_t index = 1; index < bodies.size(); ++index) {
        const auto& notes = bodies[index];
        if (!isNotes(notes)) continue;
        if (Attr(notes, L"name") == L"comments") Warn(L"The FB2 comments body was represented as an FB3 note group.", L"note-groups", L"body");
        Nodes headers{notes};
        bool groupOpen = false;
        if (const auto image = Child(notes, L"image")) {
            OpenNoteGroup(xml, headers, groupOpen);
            xml.Start(L"notebody"); xml.Attribute(L"id", Identify(Node(), false, true)); xml.value += L'>';
            Block(xml, image, Context::Note); xml.End(L"notebody");
        }
        NoteChildren(xml, notes, headers, groupOpen);
        if (groupOpen) xml.End(L"notes");
    }
    xml.End(L"fb3-body");

    Text resolved;
    size_t start = 0;
    for (const auto& ref : m_references) {
        const auto found = m_ids.find(ref.id);
        if (found == m_ids.end() || (ref.note && !m_notes.count(found->second))) {
            Fail(L"Unresolved FB2 internal link or note #" + ref.id, ref.note ? L"missing-note" : L"missing-link", L"a", ref.id);
        }
        resolved.append(xml.value, start, ref.offset - start);
        resolved += found->second;
        start = ref.offset;
    }
    resolved.append(xml.value, start, Text::npos);
    xml.value.swap(resolved);
}

void Exporter::Person(Xml& xml, const Node& node, View role) {
    const auto first = Field(node, L"first-name"), last = Field(node, L"last-name"), middle = Field(node, L"middle-name"), nickname = Field(node, L"nickname");
    auto title = nickname;
    if (title.empty()) {
        for (const auto& name : {first, middle, last}) {
            if (!name.empty()) { if (!title.empty()) title += L' '; title += name; }
        }
    }
    if (title.empty()) { title = L"Unknown"; Warn(L"A required FB3 author name was absent in FB2.", L"source-metadata", L"author"); }
    auto id = Field(node, L"id");
    if (!IsUuid(id)) id = StableUuid(m_bookId + L"/person/" + Text(role) + L"/" + title);
    xml.Start(L"subject"); xml.Attribute(L"id", id); xml.Attribute(L"link", role); xml.value += L'>';
    xml.Open(L"title"); xml.Element(L"main", title); xml.End(L"title");
    if (!first.empty()) xml.Element(L"first-name", first);
    if (!middle.empty()) xml.Element(L"middle-name", middle);
    xml.Element(L"last-name", last.empty() ? title : last);
    Text details;
    for (const auto& field : Children(node)) {
        if (Tag(field) == L"email" || Tag(field) == L"home-page") {
            if (!details.empty()) details += L'\n';
            details += Content(field);
        }
    }
    if (!details.empty()) {
        Warn(L"FB2 author contact fields were retained in the FB3 person's description.");
        xml.Element(L"description", details);
    }
    xml.End(L"subject");
}

void Exporter::Sequence(Xml& xml, const Node& node, bool paper) {
    auto name = Attr(node, L"name");
    if (name.empty()) { name = L"Unknown"; Warn(L"An FB2 series did not have a name."); }
    const auto number = Attr(node, L"number");
    const auto children = Children(node, L"sequence");
    if (paper) {
        if (!children.empty() || !number.empty()) Warn(L"A paper FB2 series was represented as a name and number in FB3.");
        xml.Element(L"sequence", number.empty() ? name : name + L" #" + number);
        for (const auto& item : children) Sequence(xml, item, true);
        return;
    }
    const auto identity = m_seriesIdentities.find(name + L'\n' + number);
    Text id;
    if (identity != m_seriesIdentities.end() && !identity->second.empty()) {
        id = std::move(identity->second.front());
        identity->second.pop_front();
    } else id = StableUuid(m_bookId + L"/sequence/" + std::to_wstring(++m_ordinal) + L"/" + name);
    xml.Start(L"sequence"); xml.Attribute(L"id", id);
    if (!number.empty() && number.front() != L'-') xml.Attribute(L"number", number);
    else if (!number.empty()) { name += L" #" + number; Warn(L"A negative FB2 series number was appended to the FB3 series title."); }
    xml.value += L'>'; xml.Open(L"title"); xml.Element(L"main", name); xml.End(L"title");
    if (!children.empty()) Sequence(xml, children.front());
    xml.End(L"sequence");
    for (size_t index = 1; index < children.size(); ++index) {
        Warn(L"Additional nested FB2 series were moved one level up in FB3.");
        Sequence(xml, children[index]);
    }
}

void Exporter::Metadata(Xml& xml, const Node& description) {
    const auto title = Child(description, L"title-info"), info = Child(description, L"document-info"), paper = Child(description, L"publish-info");
    const auto name = Field(title, L"book-title"), originalId = Field(info, L"id"), language = Field(title, L"lang");
    if (name.empty()) Fail(L"The FB2 document has no title.");
    m_bookId = IsUuid(originalId) ? originalId : StableUuid(L"book/" + (originalId.empty() ? name + L"/" + Content(Child(title, L"author")) : originalId));
    xml.value = L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    xml.Start(L"fb3-description"); xml.Attribute(L"xmlns", DescriptionNamespace); xml.Attribute(L"id", m_bookId);
    const auto version = Field(info, L"version");
    xml.Attribute(L"version", version.empty() ? L"1.0" : version);
    xml.value += L'>';
    xml.Open(L"title"); xml.Element(L"main", name); xml.End(L"title");
    const auto customFields = Children(description, L"custom-info");
    constexpr View seriesIdPrefix = L"FBE Next/FB3 series ID ";
    for (const auto& field : customFields) {
        const auto type = Attr(field, L"info-type");
        if (type.compare(0, seriesIdPrefix.size(), seriesIdPrefix) != 0) continue;
        const auto id = type.substr(seriesIdPrefix.size());
        if (IsUuid(id)) m_seriesIdentities[Content(field)].push_back(id);
    }
    for (const auto& sequence : Children(title, L"sequence")) Sequence(xml, sequence);
    xml.Open(L"fb3-relations");
    for (const auto& author : Children(title, L"author")) Person(xml, author, L"author");
    for (const auto& translator : Children(title, L"translator")) Person(xml, translator, L"translator");
    const auto editors = Children(info, L"author");
    const auto editorLabel = editors.size() == 1 && Children(editors.front()).size() == 1
        ? Field(editors.front(), L"nickname") : Text();
    if (editorLabel.empty()) {
        for (const auto& editor : editors) {
            if (!Content(editor).empty()) Person(xml, editor, L"editor");
        }
    }
    xml.End(L"fb3-relations");
    xml.Open(L"fb3-classification");
    for (const auto& genre : Children(title, L"genre")) xml.Element(L"subject", Content(genre));
    xml.End(L"fb3-classification");
    xml.Element(L"lang", language.empty() ? L"und" : language);
    const auto srcLang = Field(title, L"src-lang");
    if (const auto date = Child(title, L"date"); date || !srcLang.empty()) {
        xml.Open(L"written");
        if (!srcLang.empty()) xml.Element(L"lang", srcLang);
        if (date) { xml.Start(L"date"); xml.Attribute(L"value", Attr(date, L"value")); xml.value += L'>'; xml.Escape(Content(date)); xml.End(L"date"); }
        xml.End(L"written");
    }
    const auto owners = Children(info, L"publisher");
    if (!owners.empty()) {
        Xml rights;
        for (const auto& owner : owners) {
            auto ownerName = Field(owner, L"nickname");
            if (ownerName.empty()) {
                for (const auto* field : {L"first-name", L"middle-name", L"last-name"}) {
                    const auto namePart = Field(owner, field);
                    if (namePart.empty()) continue;
                    if (!ownerName.empty()) ownerName += L' ';
                    ownerName += namePart;
                }
            }
            if (ownerName.empty()) continue;
            rights.Start(L"copyright-owner"); rights.Attribute(L"title", ownerName);
            const auto id = Field(owner, L"id");
            if (IsUuid(id)) rights.Attribute(L"uuid", id);
            rights.Attribute(L"url", Field(owner, L"home-page"));
            rights.value += L"/>";
        }
        if (!rights.value.empty()) {
            xml.Open(L"copyrights"); xml.value += rights.value; xml.End(L"copyrights");
        }
    }
    const auto created = Attr(Child(info, L"date"), L"value");
    if (created.empty()) Warn(L"The source FB2 has no machine-readable document date. A fixed date was used in FB3.", L"source-metadata", L"date");
    const auto dateLabel = Field(info, L"date");
    static const std::wregex timestamp(
        LR"([0-9]{4}-[0-9]{2}-[0-9]{2}T(?:[01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9](?:\.[0-9]+)?(?:Z|[+-](?:(?:0[0-9]|1[0-3]):[0-5][0-9]|14:00))?)");
    auto dateTime = created.size() >= 10 ? created.substr(0, 10) + L"T00:00:00Z" : L"1970-01-01T00:00:00Z";
    if (created.size() >= 10 && dateLabel.size() >= 19 && dateLabel.size() <= 64 &&
        dateLabel.compare(0, 10, created, 0, 10) == 0 && std::regex_match(dateLabel, timestamp)) dateTime = dateLabel;
    xml.Start(L"document-info"); xml.Attribute(L"created", dateTime); xml.Attribute(L"updated", dateTime);
    xml.Attribute(L"program-used", Field(info, L"program-used")); xml.Attribute(L"src-url", Field(info, L"src-url"));
    xml.Attribute(L"ocr", Field(info, L"src-ocr"));
    xml.Attribute(L"editor", editorLabel);
    xml.value += L"/>";
    const auto annotation = [&](const Node& node, View tag) {
        if (!node || Children(node).empty()) return;
        xml.Open(tag);
        if (Tag(Children(node).front()) == L"empty-line") xml.value += L"<p/>";
        for (const auto& block : Children(node)) Block(xml, block, Context::Description);
        xml.End(tag);
    };
    annotation(Child(info, L"history"), L"history");
    const auto isbn = Field(paper, L"isbn"), schemaIsbn = SchemaIsbn(isbn);
    if (paper) {
        const auto edition = Field(paper, L"book-name");
        xml.Start(L"paper-publish-info"); xml.Attribute(L"title", edition.empty() ? name : edition);
        for (const auto* key : {L"publisher", L"city", L"year"}) xml.Attribute(key, Field(paper, key));
        xml.value += L'>';
        if (!schemaIsbn.empty()) xml.Element(L"isbn", schemaIsbn);
        for (const auto& sequence : Children(paper, L"sequence")) Sequence(xml, sequence, true);
        xml.End(L"paper-publish-info");
    }
    const auto custom = [&](View key, View data) {
        xml.Start(L"custom-info"); xml.Attribute(L"info-type", key.empty() ? L"FB2" : key); xml.value += L'>'; xml.Escape(data); xml.End(L"custom-info");
    };
    const auto supplement = [&](View key, View data) {
        if (data.empty() || std::any_of(customFields.begin(), customFields.end(), [&](const Node& field) {
            return Attr(field, L"info-type") == key && Content(field) == data;
        })) return;
        custom(key, data);
    };
    if (originalId != m_bookId) supplement(L"FB2 document ID", originalId);
    supplement(L"FBE Next/FB2 keywords", Field(title, L"keywords"));
    if (dateLabel != dateTime) supplement(L"FB2 document date", dateLabel);
    if (!isbn.empty() && isbn != schemaIsbn) {
        Info(L"The original FB2 ISBN was retained in FB3 custom metadata because its format differs from the schema.", L"metadata-retained", L"isbn");
        supplement(L"FB2 ISBN", isbn);
    }
    for (const auto& field : customFields) custom(Attr(field, L"info-type"), Content(field));
    annotation(Child(title, L"annotation"), L"annotation");
    const auto covers = Children(Child(title, L"coverpage"), L"image");
    for (const auto& cover : covers) {
        const auto& resource = Image(cover);
        if (m_cover.empty()) m_cover = resource.path;
    }
    if (covers.size() > 1) Warn(L"FB3 has one cover. Additional FB2 cover images remain stored as package resources.");
    xml.End(L"fb3-description");
}

void Relation(Xml& xml, View id, View type, View target) {
    xml.Start(L"Relationship"); xml.Attribute(L"Id", id); xml.Attribute(L"Type", type); xml.Attribute(L"Target", target); xml.value += L"/>";
}

Xml RelationshipsXml() {
    Xml xml;
    xml.Start(L"Relationships"); xml.Attribute(L"xmlns", ZipRels); xml.value += L'>';
    return xml;
}

Diagnostics Exporter::Convert(ZipWriter& zip) {
    const auto bodies = Children(m_root, L"body");
    if (bodies.empty()) Fail(L"The FB2 document has no book body.");
    ReadResources();
    Xml description, body;
    Metadata(description, Child(m_root, L"description"));
    Body(body, bodies);
    for (const auto& stylesheet : Children(m_root, L"stylesheet")) {
        if (!Content(stylesheet).empty()) Warn(L"The FB2 stylesheet was not carried into the FB3 presentation.");
    }
    Xml types;
    types.Start(L"Types"); types.Attribute(L"xmlns", ZipTypes); types.value += L'>';
    types.value += L"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>";
    const auto contentType = [&](View name, View mime) {
        types.Start(L"Override"); types.Attribute(L"PartName", name); types.Attribute(L"ContentType", mime); types.value += L"/>";
    };
    contentType(L"/fb3/description.xml", L"application/fb3-description+xml");
    contentType(L"/fb3/body.xml", L"application/fb3-body+xml");
    auto root = RelationshipsXml(), descriptionLinks = RelationshipsXml(), bodyLinks = RelationshipsXml();
    Relation(root, L"book", Text(Fb3Rels) + L"Book", L"fb3/description.xml");
    if (!m_cover.empty()) Relation(root, L"cover", Text(ZipRels) + L"/metadata/thumbnail", L"fb3/" + m_cover);
    Relation(descriptionLinks, L"body", Text(Fb3Rels) + L"body", L"body.xml");
    for (const auto& [id, resource] : m_resources) {
        const auto part = L"fb3/" + resource.path;
        contentType(L"/" + part, resource.mime);
        Relation(bodyLinks, resource.relationship, ImageRelationship, resource.path);
        zip.Put(Utf8(part), resource.bytes.data(), resource.bytes.size());
    }
    types.End(L"Types"); root.End(L"Relationships"); descriptionLinks.End(L"Relationships"); bodyLinks.End(L"Relationships");
    zip.Put("[Content_Types].xml", types.value);
    zip.Put("_rels/.rels", root.value);
    zip.Put("fb3/description.xml", description.value);
    zip.Put("fb3/body.xml", body.value);
    zip.Put("fb3/_rels/description.xml.rels", descriptionLinks.value);
    if (!m_resources.empty()) zip.Put("fb3/_rels/body.xml.rels", bodyLinks.value);
    return std::move(m_diagnostics);
}

}

std::vector<std::uint8_t> ReadFb2Stream(IStream& stream) {
    constexpr size_t limit = 96ULL << 20;
    std::vector<std::uint8_t> bytes;
    std::uint8_t buffer[64 * 1024];
    for (;;) {
        ULONG count = 0;
        const auto hr = stream.Read(buffer, sizeof(buffer), &count);
        if (FAILED(hr) || count > sizeof(buffer)) Fail(L"Cannot read the source FB2 stream.");
        if (!count) break;
        if (count > limit - bytes.size()) Fail(L"The source FB2 exceeds the 96 MiB limit.");
        bytes.insert(bytes.end(), buffer, buffer + count);
    }
    return bytes;
}

std::vector<std::uint8_t> ReadFb2File(const fs::path& source) {
    CComPtr<IStream> stream;
    if (FAILED(::SHCreateStreamOnFileEx(source.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
        FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream))) Fail(L"Cannot open the source FB2: " + source.wstring());
    return ReadFb2Stream(*stream.p);
}

Diagnostics ExportBook(const std::vector<std::uint8_t>& fb2, const fs::path& destination,
    const fs::path& runtimeDirectory, bool replaceExisting, const std::function<void()>& beforePublish) {
    const auto document = LoadFb2Snapshot(fb2);
    ValidateInput(document, runtimeDirectory / L"FictionBook.xsd");
    CComPtr<IXMLDOMElement> root;
    CComBSTR ns;
    if (FAILED(document->get_documentElement(&root)) || !root || FAILED(root->get_namespaceURI(&ns)) || ns != Fb2Namespace)
        Fail(L"The source document is not FictionBook 2.");
    const auto target = fs::absolute(destination).lexically_normal();
    Output output(target);
    ZipWriter zip(output.Path());
    auto diagnostics = Exporter(Node(root)).Convert(zip);
    zip.Close();
    {
        const Package package(output.Path());
        ValidatePackageSchemas(package, runtimeDirectory / L"FB3Schemas");
    }
    if (beforePublish) beforePublish();
    output.Publish(target, replaceExisting);
    return diagnostics;
}

namespace {

void Exporter::Block(Xml& xml, const Node& node, Context context) {
    const auto tag = Tag(node);
    if (tag == L"p" || tag == L"subtitle") Paragraph(xml, node, tag == L"subtitle" && context == Context::Section ? L"subtitle" : L"p", context);
    else if (tag == L"empty-line") xml.value += L"<br/>";
    else if (tag == L"image" && context != Context::Description) {
        const auto& resource = Image(node);
        xml.Open(L"p"); xml.Start(L"img"); xml.Attribute(L"src", resource.relationship); xml.Attribute(L"id", Identify(node));
        const auto alt = Attr(node, L"alt"); xml.Attribute(L"alt", alt.empty() ? Attr(node, L"title") : alt);
        xml.value += L"/></p>";
    } else if (tag == L"poem" && (context == Context::Section || context == Context::Note)) Poem(xml, node);
    else if (tag == L"table" && (context == Context::Section || context == Context::Note)) Table(xml, node);
    else if (tag == L"cite" && (context == Context::Section || context == Context::Note || context == Context::Annotation)) Quote(xml, node, false);
    else if (tag == L"cite" || tag == L"poem" || tag == L"table" || tag == L"epigraph" || tag == L"annotation") {
        Warn(L"FB2 <" + tag + L"> was represented by paragraphs in this FB3 context.");
        for (const auto& child : Children(node)) {
            const auto kind = Tag(child);
            if (kind == L"p" || kind == L"v" || kind == L"subtitle" || kind == L"text-author" || kind == L"date") Paragraph(xml, child, L"p", context);
            else if (kind == L"empty-line" || kind == L"image" || kind == L"cite" || kind == L"poem" || kind == L"table" || kind == L"epigraph") Block(xml, child, context);
            else if (kind == L"title" || kind == L"stanza" || kind == L"tr") {
                for (const auto& line : Children(child)) {
                    if (Tag(line) == L"empty-line") xml.value += L"<br/>";
                    else Paragraph(xml, line, L"p", context);
                }
            } else Fail(L"Unsupported FB2 nested element <" + kind + L">.");
        }
    } else Fail(L"Unsupported body element in FB2 <" + tag + L">.");
}

}
}
