#include "Fb3Import.h"
#include "Fb3Image.h"
#include "Fb3Package.h"
#include "../fb2/Fb2BinaryInspector.h"

#include <lunasvg.h>
#include <wincrypt.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <string_view>

namespace FbeFb3 {
namespace {

constexpr wchar_t Fb2Namespace[] = L"http://www.gribuser.ru/xml/fictionbook/2.0";
constexpr wchar_t XlinkNamespace[] = L"http://www.w3.org/1999/xlink";
using Node = CComPtr<IXMLDOMNode>;
using Nodes = std::vector<Node>;
using View = std::wstring_view;
using String = std::wstring;

String Value(const CComBSTR& value) { return value ? String(value, value.Length()) : String(); }

String Name(const Node& node) {
    CComBSTR value;
    if (node) node->get_baseName(&value);
    return Value(value);
}

String Namespace(const Node& node) {
    CComBSTR value;
    node->get_namespaceURI(&value);
    return Value(value);
}

String Attribute(const Node& node, const wchar_t* name, const wchar_t* ns = L"") {
    CComPtr<IXMLDOMNamedNodeMap> attributes;
    if (node) node->get_attributes(&attributes);
    Node attr;
    if (attributes) attributes->getQualifiedItem(CComBSTR(name), CComBSTR(ns), &attr);
    CComBSTR value;
    if (attr) attr->get_text(&value);
    return Value(value);
}

String Text(const Node& node) {
    CComBSTR value;
    if (node) node->get_text(&value);
    return Value(value);
}

String Xml(const Node& node) {
    CComBSTR value;
    node->get_xml(&value);
    return Value(value);
}

template<class Function>
void EachChild(const Node& parent, Function function) {
    Node node;
    if (parent) parent->get_firstChild(&node);
    while (node) {
        function(node);
        Node next;
        node->get_nextSibling(&next);
        node = next;
    }
}

DOMNodeType Type(const Node& node) {
    DOMNodeType type = NODE_INVALID;
    node->get_nodeType(&type);
    return type;
}

Nodes Children(const Node& parent, View name = {}) {
    Nodes children;
    EachChild(parent, [&](const Node& node) {
        if (Type(node) == NODE_ELEMENT && (name.empty() || Name(node) == name)) children.push_back(node);
    });
    return children;
}

Node Child(const Node& parent, View name) {
    const auto matches = Children(parent, name);
    return matches.empty() ? Node() : matches.front();
}

Node Root(const CComPtr<IXMLDOMDocument2>& document) {
    CComPtr<IXMLDOMElement> element;
    document->get_documentElement(&element);
    return Node(element);
}

String Trim(String text) {
    const auto begin = text.find_first_not_of(L" \t\r\n");
    if (begin == String::npos) return {};
    return text.substr(begin, text.find_last_not_of(L" \t\r\n") - begin + 1);
}

bool HasText(const Node& node) { return !Trim(Text(node)).empty(); }

bool SafeLink(View target) {
    const auto colon = target.find(L':');
    if (colon == View::npos || target.find_first_of(L"\t\r\n\\") != View::npos) return false;
    const String scheme(target.substr(0, colon));
    for (const auto* allowed : {L"https", L"http", L"mailto"}) {
        if (::CompareStringOrdinal(scheme.c_str(), -1, allowed, -1, TRUE) == CSTR_EQUAL) return true;
    }
    return false;
}

[[noreturn]] void Fail(String message) { throw Error{std::move(message)}; }

void StripSvg11Doctype(std::vector<BYTE>& bytes) {
    const std::string_view prefix(reinterpret_cast<const char*>(bytes.data()), (std::min)(bytes.size(), size_t(512)));
    constexpr std::string_view whitespace = " \t\r\n";
    size_t start = prefix.substr(0, 3) == "\xef\xbb\xbf" ? 3 : 0;
    if (prefix.substr(start, 6) == "<?xml ") {
        const auto endDeclaration = prefix.find("?>", start);
        if (endDeclaration == std::string_view::npos) return;
        start = endDeclaration + 2;
    }
    start = prefix.find_first_not_of(whitespace, start);
    if (start == std::string_view::npos || prefix.substr(start, 10) != "<!DOCTYPE ") return;
    const auto end = prefix.find('>', start);
    if (end == std::string_view::npos) return;
    std::string declaration;
    bool gap = false;
    for (char ch : prefix.substr(start, end - start + 1)) {
        if (whitespace.find(ch) != std::string_view::npos) { gap = true; continue; }
        if (gap) declaration += ' ';
        gap = false;
        declaration += ch;
    }
    constexpr auto svg11 = "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">";
    if (declaration == svg11) bytes.erase(bytes.begin() + start, bytes.begin() + end + 1);
}

std::vector<BYTE> RasterizeSvg(const Part& part, std::vector<BYTE>& bytes) {
    // Strip only the legacy, exact SVG 1.1 external declaration. Both parsers
    // receive the same bytes and any remaining DTD is rejected by MSXML.
    StripSvg11Doctype(bytes);
    const auto xml = LoadXmlBytes(part.name, bytes);
    const auto root = Root(xml);
    if (!root || Name(root) != L"svg" || Namespace(root) != L"http://www.w3.org/2000/svg")
        Fail(L"Invalid FB3 SVG image: " + part.name);
    const auto svg = lunasvg::Document::loadFromData(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (!svg) Fail(L"Cannot read SVG image: " + part.name);
    const auto width = svg->width(), height = svg->height();
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
        Fail(L"Invalid SVG dimensions in " + part.name);
    constexpr float maxDimension = 4096.0F;
    const auto scale = (std::min)(1.0F, maxDimension / (std::max)(width, height));
    const auto bitmap = svg->renderToBitmap((std::max)(1, static_cast<int>(std::ceil(width * scale))),
        (std::max)(1, static_cast<int>(std::ceil(height * scale))));
    struct PngOutput { std::vector<BYTE> bytes; bool failed = false; } png;
    const auto write = [](void* closure, void* data, int count) {
        auto& output = *static_cast<PngOutput*>(closure);
        if (output.failed || count <= 0) return;
        const auto* first = static_cast<const BYTE*>(data);
        try { output.bytes.insert(output.bytes.end(), first, first + count); }
        catch (const std::bad_alloc&) { output.failed = true; }
    };
    if (bitmap.isNull() || !bitmap.writeToPng(write, &png) || png.failed || png.bytes.empty())
        Fail(L"Cannot rasterize FB3 SVG image: " + part.name);
    return std::move(png.bytes);
}

void ValidateFb2(const String& xml, const std::filesystem::path& schema) {
    CComPtr<IXMLDOMDocument2> doc;
    CComPtr<IXMLDOMSchemaCollection2> schemas;
    if (FAILED(doc.CoCreateInstance(__uuidof(DOMDocument60))) ||
        FAILED(schemas.CoCreateInstance(__uuidof(XMLSchemaCache60))) ||
        FAILED(schemas->add(CComBSTR(Fb2Namespace), CComVariant(schema.c_str()))))
        Fail(L"Cannot load the FB2 schema: " + schema.wstring());
    doc->put_async(VARIANT_FALSE);
    doc->put_resolveExternals(VARIANT_FALSE);
    doc->put_validateOnParse(VARIANT_FALSE);
    VARIANT_BOOL loaded = VARIANT_FALSE;
    CComPtr<IXMLDOMParseError> error;
    if (FAILED(doc->loadXML(CComBSTR(static_cast<int>(xml.size()), xml.data()), &loaded)) || loaded != VARIANT_TRUE) {
        doc->get_parseError(&error);
    } else if (FAILED(doc->putref_schemas(CComVariant(static_cast<IDispatch*>(schemas)))) || FAILED(doc->validate(&error))) {
        Fail(L"Cannot validate the converted FB2 document.");
    }
    long code = 0;
    if (error) error->get_errorCode(&code);
    if (loaded != VARIANT_TRUE || code) {
        CComBSTR reason;
        if (error) error->get_reason(&reason);
        Fail(L"Converted FB2 document is invalid: " + Value(reason));
    }
}

class Converter {
public:
    explicit Converter(const Package& package) : m_package(package) { m_book.warnings = package.Warnings(); }
    ImportedBook Convert(const std::filesystem::path& schema);

private:
    enum class BlockContext { Section, Annotation, Quote, Epigraph, Title };
    struct Reference { size_t opening; size_t position; size_t content; size_t closing; size_t end; String id; bool note; };
    struct Anchor { String outputId; bool note; bool ambiguous = false; };

    void Warn(String message);
    void Escape(View text);
    void Attr(View name, View value);
    void Element(View name, View text);
    void Lang(const Node& node);
    void AnchorId(const Node& node, bool note = false);
    void CollectInlineIds(const Node& node, std::vector<String>& ids);
    void SetAnchor(const std::vector<String>& ids, bool note);
    void ResolveReferences();
    void Inline(const Node& node, bool insideLink = false);
    void InlineChildren(const Node& node, bool insideLink);
    void Reduced(const Node& node, View tag);
    void CellContent(const Node& node, bool& hasBlock);
    void TableAlign(const Node& node);
    void Paragraph(const Node& node, View tag = L"p", View prefix = {});
    void Image(const Node& node, bool inlineImage);
    void ImageTarget(const Part& part);
    void Binaries();
    void Block(const Node& node, BlockContext context = BlockContext::Section);
    void Title(const Node& node);
    void Quote(const Node& node, bool epigraph);
    void Poem(const Node& node);
    void Table(const Node& node);
    void List(const Node& node, BlockContext context);
    void Section(const Node& node, bool note = false);
    void Body(const Node& node);
    void Metadata(const Node& node, const std::filesystem::path& schema);
    bool Person(const Node& node, View role);
    void Sequence(const Node& node);
    void Custom(View type, View value);

    const Package& m_package;
    ImportedBook m_book;
    String m_xml;
    std::set<String> m_warningSet;
    std::map<String, Anchor, std::less<>> m_anchors;
    size_t m_generatedAnchor = 0;
    std::map<const Part*, String> m_images;
    std::vector<const Part*> m_imageOrder;
    std::vector<Reference> m_references;
};

void Converter::Warn(String message) {
    if (m_warningSet.insert(message).second) m_book.warnings.push_back(std::move(message));
}

void Converter::Escape(View text) {
    for (wchar_t character : text) {
        switch (character) {
        case L'&': m_xml += L"&amp;"; break;
        case L'<': m_xml += L"&lt;"; break;
        case L'>': m_xml += L"&gt;"; break;
        case L'\"': m_xml += L"&quot;"; break;
        case L'\r': m_xml += L"&#13;"; break;
        default: m_xml += character;
        }
    }
}

void Converter::Attr(View name, View value) {
    if (value.empty()) return;
    m_xml += L' '; m_xml += name; m_xml += L"=\"";
    Escape(value); m_xml += L'\"';
}

void Converter::Element(View name, View text) {
    m_xml += L'<'; m_xml += name; m_xml += L'>';
    Escape(text);
    m_xml += L"</"; m_xml += name; m_xml += L'>';
}

void Converter::Lang(const Node& node) {
    Attr(L"xml:lang", Attribute(node, L"lang", L"http://www.w3.org/XML/1998/namespace"));
}

void Converter::SetAnchor(const std::vector<String>& ids, bool note) {
    if (ids.empty()) return;
    // Prefix valid FB3 IDs and encode other spellings through a generated anchor.
    const auto& first = ids.front();
    const bool simple = first.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-") == String::npos;
    const String output = simple && !m_anchors.count(first) ? L"fb3-anchor-" + first : L"fb3_" + std::to_wstring(++m_generatedAnchor);
    for (const auto& id : ids) {
        const auto [anchor, added] = m_anchors.emplace(id, Anchor{output, note});
        if (!added) {
            anchor->second.ambiguous = true;
            Warn(L"Repeated FB3 content ID " + id + L" was assigned a distinct FB2 ID.");
        }
    }
    Attr(L"id", output);
}

void Converter::AnchorId(const Node& node, bool note) {
    const auto id = Attribute(node, L"id");
    if (!id.empty()) SetAnchor({id}, note);
}

void Converter::CollectInlineIds(const Node& node, std::vector<String>& ids) {
    for (const auto& child : Children(node)) {
        const auto id = Attribute(child, L"id");
        if (!id.empty()) {
            ids.push_back(id);
            Warn(L"FB3 <" + Name(child) + L"> anchor " + id + L" was moved to its containing FB2 block.");
        }
        CollectInlineIds(child, ids);
    }
}

void Converter::ResolveReferences() {
    if (m_references.empty()) return;
    String resolved;
    size_t start = 0;
    for (const auto& reference : m_references) {
        const auto found = m_anchors.find(reference.id);
        if (found == m_anchors.end() || found->second.ambiguous || (reference.note && !found->second.note)) {
            if (reference.note) Fail(L"Unresolved or ambiguous FB3 note: " + reference.id);
            Warn(L"Unresolved or ambiguous FB3 link #" + reference.id + L" was converted to its visible text.");
            resolved.append(m_xml, start, reference.opening - start);
            resolved.append(m_xml, reference.content, reference.closing - reference.content);
            start = reference.end;
            continue;
        }
        resolved.append(m_xml, start, reference.position - start);
        resolved += found->second.outputId;
        start = reference.position;
    }
    resolved.append(m_xml, start, String::npos);
    m_xml.swap(resolved);
}

void Converter::InlineChildren(const Node& node, bool insideLink) {
    EachChild(node, [&](const Node& child) { Inline(child, insideLink); });
}

void Converter::Inline(const Node& node, bool insideLink) {
    const auto kind = Type(node);
    if (kind == NODE_TEXT || kind == NODE_CDATA_SECTION) { Escape(Text(node)); return; }
    if (kind != NODE_ELEMENT) return;
    const auto tag = Name(node);
    if (tag == L"img") { Image(node, true); return; }
    if (tag == L"a" || tag == L"note") {
        if (insideLink) Fail(L"Nested link in FB3 content.");
        const bool note = tag == L"note";
        auto target = Attribute(node, L"href", XlinkNamespace);
        if (target.empty()) target = Attribute(node, L"href");
        const auto opening = m_xml.size();
        const auto index = m_references.size();
        m_xml += L"<a";
        if (note || (!target.empty() && target.front() == L'#')) {
            if (!target.empty() && target.front() == L'#') target.erase(0, 1);
            if (target.empty()) Fail(L"An FB3 link has an empty fragment.");
            m_xml += L" l:href=\"#";
            m_references.push_back({opening, m_xml.size(), 0, 0, 0, target, note});
            m_xml += L'\"';
            if (note) Attr(L"type", L"note");
        } else {
            if (!SafeLink(target)) Fail(L"Unsupported FB3 hyperlink target: " + target);
            Attr(L"l:href", target);
        }
        m_xml += L'>';
        if (index != m_references.size()) m_references[index].content = m_xml.size();
        InlineChildren(node, true);
        if (note && !HasText(node) && Children(node).empty()) Escape(L"[*]");
        if (index != m_references.size()) m_references[index].closing = m_xml.size();
        m_xml += L"</a>";
        if (index != m_references.size()) m_references[index].end = m_xml.size();
        return;
    }
    if (tag == L"paper-page-break") {
        Warn(L"FB3 paper-page-break page " + Attribute(node, L"page-before") + L" has no FB2 equivalent.");
        return;
    }
    if (tag == L"br") { Warn(L"Inline FB3 <br> was converted to a space."); Escape(L" "); return; }
    String mapped = tag;
    if (tag == L"em") mapped = L"emphasis";
    if (tag == L"underline" || tag == L"spacing" || tag == L"span" || tag == L"smallcaps") {
        if (insideLink) {
            Warn(L"FB3 <" + tag + L"> inside a link was reduced to its contents because FB2 does not allow named styles there.");
            InlineChildren(node, true);
            return;
        }
        Warn(L"FB3 <" + tag + L"> formatting was retained as a named FB2 style.");
        mapped = L"style";
    } else if (tag != L"strong" && tag != L"em" && tag != L"strikethrough" && tag != L"sub" && tag != L"sup" && tag != L"code") {
        Fail(L"Unsupported inline FB3 element <" + tag + L">.");
    }
    m_xml += L"<" + mapped;
    if (mapped == L"style") {
        const auto name = tag == L"span" ? Attribute(node, L"class") : String();
        Attr(L"name", name.empty() ? L"fb3-" + tag : name);
    }
    if (!insideLink) Lang(node);
    m_xml += L'>';
    InlineChildren(node, insideLink);
    m_xml += L"</" + mapped + L">";
}

void Converter::Paragraph(const Node& node, View tag, View prefix) {
    m_xml += L'<'; m_xml += tag;
    std::vector<String> ids;
    const auto id = Attribute(node, L"id");
    if (!id.empty()) ids.push_back(id);
    CollectInlineIds(node, ids);
    SetAnchor(ids, false);
    Lang(node);
    m_xml += L'>';
    Escape(prefix);
    InlineChildren(node, false);
    m_xml += L"</"; m_xml += tag; m_xml += L'>';
}

void Converter::Reduced(const Node& node, View tag) {
    const auto kind = Name(node);
    if (kind == L"p" || kind == L"subtitle" || kind == L"marker") { Paragraph(node, tag); return; }
    if (kind == L"br") { Element(tag, L""); return; }
    if (kind == L"li" && Children(node, L"p").empty() && Children(node, L"ol").empty() && Children(node, L"ul").empty()) {
        Paragraph(node, tag, L"\u2022 ");
        return;
    }
    if (kind != L"title" && kind != L"blockquote" && kind != L"subscription" && kind != L"epigraph" &&
        kind != L"ol" && kind != L"ul" && kind != L"li" && kind != L"pre")
        Fail(L"Unsupported FB3 <" + kind + L"> when converting to FB2 <" + String(tag) + L">.");
    if (!Attribute(node, L"id").empty()) {
        m_xml += L'<'; m_xml += tag; AnchorId(node); m_xml += L"/>";
    }
    EachChild(node, [&](const Node& child) {
        const auto type = Type(child);
        if (type == NODE_ELEMENT) Reduced(child, tag);
        else if ((type == NODE_TEXT || type == NODE_CDATA_SECTION) && HasText(child)) Element(tag, Text(child));
    });
}

void Converter::CellContent(const Node& node, bool& hasBlock) {
    const auto kind = Name(node);
    if (kind == L"p" || kind == L"subtitle" || kind == L"br" || kind == L"ol" || kind == L"ul" || kind == L"li" ||
        kind == L"title" || kind == L"subscription" || kind == L"pre" || kind == L"blockquote") {
        Warn(L"FB3 table-cell blocks were joined inside the corresponding FB2 cell.");
        if (kind == L"p" || kind == L"subtitle" || kind == L"br" || kind == L"li") {
            if (hasBlock) Escape(L" \u2022 ");
            hasBlock = true;
        }
        EachChild(node, [&](const Node& child) { CellContent(child, hasBlock); });
    } else Inline(node);
}

void Converter::TableAlign(const Node& node) {
    const auto value = Attribute(node, L"align");
    if (value == L"justify") {
        Warn(L"FB3 table alignment 'justify' was converted to left alignment in FB2.");
        Attr(L"align", L"left");
    } else Attr(L"align", value);
}

void Converter::ImageTarget(const Part& part) {
    const auto [image, added] = m_images.emplace(&part, L"fb3-image-" + std::to_wstring(m_images.size() + 1));
    if (added) m_imageOrder.push_back(&part);
    Attr(L"l:href", L"#" + image->second);
}

void Converter::Image(const Node& node, bool inlineImage) {
    const auto source = Attribute(node, L"src");
    const auto& links = m_package.Links(m_package.Body().name);
    const auto link = links.find(source);
    if (link == links.end() || link->second.type != ImageRelationship || link->second.external)
        Fail(L"Invalid FB3 image relationship: " + source);
    const auto* part = m_package.FindPart(link->second.target);
    if (!part) Fail(L"Missing FB3 image: " + link->second.target);
    m_xml += L"<image";
    if (!inlineImage) AnchorId(node);
    ImageTarget(*part);
    Attr(L"alt", Attribute(node, L"alt"));
    if (!inlineImage) Attr(L"title", Attribute(node, L"title"));
    for (const auto* attr : {L"width", L"height", L"float", L"max-width", L"max-height"}) {
        if (!Attribute(node, attr).empty()) Warn(L"FB3 image " + source + L" uses unsupported layout attribute " + attr + L".");
    }
    m_xml += L"/>";
}

void Converter::Binaries() {
    for (const auto* part : m_imageOrder) {
        auto type = FbeFb2Binary::NormalizeMimeType(part->contentType);
        const bool svg = type == L"image/svg+xml";
        if (!svg && type != L"image/jpeg" && type != L"image/png" && type != L"image/gif" && type != L"image/bmp" && type != L"image/webp")
            Fail(L"Unsupported FB3 image format in " + part->name + L": " + type);
        std::error_code error;
        const auto size = std::filesystem::file_size(part->file, error);
        if (error || !size || size > (64ULL << 20)) Fail(L"Cannot read FB3 image: " + part->name);
        std::vector<BYTE> bytes(static_cast<size_t>(size));
        std::ifstream stream(part->file, std::ios::binary);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
            Fail(L"Cannot read FB3 image: " + part->name);
        if (svg) {
            bytes = RasterizeSvg(*part, bytes);
            type = L"image/png";
            Warn(L"FB3 SVG image " + part->name + L" was rasterized to PNG. Vector data and animation are unavailable in the imported image.");
        } else ValidateRasterImage(part->name, type, bytes);
        DWORD count = 0;
        constexpr DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
        if (!::CryptBinaryToStringW(bytes.data(), static_cast<DWORD>(bytes.size()), flags, nullptr, &count))
            Fail(L"Cannot encode FB3 image: " + part->name);
        String base64(count, L'\0');
        if (!::CryptBinaryToStringW(bytes.data(), static_cast<DWORD>(bytes.size()), flags, base64.data(), &count))
            Fail(L"Cannot encode FB3 image: " + part->name);
        m_xml += L"<binary";
        Attr(L"id", m_images.at(part));
        Attr(L"content-type", type);
        m_xml += L'>';
        m_xml.append(base64.data(), count);
        m_xml += L"</binary>";
    }
}

void Converter::Title(const Node& node) {
    m_xml += L"<title";
    Lang(node);
    m_xml += L'>';
    if (!Attribute(node, L"id").empty()) {
        m_xml += L"<p"; AnchorId(node); m_xml += L"/>";
        Warn(L"FB3 <title> anchor was placed in a paragraph inside the FB2 title.");
    }
    const auto children = Children(node);
    if (children.empty() && HasText(node)) Element(L"p", Text(node));
    else for (const auto& child : children) Block(child, BlockContext::Title);
    m_xml += L"</title>";
}

void Converter::Quote(const Node& node, bool epigraph) {
    m_xml += epigraph ? L"<epigraph" : L"<cite";
    AnchorId(node);
    if (!epigraph) Lang(node);
    m_xml += L'>';
    const auto context = epigraph ? BlockContext::Epigraph : BlockContext::Quote;
    for (const auto& child : Children(node)) {
        if (Name(child) != L"subscription") { Block(child, context); continue; }
        const auto lines = Children(child);
        if (std::any_of(lines.begin(), lines.end(), [](const Node& line) { return Name(line) != L"p"; }))
            Warn(L"Complex FB3 <subscription> content was moved to FB2 attribution paragraphs.");
        Reduced(child, L"text-author");
    }
    m_xml += epigraph ? L"</epigraph>" : L"</cite>";
}

void Converter::Poem(const Node& node) {
    m_xml += L"<poem"; AnchorId(node); Lang(node); m_xml += L'>';
    if (const auto title = Child(node, L"title")) Title(title);
    for (const auto& quote : Children(node, L"epigraph")) Quote(quote, true);
    bool hasStanza = false;
    for (const auto& stanza : Children(node, L"stanza")) {
        m_xml += L"<stanza"; Lang(stanza); m_xml += L'>';
        if (const auto title = Child(stanza, L"title")) Title(title);
        if (const auto subtitle = Child(stanza, L"subtitle")) Paragraph(subtitle, L"subtitle");
        if (!Attribute(stanza, L"id").empty()) {
            m_xml += L"<v"; AnchorId(stanza); m_xml += L"/>";
            Warn(L"FB3 <stanza> anchor was placed in an FB2 verse.");
        }
        bool hasVerse = false;
        for (const auto& line : Children(stanza)) {
            const auto kind = Name(line);
            if (kind == L"p" || kind == L"v") { Paragraph(line, L"v"); hasVerse = true; }
            else if (kind == L"br") { m_xml += L"<v/>"; hasVerse = true; }
            else if (kind == L"epigraph" || kind == L"subscription") {
                Warn(L"FB3 stanza <" + kind + L"> was represented as FB2 verses in source order.");
                const auto start = m_xml.size();
                Reduced(line, L"v");
                hasVerse = hasVerse || m_xml.size() != start;
            }
            else if (kind != L"title" && kind != L"subtitle") Fail(L"Unsupported FB3 poem line <" + kind + L">.");
        }
        if (!hasVerse) m_xml += L"<v/>";
        m_xml += L"</stanza>";
        hasStanza = true;
    }
    if (!hasStanza) Fail(L"An FB3 poem has no stanza.");
    for (const auto& child : Children(node)) {
        const auto kind = Name(child);
        if (kind != L"title" && kind != L"epigraph" && kind != L"stanza" && kind != L"subscription")
            Fail(L"Unsupported FB3 poem element <" + kind + L">.");
    }
    if (const auto attribution = Child(node, L"subscription")) {
        Warn(L"FB3 poem attribution was represented with FB2 text-author paragraphs.");
        Reduced(attribution, L"text-author");
    }
    m_xml += L"</poem>";
}

void Converter::Table(const Node& node) {
    for (const auto& child : Children(node)) {
        if (Name(child) == L"title") {
            Warn(L"FB3 table heading was moved to an FB2 paragraph before the table.");
            Reduced(child, L"p");
        } else if (Name(child) != L"tr") Fail(L"Unsupported FB3 table element <" + Name(child) + L">.");
    }
    m_xml += L"<table"; AnchorId(node); m_xml += L'>';
    const auto rows = Children(node, L"tr");
    if (rows.empty()) Fail(L"An FB3 table has no rows.");
    for (const auto& row : rows) {
        m_xml += L"<tr"; TableAlign(row); m_xml += L'>';
        const auto cells = Children(row);
        if (cells.empty()) Fail(L"An FB3 table row has no cells.");
        for (const auto& cell : cells) {
            const auto tag = Name(cell);
            if (tag != L"th" && tag != L"td") Fail(L"Unsupported FB3 table cell <" + tag + L">.");
            m_xml += L"<" + tag;
            std::vector<String> ids;
            const auto id = Attribute(cell, L"id");
            if (!id.empty()) ids.push_back(id);
            CollectInlineIds(cell, ids);
            SetAnchor(ids, false);
            Lang(cell);
            for (const auto* attr : {L"colspan", L"rowspan", L"valign"}) Attr(attr, Attribute(cell, attr));
            TableAlign(cell);
            m_xml += L'>';
            bool hasBlock = false;
            EachChild(cell, [&](const Node& child) { CellContent(child, hasBlock); });
            m_xml += L"</" + tag + L">";
        }
        m_xml += L"</tr>";
    }
    m_xml += L"</table>";
}

void Converter::List(const Node& node, BlockContext context) {
    Warn(L"FB3 list nesting and item markers were represented with FB2 paragraphs.");
    const bool ordered = Name(node) == L"ol";
    size_t ordinal = 0;
    for (const auto& item : Children(node)) {
        if (Name(item) == L"title") { Block(item, context); continue; }
        if (Name(item) == L"ol" || Name(item) == L"ul") { List(item, context); continue; }
        if (Name(item) != L"li") Fail(L"Unsupported FB3 list element <" + Name(item) + L">.");
        const auto marker = ordered ? std::to_wstring(++ordinal) + L". " : String(L"\u2022 ");
        const auto children = Children(item);
        const auto hasBlocks = [](const Node& child) { const auto tag = Name(child); return tag == L"p" || tag == L"ol" || tag == L"ul"; };
        if (std::none_of(children.begin(), children.end(), hasBlocks)) { Paragraph(item, L"p", marker); continue; }
        bool first = true;
        if (!Attribute(item, L"id").empty()) {
            m_xml += L"<p"; AnchorId(item); m_xml += L'>'; Escape(marker); m_xml += L"</p>";
            first = false;
        }
        EachChild(item, [&](const Node& child) {
            const auto type = Type(child);
            if (type == NODE_ELEMENT) {
                if (Name(child) == L"p") { Paragraph(child, L"p", first ? View(marker) : View()); first = false; }
                else Block(child, context);
            } else if ((type == NODE_TEXT || type == NODE_CDATA_SECTION) && HasText(child)) {
                Element(L"p", (first ? marker : String()) + Text(child)); first = false;
            }
        });
    }
}

void Converter::Block(const Node& node, BlockContext context) {
    const auto tag = Name(node);
    if (tag == L"p" || tag == L"subtitle" || tag == L"marker") {
        const bool subtitle = tag != L"p" && context != BlockContext::Title && context != BlockContext::Epigraph;
        Paragraph(node, subtitle ? L"subtitle" : L"p");
    } else if (tag == L"title") {
        Warn(L"An FB3 block heading was moved to FB2 paragraphs.");
        Reduced(node, L"p");
    } else if (tag == L"poem" && context != BlockContext::Title) {
        Poem(node);
    } else if (tag == L"blockquote" && context != BlockContext::Title && context != BlockContext::Quote) {
        Quote(node, false);
    } else if (tag == L"blockquote") {
        Warn(L"An FB3 quote nested in a heading or quote was represented in FB2 paragraphs.");
        Reduced(node, L"p");
    } else if (tag == L"table" && context != BlockContext::Title && context != BlockContext::Epigraph) {
        Table(node);
    } else if (tag == L"ol" || tag == L"ul") {
        List(node, context);
    } else if (tag == L"br") {
        if (context == BlockContext::Epigraph) m_xml += L"<p/>";
        else m_xml += L"<empty-line/>";
        if (!Attribute(node, L"clear").empty()) Warn(L"FB3 <br> layout attribute clear has no FB2 equivalent.");
    } else if (tag == L"img") {
        // A paragraph is valid even when the image is the first section content.
        m_xml += L"<p"; AnchorId(node); m_xml += L'>'; Image(node, true); m_xml += L"</p>";
    } else if (tag == L"div" || tag == L"pre" || tag == L"subscription" || tag == L"clipped" || tag == L"annotation") {
        if (tag == L"clipped") Warn(L"The source FB3 marks omitted content with <clipped>. The imported FB2 contains the available fragment.");
        else Warn(L"FB3 <" + tag + L"> was represented with ordinary FB2 content blocks.");
        const auto start = m_xml.size();
        if (!Attribute(node, L"id").empty()) { m_xml += L"<p"; AnchorId(node); m_xml += L"/>"; }
        EachChild(node, [&](const Node& child) {
            if (Type(child) == NODE_ELEMENT) Block(child, context);
            else if ((Type(child) == NODE_TEXT || Type(child) == NODE_CDATA_SECTION) && HasText(child)) Element(L"p", Text(child));
        });
        if (start == m_xml.size() && tag == L"clipped") m_xml += L"<empty-line/>";
    } else if (tag == L"fb3-relations" || tag == L"fb3-classification") {
        Warn(L"Section-level FB3 <" + tag + L"> metadata was represented as FB2 text.");
        const auto text = Trim(Text(node));
        if (!text.empty()) Element(L"p", text);
    } else {
        Fail(L"Unsupported FB3 content element <" + tag + L">.");
    }
}

void Converter::Section(const Node& node, bool note) {
    m_xml += L"<section"; AnchorId(node, note); Lang(node); m_xml += L'>';
    const auto children = Children(node);
    const auto start = m_xml.size();
    size_t next = 0;
    if (next < children.size() && Name(children[next]) == L"title") Title(children[next++]);
    while (next < children.size() && Name(children[next]) == L"epigraph") Quote(children[next++], true);
    if (next < children.size() && Name(children[next]) == L"annotation") {
        const auto& annotation = children[next++];
        m_xml += L"<annotation"; AnchorId(annotation); Lang(annotation); m_xml += L'>';
        for (const auto& child : Children(annotation)) Block(child, BlockContext::Annotation);
        m_xml += L"</annotation>";
    }
    const bool nested = std::any_of(children.begin() + next, children.end(), [](const Node& child) {
        return Name(child) == L"section" || Name(child) == L"notebody";
    });
    bool wrapper = false;
    bool content = false;
    for (; next < children.size(); ++next) {
        const auto& child = children[next];
        const auto tag = Name(child);
        if (tag == L"section" || tag == L"notebody") {
            if (wrapper) { m_xml += L"</section>"; wrapper = false; }
            Section(child, tag == L"notebody");
            content = true;
        } else {
            if (nested && !wrapper) {
                Warn(L"An FB3 section mixes text and subsections. The FB2 conversion grouped text into child sections in source order.");
                m_xml += L"<section>"; wrapper = true;
            }
            const auto before = m_xml.size();
            Block(child);
            if (m_xml.size() != before) content = true;
            else if (wrapper) m_xml += L"<empty-line/>";
        }
    }
    if (wrapper) m_xml += L"</section>";
    if (!content && m_xml.size() != start) m_xml += L"<empty-line/>";
    m_xml += L"</section>";
}

void Converter::Body(const Node& node) {
    m_xml += L"<body"; Lang(node); m_xml += L'>';
    if (const auto title = Child(node, L"title")) Title(title);
    for (const auto& quote : Children(node, L"epigraph")) Quote(quote, true);
    const auto id = Attribute(node, L"id");
    if (!id.empty()) {
        m_xml += L"<section"; SetAnchor({id}, false); m_xml += L'>';
    }
    bool hasSection = false;
    for (const auto& child : Children(node)) {
        const auto tag = Name(child);
        if (tag == L"section") { Section(child); hasSection = true; }
        else if (tag != L"notes" && tag != L"title" && tag != L"epigraph") Fail(L"Unsupported FB3 body element <" + tag + L">.");
    }
    if (!hasSection) Fail(L"The FB3 book has no main text section.");
    if (!id.empty()) m_xml += L"</section>";
    m_xml += L"</body>";
    const auto notes = Children(node, L"notes");
    if (!notes.empty()) {
        m_xml += L"<body name=\"notes\">";
        for (const auto& group : notes) Section(group);
        m_xml += L"</body>";
    }
}

bool Converter::Person(const Node& node, View role) {
    const auto title = Trim(Text(Child(node, L"title")));
    const auto first = Trim(Text(Child(node, L"first-name")));
    const auto middle = Trim(Text(Child(node, L"middle-name")));
    const auto last = Trim(Text(Child(node, L"last-name")));
    if (title.empty() && first.empty() && middle.empty() && last.empty()) return false;
    m_xml += L'<'; m_xml += role; m_xml += L'>';
    if (!first.empty() || !last.empty()) {
        Element(L"first-name", first);
        if (!middle.empty()) Element(L"middle-name", middle);
        Element(L"last-name", last);
        if (!title.empty()) Element(L"nickname", title);
    } else Element(L"nickname", title.empty() ? middle : title);
    const auto href = Attribute(node, L"href", XlinkNamespace);
    if (SafeLink(href)) Element(L"home-page", href);
    auto uuid = Attribute(node, L"id");
    if (uuid.empty()) uuid = Attribute(node, L"uuid");
    if (!uuid.empty()) Element(L"id", uuid);
    m_xml += L"</"; m_xml += role; m_xml += L'>';
    return true;
}

void Converter::Sequence(const Node& node) {
    auto title = Text(Child(node, L"title"));
    if (title.empty()) title = Attribute(node, L"title");
    // The FB2 sequence name attribute is required even for an empty source title.
    m_xml += L"<sequence name=\""; Escape(title); m_xml += L'\"';
    Attr(L"number", Attribute(node, L"number"));
    Lang(node);
    m_xml += L'>';
    for (const auto& entry : Children(node, L"sequence")) Sequence(entry);
    m_xml += L"</sequence>";
}

void Converter::Custom(View type, View value) {
    m_xml += L"<custom-info";
    Attr(L"info-type", type.empty() ? L"FB3" : type);
    m_xml += L'>'; Escape(value); m_xml += L"</custom-info>";
}

void Converter::Metadata(const Node& node, const std::filesystem::path& schema) {
    const auto genresPath = schema.parent_path() / L"FictionBookGenres.xsd";
    const auto genresDoc = LoadXmlPart({L"FictionBookGenres.xsd", genresPath, L"application/xml"});
    const auto enumeration = Child(Child(Root(genresDoc), L"simpleType"), L"restriction");
    std::set<String> genres;
    for (const auto& item : Children(enumeration, L"enumeration")) genres.insert(Attribute(item, L"value"));
    if (genres.empty()) Fail(L"The installed FB2 genre schema has no genre vocabulary.");

    const auto title = Child(node, L"title");
    const auto main = Trim(Text(Child(title, L"main")));
    if (main.empty()) Fail(L"The FB3 description has no main book title.");
    const auto classification = Child(node, L"fb3-classification");
    const auto relations = Children(Child(node, L"fb3-relations"), L"subject");
    const auto edition = Child(node, L"paper-publish-info");
    const auto info = Child(node, L"document-info");
    const auto written = Child(node, L"written");
    const auto writtenDate = Child(written, L"date");
    m_xml += L"<description><title-info>";
    std::set<String> emittedGenres;
    String keywords;
    for (const auto& subject : Children(classification, L"subject")) {
        auto genre = Trim(Text(subject));
        if (genre.empty()) continue;
        for (auto& ch : genre) if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
        if (genres.count(genre)) {
            if (emittedGenres.insert(genre).second) Element(L"genre", genre);
        } else {
            Warn(L"The FB3 subject '" + Trim(Text(subject)) + L"' has no matching FB2 genre and was retained as a keyword.");
            if (!keywords.empty()) keywords += L", ";
            keywords += Trim(Text(subject));
        }
    }
    if (emittedGenres.empty()) Element(L"genre", L"other");
    bool hasAuthor = false;
    for (const auto& person : relations) {
        if (Attribute(person, L"link") == L"author" && Person(person, L"author")) hasAuthor = true;
    }
    if (!hasAuthor) {
        Warn(L"The FB3 description has no named author. An empty FB2 author field was added.");
        m_xml += L"<author><nickname/></author>";
    }
    Element(L"book-title", main);
    if (const auto annotation = Child(node, L"annotation")) {
        m_xml += L"<annotation"; AnchorId(annotation); Lang(annotation); m_xml += L'>';
        for (const auto& child : Children(annotation)) Block(child, BlockContext::Annotation);
        m_xml += L"</annotation>";
    }
    if (!keywords.empty()) Element(L"keywords", keywords);
    if (writtenDate) {
        m_xml += L"<date"; Attr(L"value", Attribute(writtenDate, L"value")); m_xml += L'>';
        Escape(Text(writtenDate)); m_xml += L"</date>";
    }
    if (const auto* cover = m_package.Cover()) {
        m_xml += L"<coverpage><image"; ImageTarget(*cover); m_xml += L"/></coverpage>";
    }
    const auto language = Trim(Text(Child(node, L"lang")));
    if (language.empty()) Warn(L"The FB3 description has no language. The FB2 document uses 'und'.");
    Element(L"lang", language.empty() ? L"und" : language);
    const auto originalLanguage = Trim(Text(Child(written, L"lang")));
    if (!originalLanguage.empty()) Element(L"src-lang", originalLanguage);
    for (const auto& person : relations) if (Attribute(person, L"link") == L"translator") Person(person, L"translator");
    for (const auto& sequence : Children(node, L"sequence")) Sequence(sequence);
    m_xml += L"</title-info><document-info>";
    bool hasEditor = false;
    for (const auto& person : relations) {
        if (Attribute(person, L"link") == L"editor" && Person(person, L"author")) hasEditor = true;
    }
    if (!hasEditor) {
        m_xml += L"<author>";
        Element(L"nickname", Attribute(info, L"editor"));
        m_xml += L"</author>";
    }
    const auto program = Attribute(info, L"program-used");
    if (!program.empty()) Element(L"program-used", program);
    const auto created = Attribute(info, L"created");
    m_xml += L"<date";
    const auto time = created.find(L'T');
    if (time != String::npos) Attr(L"value", created.substr(0, time));
    m_xml += L'>'; Escape(created); m_xml += L"</date>";
    const auto source = Attribute(info, L"src-url");
    if (!source.empty()) Element(L"src-url", source);
    const auto ocr = Attribute(info, L"ocr");
    if (!ocr.empty()) Element(L"src-ocr", ocr);
    const auto id = Attribute(node, L"id");
    if (id.empty()) Fail(L"The FB3 description has no book identifier.");
    Element(L"id", id);
    const auto version = Attribute(node, L"version");
    Element(L"version", version.empty() ? L"1.0" : version);
    if (const auto history = Child(node, L"history")) {
        m_xml += L"<history>";
        for (const auto& child : Children(history)) Block(child, BlockContext::Annotation);
        m_xml += L"</history>";
    }
    for (const auto& owner : Children(Child(node, L"copyrights"), L"copyright-owner")) {
        const auto name = Attribute(owner, L"title");
        if (!name.empty()) { m_xml += L"<publisher>"; Element(L"nickname", name); m_xml += L"</publisher>"; }
    }
    m_xml += L"</document-info>";

    if (edition) {
        m_xml += L"<publish-info>";
        for (const auto* name : {L"title", L"publisher", L"city", L"year"}) {
            const auto value = Attribute(edition, name);
            if (!value.empty()) Element(View(name) == L"title" ? L"book-name" : name, value);
        }
        const auto isbn = Text(Child(edition, L"isbn"));
        if (!isbn.empty()) Element(L"isbn", isbn);
        for (const auto& sequence : Children(edition, L"sequence")) Sequence(sequence);
        m_xml += L"</publish-info>";
    }
    for (const auto& field : Children(node, L"custom-info")) Custom(Attribute(field, L"info-type"), Text(field));
    for (const auto& field : Children(node)) {
        const auto tag = Name(field);
        if (tag == L"custom-info" || tag == L"annotation" || tag == L"history" || tag == L"lang") continue;
        // Preserve metadata with additional attributes or subfields in its source form.
        if (tag == L"title" && Children(field).size() == 1) continue;
        if (tag == L"paper-publish-info" && field == edition && Children(field, L"isbn").size() <= 1 && !Child(field, L"biblio-description")) continue;
        Warn(L"FB3 metadata <" + tag + L"> with no complete FB2 equivalent was also stored in custom-info.");
        Custom(L"FB3 " + tag, Xml(field));
    }
    m_xml += L"</description>";
}

ImportedBook Converter::Convert(const std::filesystem::path& schema) {
    const auto description = LoadXmlPart(m_package.Description());
    const auto body = LoadXmlPart(m_package.Body());
    m_xml = L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        L"<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" xmlns:l=\"http://www.w3.org/1999/xlink\">";
    Metadata(Root(description), schema);
    Body(Root(body));
    ResolveReferences();
    Binaries();
    m_xml += L"</FictionBook>";
    ValidateFb2(m_xml, schema);
    m_book.xml = std::move(m_xml);
    return std::move(m_book);
}

}

ImportedBook ImportBook(const std::filesystem::path& source, const std::filesystem::path& fb2Schema) {
    const Package package(source);
    return Converter(package).Convert(fb2Schema);
}

}
