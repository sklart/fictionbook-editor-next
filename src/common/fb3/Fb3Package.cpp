#include "Fb3Package.h"
#include "../../import-epub/SecureEpubZip.h"

#include <shlwapi.h>
#include <vector>
#include <string_view>

namespace FbeFb3 {
namespace {

constexpr wchar_t TypesNamespace[] = L"http://schemas.openxmlformats.org/package/2006/content-types";
constexpr wchar_t RelationshipsNamespace[] = L"http://schemas.openxmlformats.org/package/2006/relationships";
constexpr wchar_t RelationshipsType[] = L"application/vnd.openxmlformats-package.relationships+xml";
constexpr wchar_t BookRelationship[] = L"http://www.fictionbook.org/FictionBook3/relationships/Book";
constexpr wchar_t BodyRelationship[] = L"http://www.fictionbook.org/FictionBook3/relationships/body";
constexpr wchar_t CoverRelationship[] = L"http://schemas.openxmlformats.org/package/2006/relationships/metadata/thumbnail";
constexpr wchar_t DescriptionType[] = L"application/fb3-description+xml";
constexpr wchar_t BodyType[] = L"application/fb3-body+xml";
constexpr SecureZipLimits PackageLimits{64ULL << 20, 256ULL << 20, 20000};
constexpr unsigned long long XmlBytes = 16ULL << 20;

[[noreturn]] void Fail(const std::wstring& message) { throw Error{message}; }

bool Equals(const std::wstring& a, const std::wstring& b) {
    return ::CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

int Hex(wchar_t digit) {
    if (digit >= L'0' && digit <= L'9') return digit - L'0';
    if (digit >= L'a' && digit <= L'f') return digit - L'a' + 10;
    if (digit >= L'A' && digit <= L'F') return digit - L'A' + 10;
    return -1;
}

std::wstring DecodeSegment(std::wstring_view segment) {
    std::wstring result;
    for (size_t i = 0; i < segment.size();) {
        if (segment[i] != L'%') {
            result += segment[i++];
            continue;
        }
        std::string bytes;
        while (i < segment.size() && segment[i] == L'%') {
            if (segment.size() - i < 3) Fail(L"Incomplete escape in an OPC part name.");
            const int high = Hex(segment[i + 1]), low = Hex(segment[i + 2]);
            if (high < 0 || low < 0) Fail(L"Invalid escape in an OPC part name.");
            bytes.push_back(static_cast<char>(high * 16 + low));
            i += 3;
        }
        const auto length = static_cast<int>(bytes.size());
        const int count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), length, nullptr, 0);
        if (!count) Fail(L"An OPC part name contains invalid UTF-8.");
        std::wstring decoded(static_cast<size_t>(count), L'\0');
        if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), length, decoded.data(), count) != count)
            Fail(L"Cannot decode an OPC part name.");
        result += decoded;
    }
    if (result.empty() || result == L"." || result == L"..") Fail(L"Invalid segment in an OPC part name.");
    for (wchar_t character : result) {
        if (character < 32 || character == 127 || std::wstring_view(L"/\\:?#<>\"|*").find(character) != std::wstring_view::npos)
            Fail(L"Unsafe character in an OPC part name.");
    }
    return result;
}

// Returns an absolute package name, never a Windows path. Escaped and raw Unicode
// spellings select the same part, but two entries with those spellings are rejected.
std::wstring PartName(const std::wstring& path, const std::wstring& source = {}, bool relationship = false) {
    if (path.empty() || path.size() > 4096 || path.back() == L'/' || path.compare(0, 2, L"//") == 0)
        Fail(L"Invalid OPC part path: " + path);
    std::wstring result;
    if (relationship && path.front() != L'/') {
        const auto slash = source.rfind(L'/');
        if (slash != std::wstring::npos) result.assign(source, 0, slash);
    }
    std::wstring_view remaining(path);
    if (remaining.front() == L'/') remaining.remove_prefix(1);
    while (!remaining.empty()) {
        const auto slash = remaining.find(L'/');
        const auto segment = remaining.substr(0, slash);
        if (relationship && segment == L"..") {
            const auto parent = result.rfind(L'/');
            if (parent == std::wstring::npos) Fail(L"Relationship escapes the root of the package: " + path);
            result.resize(parent);
        } else if (!relationship || segment != L".") {
            result += L'/' + DecodeSegment(segment);
        }
        if (slash == std::wstring_view::npos) break;
        remaining.remove_prefix(slash + 1);
    }
    if (result.empty()) Fail(L"A relationship must reference a part: " + path);
    return result;
}

std::wstring Attribute(IXMLDOMNode* node, const wchar_t* name) {
    CComPtr<IXMLDOMNamedNodeMap> attributes;
    CComPtr<IXMLDOMNode> attribute;
    CComBSTR value;
    if (!node || FAILED(node->get_attributes(&attributes)) || !attributes ||
        FAILED(attributes->getNamedItem(CComBSTR(name), &attribute)) || !attribute || FAILED(attribute->get_text(&value)))
        return {};
    return value ? std::wstring(value, value.Length()) : std::wstring();
}

bool IsElement(IXMLDOMNode* node, const wchar_t* name, const wchar_t* ns) {
    CComBSTR local, uri;
    return node && SUCCEEDED(node->get_baseName(&local)) && local && wcscmp(local, name) == 0 &&
        SUCCEEDED(node->get_namespaceURI(&uri)) && uri && wcscmp(uri, ns) == 0;
}

CComPtr<IXMLDOMElement> DocumentRoot(IXMLDOMDocument2* xml, const wchar_t* name, const wchar_t* ns,
    const Part& part) {
    CComPtr<IXMLDOMElement> root;
    if (FAILED(xml->get_documentElement(&root)) || !IsElement(root, name, ns))
        Fail(L"Unexpected XML element or namespace in " + part.name);
    return root;
}

template<class Reader>
void ReadElements(IXMLDOMNode* parent, const Reader& read) {
    CComPtr<IXMLDOMNodeList> nodes;
    long count = 0;
    if (FAILED(parent->get_childNodes(&nodes)) || !nodes || FAILED(nodes->get_length(&count)) ||
        count < 0 || count > static_cast<long>(PackageLimits.entries)) Fail(L"Invalid OPC manifest element count.");
    for (long i = 0; i < count; ++i) {
        CComPtr<IXMLDOMNode> node;
        DOMNodeType type = NODE_INVALID;
        if (FAILED(nodes->get_item(i, &node)) || !node || FAILED(node->get_nodeType(&type)))
            Fail(L"Cannot read an OPC manifest element.");
        if (type == NODE_ELEMENT) read(node);
    }
}

std::wstring RelationshipSource(const std::wstring& name) {
    if (Equals(name, L"/_rels/.rels")) return L"/";
    const auto slash = name.rfind(L'/');
    if (slash == std::wstring::npos || slash + 6 >= name.size() || !Equals(name.substr(name.size() - 5), L".rels")) return {};
    const auto parent = name.rfind(L'/', slash - 1);
    if (parent == std::wstring::npos || !Equals(name.substr(parent + 1, slash - parent - 1), L"_rels")) return {};
    return name.substr(0, parent) + name.substr(slash, name.size() - slash - 5);
}

bool IsExternalTarget(const std::wstring& target) {
    const auto colon = target.find(L':');
    if (colon == std::wstring::npos || target.find_first_of(L"\t\r\n\\") != std::wstring::npos) return false;
    const auto scheme = target.substr(0, colon);
    if (Equals(scheme, L"mailto")) return colon + 1 < target.size();
    return (Equals(scheme, L"http") || Equals(scheme, L"https")) && target.compare(colon, 3, L"://") == 0 &&
        colon + 3 < target.size() && target[colon + 3] != L'/';
}

const Part* RelatedPart(const Package& package, const std::wstring& source, const wchar_t* type, bool required) {
    const Part* found = nullptr;
    bool seen = false;
    for (const auto& entry : package.Links(source)) {
        const auto& relationship = entry.second;
        if (relationship.type != type) continue;
        if (seen || relationship.external) Fail(L"Invalid or ambiguous relationship in " + source + L": " + type);
        seen = true;
        found = package.FindPart(relationship.target);
    }
    if (required && !found) Fail(L"Required relationship missing in " + source + L": " + type);
    return found;
}

}

bool PartNameLess::operator()(const std::wstring& left, const std::wstring& right) const {
    return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_LESS_THAN;
}

namespace {
CComPtr<IXMLDOMDocument2> LoadXmlStream(const std::wstring& name, IStream* stream) {
    CComPtr<IXMLDOMDocument2> document;
    CComVariant prohibitDtd;
    prohibitDtd.vt = VT_BOOL;
    prohibitDtd.boolVal = VARIANT_TRUE;
    if (FAILED(document.CoCreateInstance(__uuidof(DOMDocument60))) ||
        FAILED(document->put_async(VARIANT_FALSE)) || FAILED(document->put_validateOnParse(VARIANT_FALSE)) ||
        FAILED(document->put_resolveExternals(VARIANT_FALSE)) || FAILED(document->put_preserveWhiteSpace(VARIANT_TRUE)) ||
        FAILED(document->setProperty(CComBSTR(L"ProhibitDTD"), prohibitDtd)) ||
        FAILED(document->setProperty(CComBSTR(L"MaxElementDepth"), CComVariant(192L))))
        Fail(L"Cannot configure the XML reader for " + name);
    VARIANT_BOOL loaded = VARIANT_FALSE;
    if (FAILED(document->load(CComVariant(static_cast<IUnknown*>(stream)), &loaded)) || loaded != VARIANT_TRUE) {
        CComPtr<IXMLDOMParseError> parseError;
        CComBSTR reason;
        if (SUCCEEDED(document->get_parseError(&parseError)) && parseError) parseError->get_reason(&reason);
        Fail(L"Invalid XML in " + name + (reason ? L": " + std::wstring(reason, reason.Length()) : std::wstring()));
    }
    return document;
}
}

CComPtr<IXMLDOMDocument2> LoadXmlPart(const Part& part) {
    std::error_code error;
    const auto size = std::filesystem::file_size(part.file, error);
    if (error || !size || size > XmlBytes) Fail(L"Missing, empty or oversized XML part: " + part.name);
    CComPtr<IStream> stream;
    if (FAILED(::SHCreateStreamOnFileEx(part.file.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
        FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream))) Fail(L"Cannot read " + part.name);
    return LoadXmlStream(part.name, stream);
}

namespace {
CComPtr<IXMLDOMDocument2> LoadMemoryXml(const std::wstring& name, const std::vector<BYTE>& bytes, size_t limit) {
    if (bytes.empty() || bytes.size() > limit) Fail(L"Empty or oversized XML part: " + name);
    CComPtr<IStream> stream;
    stream.Attach(::SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
    if (!stream) Fail(L"Cannot read " + name);
    return LoadXmlStream(name, stream);
}
}

CComPtr<IXMLDOMDocument2> LoadXmlBytes(const std::wstring& name, const std::vector<BYTE>& bytes) {
    return LoadMemoryXml(name, bytes, XmlBytes);
}

CComPtr<IXMLDOMDocument2> LoadFb2Snapshot(const std::vector<BYTE>& bytes) {
    return LoadMemoryXml(L"FB2 document", bytes, 96ULL << 20);
}

Package::Package(const std::filesystem::path& source) {
    try {
        Open(source);
    } catch (...) {
        std::error_code ignored;
        if (!m_scratch.empty()) std::filesystem::remove_all(m_scratch, ignored);
        throw;
    }
}

Package::~Package() {
    std::error_code ignored;
    if (!m_scratch.empty()) std::filesystem::remove_all(m_scratch, ignored);
}

const Part* Package::FindPart(const std::wstring& name) const {
    const auto part = m_parts.find(name);
    return part == m_parts.end() ? nullptr : &part->second;
}

const Relationships& Package::Links(const std::wstring& sourcePart) const {
    static const Relationships empty;
    const auto found = m_links.find(sourcePart);
    return found == m_links.end() ? empty : found->second;
}

void Package::Open(const std::filesystem::path& source) {
    GUID id = {};
    wchar_t text[40] = {};
    if (FAILED(::CoCreateGuid(&id)) || !::StringFromGUID2(id, text, _countof(text))) Fail(L"Cannot allocate an FB3 workspace.");
    const auto scratch = std::filesystem::temp_directory_path() / (L"FBE-FB3-" + std::wstring(text));
    if (!std::filesystem::create_directory(scratch)) Fail(L"Cannot create a private FB3 workspace.");
    m_scratch = scratch;
    const auto directory = m_scratch / L"parts";
    CStringW zipError;
    if (!ExtractSecureZip(CStringW(source.c_str()), CStringW(directory.c_str()), PackageLimits, zipError))
        Fail(L"Invalid FB3 package: " + std::wstring(zipError.GetString()));

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        const auto relative = entry.path().lexically_relative(directory).generic_wstring();
        if (relative.rfind(L"__MACOSX/", 0) == 0) continue;
        const auto name = PartName(relative);
        if (!m_parts.emplace(name, Part{name, entry.path(), {}}).second) Fail(L"Duplicate OPC part: " + name);
    }
    const auto* manifest = FindPart(L"/[Content_Types].xml");
    if (!manifest) Fail(L"FB3 package has no [Content_Types].xml.");
    ReadContentTypes(*manifest);

    for (const auto& entry : m_parts) {
        const auto sourcePart = RelationshipSource(entry.first);
        if (sourcePart.empty()) continue;
        if ((sourcePart != L"/" && !FindPart(sourcePart)) || entry.second.contentType != RelationshipsType)
            Fail(L"Invalid relationship part: " + entry.first);
        m_links.emplace(sourcePart, ReadRelationships(entry.second, sourcePart));
    }
    m_description = RelatedPart(*this, L"/", BookRelationship, true);
    m_body = RelatedPart(*this, m_description->name, BodyRelationship, true);
    m_cover = RelatedPart(*this, L"/", CoverRelationship, false);
    if (m_description->contentType != DescriptionType || m_body->contentType != BodyType)
        Fail(L"Wrong content type for the FB3 description or body.");
    if (m_cover && m_cover->contentType.compare(0, 6, L"image/") != 0)
        Fail(L"The FB3 cover relationship must reference an image.");
    const auto description = LoadXmlPart(*m_description);
    DocumentRoot(description, L"fb3-description", DescriptionNamespace, *m_description);
    const auto body = LoadXmlPart(*m_body);
    DocumentRoot(body, L"fb3-body", BodyNamespace, *m_body);
}

void Package::ReadContentTypes(const Part& part) {
    std::map<std::wstring, std::wstring, PartNameLess> defaults, overrides;
    const auto xml = LoadXmlPart(part);
    const auto root = DocumentRoot(xml, L"Types", TypesNamespace, part);
    ReadElements(root, [&](IXMLDOMNode* node) {
        const auto type = Attribute(node, L"ContentType");
        if (type.empty() || type.find(L'/') == std::wstring::npos || type.find_first_of(L" \t\r\n") != std::wstring::npos)
            Fail(L"Invalid OPC content type: " + type);
        if (IsElement(node, L"Default", TypesNamespace)) {
            const auto extension = Attribute(node, L"Extension");
            if (extension.empty() || extension.find_first_of(L"./\\ \t\r\n") != std::wstring::npos ||
                !defaults.emplace(extension, type).second) Fail(L"Invalid or duplicate default content type: " + extension);
        } else if (IsElement(node, L"Override", TypesNamespace)) {
            const auto path = Attribute(node, L"PartName");
            if (path.empty() || path.front() != L'/' || !overrides.emplace(PartName(path), type).second)
                Fail(L"Invalid or duplicate content type for " + path);
        } else {
            Fail(L"Unexpected element in [Content_Types].xml.");
        }
    });
    for (auto& entry : m_parts) {
        const auto& name = entry.first;
        const auto specified = overrides.find(name);
        if (specified != overrides.end()) {
            entry.second.contentType = specified->second;
        } else {
            const auto dot = name.rfind(L'.');
            if (dot != std::wstring::npos && dot > name.rfind(L'/')) {
                const auto standard = defaults.find(name.substr(dot + 1));
                if (standard != defaults.end()) entry.second.contentType = standard->second;
            }
        }
    }
}

Relationships Package::ReadRelationships(const Part& part, const std::wstring& source) {
    Relationships result;
    const auto xml = LoadXmlPart(part);
    const auto root = DocumentRoot(xml, L"Relationships", RelationshipsNamespace, part);
    ReadElements(root, [&](IXMLDOMNode* node) {
        if (!IsElement(node, L"Relationship", RelationshipsNamespace)) Fail(L"Unexpected element in " + part.name);
        const auto id = Attribute(node, L"Id"), mode = Attribute(node, L"TargetMode");
        Relationship item{Attribute(node, L"Type"), Attribute(node, L"Target"), mode == L"External"};
        if (id.empty() || item.type.empty() || item.target.empty() || result.count(id) ||
            (!mode.empty() && mode != L"Internal" && !item.external)) Fail(L"Invalid relationship in " + part.name + L": " + id);
        if (item.external) {
            if (!IsExternalTarget(item.target)) Fail(L"Unsafe external relationship in " + part.name + L": " + id);
        } else {
            item.target = PartName(item.target, source, true);
            const auto* target = FindPart(item.target);
            if (!target && source == L"/" && item.type == CoverRelationship) {
                m_warnings.emplace_back(L"Optional FB3 cover part is missing: " + item.target);
            } else if (!target || target->contentType.empty() || !RelationshipSource(target->name).empty() ||
                Equals(target->name, L"/[Content_Types].xml")) Fail(L"Unresolved or untyped relationship in " + part.name + L": " + item.target);
        }
        result.emplace(id, std::move(item));
    });
    return result;
}

}
