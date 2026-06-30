#include "StdAfx.h"
#include "ExportEPUBPlugin.h"

#include "FbeEpubExport.h"

#include <Shlwapi.h>
#include <Shellapi.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")

// PVS-Studio: ATL object map macro intentionally expands to ATL-generated code.
//-V835
//-V1096
OBJECT_ENTRY_AUTO(__uuidof(ExportEPUBPlugin), CExportEPUBPlugin)

namespace {

const wchar_t* FB_NS = L"http://www.gribuser.ru/xml/fictionbook/2.0";
const wchar_t* XLINK_NS = L"http://www.w3.org/1999/xlink";

void ShowError(HWND owner, const CString& message)
{
    CString title;
    title.LoadString(IDS_ERROR_TITLE);
    MessageBox(owner, message, title, MB_ICONERROR | MB_OK);
}

void ShowExportError(HWND owner, const std::wstring& error)
{
    CString fmt;
    fmt.LoadString(IDS_ERROR_EXPORT_FAILED);
    CString msg;
    msg.Format(fmt, error.empty() ? L"Unknown error" : error.c_str());
    ShowError(owner, msg);
}

std::wstring NormalizeWhitespace(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    bool inSpace = true;
    for (wchar_t ch : s) {
        const bool space = (ch <= 32 || ch == 160);
        if (space) {
            if (!inSpace) {
                out.push_back(L' ');
                inSpace = true;
            }
        } else {
            out.push_back(ch);
            inSpace = false;
        }
    }
    while (!out.empty() && out.back() == L' ') out.pop_back();
    return out;
}

std::wstring NodeText(IXMLDOMNodePtr node)
{
    if (node == nullptr) return {};
    CComBSTR text;
    CheckHR(node->get_text(&text));
    return NormalizeWhitespace(BstrToWString(text));
}

std::wstring SelectText(IXMLDOMNodePtr node, const wchar_t* xpath)
{
    if (node == nullptr) return {};
    IXMLDOMNodePtr found;
    CheckHR(node->selectSingleNode(bstr_t(xpath), &found));
    return NodeText(found);
}


std::wstring LocalName(IXMLDOMNodePtr node)
{
    CComBSTR name;
    if (SUCCEEDED(node->get_baseName(&name)) && name.Length() > 0) {
        return BstrToWString(name);
    }
    name.Empty();
    if (SUCCEEDED(node->get_nodeName(&name)) && name.Length() > 0) {
        std::wstring n = BstrToWString(name);
        const auto pos = n.find(L':');
        if (pos != std::wstring::npos) n.erase(0, pos + 1);
        return n;
    }
    return {};
}

std::wstring GetAttr(IXMLDOMElementPtr element, const wchar_t* name)
{
    if (element == nullptr) return {};
    _variant_t v;
    if (FAILED(element->getAttribute(bstr_t(name), &v))) return {};
    if (V_VT(&v) == VT_BSTR) return BstrToWString(V_BSTR(&v));
    return {};
}

std::wstring GetHrefAttr(IXMLDOMElementPtr element)
{
    std::wstring href = GetAttr(element, L"l:href");
    if (href.empty()) href = GetAttr(element, L"xlink:href");
    if (href.empty()) href = GetAttr(element, L"href");
    return href;
}

std::wstring SelectDateValue(IXMLDOMNodePtr node, const wchar_t* xpath)
{
    if (node == nullptr) return {};
    IXMLDOMNodePtr found;
    CheckHR(node->selectSingleNode(bstr_t(xpath), &found));
    if (found == nullptr) return {};

    IXMLDOMElementPtr element(found);
    if (element != nullptr) {
        const std::wstring value = GetAttr(element, L"value");
        if (!NormalizeWhitespace(value).empty()) {
            return NormalizeWhitespace(value);
        }
    }
    return NodeText(found);
}

std::wstring StripFragmentPrefix(std::wstring id)
{
    if (!id.empty() && id[0] == L'#') id.erase(0, 1);
    return id;
}

bool HasExtension(const std::wstring& fileName);

std::wstring NormalizeMimeType(std::wstring mime)
{
    std::transform(mime.begin(), mime.end(), mime.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    if (mime == L"image/jpg" || mime == L"image/pjpeg") return L"image/jpeg";
    return mime;
}

std::wstring ExtensionForMime(const std::wstring& mime)
{
    const std::wstring normalized = NormalizeMimeType(mime);
    if (normalized == L"image/jpeg") return L".jpg";
    if (normalized == L"image/png") return L".png";
    if (normalized == L"image/gif") return L".gif";
    if (normalized == L"image/svg+xml") return L".svg";
    if (normalized == L"image/webp") return L".webp";
    return L".bin";
}

std::wstring DetectImageMimeType(const std::vector<std::uint8_t>& data)
{
    if (data.size() >= 8 &&
        data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
        return L"image/png";
    }
    if (data.size() >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return L"image/jpeg";
    }
    if (data.size() >= 6 &&
        data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8' &&
        (data[4] == '7' || data[4] == '9') && data[5] == 'a') {
        return L"image/gif";
    }
    if (data.size() >= 12 &&
        data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
        return L"image/webp";
    }

    const size_t probeLen = std::min<size_t>(data.size(), 512);
    std::string probe(reinterpret_cast<const char*>(data.data()), reinterpret_cast<const char*>(data.data()) + probeLen);
    std::transform(probe.begin(), probe.end(), probe.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (probe.find("<svg") != std::string::npos) {
        return L"image/svg+xml";
    }
    return {};
}

bool ExtensionMatchesMime(const std::wstring& fileName, const std::wstring& mime)
{
    const std::wstring ext = ExtensionForMime(mime);
    if (ext == L".bin") return HasExtension(fileName);

    const auto dot = fileName.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring current = fileName.substr(dot);
    std::transform(current.begin(), current.end(), current.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    if (mime == L"image/jpeg") return current == L".jpg" || current == L".jpeg" || current == L".jpe";
    return current == ext;
}

std::wstring ReplaceOrAppendExtensionForMime(const std::wstring& fileName, const std::wstring& mime)
{
    const std::wstring ext = ExtensionForMime(mime);
    if (ext == L".bin") {
        return HasExtension(fileName) ? fileName : (fileName + ext);
    }

    const auto slash = fileName.find_last_of(L"/\\");
    const auto dot = fileName.find_last_of(L'.');
    if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)) {
        return fileName.substr(0, dot) + ext;
    }
    return fileName + ext;
}

std::wstring SanitizeFileName(std::wstring s)
{
    s = StripFragmentPrefix(s);
    if (s.empty()) s = L"resource";
    for (wchar_t& ch : s) {
        if (ch < 32 || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
            ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*') {
            ch = L'_';
        }
    }
    while (!s.empty() && (s.back() == L'.' || s.back() == L' ')) s.pop_back();
    if (s.empty()) s = L"resource";
    return s;
}

bool HasExtension(const std::wstring& fileName)
{
    const auto slash = fileName.find_last_of(L"/\\");
    const auto dot = fileName.find_last_of(L'.');
    return dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash);
}

std::wstring UniqueFileName(const std::wstring& fileName, std::set<std::wstring>& used)
{
    std::wstring base = fileName;
    std::wstring ext;
    const auto dot = fileName.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        base = fileName.substr(0, dot);
        ext = fileName.substr(dot);
    }

    std::wstring candidate = fileName;
    int n = 2;
    while (!used.insert(candidate).second) {
        std::wstringstream ss;
        ss << base << L"_" << n++ << ext;
        candidate = ss.str();
    }
    return candidate;
}

std::wstring IdFromIndex(const wchar_t* prefix, size_t index)
{
    std::wstringstream ss;
    ss << prefix << index;
    return ss.str();
}

int Base64Value(wchar_t ch)
{
    if (ch >= L'A' && ch <= L'Z') return static_cast<int>(ch - L'A');
    if (ch >= L'a' && ch <= L'z') return static_cast<int>(ch - L'a') + 26;
    if (ch >= L'0' && ch <= L'9') return static_cast<int>(ch - L'0') + 52;
    if (ch == L'+') return 62;
    if (ch == L'/') return 63;
    return -1;
}

bool IsXmlWhitespace(wchar_t ch)
{
    return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
}

std::vector<std::uint8_t> DecodeBase64Text(const std::wstring& text)
{
    std::vector<std::uint8_t> out;
    out.reserve(text.size() * 3 / 4);

    int quartet[4] = {0, 0, 0, 0};
    int count = 0;
    bool seenPadding = false;

    auto flush = [&]() -> bool {
        if (count != 4) return false;
        out.push_back(static_cast<std::uint8_t>((quartet[0] << 2) | (quartet[1] >> 4)));
        if (quartet[2] >= 0) {
            out.push_back(static_cast<std::uint8_t>(((quartet[1] & 0x0F) << 4) | (quartet[2] >> 2)));
        }
        if (quartet[3] >= 0) {
            out.push_back(static_cast<std::uint8_t>(((quartet[2] & 0x03) << 6) | quartet[3]));
        }
        count = 0;
        return true;
    };

    for (wchar_t ch : text) {
        if (IsXmlWhitespace(ch)) continue;

        if (ch == L'=') {
            seenPadding = true;
            quartet[count++] = -1;
        } else {
            if (seenPadding) {
                // Non-padding character after '=' means the FB2 binary is malformed.
                return {};
            }
            const int value = Base64Value(ch);
            if (value < 0) return {};
            quartet[count++] = value;
        }

        if (count == 4 && !flush()) return {};
    }

    // In strict base64 count must end at 0. Some real FB2 files omit trailing padding;
    // handle that leniently instead of failing the whole book.
    if (count == 2) {
        out.push_back(static_cast<std::uint8_t>((quartet[0] << 2) | (quartet[1] >> 4)));
    } else if (count == 3) {
        out.push_back(static_cast<std::uint8_t>((quartet[0] << 2) | (quartet[1] >> 4)));
        out.push_back(static_cast<std::uint8_t>(((quartet[1] & 0x0F) << 4) | (quartet[2] >> 2)));
    } else if (count != 0) {
        return {};
    }

    return out;
}

std::vector<std::uint8_t> BinaryDataFromNode(IXMLDOMNodePtr node)
{
    if (node == nullptr) return {};

    CComBSTR text;
    CheckHR(node->get_text(&text));
    const std::wstring base64 = BstrToWString(text);
    if (NormalizeWhitespace(base64).empty()) return {};

    // Decode in our own code instead of relying on MSXML dataType=bin.base64.
    // On some real FB2 files MSXML can throw an opaque _com_error while decoding
    // otherwise valid binary data, which stops batch conversion with ExitCode 100.
    return DecodeBase64Text(base64);
}

struct ImageInfo {
    std::wstring fileName;   // EPUB image file name under images/.
    std::wstring resourceId; // OPF manifest id.
    std::string mediaType;
};

using ImageMap = std::map<std::wstring, ImageInfo>; // FB2 binary id -> EPUB image info.

struct LinkTarget {
    std::wstring fileName; // Relative to OEBPS/text directory, e.g. "chapter_012.xhtml".
    std::wstring id;       // XHTML-safe fragment id.
};

using LinkMap = std::map<std::wstring, LinkTarget>; // Original FB2 id -> EPUB target.
using NoteBacklinkMap = std::map<std::wstring, std::vector<LinkTarget>>; // Note id -> back references.

struct XhtmlContext {
    const ImageMap& images;
    const LinkMap& links;
    const NoteBacklinkMap* noteBacklinks = nullptr;
    std::map<std::wstring, size_t>* emittedNoteRefCounters = nullptr;
    std::wstring currentFileName;
    bool isEpub3 = false;
    bool notesChapter = false;
};

std::wstring ChapterFileNameForIndex(size_t index)
{
    std::wstringstream ss;
    ss << L"chapter_" << std::setw(3) << std::setfill(L'0') << (index + 1) << L".xhtml";
    return ss.str();
}

bool IsAsciiLetter(wchar_t ch)
{
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
}

bool IsAsciiDigit(wchar_t ch)
{
    return ch >= L'0' && ch <= L'9';
}

bool IsSafeXmlIdChar(wchar_t ch)
{
    return IsAsciiLetter(ch) || IsAsciiDigit(ch) || ch == L'_' || ch == L'-' || ch == L'.';
}

std::wstring Hex4(unsigned value)
{
    const wchar_t* digits = L"0123456789ABCDEF";
    std::wstring out;
    out.push_back(digits[(value >> 12) & 0xF]);
    out.push_back(digits[(value >> 8) & 0xF]);
    out.push_back(digits[(value >> 4) & 0xF]);
    out.push_back(digits[value & 0xF]);
    return out;
}

std::wstring MakeSafeXmlId(const std::wstring& original)
{
    std::wstring id = StripFragmentPrefix(original);
    if (id.empty()) id = L"fb2xml";

    std::wstring out;
    out.reserve(id.size() + 4);
    for (wchar_t ch : id) {
        if (IsSafeXmlIdChar(ch)) {
            out.push_back(ch);
        } else {
            out += L"_x";
            out += Hex4(static_cast<unsigned>(ch) & 0xFFFFu);
            out += L"_";
        }
    }

    if (out.empty()) out = L"fb2xml";
    if (!IsAsciiLetter(out[0]) && out[0] != L'_') {
        out.insert(out.begin(), L'_');
    }
    return out;
}

std::wstring UniqueXmlId(const std::wstring& base, std::set<std::wstring>& used)
{
    std::wstring candidate = base.empty() ? L"fb2xml" : base;
    int n = 2;
    while (!used.insert(candidate).second) {
        std::wstringstream ss;
        ss << base << L"_" << n++;
        candidate = ss.str();
    }
    return candidate;
}

std::wstring ElementIdAttr(IXMLDOMElementPtr element, const LinkMap& links)
{
    const std::wstring sourceId = StripFragmentPrefix(GetAttr(element, L"id"));
    if (sourceId.empty()) return {};

    const auto it = links.find(sourceId);
    if (it == links.end()) return {};

    return L" id=\"" + fbe::epub::XmlEscape(it->second.id) + L"\"";
}

std::wstring AttrIfPresent(IXMLDOMElementPtr element, const wchar_t* name)
{
    const std::wstring value = GetAttr(element, name);
    if (value.empty()) return {};
    std::wstring out = L" ";
    out += name;
    out += L"=\"";
    out += fbe::epub::XmlEscape(value);
    out += L"\"";
    return out;
}

std::wstring CssDeclaration(const std::wstring& name, const std::wstring& value)
{
    if (value.empty()) return {};
    return name + L": " + value + L";";
}

std::wstring CssTextAlignFromFb2(const std::wstring& align)
{
    if (align == L"left" || align == L"right" || align == L"center" || align == L"justify") return align;
    return {};
}

std::wstring CssVerticalAlignFromFb2(const std::wstring& valign)
{
    if (valign == L"top" || valign == L"middle" || valign == L"bottom" || valign == L"baseline") return valign;
    return {};
}

std::wstring TableCellAttrs(IXMLDOMElementPtr element, const XhtmlContext& ctx)
{
    std::wstring attrs;
    attrs += AttrIfPresent(element, L"colspan");
    attrs += AttrIfPresent(element, L"rowspan");

    if (ctx.isEpub3) {
        const std::wstring align = CssTextAlignFromFb2(GetAttr(element, L"align"));
        const std::wstring valign = CssVerticalAlignFromFb2(GetAttr(element, L"valign"));
        std::wstring style;
        style += CssDeclaration(L"text-align", align);
        style += CssDeclaration(L"vertical-align", valign);
        if (!style.empty()) {
            attrs += L" style=\"" + fbe::epub::XmlEscape(style) + L"\"";
        }
    } else {
        attrs += AttrIfPresent(element, L"align");
        attrs += AttrIfPresent(element, L"valign");
    }
    return attrs;
}

std::wstring LangAttrs(IXMLDOMElementPtr element)
{
    std::wstring lang = GetAttr(element, L"xml:lang");
    if (lang.empty()) lang = GetAttr(element, L"lang");
    if (lang.empty()) return {};
    const std::wstring escaped = fbe::epub::XmlEscape(lang);
    return L" xml:lang=\"" + escaped + L"\" lang=\"" + escaped + L"\"";
}

std::wstring CommonAttrs(IXMLDOMElementPtr element, const XhtmlContext& ctx)
{
    return ElementIdAttr(element, ctx.links) + LangAttrs(element);
}

std::wstring EpubTypeAttr(const XhtmlContext& ctx, const std::wstring& value)
{
    if (!ctx.isEpub3 || value.empty()) return {};
    return L" epub:type=\"" + fbe::epub::XmlEscape(value) + L"\"";
}

std::wstring CssClassNameFromFb2Style(const std::wstring& name)
{
    std::wstring cls = L"fb2-style";
    if (!name.empty()) cls += L"-";
    for (wchar_t ch : name) {
        if (IsAsciiLetter(ch) || IsAsciiDigit(ch) || ch == L'_' || ch == L'-') {
            cls.push_back(ch);
        } else {
            cls.push_back(L'_');
        }
    }
    return cls;
}

std::wstring ClassAttr(const std::wstring& cls)
{
    if (cls.empty()) return {};
    return L" class=\"" + fbe::epub::XmlEscape(cls) + L"\"";
}

std::wstring ImageAltAttr(IXMLDOMElementPtr element)
{
    std::wstring alt = GetAttr(element, L"alt");
    if (alt.empty()) alt = GetAttr(element, L"title");
    if (alt.empty()) alt = StripFragmentPrefix(GetHrefAttr(element));
    return L" alt=\"" + fbe::epub::XmlEscape(alt) + L"\"";
}

bool IsExternalHref(const std::wstring& href)
{
    return href.rfind(L"http://", 0) == 0 || href.rfind(L"https://", 0) == 0;
}

bool IsForbiddenFileHref(const std::wstring& href)
{
    return href.rfind(L"file://", 0) == 0 || href.rfind(L"FILE://", 0) == 0 ||
           href.rfind(L"file:\\", 0) == 0 || href.rfind(L"FILE:\\", 0) == 0;
}

std::wstring TrimWide(std::wstring value)
{
    while (!value.empty() && (value.front() <= L' ' || value.front() == 0x00A0)) value.erase(value.begin());
    while (!value.empty() && (value.back() <= L' ' || value.back() == 0x00A0)) value.pop_back();
    return value;
}

std::wstring PercentEncodeSpacesOutsideHost(const std::wstring& href)
{
    std::wstring out;
    out.reserve(href.size());

    const size_t scheme = href.find(L"://");
    size_t hostEnd = std::wstring::npos;
    if (scheme != std::wstring::npos) {
        hostEnd = href.find_first_of(L"/?#", scheme + 3);
        if (hostEnd == std::wstring::npos) hostEnd = href.size();
    }

    for (size_t i = 0; i < href.size(); ++i) {
        // In the host part spaces and already encoded %20 are invalid; remove them.
        if (hostEnd != std::wstring::npos && scheme != std::wstring::npos && i > scheme + 2 && i < hostEnd) {
            if (href[i] == L' ' || href[i] == L'\t' || href[i] == L'\r' || href[i] == L'\n') continue;
            if (i + 2 < hostEnd && href[i] == L'%' && href[i + 1] == L'2' && href[i + 2] == L'0') {
                i += 2;
                continue;
            }
        }

        if (href[i] == L' ' || href[i] == L'\t' || href[i] == L'\r' || href[i] == L'\n') {
            out += L"%20";
        } else {
            out.push_back(href[i]);
        }
    }
    return out;
}

std::wstring NormalizeHttpHrefDelimiters(std::wstring href)
{
    if (!IsExternalHref(href)) return href;

    // EPUBCheck rejects backslashes in URLs. FB2 collections often contain
    // Windows-like URLs such as http://host\path. For HTTP(S) links the
    // safest normalization is to use normal URL path separators.
    for (wchar_t& ch : href) {
        if (ch == L'\\') ch = L'/';
    }
    return href;
}

std::wstring SanitizeHrefForXhtml(const std::wstring& href)
{
    std::wstring out = TrimWide(href);

    // file:// links point to local paths from the original author's machine.
    // EPUBCheck rejects them and e-readers cannot resolve them, so render such
    // links as plain text by returning an empty href to the caller.
    if (IsForbiddenFileHref(out)) return {};

    if (IsExternalHref(out)) {
        out = NormalizeHttpHrefDelimiters(out);
        out = PercentEncodeSpacesOutsideHost(out);
    }
    return out;
}

bool HasAncestorElementNamed(IXMLDOMNodePtr node, const std::wstring& name)
{
    IXMLDOMNodePtr parent;
    if (FAILED(node->get_parentNode(&parent))) return false;
    while (parent != nullptr) {
        if (LocalName(parent) == name) return true;
        IXMLDOMNodePtr next;
        if (FAILED(parent->get_parentNode(&next))) break;
        parent = std::move(next);
    }
    return false;
}

bool HasDirectParentElementNamed(IXMLDOMNodePtr node, const std::wstring& name)
{
    IXMLDOMNodePtr parent;
    if (FAILED(node->get_parentNode(&parent)) || parent == nullptr) return false;
    return LocalName(parent) == name;
}


bool HasDescendantElementNamed(IXMLDOMNodePtr node, const std::wstring& name)
{
    if (node == nullptr) return false;
    IXMLDOMNodeListPtr children;
    if (FAILED(node->get_childNodes(&children)) || children == nullptr) return false;
    long length = 0;
    if (FAILED(children->get_length(&length))) return false;
    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr child;
        if (FAILED(children->get_item(i, &child)) || child == nullptr) continue;
        DOMNodeType type = NODE_INVALID;
        if (SUCCEEDED(child->get_nodeType(&type)) && type == NODE_ELEMENT) {
            if (LocalName(child) == name) return true;
            if (HasDescendantElementNamed(child, name)) return true;
        }
    }
    return false;
}

bool IsInsidePoemLikeBlock(IXMLDOMNodePtr node)
{
    return HasAncestorElementNamed(node, L"poem") || HasAncestorElementNamed(node, L"stanza");
}

bool ContainsXhtmlElement(const std::wstring& fragment, const wchar_t* tagName)
{
    const std::wstring needle1 = std::wstring(L"<") + tagName + L">";
    const std::wstring needle2 = std::wstring(L"<") + tagName + L" ";
    return fragment.find(needle1) != std::wstring::npos || fragment.find(needle2) != std::wstring::npos;
}

bool ContainsBlockXhtml(const std::wstring& fragment)
{
    return ContainsXhtmlElement(fragment, L"p") ||
           ContainsXhtmlElement(fragment, L"div") ||
           ContainsXhtmlElement(fragment, L"blockquote") ||
           ContainsXhtmlElement(fragment, L"table") ||
           ContainsXhtmlElement(fragment, L"aside") ||
           ContainsXhtmlElement(fragment, L"h1") ||
           ContainsXhtmlElement(fragment, L"h2") ||
           ContainsXhtmlElement(fragment, L"h3");
}

bool IsXhtmlBlockTagName(const std::wstring& name)
{
    return name == L"address" || name == L"aside" || name == L"blockquote" || name == L"div" ||
           name == L"dl" || name == L"figure" || name == L"h1" || name == L"h2" || name == L"h3" ||
           name == L"h4" || name == L"h5" || name == L"h6" || name == L"hr" || name == L"nav" ||
           name == L"ol" || name == L"p" || name == L"pre" || name == L"section" ||
           name == L"table" || name == L"ul";
}

bool TryReadXhtmlTag(const std::wstring& text, size_t pos, std::wstring& name, bool& closing, bool& selfClosing, size_t& endPos)
{
    name.clear();
    closing = false;
    selfClosing = false;
    endPos = std::wstring::npos;

    if (pos >= text.size() || text[pos] != L'<') return false;
    size_t i = pos + 1;
    if (i < text.size() && text[i] == L'/') {
        closing = true;
        ++i;
    }

    if (i >= text.size()) return false;
    const size_t nameStart = i;
    while (i < text.size()) {
        const wchar_t ch = text[i];
        if (ch == L'>' || ch == L'/' || ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') break;
        ++i;
    }
    if (i == nameStart) return false;
    name = text.substr(nameStart, i - nameStart);

    endPos = text.find(L'>', i);
    if (endPos == std::wstring::npos) return false;

    size_t j = endPos;
    while (j > pos && (text[j - 1] == L' ' || text[j - 1] == L'\t' || text[j - 1] == L'\r' || text[j - 1] == L'\n')) {
        --j;
    }
    selfClosing = j > pos && text[j - 1] == L'/';
    return true;
}

size_t FindMatchingTopLevelEnd(const std::wstring& text, size_t startPos, const std::wstring& tagName)
{
    std::wstring name;
    bool closing = false;
    bool selfClosing = false;
    size_t tagEnd = std::wstring::npos;
    if (!TryReadXhtmlTag(text, startPos, name, closing, selfClosing, tagEnd)) return tagEnd;
    if (closing || selfClosing) return tagEnd;

    int depth = 1;
    size_t pos = tagEnd + 1;
    while (pos < text.size()) {
        const size_t lt = text.find(L'<', pos);
        if (lt == std::wstring::npos) break;

        std::wstring current;
        bool currentClosing = false;
        bool currentSelfClosing = false;
        size_t currentEnd = std::wstring::npos;
        if (!TryReadXhtmlTag(text, lt, current, currentClosing, currentSelfClosing, currentEnd)) {
            pos = lt + 1;
            continue;
        }

        if (current == tagName) {
            if (currentClosing) {
                --depth;
                if (depth == 0) return currentEnd;
            } else if (!currentSelfClosing) {
                ++depth;
            }
        }
        pos = currentEnd + 1;
    }
    return tagEnd;
}

bool HasVisibleXhtmlText(const std::wstring& fragment)
{
    bool inTag = false;
    for (wchar_t ch : fragment) {
        if (inTag) {
            if (ch == L'>') inTag = false;
            continue;
        }
        if (ch == L'<') {
            inTag = true;
            continue;
        }
        if (ch > L' ' && ch != 0x00A0) return true;
    }
    return false;
}

std::wstring WrapInlineFragmentAsParagraph(const std::wstring& fragment)
{
    if (!HasVisibleXhtmlText(fragment)) return {};
    return L"<p class=\"inline-fragment\">" + fragment + L"</p>\n";
}

std::wstring BlockContentForXhtml(const std::wstring& fragment)
{
    if (fragment.empty()) return {};

    std::wstring out;
    std::wstring inlineChunk;
    size_t pos = 0;
    while (pos < fragment.size()) {
        const size_t lt = fragment.find(L'<', pos);
        if (lt == std::wstring::npos) {
            inlineChunk += fragment.substr(pos);
            break;
        }

        inlineChunk += fragment.substr(pos, lt - pos);

        std::wstring tagName;
        bool closing = false;
        bool selfClosing = false;
        size_t tagEnd = std::wstring::npos;
        if (!TryReadXhtmlTag(fragment, lt, tagName, closing, selfClosing, tagEnd) ||
            closing || !IsXhtmlBlockTagName(tagName)) {
            inlineChunk += fragment[lt];
            pos = lt + 1;
            continue;
        }

        out += WrapInlineFragmentAsParagraph(inlineChunk);
        inlineChunk.clear();

        const size_t blockEnd = selfClosing ? tagEnd : FindMatchingTopLevelEnd(fragment, lt, tagName);
        if (blockEnd == std::wstring::npos || blockEnd < lt) {
            inlineChunk += fragment.substr(lt);
            break;
        }
        out += fragment.substr(lt, blockEnd - lt + 1);
        if (out.empty() || out.back() != L'\n') out += L'\n';
        pos = blockEnd + 1;
    }

    out += WrapInlineFragmentAsParagraph(inlineChunk);
    return out;
}

std::wstring WrapInlineOrBlock(const std::wstring& inlineTag,
                               const std::wstring& blockClass,
                               const std::wstring& commonAttrs,
                               const std::wstring& body)
{
    if (ContainsBlockXhtml(body)) {
        return L"<div" + commonAttrs + ClassAttr(blockClass) + L">\n" + BlockContentForXhtml(body) + L"</div>\n";
    }
    return L"<" + inlineTag + commonAttrs + L">" + body + L"</" + inlineTag + L">";
}

std::wstring RenderHeadingOrBlock(const std::wstring& headingTag,
                                  const std::wstring& blockClass,
                                  const std::wstring& attrs,
                                  const std::wstring& body)
{
    if (body.empty()) return {};

    // FB2 sometimes puts images or other block-like content into <subtitle> or
    // into <title><p>...</p></title>. XHTML headings cannot contain <p>, <div>,
    // <table>, etc., so such content must be rendered as a block container.
    if (ContainsBlockXhtml(body)) {
        return L"<div" + attrs + ClassAttr(blockClass) + L">\n" + BlockContentForXhtml(body) + L"</div>\n";
    }

    return L"<" + headingTag + attrs + L">" + body + L"</" + headingTag + L">\n";
}

std::wstring TitleChildAttrs(IXMLDOMNodePtr child,
                             const std::wstring& titleAttrs,
                             bool titleAttrsAlreadyUsed,
                             const XhtmlContext& ctx)
{
    IXMLDOMElementPtr childElement(child);
    const std::wstring childAttrs = childElement ? CommonAttrs(childElement, ctx) : std::wstring();
    if (!childAttrs.empty()) {
        // Prefer the id/lang attributes that belong to the concrete title paragraph.
        // This preserves FB2 anchors like <title><p id="BdToc_...">...</p></title>.
        return childAttrs;
    }

    return titleAttrsAlreadyUsed ? std::wstring() : titleAttrs;
}

std::wstring RewriteHref(const std::wstring& href, const XhtmlContext& ctx)
{
    if (href.empty()) return {};

    if (href[0] == L'#') {
        const std::wstring sourceId = StripFragmentPrefix(href);
        const auto it = ctx.links.find(sourceId);
        if (it == ctx.links.end()) {
            // EPUBCheck reports RSC-012 for unresolved local fragments.
            // Returning an empty href lets the caller render plain text instead of a broken link.
            return {};
        }

        const LinkTarget& target = it->second;
        if (target.fileName == ctx.currentFileName) {
            return L"#" + target.id;
        }
        return target.fileName + L"#" + target.id;
    }

    return SanitizeHrefForXhtml(href);
}

std::wstring IdAttrFromValue(const std::wstring& id)
{
    if (id.empty()) return {};
    return L" id=\"" + fbe::epub::XmlEscape(id) + L"\"";
}

std::wstring GeneratedNoteRefId(const std::wstring& targetId, const XhtmlContext& ctx)
{
    if (targetId.empty() || ctx.noteBacklinks == nullptr || ctx.emittedNoteRefCounters == nullptr) return {};

    const auto it = ctx.noteBacklinks->find(targetId);
    if (it == ctx.noteBacklinks->end()) return {};

    size_t& index = (*ctx.emittedNoteRefCounters)[targetId];
    if (index >= it->second.size()) return {};

    const std::wstring id = it->second[index].id;
    ++index;
    return id;
}

std::wstring BacklinksForNote(const std::wstring& sourceId, const XhtmlContext& ctx)
{
    if (sourceId.empty() || ctx.noteBacklinks == nullptr) return {};

    const auto it = ctx.noteBacklinks->find(sourceId);
    if (it == ctx.noteBacklinks->end() || it->second.empty()) return {};

    const std::vector<LinkTarget>& targets = it->second;

    std::wstring out = L"<p class=\"backlinks\">";
    for (size_t i = 0; i < targets.size(); ++i) {
        const LinkTarget& target = targets[i];
        if (target.id.empty() || target.fileName.empty()) continue;

        const std::wstring href = (target.fileName == ctx.currentFileName)
            ? (L"#" + target.id)
            : (target.fileName + L"#" + target.id);

        if (i > 0) out += L" ";
        out += L"<a" + EpubTypeAttr(ctx, L"backlink") + L" class=\"backlink\" href=\"" +
               fbe::epub::XmlEscape(href) + L"\">&#8617;";
        if (targets.size() > 1) {
            std::wstringstream ss;
            ss << (i + 1);
            out += ss.str();
        }
        out += L"</a>";
    }
    out += L"</p>\n";
    return out;
}

std::wstring ChildrenToXhtml(IXMLDOMNodePtr node, const XhtmlContext& ctx);

std::wstring EmptyLineXhtml(const std::wstring& commonAttrs)
{
    // FB2 empty-line is a block separator. A <div> is safer than a fake
    // paragraph in titles, quotes, poems and XHTML 1.1/EPUB 2 output.
    return L"<div" + commonAttrs + L" class=\"empty-line\">&#160;</div>\n";
}

std::wstring NodeToXhtml(IXMLDOMNodePtr node, const XhtmlContext& ctx)
{
    if (node == nullptr) return {};

    DOMNodeType type = NODE_INVALID;
    CheckHR(node->get_nodeType(&type));
    if (type == NODE_TEXT || type == NODE_CDATA_SECTION) {
        CComBSTR text;
        CheckHR(node->get_text(&text));
        return fbe::epub::XmlEscape(BstrToWString(text));
    }
    if (type != NODE_ELEMENT) return {};

    const std::wstring name = LocalName(node);
    IXMLDOMElementPtr element(node);
    const std::wstring commonAttrs = CommonAttrs(element, ctx);
    if (name == L"binary") return {};

    if (name == L"p") {
        std::wstring body = ChildrenToXhtml(node, ctx);
        if (ContainsBlockXhtml(body)) {
            return L"<div" + commonAttrs + L" class=\"paragraph\">\n" + BlockContentForXhtml(body) + L"</div>\n";
        }
        if (body.empty()) {
            body = L"&#160;";
        }
        const bool imageOnlyParagraph = HasDescendantElementNamed(node, L"image") && NormalizeWhitespace(NodeText(node)).empty();
        return L"<p" + commonAttrs + (imageOnlyParagraph ? L" class=\"image\"" : L"") + L">" + body + L"</p>\n";
    }
    if (name == L"subtitle") {
        const std::wstring body = ChildrenToXhtml(node, ctx);
        if (body.empty()) return {};
        // In FB2 subtitle is context-sensitive. Inside poems/stanzas it is a
        // verse-like line; directly inside <title> it behaves like a heading;
        // otherwise use a block container when the content itself is block-like.
        if (IsInsidePoemLikeBlock(node)) {
            return L"<p" + commonAttrs + L" class=\"subtitle poem-subtitle\">" + body + L"</p>\n";
        }
        if (HasDirectParentElementNamed(node, L"title")) {
            return RenderHeadingOrBlock(L"h3", L"subtitle", commonAttrs, body);
        }
        if (ContainsBlockXhtml(body)) {
            return L"<div" + commonAttrs + L" class=\"subtitle\">\n" + BlockContentForXhtml(body) + L"</div>\n";
        }
        return L"<p" + commonAttrs + L" class=\"subtitle\">" + body + L"</p>\n";
    }
    if (name == L"title") {
        // FB2 title usually contains one or more <p> elements. Do not flatten it with
        // NodeText(): that would drop inline markup and, more importantly, note anchors
        // inside titles. Render title paragraphs as headings, but render only their
        // inline children so XHTML never gets <p> inside <h2>.
        IXMLDOMNodeListPtr titleChildren;
        CheckHR(node->get_childNodes(&titleChildren));
        long titleLen = 0;
        CheckHR(titleChildren->get_length(&titleLen));

        std::wstring out;
        bool usedTitleAttrs = false;
        for (long i = 0; i < titleLen; ++i) {
            IXMLDOMNodePtr child;
            CheckHR(titleChildren->get_item(i, &child));
            if (child == nullptr) continue;

            DOMNodeType childType = NODE_INVALID;
            CheckHR(child->get_nodeType(&childType));

            if (childType == NODE_ELEMENT && LocalName(child) == L"p") {
                const std::wstring attrs = TitleChildAttrs(child, commonAttrs, usedTitleAttrs, ctx);
                out += RenderHeadingOrBlock(L"h2", L"title", attrs, ChildrenToXhtml(child, ctx));
                usedTitleAttrs = true;
                continue;
            }

            if (childType == NODE_ELEMENT && LocalName(child) == L"empty-line") {
                continue;
            }

            const std::wstring part = NodeToXhtml(child, ctx);
            if (!part.empty()) {
                const std::wstring attrs = TitleChildAttrs(child, commonAttrs, usedTitleAttrs, ctx);
                out += RenderHeadingOrBlock(L"h2", L"title", attrs, part);
                usedTitleAttrs = true;
            }
        }

        if (!out.empty()) return out;

        const std::wstring title = NodeText(node);
        if (title.empty()) return {};
        return RenderHeadingOrBlock(L"h2", L"title", commonAttrs, fbe::epub::XmlEscape(title));
    }
    if (name == L"empty-line") {
        return EmptyLineXhtml(commonAttrs);
    }
    if (name == L"date") {
        const std::wstring value = GetAttr(element, L"value");
        std::wstring body = ChildrenToXhtml(node, ctx);
        if (body.empty()) body = fbe::epub::XmlEscape(value);
        if (ContainsBlockXhtml(body)) {
            return L"<div" + commonAttrs + L" class=\"date\">\n" + BlockContentForXhtml(body) + L"</div>\n";
        }
        if (ctx.isEpub3 && !value.empty()) {
            const std::wstring datetime = L" datetime=\"" + fbe::epub::XmlEscape(value) + L"\"";
            return L"<time" + commonAttrs + datetime + L">" + body + L"</time>";
        }
        return L"<span" + commonAttrs + L" class=\"date\">" + body + L"</span>";
    }
    if (name == L"style") {
        const std::wstring cls = CssClassNameFromFb2Style(GetAttr(element, L"name"));
        const std::wstring body = ChildrenToXhtml(node, ctx);
        if (ContainsBlockXhtml(body)) return L"<div" + commonAttrs + ClassAttr(cls) + L">\n" + BlockContentForXhtml(body) + L"</div>\n";
        return L"<span" + commonAttrs + ClassAttr(cls) + L">" + body + L"</span>";
    }
    if (name == L"email") {
        std::wstring body = ChildrenToXhtml(node, ctx);
        const std::wstring address = NormalizeWhitespace(NodeText(node));
        if (body.empty()) body = fbe::epub::XmlEscape(address);
        if (address.empty()) return L"<span" + commonAttrs + L" class=\"email\">" + body + L"</span>";
        return L"<a" + commonAttrs + L" class=\"email\" href=\"mailto:" + fbe::epub::XmlEscape(address) + L"\">" + body + L"</a>";
    }
    if (name == L"home-page") {
        std::wstring body = ChildrenToXhtml(node, ctx);
        const std::wstring url = NormalizeWhitespace(NodeText(node));
        if (body.empty()) body = fbe::epub::XmlEscape(url);
        if (url.empty()) return L"<span" + commonAttrs + L" class=\"home-page\">" + body + L"</span>";
        const std::wstring safeUrl = SanitizeHrefForXhtml(url);
        if (safeUrl.empty()) return L"<span" + commonAttrs + L" class=\"home-page invalid-url\">" + body + L"</span>";
        if (ContainsBlockXhtml(body)) {
            return L"<div" + commonAttrs + L" class=\"home-page link-block\">\n" +
                   BlockContentForXhtml(body) + L"</div>\n";
        }
        return L"<a" + commonAttrs + L" class=\"home-page\" href=\"" + fbe::epub::XmlEscape(safeUrl) + L"\">" + body + L"</a>";
    }
    if (name == L"emphasis") {
        return WrapInlineOrBlock(L"em", L"emphasis", commonAttrs, ChildrenToXhtml(node, ctx));
    }
    if (name == L"strong") {
        return WrapInlineOrBlock(L"strong", L"strong", commonAttrs, ChildrenToXhtml(node, ctx));
    }
    if (name == L"sub") {
        return WrapInlineOrBlock(L"sub", L"sub", commonAttrs, ChildrenToXhtml(node, ctx));
    }
    if (name == L"sup") {
        return WrapInlineOrBlock(L"sup", L"sup", commonAttrs, ChildrenToXhtml(node, ctx));
    }
    if (name == L"code") {
        const std::wstring body = ChildrenToXhtml(node, ctx);
        if (ContainsBlockXhtml(body)) {
            return L"<div" + commonAttrs + L" class=\"code code-block\">\n" + BlockContentForXhtml(body) + L"</div>\n";
        }
        return L"<code" + commonAttrs + L" class=\"code\">" + body + L"</code>";
    }
    if (name == L"strikethrough") {
        const std::wstring body = ChildrenToXhtml(node, ctx);
        if (ContainsBlockXhtml(body)) return L"<div" + commonAttrs + L" class=\"strikethrough\">\n" + BlockContentForXhtml(body) + L"</div>\n";
        return L"<span" + commonAttrs + L" class=\"strikethrough\">" + body + L"</span>";
    }
    if (name == L"a") {
        const std::wstring body = ChildrenToXhtml(node, ctx);
        const std::wstring rawHref = GetHrefAttr(element);
        const std::wstring href = RewriteHref(rawHref, ctx);
        if (href.empty()) {
            if (ContainsBlockXhtml(body)) {
                return L"<div" + commonAttrs + L" class=\"broken-link\">\n" +
                       BlockContentForXhtml(body) + L"</div>\n";
            }
            return L"<span" + commonAttrs + L" class=\"broken-link\">" + body + L"</span>";
        }

        if (ContainsBlockXhtml(body)) {
            // XHTML 1.1 and EPUBCheck do not allow blockquote/div/p inside <a>.
            // Keep the visible block content valid instead of producing invalid EPUB.
            return L"<div" + commonAttrs + L" class=\"link-block\">\n" +
                   BlockContentForXhtml(body) + L"</div>\n";
        }

        std::wstring typeAttr;
        std::wstring classAttr;
        std::wstring generatedIdAttr;
        const std::wstring fbType = GetAttr(element, L"type");
        if (fbType == L"note") {
            classAttr = L" class=\"noteref\"";
            if (ctx.isEpub3) {
                typeAttr = EpubTypeAttr(ctx, L"noteref");
            }

            if (rawHref.size() > 1 && rawHref[0] == L'#' && commonAttrs.find(L" id=") == std::wstring::npos) {
                generatedIdAttr = IdAttrFromValue(GeneratedNoteRefId(StripFragmentPrefix(rawHref), ctx));
            }
        }
        return L"<a" + commonAttrs + generatedIdAttr + typeAttr + classAttr + L" href=\"" +
               fbe::epub::XmlEscape(href) + L"\">" + body + L"</a>";
    }
    if (name == L"image") {
        std::wstring id = StripFragmentPrefix(GetHrefAttr(element));
        auto it = ctx.images.find(id);
        if (it == ctx.images.end()) return {};

        const std::wstring src = fbe::epub::XmlEscape(it->second.fileName);

        // FB2 allows <image> not only directly inside <p>, but also inside inline
        // wrappers nested in a paragraph, for example <p><emphasis><image/>...</emphasis></p>.
        // In all such cases XHTML/EPUB must get an inline <img>, not a nested <p>.
        if (HasAncestorElementNamed(node, L"p")) {
            return L"<img" + commonAttrs + ImageAltAttr(element) + L" src=\"../images/" + src + L"\"/>";
        }

        return L"<p" + commonAttrs + L" class=\"image\"><img" + ImageAltAttr(element) + L" src=\"../images/" +
               src + L"\"/></p>\n";
    }
    if (name == L"section") {
        const std::wstring children = ChildrenToXhtml(node, ctx);
        if (ctx.notesChapter) {
            const std::wstring backlinks = BacklinksForNote(StripFragmentPrefix(GetAttr(element, L"id")), ctx);
            if (ctx.isEpub3) {
                return L"<aside" + commonAttrs + EpubTypeAttr(ctx, L"endnote") + L" class=\"section note\">\n" +
                       BlockContentForXhtml(children + backlinks) + L"</aside>\n";
            }
            return L"<div" + commonAttrs + L" class=\"section note\">\n" +
                   BlockContentForXhtml(children + backlinks) + L"</div>\n";
        }
        return L"<div" + commonAttrs + L" class=\"section\">\n" + BlockContentForXhtml(children) + L"</div>\n";
    }
    if (name == L"poem") {
        return L"<div" + commonAttrs + L" class=\"poem\">\n" +
               BlockContentForXhtml(ChildrenToXhtml(node, ctx)) + L"</div>\n";
    }
    if (name == L"stanza") {
        return L"<div" + commonAttrs + L" class=\"stanza\">\n" +
               BlockContentForXhtml(ChildrenToXhtml(node, ctx)) + L"</div>\n";
    }
    if (name == L"v") {
        return L"<p" + commonAttrs + L" class=\"verse\">" + ChildrenToXhtml(node, ctx) + L"</p>\n";
    }
    if (name == L"epigraph") {
        return L"<blockquote" + commonAttrs + EpubTypeAttr(ctx, L"epigraph") + L" class=\"epigraph\">\n" +
               BlockContentForXhtml(ChildrenToXhtml(node, ctx)) + L"</blockquote>\n";
    }
    if (name == L"cite") {
        return L"<blockquote" + commonAttrs + L" class=\"cite\">\n" +
               BlockContentForXhtml(ChildrenToXhtml(node, ctx)) + L"</blockquote>\n";
    }
    if (name == L"text-author") {
        const std::wstring body = ChildrenToXhtml(node, ctx);
        if (HasAncestorElementNamed(node, L"epigraph") || HasAncestorElementNamed(node, L"cite")) {
            return L"<div" + commonAttrs + L" class=\"text-author\">" + body + L"</div>\n";
        }
        return L"<p" + commonAttrs + L" class=\"text-author\">" + body + L"</p>\n";
    }
    if (name == L"table" || name == L"tr") {
        return L"<" + name + commonAttrs + L">" + ChildrenToXhtml(node, ctx) + L"</" + name + L">\n";
    }
    if (name == L"td" || name == L"th") {
        std::wstring body = ChildrenToXhtml(node, ctx);
        if (body.empty()) body = L"&#160;";
        return L"<" + name + commonAttrs + TableCellAttrs(element, ctx) + L">" + body + L"</" + name + L">\n";
    }

    // Unknown FB2 element: keep its text/children, but do not leak unsupported tag names into XHTML.
    return ChildrenToXhtml(node, ctx);
}

bool IsCyrillicLetter(wchar_t ch)
{
    return (ch >= 0x0400 && ch <= 0x052F) || (ch >= 0x2DE0 && ch <= 0x2DFF) ||
           (ch >= 0xA640 && ch <= 0xA69F);
}

bool IsLatinExtendedLetter(wchar_t ch)
{
    return (ch >= 0x00C0 && ch <= 0x024F) || (ch >= 0x1E00 && ch <= 0x1EFF);
}

bool IsWordLikeChar(wchar_t ch)
{
    return IsAsciiLetter(ch) || IsAsciiDigit(ch) || IsCyrillicLetter(ch) ||
           IsLatinExtendedLetter(ch) || ch == L'_';
}

bool IsOpeningPunctuationThatUsuallyNeedsSpace(wchar_t ch)
{
    return ch == L'(' || ch == L'[' || ch == L'{' || ch == 0x00AB /* « */;
}

bool IsNoSpaceBeforeChar(wchar_t ch)
{
    return ch == L',' || ch == L'.' || ch == L':' || ch == L';' || ch == L'!' || ch == L'?' ||
           ch == L')' || ch == L']' || ch == L'}' || ch == 0x00BB /* » */ || ch == L'%' ||
           ch == 0x2026 /* … */;
}

// Returns the first/last visible character of an XHTML fragment. Tags are skipped.
// The helper is intentionally conservative: it is used only to restore spaces that
// can be lost when FBE splits a word sequence into differently formatted runs.
bool FirstVisibleChar(const std::wstring& s, wchar_t& ch)
{
    bool inTag = false;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (inTag) {
            if (c == L'>') inTag = false;
            continue;
        }
        if (c == L'<') {
            inTag = true;
            continue;
        }
        if (c == L'&') {
            const size_t semicolon = s.find(L';', i + 1);
            if (semicolon != std::wstring::npos) {
                std::wstring entity = s.substr(i, semicolon - i + 1);
                if (entity == L"&nbsp;" || entity == L"&#160;" || entity == L"&#xA0;" || entity == L"&#xa0;") {
                    ch = L' ';
                } else {
                    ch = L'X';
                }
                return true;
            }
        }
        ch = c;
        return true;
    }
    return false;
}

bool LastVisibleChar(const std::wstring& s, wchar_t& ch)
{
    bool inTag = false;
    for (size_t i = s.size(); i > 0; --i) {
        wchar_t c = s[i - 1];
        if (inTag) {
            if (c == L'<') inTag = false;
            continue;
        }
        if (c == L'>') {
            inTag = true;
            continue;
        }
        if (c == L';') {
            const size_t amp = s.rfind(L'&', i - 1);
            if (amp != std::wstring::npos) {
                const size_t lt = s.rfind(L'<', i - 1);
                const size_t gt = s.rfind(L'>', i - 1);
                if (lt == std::wstring::npos || (gt != std::wstring::npos && gt > lt)) {
                    std::wstring entity = s.substr(amp, i - amp);
                    if (entity == L"&nbsp;" || entity == L"&#160;" || entity == L"&#xA0;" || entity == L"&#xa0;") {
                        ch = L' ';
                    } else {
                        ch = L'X';
                    }
                    return true;
                }
            }
        }
        ch = c;
        return true;
    }
    return false;
}

bool IsNumericOrBracketedNoteLink(const std::wstring& fragment, const std::wstring& childName)
{
    if (childName != L"a") return false;

    wchar_t first = 0;
    if (!FirstVisibleChar(fragment, first)) return false;
    return IsAsciiDigit(first) || first == L'[' || first == L'(';
}

bool StartsWithElementName(const std::wstring& fragment, const wchar_t* tagName)
{
    size_t i = 0;
    while (i < fragment.size() && (fragment[i] == L' ' || fragment[i] == L'\n' || fragment[i] == L'\t' || fragment[i] == L'\r')) {
        ++i;
    }

    const std::wstring prefix = std::wstring(L"<") + tagName;
    if (fragment.compare(i, prefix.size(), prefix) != 0) return false;

    const size_t next = i + prefix.size();
    return next >= fragment.size() || fragment[next] == L'>' || fragment[next] == L' ' ||
           fragment[next] == L'\n' || fragment[next] == L'\t' || fragment[next] == L'\r';
}

bool EndsWithClosingElementName(const std::wstring& fragment, const wchar_t* tagName)
{
    size_t end = fragment.size();
    while (end > 0 && (fragment[end - 1] == L' ' || fragment[end - 1] == L'\n' ||
                       fragment[end - 1] == L'\t' || fragment[end - 1] == L'\r')) {
        --end;
    }

    const std::wstring suffix = std::wstring(L"</") + tagName + L">";
    return end >= suffix.size() && fragment.compare(end - suffix.size(), suffix.size(), suffix) == 0;
}

bool IsSubOrSupBoundary(const std::wstring& left, const std::wstring& right, const std::wstring& childName)
{
    // FB2 often uses sub/sup inside formulas: H<sub>2</sub>O, x<sup>2</sup>.
    // MSXML may drop insignificant spaces around inline nodes, but we must not
    // invent spaces at subscript/superscript boundaries.
    if (childName == L"sub" || childName == L"sup") return true;
    if (StartsWithElementName(right, L"sub") || StartsWithElementName(right, L"sup")) return true;
    if (EndsWithClosingElementName(left, L"sub") || EndsWithClosingElementName(left, L"sup")) return true;
    return false;
}

bool IsPunctuationThatUsuallyNeedsFollowingSpace(wchar_t ch)
{
    return ch == L',' || ch == L'.' || ch == L':' || ch == L';' || ch == L'!' ||
           ch == L'?' || ch == 0x2026 /* … */;
}

bool NeedsSyntheticSpace(const std::wstring& left, const std::wstring& right, const std::wstring& childName)
{
    if (left.empty() || right.empty()) return false;

    wchar_t last = 0;
    wchar_t first = 0;
    if (!LastVisibleChar(left, last) || !FirstVisibleChar(right, first)) return false;

    if (last == L' ' || last == L'\n' || last == L'\t' || first == L' ' || first == L'\n' || first == L'\t') return false;
    if (IsNoSpaceBeforeChar(first)) return false;
    if (last == L'-' || last == 0x2010 || last == 0x2011 || last == 0x2012 || last == 0x2013) return false;
    if (first == L'-' || first == 0x2010 || first == 0x2011 || first == 0x2012 || first == 0x2013) return false;
    if (IsSubOrSupBoundary(left, right, childName)) return false;

    // Do not add a guessed space before note reference markers like [1] or 1.
    // If the source FB2 has a real space before the link, it is preserved by text nodes.
    if (IsNumericOrBracketedNoteLink(right, childName)) return false;

    if (IsWordLikeChar(last) && IsWordLikeChar(first)) return true;
    if (IsWordLikeChar(last) && IsOpeningPunctuationThatUsuallyNeedsSpace(first)) return true;
    if ((last == L')' || last == L']' || last == L'}' || last == 0x00BB) && IsWordLikeChar(first)) return true;
    if (IsPunctuationThatUsuallyNeedsFollowingSpace(last) &&
        (IsWordLikeChar(first) || IsOpeningPunctuationThatUsuallyNeedsSpace(first) || first == 0x00AB /* « */)) return true;

    return false;
}

std::wstring ChildrenToXhtml(IXMLDOMNodePtr node, const XhtmlContext& ctx)
{
    IXMLDOMNodeListPtr children;
    CheckHR(node->get_childNodes(&children));
    long length = 0;
    CheckHR(children->get_length(&length));

    std::wstring out;
    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr child;
        CheckHR(children->get_item(i, &child));

        const std::wstring part = NodeToXhtml(child, ctx);
        if (part.empty()) continue;

        std::wstring childName;
        DOMNodeType childType = NODE_INVALID;
        if (SUCCEEDED(child->get_nodeType(&childType)) && childType == NODE_ELEMENT) {
            childName = LocalName(child);
        }

        if (NeedsSyntheticSpace(out, part, childName)) {
            out += L' ';
        }
        out += part;
    }
    return out;
}

void CollectIdsFromNode(IXMLDOMNodePtr node,
                        const std::wstring& fileName,
                        LinkMap& links,
                        std::set<std::wstring>& usedIds)
{
    if (node == nullptr) return;

    DOMNodeType type = NODE_INVALID;
    CheckHR(node->get_nodeType(&type));
    if (type == NODE_ELEMENT) {
        IXMLDOMElementPtr element(node);
        const std::wstring sourceId = StripFragmentPrefix(GetAttr(element, L"id"));
        if (!sourceId.empty() && links.find(sourceId) == links.end()) {
            LinkTarget target;
            target.fileName = fileName;
            target.id = UniqueXmlId(MakeSafeXmlId(sourceId), usedIds);
            links[sourceId] = target;
        }
    }

    IXMLDOMNodeListPtr children;
    CheckHR(node->get_childNodes(&children));
    long length = 0;
    CheckHR(children->get_length(&length));
    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr child;
        CheckHR(children->get_item(i, &child));
        CollectIdsFromNode(child, fileName, links, usedIds);
    }
}

void CollectNoteBacklinksFromNode(IXMLDOMNodePtr node,
                                  const std::wstring& fileName,
                                  const LinkMap& links,
                                  NoteBacklinkMap& noteBacklinks,
                                  std::set<std::wstring>& usedIds)
{
    if (node == nullptr) return;

    DOMNodeType type = NODE_INVALID;
    CheckHR(node->get_nodeType(&type));
    if (type == NODE_ELEMENT) {
        IXMLDOMElementPtr element(node);
        if (element != nullptr && LocalName(node) == L"a" && GetAttr(element, L"type") == L"note") {
            const std::wstring targetId = StripFragmentPrefix(GetHrefAttr(element));
            if (!targetId.empty() && links.find(targetId) != links.end()) {
                LinkTarget back;
                back.fileName = fileName;

                const std::wstring sourceId = StripFragmentPrefix(GetAttr(element, L"id"));
                const auto existingSource = links.find(sourceId);
                if (!sourceId.empty() && existingSource != links.end()) {
                    back.id = existingSource->second.id;
                } else {
                    std::wstringstream ss;
                    ss << L"noteref_" << MakeSafeXmlId(targetId) << L"_" << (noteBacklinks[targetId].size() + 1);
                    back.id = UniqueXmlId(ss.str(), usedIds);
                }

                noteBacklinks[targetId].push_back(back);
            }
        }
    }

    IXMLDOMNodeListPtr children;
    CheckHR(node->get_childNodes(&children));
    long length = 0;
    CheckHR(children->get_length(&length));
    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr child;
        CheckHR(children->get_item(i, &child));
        CollectNoteBacklinksFromNode(child, fileName, links, noteBacklinks, usedIds);
    }
}


std::wstring OebpsRootHref(const LinkTarget& target)
{
    if (target.fileName.empty()) return {};
    std::wstring href = L"text/" + target.fileName;
    if (!target.id.empty()) href += L"#" + target.id;
    return href;
}

std::wstring FirstShortParagraphText(IXMLDOMNodePtr node, std::size_t maxLen = 80)
{
    std::wstring text = SelectText(node, L"fb:p[1]");
    if (text.size() > maxLen) {
        text.resize(maxLen);
        text += L"…";
    }
    return text;
}

std::wstring SectionNavTitle(IXMLDOMNodePtr section)
{
    std::wstring title = SelectText(section, L"fb:title");
    if (title.empty()) title = SelectText(section, L"fb:subtitle[1]");
    if (title.empty()) title = FirstShortParagraphText(section);
    return title;
}

void BuildSectionNavChildren(IXMLDOMNodePtr parent,
                             const LinkMap& links,
                             std::vector<fbe::epub::EpubNavPoint>& out)
{
    if (parent == nullptr) return;

    IXMLDOMNodeListPtr sections;
    CheckHR(parent->selectNodes(bstr_t(L"fb:section"), &sections));
    long length = 0;
    CheckHR(sections->get_length(&length));

    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr section;
        CheckHR(sections->get_item(i, &section));

        std::vector<fbe::epub::EpubNavPoint> children;
        BuildSectionNavChildren(section, links, children);

        IXMLDOMElementPtr element(section);
        const std::wstring sourceId = StripFragmentPrefix(GetAttr(element, L"id"));
        const auto it = links.find(sourceId);

        if (!sourceId.empty() && it != links.end()) {
            fbe::epub::EpubNavPoint point;
            point.title = SectionNavTitle(section);
            if (point.title.empty()) point.title = sourceId;
            point.href = OebpsRootHref(it->second);
            point.epubType = L"section";
            point.children = std::move(children);
            out.push_back(std::move(point));
        } else {
            // If the FB2 subsection has no id, it cannot be linked reliably from nav.xhtml/NCX.
            // Keep any deeper id-bearing subsections instead of creating duplicate chapter links.
            out.insert(out.end(), children.begin(), children.end());
        }
    }
}

std::wstring ChapterAnchorXhtml(IXMLDOMNodePtr chapterRoot, const LinkMap& links)
{
    IXMLDOMElementPtr element(chapterRoot);
    if (element == nullptr) return {};

    const std::wstring sourceId = StripFragmentPrefix(GetAttr(element, L"id"));
    if (sourceId.empty()) return {};

    const auto it = links.find(sourceId);
    if (it == links.end() || it->second.id.empty()) return {};

    // Top-level FB2 sections are exported as separate XHTML files by rendering
    // their children only. Without this explicit anchor, links to the top-level
    // section keep a #fragment that is never emitted to XHTML.
    return L"<div id=\"" + fbe::epub::XmlEscape(it->second.id) + L"\" class=\"chapter-anchor\"></div>\n";
}

void SetupNamespaces(IXMLDOMDocument2Ptr doc)
{
    CheckHR(doc->setProperty(
        bstr_t(L"SelectionNamespaces"),
        _variant_t(L"xmlns:fb='http://www.gribuser.ru/xml/fictionbook/2.0' xmlns:l='http://www.w3.org/1999/xlink' xmlns:xlink='http://www.w3.org/1999/xlink'")
    ));
    CheckHR(doc->setProperty(bstr_t(L"SelectionLanguage"), _variant_t(L"XPath")));
}

std::wstring PersonText(IXMLDOMNodePtr person)
{
    if (person == nullptr) return {};

    const std::wstring first = SelectText(person, L"fb:first-name");
    const std::wstring middle = SelectText(person, L"fb:middle-name");
    const std::wstring last = SelectText(person, L"fb:last-name");

    std::wstring one;
    auto add = [&one](const std::wstring& s) {
        if (!s.empty()) {
            if (!one.empty()) one += L" ";
            one += s;
        }
    };
    add(first); add(middle); add(last);
    if (one.empty()) one = SelectText(person, L"fb:nickname");
    if (one.empty()) one = NodeText(person);
    return one;
}

std::vector<std::wstring> PersonList(IXMLDOMDocument2Ptr doc, const wchar_t* xpath)
{
    std::vector<std::wstring> result;
    IXMLDOMNodeListPtr people;
    CheckHR(doc->selectNodes(bstr_t(xpath), &people));
    long length = 0;
    CheckHR(people->get_length(&length));

    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr p;
        CheckHR(people->get_item(i, &p));
        const std::wstring one = PersonText(p);
        if (!one.empty()) result.push_back(one);
    }
    return result;
}

std::wstring JoinList(const std::vector<std::wstring>& items, const wchar_t* sep)
{
    std::wstring result;
    for (const auto& item : items) {
        if (item.empty()) continue;
        if (!result.empty()) result += sep;
        result += item;
    }
    return result;
}

std::wstring AuthorText(IXMLDOMDocument2Ptr doc)
{
    return JoinList(PersonList(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:author"), L", ");
}

std::vector<std::wstring> TextList(IXMLDOMDocument2Ptr doc, const wchar_t* xpath)
{
    std::vector<std::wstring> result;
    IXMLDOMNodeListPtr nodes;
    CheckHR(doc->selectNodes(bstr_t(xpath), &nodes));
    long length = 0;
    CheckHR(nodes->get_length(&length));
    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr node;
        CheckHR(nodes->get_item(i, &node));
        const std::wstring text = NodeText(node);
        if (!text.empty()) result.push_back(text);
    }
    return result;
}

std::vector<std::wstring> SplitKeywords(const std::wstring& value)
{
    std::vector<std::wstring> result;
    std::wstring current;
    auto flush = [&]() {
        current = NormalizeWhitespace(current);
        if (!current.empty()) result.push_back(current);
        current.clear();
    };

    for (wchar_t ch : value) {
        if (ch == L',' || ch == L';') {
            flush();
        } else {
            current.push_back(ch);
        }
    }
    flush();
    return result;
}

std::wstring FirstHrefFromCoverPage(IXMLDOMDocument2Ptr doc)
{
    IXMLDOMNodePtr coverImage;
    CheckHR(doc->selectSingleNode(bstr_t(L"/fb:FictionBook/fb:description/fb:title-info/fb:coverpage/fb:image[1]"), &coverImage));
    IXMLDOMElementPtr element(coverImage);
    if (element == nullptr) return {};
    return StripFragmentPrefix(GetHrefAttr(element));
}

void ApplyCoverMetadata(IXMLDOMDocument2Ptr doc, const ImageMap& images, fbe::epub::EpubBook& book)
{
    const std::wstring coverId = FirstHrefFromCoverPage(doc);
    if (coverId.empty()) return;

    const auto it = images.find(coverId);
    if (it == images.end()) return;

    book.coverImageId = it->second.resourceId;
}

void ApplySequenceMetadata(IXMLDOMDocument2Ptr doc, fbe::epub::EpubBook& book)
{
    IXMLDOMNodeListPtr sequences;
    CheckHR(doc->selectNodes(bstr_t(L"/fb:FictionBook/fb:description/fb:title-info/fb:sequence"), &sequences));
    long length = 0;
    CheckHR(sequences->get_length(&length));

    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr sequence;
        CheckHR(sequences->get_item(i, &sequence));
        IXMLDOMElementPtr element(sequence);
        if (element == nullptr) continue;

        fbe::epub::EpubCollection collection;
        collection.name = GetAttr(element, L"name");
        collection.number = GetAttr(element, L"number");
        if (collection.name.empty()) continue;

        if (book.seriesName.empty()) {
            book.seriesName = collection.name;
            book.seriesNumber = collection.number;
        }
        book.collections.push_back(collection);
    }
}

ImageMap AddResourcesFromBinaries(IXMLDOMDocument2Ptr doc, fbe::epub::EpubBook& book)
{
    ImageMap images;
    std::set<std::wstring> usedNames;

    IXMLDOMNodeListPtr bins;
    CheckHR(doc->selectNodes(bstr_t(L"/fb:FictionBook/fb:binary"), &bins));
    long length = 0;
    CheckHR(bins->get_length(&length));

    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr node;
        CheckHR(bins->get_item(i, &node));
        IXMLDOMElementPtr element(node);
        if (element == nullptr) continue;

        const std::wstring id = GetAttr(element, L"id");
        std::wstring contentTypeW = NormalizeMimeType(GetAttr(element, L"content-type"));
        if (id.empty()) continue;

        auto data = BinaryDataFromNode(node);
        if (data.empty()) continue;

        const std::wstring detectedType = DetectImageMimeType(data);
        if (!detectedType.empty()) {
            // Prefer the real binary signature over the declared FB2 content-type.
            contentTypeW = detectedType;
        }
        if (contentTypeW.empty() || contentTypeW.rfind(L"image/", 0) != 0) continue;

        std::wstring fileName = SanitizeFileName(id);
        if (!ExtensionMatchesMime(fileName, contentTypeW)) {
            fileName = ReplaceOrAppendExtensionForMime(fileName, contentTypeW);
        }
        fileName = UniqueFileName(fileName, usedNames);

        fbe::epub::EpubResource res;
        res.id = IdFromIndex(L"img", static_cast<size_t>(book.resources.size() + 1));
        res.href = L"images/" + fileName;
        res.mediaType = fbe::epub::Utf8FromWide(contentTypeW);
        if (contentTypeW == L"image/svg+xml") {
            res.properties = L"svg";
        }

        ImageInfo info;
        info.fileName = std::move(fileName);
        info.resourceId = res.id;
        info.mediaType = res.mediaType;

        res.data = std::move(data);
        book.resources.push_back(std::move(res));

        images[StripFragmentPrefix(id)] = info;
    }
    return images;
}

std::wstring DefaultCss()
{
    return LR"CSS(body {
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
  margin: 0.75em 0;
  text-align: center;
  text-indent: 0;
  font-weight: bold;
}
.subtitle {
  font-size: 1.05em;
}
img {
  max-width: 100%;
  height: auto;
}
.cover {
  text-align: center;
  page-break-after: always;
}
.cover img, .cover object {
  max-width: 100%;
  max-height: 95%;
}
.image {
  text-align: center;
  text-indent: 0;
}
.empty-line {
  height: 1em;
  margin: 0;
  padding: 0;
  line-height: 1em;
}
.annotation {
  margin: 1em 0;
}
.annotation p,
.epigraph p,
.cite p,
.poem p,
.stanza p,
.table p,
.image p {
  text-indent: 0;
}
.poem {
  margin: 1em 0 1em 2em;
  text-align: left;
}
.stanza {
  margin: 0.75em 0;
}
.verse {
  margin: 0;
  text-align: left;
  text-indent: 0;
}
.poem-subtitle {
  margin: 0.75em 0 0.25em 0;
  text-align: left;
  text-indent: 0;
  font-weight: bold;
}
.epigraph, .cite {
  margin: 1.2em 2em;
  text-indent: 0;
}
.epigraph {
  font-style: italic;
}
.text-author {
  text-align: right;
  font-style: normal;
  text-indent: 0;
  margin-top: 0.5em;
}
.paragraph {
  margin: 0 0 0.75em 0;
}
.emphasis {
  font-style: italic;
}
.strong {
  font-weight: bold;
}
code, .code {
  font-family: monospace;
  white-space: pre-wrap;
}
.sub {
  vertical-align: sub;
  font-size: 0.8em;
}
.sup, .noteref {
  vertical-align: super;
  font-size: 0.8em;
}
.noteref {
  text-decoration: none;
  white-space: nowrap;
}
.note {
  font-size: 0.95em;
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
th {
  font-weight: bold;
}
.date {
  white-space: nowrap;
}
.strikethrough {
  text-decoration: line-through;
}
.code-block p {
  text-indent: 0;
  text-align: left;
}
.titlepage {
  margin: 2em 0;
  text-align: center;
  page-break-after: always;
}
.titlepage-title {
  margin: 1.5em 0 0.75em 0;
}
.titlepage-author {
  margin: 0.5em 0 1.5em 0;
  font-size: 1.1em;
  text-indent: 0;
  text-align: center;
}
.titlepage-meta {
  text-indent: 0;
  text-align: center;
}
.titlepage-label {
  font-weight: bold;
}
.titlepage-description {
  margin: 1.5em 2em 0 2em;
  text-align: left;
}
.titlepage-description p {
  text-align: left;
  text-indent: 0;
}
.broken-link, .invalid-url {
  color: inherit;
}
.link-block {
  margin: 0 0 0.75em 0;
}
.inline-fragment {
  margin: 0 0 0.75em 0;
}
)CSS";
}

std::wstring Fb2StylesheetsCss(IXMLDOMDocument2Ptr doc)
{
    std::wstring css;
    IXMLDOMNodeListPtr stylesheets;
    CheckHR(doc->selectNodes(bstr_t(L"/fb:FictionBook/fb:stylesheet"), &stylesheets));
    long length = 0;
    CheckHR(stylesheets->get_length(&length));

    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr node;
        CheckHR(stylesheets->get_item(i, &node));
        IXMLDOMElementPtr element(node);
        const std::wstring type = element ? GetAttr(element, L"type") : std::wstring();
        if (!type.empty() && type.find(L"css") == std::wstring::npos) continue;
        CComBSTR textBstr;
        CheckHR(node->get_text(&textBstr));
        const std::wstring text = BstrToWString(textBstr);
        if (NormalizeWhitespace(text).empty()) continue;
        css += L"\n/* FB2 stylesheet */\n";
        css += text;
        if (!css.empty() && css.back() != L'\n') css += L"\n";
    }
    return css;
}


bool IsHexDigit(wchar_t ch)
{
    return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F');
}

bool LooksLikeUuid(const std::wstring& s)
{
    if (s.size() != 36) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != L'-') return false;
        } else if (!IsHexDigit(s[i])) {
            return false;
        }
    }
    return true;
}

std::wstring NormalizeFb2Identifier(std::wstring id)
{
    id = NormalizeWhitespace(id);
    if (id.empty()) return id;
    if (id.rfind(L"urn:", 0) == 0) return id;
    if (id.size() >= 2 && id.front() == L'{' && id.back() == L'}') {
        id = id.substr(1, id.size() - 2);
    }
    if (LooksLikeUuid(id)) return L"urn:uuid:" + id;
    return id;
}

struct ChapterPlan {
    IXMLDOMNodePtr node;
    std::wstring title;
    std::wstring id;
    std::wstring fileName;
    std::wstring epubType;
};

std::wstring BodyName(IXMLDOMNodePtr body)
{
    IXMLDOMElementPtr element(body);
    if (element == nullptr) return {};
    return GetAttr(element, L"name");
}

std::wstring BodyTitle(IXMLDOMNodePtr body)
{
    std::wstring title = SelectText(body, L"fb:title");
    return title;
}

bool IsNotesBodyName(const std::wstring& name)
{
    return name == L"notes" || name == L"comments";
}

std::wstring DefaultBodyTitle(const std::wstring& name, const std::wstring& bookTitle, size_t bodyIndex)
{
    if (name == L"notes") return L"Примечания";
    if (name == L"comments") return L"Комментарии";
    if (!bookTitle.empty()) return bookTitle;

    std::wstringstream ss;
    ss << L"Текст " << (bodyIndex + 1);
    return ss.str();
}

void AddBodyChapters(IXMLDOMNodePtr body,
                     const std::wstring& bookTitle,
                     size_t bodyIndex,
                     std::vector<ChapterPlan>& plans)
{
    const std::wstring name = BodyName(body);
    const bool notesBody = IsNotesBodyName(name);

    // Notes/comments are intentionally kept as one chapter. This is important because
    // FB2 note links usually point to section ids inside body name="notes".
    if (notesBody) {
        ChapterPlan plan;
        plan.node = body;
        plan.title = BodyTitle(body);
        if (plan.title.empty()) plan.title = DefaultBodyTitle(name, bookTitle, bodyIndex);
        plan.id = IdFromIndex(name == L"comments" ? L"comments" : L"notes", plans.size() + 1);
        plan.fileName = ChapterFileNameForIndex(plans.size());
        plan.epubType = L"endnotes";
        plans.push_back(plan);
        return;
    }

    IXMLDOMNodeListPtr sections;
    CheckHR(body->selectNodes(bstr_t(L"fb:section"), &sections));
    long length = 0;
    CheckHR(sections->get_length(&length));

    if (length > 0) {
        for (long i = 0; i < length; ++i) {
            IXMLDOMNodePtr section;
            CheckHR(sections->get_item(i, &section));

            ChapterPlan plan;
            plan.node = section;
            plan.title = SectionNavTitle(section);
            if (plan.title.empty()) {
                std::wstringstream ss;
                ss << L"Глава " << (plans.size() + 1);
                plan.title = ss.str();
            }
            plan.id = IdFromIndex(L"chap", plans.size() + 1);
            plan.fileName = ChapterFileNameForIndex(plans.size());
            plan.epubType = L"bodymatter";
            plans.push_back(plan);
        }
    } else {
        ChapterPlan plan;
        plan.node = body;
        plan.title = BodyTitle(body);
        if (plan.title.empty()) plan.title = DefaultBodyTitle(name, bookTitle, bodyIndex);
        plan.id = IdFromIndex(L"chap", plans.size() + 1);
        plan.fileName = ChapterFileNameForIndex(plans.size());
        plan.epubType = L"bodymatter";
        plans.push_back(plan);
    }
}

void AddAnnotationChapter(IXMLDOMDocument2Ptr doc, std::vector<ChapterPlan>& plans)
{
    IXMLDOMNodePtr annotation;
    CheckHR(doc->selectSingleNode(bstr_t(L"/fb:FictionBook/fb:description/fb:title-info/fb:annotation"), &annotation));
    if (annotation == nullptr) return;

    const std::wstring text = NormalizeWhitespace(NodeText(annotation));
    if (text.empty()) return;

    ChapterPlan plan;
    plan.node.Attach(annotation.Detach());
    plan.title = L"Аннотация";
    plan.id = L"annotation";
    plan.fileName = ChapterFileNameForIndex(plans.size());
    plan.epubType = L"frontmatter";
    plans.push_back(plan);
}

bool NodeContainsImageElement(IXMLDOMNodePtr node)
{
    if (node == nullptr) return false;

    DOMNodeType type = NODE_INVALID;
    CheckHR(node->get_nodeType(&type));
    if (type == NODE_ELEMENT && LocalName(node) == L"image") return true;

    IXMLDOMNodeListPtr children;
    CheckHR(node->get_childNodes(&children));
    long length = 0;
    CheckHR(children->get_length(&length));
    for (long i = 0; i < length; ++i) {
        IXMLDOMNodePtr child;
        CheckHR(children->get_item(i, &child));
        if (NodeContainsImageElement(child)) return true;
    }
    return false;
}

bool IsExportableChapterPlan(const ChapterPlan& plan)
{
    if (!NormalizeWhitespace(NodeText(plan.node)).empty()) return true;
    return NodeContainsImageElement(plan.node);
}

void RemoveEmptyPlansAndReassignFileNames(std::vector<ChapterPlan>& plans)
{
    std::vector<ChapterPlan> kept;
    kept.reserve(plans.size());
    for (auto& plan : plans) {
        if (IsExportableChapterPlan(plan)) {
            kept.push_back(std::move(plan));
        }
    }

    for (size_t i = 0; i < kept.size(); ++i) {
        kept[i].fileName = ChapterFileNameForIndex(i);
    }
    plans = std::move(kept);
}

std::vector<ChapterPlan> BuildChapterPlans(IXMLDOMDocument2Ptr doc, const std::wstring& bookTitle, bool includeAnnotationPage)
{
    std::vector<ChapterPlan> plans;

    // FB2 annotation is metadata, but EPUB readers usually benefit from a real
    // front-matter page as well. It is placed before the main text and included
    // in TOC/landmarks.
    if (includeAnnotationPage) {
        AddAnnotationChapter(doc, plans);
    }

    IXMLDOMNodeListPtr bodies;
    CheckHR(doc->selectNodes(bstr_t(L"/fb:FictionBook/fb:body"), &bodies));
    long bodyCount = 0;
    CheckHR(bodies->get_length(&bodyCount));

    // First add normal book bodies, then notes/comments bodies. This keeps the reading order natural,
    // and makes note targets available for EPUBCheck link validation.
    for (int pass = 0; pass < 2; ++pass) {
        for (long i = 0; i < bodyCount; ++i) {
            IXMLDOMNodePtr body;
            CheckHR(bodies->get_item(i, &body));
            const bool isNotes = IsNotesBodyName(BodyName(body));
            if ((pass == 0 && isNotes) || (pass == 1 && !isNotes)) continue;
            AddBodyChapters(body, bookTitle, static_cast<size_t>(i), plans);
        }
    }

    return plans;
}

void BuildBookFromFbeDocument(IDispatch* dispatch, fbe::epub::EpubBook& book, bool isEpub3, bool includeAnnotationPage)
{
    if (dispatch == nullptr) {
        _com_issue_error(E_POINTER);
    }

    IXMLDOMDocument2Ptr doc(dispatch);
    if (doc == nullptr) {
        _com_issue_error(E_NOINTERFACE);
    }
    SetupNamespaces(doc);

    book.title = SelectText(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:book-title");
    if (book.title.empty()) book.title = L"Untitled";

    book.authors = PersonList(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:author");
    book.author = JoinList(book.authors, L", ");
    book.translators = PersonList(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:translator");

    book.language = SelectText(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:lang");
    if (book.language.empty()) book.language = L"ru";

    book.identifier = NormalizeFb2Identifier(SelectText(doc, L"/fb:FictionBook/fb:description/fb:document-info/fb:id"));
    book.publisher = SelectText(doc, L"/fb:FictionBook/fb:description/fb:publish-info/fb:publisher");
    book.description = SelectText(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:annotation");
    // Prefer FB2 date/@value because it is usually W3CDTF-compatible
    // (for example, value="2026-06-25" with visible text "25.06.2026").
    book.date = SelectDateValue(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:date");
    if (book.date.empty()) book.date = SelectDateValue(doc, L"/fb:FictionBook/fb:description/fb:publish-info/fb:year");
    book.isbn = SelectText(doc, L"/fb:FictionBook/fb:description/fb:publish-info/fb:isbn");
    book.source = SelectText(doc, L"/fb:FictionBook/fb:description/fb:document-info/fb:src-url");
    if (book.source.empty()) book.source = SelectText(doc, L"/fb:FictionBook/fb:description/fb:document-info/fb:src-ocr");
    book.subjects = TextList(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:genre");
    book.keywords = SplitKeywords(SelectText(doc, L"/fb:FictionBook/fb:description/fb:title-info/fb:keywords"));
    ApplySequenceMetadata(doc, book);

    book.cssUtf8 = fbe::epub::Utf8FromWide(DefaultCss() + Fb2StylesheetsCss(doc));

    ImageMap images = AddResourcesFromBinaries(doc, book);
    ApplyCoverMetadata(doc, images, book);

    std::vector<ChapterPlan> plans = BuildChapterPlans(doc, book.title, includeAnnotationPage);
    RemoveEmptyPlansAndReassignFileNames(plans);

    LinkMap links;
    std::set<std::wstring> usedFragmentIds;
    for (const ChapterPlan& plan : plans) {
        CollectIdsFromNode(plan.node, plan.fileName, links, usedFragmentIds);
    }

    NoteBacklinkMap noteBacklinks;
    for (const ChapterPlan& plan : plans) {
        const bool isNotes = (plan.id.rfind(L"notes", 0) == 0 || plan.id.rfind(L"comments", 0) == 0);
        if (!isNotes) {
            CollectNoteBacklinksFromNode(plan.node, plan.fileName, links, noteBacklinks, usedFragmentIds);
        }
    }

    std::map<std::wstring, size_t> emittedNoteRefCounters;
    for (const ChapterPlan& plan : plans) {
        const bool isNotes = (plan.id.rfind(L"notes", 0) == 0 || plan.id.rfind(L"comments", 0) == 0);
        XhtmlContext ctx{ images, links, &noteBacklinks, &emittedNoteRefCounters, plan.fileName, isEpub3, isNotes };

        fbe::epub::EpubChapter chapter;
        chapter.id = plan.id;
        chapter.title = plan.title.empty() ? book.title : plan.title;
        chapter.epubType = plan.epubType.empty() ? L"bodymatter" : plan.epubType;
        if (!isNotes) {
            BuildSectionNavChildren(plan.node, links, chapter.navChildren);
        }
        chapter.bodyXhtml = BlockContentForXhtml(ChapterAnchorXhtml(plan.node, links) + ChildrenToXhtml(plan.node, ctx));
        if (plan.id == L"annotation") {
            chapter.bodyXhtml = L"<div class=\"annotation\">\n" +
                                BlockContentForXhtml(chapter.bodyXhtml) + L"</div>\n";
        }

        // Plans have already been filtered by RemoveEmptyPlansAndReassignFileNames().
        // Do not filter a second time here: link maps and note backlinks were built for
        // this exact plan list, and dropping a plan at this stage can shift chapter_XXX
        // file names, producing links to non-existent XHTML files.
        book.chapters.push_back(std::move(chapter));
    }
}


bool HasDirectoryPart(const CString& path)
{
    return path.ReverseFind(L'\\') >= 0 || path.ReverseFind(L'/') >= 0 || path.Find(L':') >= 0;
}

CString ChangeExtensionToEpub(CString path)
{
    path.Trim();
    if (path.IsEmpty()) {
        path = L"book";
    }

    const int slash = std::max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
    const int dot = path.ReverseFind(L'.');
    if (dot > slash) {
        path.Delete(dot, path.GetLength() - dot);
    }
    path += L".epub";
    return path;
}

CString SanitizeBaseFileName(CString name)
{
    name.Trim();
    for (int i = 0; i < name.GetLength(); ++i) {
        const wchar_t ch = name[i];
        if (ch < 32 || wcschr(L"<>:\"/\\|?*", ch) != nullptr) {
            name.SetAt(i, L'_');
        }
    }
    name.TrimRight(L" .");
    if (name.IsEmpty()) {
        name = L"book";
    }
    return name;
}

CString PathFromFileUrlOrPath(const CString& value)
{
    CString s(value);
    s.Trim();
    if (s.IsEmpty()) {
        return s;
    }

    if (s.Left(5).CompareNoCase(L"file:") == 0) {
        wchar_t buffer[32768] = {};
        DWORD len = _countof(buffer);
        if (SUCCEEDED(PathCreateFromUrlW(s, buffer, &len, 0))) {
            return CString(buffer);
        }
    }

    return s;
}

CString DocumentSourcePath(IXMLDOMDocument2Ptr source)
{
    if (source == nullptr) {
        return CString();
    }

    CComBSTR url;
    if (SUCCEEDED(source->get_url(&url)) && url.Length() > 0) {
        CString path = PathFromFileUrlOrPath(CString(url));
        // get_url may theoretically return a non-file URL. The Save As dialog
        // needs a real file-system path or a plain file name.
        if (path.Find(L"://") < 0) {
            return path;
        }
    }

    return CString();
}

bool HasFileExtension(const CString& path)
{
    CString s(path);
    s.Trim();
    if (s.IsEmpty()) {
        return false;
    }

    const wchar_t* ext = PathFindExtensionW(s);
    return ext != nullptr && *ext != 0;
}

CString CandidateFromPathLikeValue(const CString& value)
{
    CString path = PathFromFileUrlOrPath(value);
    path.Trim(L" \"");
    if (path.IsEmpty()) {
        return CString();
    }

    // Use values that look like an actual file name/path. This avoids falling
    // back to a shortened book title when FBE passes a display string without
    // the original .fb2 extension.
    if (HasFileExtension(path) || HasDirectoryPart(path)) {
        return ChangeExtensionToEpub(path);
    }

    return CString();
}

CString DocumentTitleFromWindow(HWND owner)
{
    HWND candidates[3] = { owner, owner ? GetAncestor(owner, GA_ROOT) : nullptr, GetActiveWindow() };

    for (HWND h : candidates) {
        if (h == nullptr) {
            continue;
        }

        wchar_t text[1024] = {};
        if (GetWindowTextW(h, text, _countof(text)) <= 0) {
            continue;
        }

        CString title(text);
        title.Trim();
        if (title.IsEmpty()) {
            continue;
        }

        // FBE main window usually has a caption like:
        //   My book.fb2 - FB Editor
        const int suffix = title.Find(L" - FB Editor");
        if (suffix > 0) {
            title = title.Left(suffix);
            title.Trim();
        }

        if (title.Right(1) == L"*") {
            title = title.Left(title.GetLength() - 1);
            title.TrimRight();
        }

        if (HasFileExtension(title)) {
            return ChangeExtensionToEpub(title);
        }
    }

    return CString();
}

CString JoinDirectoryAndFileName(const CString& pathWithDirectory, const CString& fileNameOnly)
{
    const int slash = std::max(pathWithDirectory.ReverseFind(L'\\'), pathWithDirectory.ReverseFind(L'/'));
    if (slash < 0) {
        return fileNameOnly;
    }

    CString result = pathWithDirectory.Left(slash + 1);
    result += fileNameOnly;
    return result;
}

CString DefaultOutputFileName(BSTR filename, IXMLDOMDocument2Ptr source, HWND owner)
{
    // 1. Prefer the actual source document path when MSXML knows it.
    CString sourcePath = DocumentSourcePath(source);
    if (!sourcePath.IsEmpty()) {
        return ChangeExtensionToEpub(sourcePath);
    }

    CString fromArgument(filename ? filename : L"");
    CString fromArgumentRaw = PathFromFileUrlOrPath(fromArgument);
    fromArgumentRaw.Trim(L" \"");

    // 2. FBE often has the original .fb2 file name in the main window caption.
    // This is more reliable than the filename argument when FBE passes a
    // shortened default output name based on book-title.
    CString fromWindowTitle = DocumentTitleFromWindow(owner);
    if (!fromWindowTitle.IsEmpty()) {
        if (HasDirectoryPart(fromArgumentRaw) && !HasDirectoryPart(fromWindowTitle)) {
            return JoinDirectoryAndFileName(fromArgumentRaw, fromWindowTitle);
        }
        return fromWindowTitle;
    }

    // 3. Then use the filename argument, but only if it looks like a real file
    // name/path rather than a shortened display title.
    CString fromArgumentPath = CandidateFromPathLikeValue(fromArgumentRaw);
    if (!fromArgumentPath.IsEmpty()) {
        return fromArgumentPath;
    }

    // 4. Last fallback: book title from FB2 metadata.
    CString title(SanitizeBaseFileName(SelectText(source, L"/fb:FictionBook/fb:description/fb:title-info/fb:book-title").c_str()));
    return ChangeExtensionToEpub(title);
}

struct ExportPluginSettings;
bool AskExportOptions(HWND owner, ExportPluginSettings& settings, fbe::epub::EpubVersion version);

fbe::epub::EpubVersion VersionFromFilterIndex(DWORD filterIndex)
{
    return (filterIndex == 2) ? fbe::epub::EpubVersion::Epub2 : fbe::epub::EpubVersion::Epub3;
}

struct SaveDialogContext {
    ExportPluginSettings* settings = nullptr;
    fbe::epub::EpubVersion currentVersion = fbe::epub::EpubVersion::Epub3;
    std::wstring saveButtonText;
};

void RestoreCommonSaveButtonText(HWND commonDialog, SaveDialogContext* ctx)
{
    if (commonDialog == nullptr || ctx == nullptr || ctx->saveButtonText.empty()) {
        return;
    }
    HWND okButton = GetDlgItem(commonDialog, IDOK);
    if (okButton != nullptr) {
        SetWindowTextW(okButton, ctx->saveButtonText.c_str());
    }
}

UINT_PTR CALLBACK SaveDialogHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<SaveDialogContext*>(GetWindowLongPtr(hwnd, DWLP_USER));

    switch (msg) {
    case WM_INITDIALOG: {
        auto* ofn = reinterpret_cast<OPENFILENAME*>(lParam);
        auto* initCtx = ofn ? reinterpret_cast<SaveDialogContext*>(ofn->lCustData) : nullptr;
        SetWindowLongPtr(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(initCtx));

        if (initCtx != nullptr && ofn != nullptr) {
            initCtx->currentVersion = VersionFromFilterIndex(ofn->nFilterIndex);
        }
        return TRUE;
    }

    case WM_NOTIFY: {
        auto* notify = reinterpret_cast<OFNOTIFY*>(lParam);
        if (notify != nullptr && notify->hdr.code == CDN_TYPECHANGE &&
            notify->lpOFN != nullptr && ctx != nullptr) {
            ctx->currentVersion = VersionFromFilterIndex(notify->lpOFN->nFilterIndex);
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BUTTON_EXPORT_OPTIONS && HIWORD(wParam) == BN_CLICKED) {
            if (ctx != nullptr && ctx->settings != nullptr) {
                HWND parent = GetParent(hwnd);
                if (parent == nullptr) parent = hwnd;

                // Do not close the Save As dialog. The settings dialog is opened only
                // when the user explicitly presses the extra button. Some common-dialog
                // implementations reset the standard IDOK button text to "OK" after a
                // nested modal dialog; save and restore the original localized caption
                // (usually "Сохранить").
                wchar_t okText[128] = {};
                HWND okButton = GetDlgItem(parent, IDOK);
                if (okButton != nullptr && GetWindowTextW(okButton, okText, static_cast<int>(_countof(okText))) > 0) {
                    ctx->saveButtonText = okText;
                }

                AskExportOptions(parent, *ctx->settings, ctx->currentVersion);
                RestoreCommonSaveButtonText(parent, ctx);
            }
            return TRUE;
        }
        break;
    }

    return FALSE;
}

bool AskOutputFile(HWND owner,
                   BSTR filename,
                   IXMLDOMDocument2Ptr source,
                   ExportPluginSettings& settings,
                   CString& outPath,
                   fbe::epub::EpubVersion& version)
{
    CString filter;
    filter.LoadString(IDS_SAVE_FILE_FILTER);
    filter.Replace(L'|', L'\0');

    CString proposed = DefaultOutputFileName(filename, source, owner);

    wchar_t fileName[32768] = {};
    wcsncpy_s(fileName, proposed, _TRUNCATE);

    SaveDialogContext dialogCtx;
    dialogCtx.settings = &settings;
    dialogCtx.currentVersion = version;

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.hInstance = _AtlBaseModule.GetResourceInstance();
    ofn.lpTemplateName = MAKEINTRESOURCEW(IDD_SAVE_DIALOG_EXTRA);
    ofn.lpfnHook = SaveDialogHookProc;
    ofn.lCustData = reinterpret_cast<LPARAM>(&dialogCtx);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = _countof(fileName);
    ofn.lpstrDefExt = L"epub";
    ofn.Flags = OFN_HIDEREADONLY | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT |
                OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE |
                OFN_ENABLESIZING;
    ofn.nFilterIndex = (version == fbe::epub::EpubVersion::Epub2) ? 2 : 1; // EPUB 3 by default.

    if (!GetSaveFileName(&ofn)) {
        return false;
    }

    outPath = fileName;
    version = VersionFromFilterIndex(ofn.nFilterIndex);
    return true;
}



struct ExportPluginSettings {
    bool includeNcxFallbackInEpub3 = true;
    bool includeCoverPage = true;
    bool includeAnnotationPage = true;
    bool includeTitlePage = false;
    bool writeDiagnosticLog = false;

    // User-facing quality options. These do not change source text; they only
    // affect generated EPUB structure, navigation, CSS, diagnostics and images.
    bool showSummaryAfterExport = false;
    bool openFileAfterExport = false;
    bool openFolderAfterExport = false;
    bool showPreflightWarnings = true;
    bool cssJustifyText = true;
    bool cssFirstLineIndent = false;
    bool cssHyphenate = false;
    bool includeCoverInToc = true;
    bool includeAnnotationInToc = true;
    bool includeNotesInToc = true;
    bool includeNoteBacklinks = true;
    bool useFirstImageAsCoverWhenMissing = true;
    bool removeUnusedImages = false;
    DWORD warnLargeXhtmlKb = 512;
    DWORD maxTocDepth = 6;
};

ExportPluginSettings DefaultExportSettings()
{
    return ExportPluginSettings();
}

ExportPluginSettings CompatibilityExportSettings()
{
    ExportPluginSettings settings;
    settings.cssJustifyText = false;
    settings.warnLargeXhtmlKb = 384;
    settings.maxTocDepth = 4;
    return settings;
}

ExportPluginSettings RichStructureExportSettings()
{
    ExportPluginSettings settings;
    settings.showSummaryAfterExport = true;
    settings.includeTitlePage = true;
    settings.cssHyphenate = true;
    settings.removeUnusedImages = true;
    settings.maxTocDepth = 8;
    return settings;
}

const wchar_t* SETTINGS_KEY = L"Software\\FBETeam\\FictionBook Editor\\Plugins\\ExportEPUB";

bool ReadBoolSetting(HKEY key, const wchar_t* name, bool defaultValue)
{
    DWORD value = defaultValue ? 1u : 0u;
    DWORD type = 0;
    DWORD size = sizeof(value);
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD) {
        return value != 0;
    }
    return defaultValue;
}

void WriteBoolSetting(HKEY key, const wchar_t* name, bool value)
{
    const DWORD dw = value ? 1u : 0u;
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dw), sizeof(dw));
}

DWORD ReadDwordSetting(HKEY key, const wchar_t* name, DWORD defaultValue)
{
    DWORD value = defaultValue;
    DWORD type = 0;
    DWORD size = sizeof(value);
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD) {
        return value;
    }
    return defaultValue;
}

void WriteDwordSetting(HKEY key, const wchar_t* name, DWORD value)
{
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

ExportPluginSettings LoadExportSettings()
{
    ExportPluginSettings settings;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        settings.includeNcxFallbackInEpub3 = ReadBoolSetting(key, L"IncludeNcxFallbackInEpub3", settings.includeNcxFallbackInEpub3);
        settings.includeCoverPage = ReadBoolSetting(key, L"IncludeCoverPage", settings.includeCoverPage);
        settings.includeAnnotationPage = ReadBoolSetting(key, L"IncludeAnnotationPage", settings.includeAnnotationPage);
        settings.includeTitlePage = ReadBoolSetting(key, L"IncludeTitlePage", settings.includeTitlePage);
        settings.writeDiagnosticLog = ReadBoolSetting(key, L"WriteDiagnosticLog", settings.writeDiagnosticLog);
        settings.showSummaryAfterExport = ReadBoolSetting(key, L"ShowSummaryAfterExport", settings.showSummaryAfterExport);
        settings.openFileAfterExport = ReadBoolSetting(key, L"OpenFileAfterExport", settings.openFileAfterExport);
        settings.openFolderAfterExport = ReadBoolSetting(key, L"OpenFolderAfterExport", settings.openFolderAfterExport);
        settings.showPreflightWarnings = ReadBoolSetting(key, L"ShowPreflightWarnings", settings.showPreflightWarnings);
        settings.cssJustifyText = ReadBoolSetting(key, L"CssJustifyText", settings.cssJustifyText);
        settings.cssFirstLineIndent = ReadBoolSetting(key, L"CssFirstLineIndent", settings.cssFirstLineIndent);
        settings.cssHyphenate = ReadBoolSetting(key, L"CssHyphenate", settings.cssHyphenate);
        settings.includeCoverInToc = ReadBoolSetting(key, L"IncludeCoverInToc", settings.includeCoverInToc);
        settings.includeAnnotationInToc = ReadBoolSetting(key, L"IncludeAnnotationInToc", settings.includeAnnotationInToc);
        settings.includeNotesInToc = ReadBoolSetting(key, L"IncludeNotesInToc", settings.includeNotesInToc);
        settings.includeNoteBacklinks = ReadBoolSetting(key, L"IncludeNoteBacklinks", settings.includeNoteBacklinks);
        settings.useFirstImageAsCoverWhenMissing = ReadBoolSetting(key, L"UseFirstImageAsCoverWhenMissing", settings.useFirstImageAsCoverWhenMissing);
        settings.removeUnusedImages = ReadBoolSetting(key, L"RemoveUnusedImages", settings.removeUnusedImages);
        settings.warnLargeXhtmlKb = ReadDwordSetting(key, L"WarnLargeXhtmlKb", settings.warnLargeXhtmlKb);
        settings.maxTocDepth = ReadDwordSetting(key, L"MaxTocDepth", settings.maxTocDepth);
        if (settings.warnLargeXhtmlKb < 64) settings.warnLargeXhtmlKb = 64;
        if (settings.warnLargeXhtmlKb > 8192) settings.warnLargeXhtmlKb = 8192;
        RegCloseKey(key);
    }
    return settings;
}

void SaveExportSettings(const ExportPluginSettings& settings)
{
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &key, &disposition) == ERROR_SUCCESS) {
        WriteBoolSetting(key, L"IncludeNcxFallbackInEpub3", settings.includeNcxFallbackInEpub3);
        WriteBoolSetting(key, L"IncludeCoverPage", settings.includeCoverPage);
        WriteBoolSetting(key, L"IncludeAnnotationPage", settings.includeAnnotationPage);
        WriteBoolSetting(key, L"IncludeTitlePage", settings.includeTitlePage);
        WriteBoolSetting(key, L"WriteDiagnosticLog", settings.writeDiagnosticLog);
        WriteBoolSetting(key, L"ShowSummaryAfterExport", settings.showSummaryAfterExport);
        WriteBoolSetting(key, L"OpenFileAfterExport", settings.openFileAfterExport);
        WriteBoolSetting(key, L"OpenFolderAfterExport", settings.openFolderAfterExport);
        WriteBoolSetting(key, L"ShowPreflightWarnings", settings.showPreflightWarnings);
        WriteBoolSetting(key, L"CssJustifyText", settings.cssJustifyText);
        WriteBoolSetting(key, L"CssFirstLineIndent", settings.cssFirstLineIndent);
        WriteBoolSetting(key, L"CssHyphenate", settings.cssHyphenate);
        WriteBoolSetting(key, L"IncludeCoverInToc", settings.includeCoverInToc);
        WriteBoolSetting(key, L"IncludeAnnotationInToc", settings.includeAnnotationInToc);
        WriteBoolSetting(key, L"IncludeNotesInToc", settings.includeNotesInToc);
        WriteBoolSetting(key, L"IncludeNoteBacklinks", settings.includeNoteBacklinks);
        WriteBoolSetting(key, L"UseFirstImageAsCoverWhenMissing", settings.useFirstImageAsCoverWhenMissing);
        WriteBoolSetting(key, L"RemoveUnusedImages", settings.removeUnusedImages);
        WriteDwordSetting(key, L"WarnLargeXhtmlKb", settings.warnLargeXhtmlKb);
        WriteDwordSetting(key, L"MaxTocDepth", settings.maxTocDepth);
        RegCloseKey(key);
    }
}

void SetDlgBool(HWND hwnd, int id, bool value)
{
    CheckDlgButton(hwnd, id, value ? BST_CHECKED : BST_UNCHECKED);
}

bool GetDlgBool(HWND hwnd, int id)
{
    return IsDlgButtonChecked(hwnd, id) == BST_CHECKED;
}

void SetDlgDword(HWND hwnd, int id, DWORD value)
{
    CString text;
    text.Format(L"%lu", static_cast<unsigned long>(value));
    SetDlgItemTextW(hwnd, id, text);
}

DWORD GetDlgDword(HWND hwnd, int id, DWORD defaultValue, DWORD minValue, DWORD maxValue)
{
    wchar_t buf[32] = {};
    GetDlgItemTextW(hwnd, id, buf, static_cast<int>(_countof(buf)));
    wchar_t* end = nullptr;
    unsigned long value = wcstoul(buf, &end, 10);
    if (end == buf) {
        value = defaultValue;
    }
    if (value < minValue) value = minValue;
    if (value > maxValue) value = maxValue;
    return static_cast<DWORD>(value);
}


void AddTooltip(HWND tooltip, HWND dialog, int controlId, const wchar_t* text)
{
    HWND control = GetDlgItem(dialog, controlId);
    if (tooltip == nullptr || control == nullptr || text == nullptr || *text == L'\0') {
        return;
    }

    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = dialog;
    ti.uId = reinterpret_cast<UINT_PTR>(control);
    ti.lpszText = const_cast<LPWSTR>(text);
    SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
}

void InstallExportOptionsTooltips(HWND hwnd)
{
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    HWND tooltip = CreateWindowExW(WS_EX_TOPMOST,
                                   TOOLTIPS_CLASSW,
                                   nullptr,
                                   WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                   CW_USEDEFAULT,
                                   CW_USEDEFAULT,
                                   CW_USEDEFAULT,
                                   CW_USEDEFAULT,
                                   hwnd,
                                   nullptr,
                                   _AtlBaseModule.GetResourceInstance(),
                                   nullptr);
    if (tooltip == nullptr) {
        return;
    }

    SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, 420);
    SendMessageW(tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, MAKELPARAM(350, 0));
    SetPropW(hwnd, L"ExportEPUB.OptionsTooltip", tooltip);

    AddTooltip(tooltip, hwnd, IDC_CHECK_NCX_FALLBACK,
               L"Для EPUB 3 дополнительно создаёт toc.ncx. Это повышает совместимость со старыми читалками. Для EPUB 2 NCX создаётся всегда, независимо от этой настройки.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_COVER_PAGE,
               L"Создаёт отдельную XHTML-страницу с обложкой и добавляет её в порядок чтения/навигацию согласно выбранным настройкам.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_ANNOTATION_PAGE,
               L"Переносит FB2 annotation на отдельную XHTML-страницу frontmatter. Если выключено, аннотация остаётся только в метаданных EPUB.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_TITLE_PAGE,
               L"Добавляет в начало первой текстовой главы титульный блок с названием, авторами, серией, издателем, датой, ISBN и краткой аннотацией. Не меняет ссылки и структуру файлов EPUB.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_WRITE_LOG,
               L"Создаёт рядом с EPUB диагностический .log с параметрами экспорта, списком глав и ресурсов. По умолчанию выключено.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_SHOW_SUMMARY,
               L"После успешного экспорта показывает итоговое окно со статистикой, предупреждениями и кнопками открытия файла/папки.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_OPEN_FILE_AFTER_EXPORT,
               L"Автоматически открывает созданный EPUB программой, назначенной в Windows для файлов .epub.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_OPEN_FOLDER_AFTER_EXPORT,
               L"Автоматически открывает Проводник с выделением созданного EPUB-файла.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_SHOW_PREFLIGHT,
               L"Показывает предупреждения встроенной проверки: битые ссылки, дубли id, отсутствующие картинки, слишком большие XHTML и другие проблемы структуры.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_CSS_JUSTIFY,
               L"Добавляет в CSS выравнивание основного текста по ширине. Отображение зависит от конкретной EPUB-читалки.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_CSS_FIRST_INDENT,
               L"Добавляет в CSS отступ первой строки для обычных абзацев. Исходный текст FB2 не изменяется.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_CSS_HYPHENATE,
               L"Разрешает переносы слов через CSS hyphens:auto. Поддержка зависит от EPUB-читалки и языка книги.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_TOC_COVER,
               L"Добавляет страницу обложки в оглавление nav.xhtml/toc.ncx, если страница обложки создаётся.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_TOC_ANNOTATION,
               L"Добавляет страницу аннотации в оглавление nav.xhtml/toc.ncx, если страница аннотации создаётся.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_TOC_NOTES,
               L"Добавляет главы с примечаниями и комментариями в оглавление EPUB.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_NOTE_BACKLINKS,
               L"Добавляет в примечания обратные ссылки ↩ к месту вызова сноски в основном тексте.");
    AddTooltip(tooltip, hwnd, IDC_STATIC_MAX_TOC_DEPTH,
               L"Максимальная глубина вложенных FB2 section, попадающая в оглавление. Допустимый диапазон: 1–12.");
    AddTooltip(tooltip, hwnd, IDC_EDIT_MAX_TOC_DEPTH,
               L"Максимальная глубина вложенных FB2 section, попадающая в оглавление. Допустимый диапазон: 1–12.");
    AddTooltip(tooltip, hwnd, IDC_STATIC_MAX_XHTML_KB,
               L"Порог размера XHTML-главы в КБ. Если сгенерированный chapter_*.xhtml больше этого значения, встроенная проверка выдаст предупреждение.");
    AddTooltip(tooltip, hwnd, IDC_EDIT_MAX_XHTML_KB,
               L"Порог размера XHTML-главы в КБ. Допустимый диапазон: 64–8192 КБ. Это только предупреждение, файл не обрезается.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_COVER_FIRST_IMAGE,
               L"Если в FB2 нет coverpage, первая найденная binary-картинка будет использована как обложка EPUB.");
    AddTooltip(tooltip, hwnd, IDC_CHECK_REMOVE_UNUSED_IMAGES,
               L"Не включает в EPUB binary-изображения, на которые нет ссылок из текста, обложки или аннотации. Уменьшает размер файла.");
    AddTooltip(tooltip, hwnd, IDC_BUTTON_PRESET_DEFAULT,
               L"Возвращает рекомендуемые настройки по умолчанию для обычного экспорта FB2 в EPUB.");
    AddTooltip(tooltip, hwnd, IDC_BUTTON_PRESET_COMPAT,
               L"Включает более консервативный набор параметров для старых и капризных читалок: проще CSS, меньше глубина оглавления, NCX fallback для EPUB 3.");
    AddTooltip(tooltip, hwnd, IDC_BUTTON_PRESET_RICH,
               L"Включает более подробную структуру EPUB: глубокое оглавление, предупреждения, итоговое окно и удаление неиспользуемых изображений.");
}

void DestroyExportOptionsTooltips(HWND hwnd)
{
    HWND tooltip = reinterpret_cast<HWND>(GetPropW(hwnd, L"ExportEPUB.OptionsTooltip"));
    if (tooltip != nullptr) {
        RemovePropW(hwnd, L"ExportEPUB.OptionsTooltip");
        DestroyWindow(tooltip);
    }
}

struct ExportOptionsDialogContext {
    ExportPluginSettings* settings = nullptr;
    fbe::epub::EpubVersion version = fbe::epub::EpubVersion::Epub3;
};

void FillExportOptionsDialog(HWND hwnd, const ExportPluginSettings& settings)
{
    SetDlgBool(hwnd, IDC_CHECK_NCX_FALLBACK, settings.includeNcxFallbackInEpub3);
    SetDlgBool(hwnd, IDC_CHECK_COVER_PAGE, settings.includeCoverPage);
    SetDlgBool(hwnd, IDC_CHECK_ANNOTATION_PAGE, settings.includeAnnotationPage);
    SetDlgBool(hwnd, IDC_CHECK_TITLE_PAGE, settings.includeTitlePage);
    SetDlgBool(hwnd, IDC_CHECK_WRITE_LOG, settings.writeDiagnosticLog);
    SetDlgBool(hwnd, IDC_CHECK_SHOW_SUMMARY, settings.showSummaryAfterExport);
    SetDlgBool(hwnd, IDC_CHECK_OPEN_FILE_AFTER_EXPORT, settings.openFileAfterExport);
    SetDlgBool(hwnd, IDC_CHECK_OPEN_FOLDER_AFTER_EXPORT, settings.openFolderAfterExport);
    SetDlgBool(hwnd, IDC_CHECK_SHOW_PREFLIGHT, settings.showPreflightWarnings);
    SetDlgBool(hwnd, IDC_CHECK_CSS_JUSTIFY, settings.cssJustifyText);
    SetDlgBool(hwnd, IDC_CHECK_CSS_FIRST_INDENT, settings.cssFirstLineIndent);
    SetDlgBool(hwnd, IDC_CHECK_CSS_HYPHENATE, settings.cssHyphenate);
    SetDlgBool(hwnd, IDC_CHECK_TOC_COVER, settings.includeCoverInToc);
    SetDlgBool(hwnd, IDC_CHECK_TOC_ANNOTATION, settings.includeAnnotationInToc);
    SetDlgBool(hwnd, IDC_CHECK_TOC_NOTES, settings.includeNotesInToc);
    SetDlgBool(hwnd, IDC_CHECK_NOTE_BACKLINKS, settings.includeNoteBacklinks);
    SetDlgBool(hwnd, IDC_CHECK_COVER_FIRST_IMAGE, settings.useFirstImageAsCoverWhenMissing);
    SetDlgBool(hwnd, IDC_CHECK_REMOVE_UNUSED_IMAGES, settings.removeUnusedImages);
    SetDlgDword(hwnd, IDC_EDIT_MAX_XHTML_KB, settings.warnLargeXhtmlKb);
    SetDlgDword(hwnd, IDC_EDIT_MAX_TOC_DEPTH, settings.maxTocDepth);
}

void ReadExportOptionsDialog(HWND hwnd, ExportPluginSettings& settings)
{
    settings.includeNcxFallbackInEpub3 = GetDlgBool(hwnd, IDC_CHECK_NCX_FALLBACK);
    settings.includeCoverPage = GetDlgBool(hwnd, IDC_CHECK_COVER_PAGE);
    settings.includeAnnotationPage = GetDlgBool(hwnd, IDC_CHECK_ANNOTATION_PAGE);
    settings.includeTitlePage = GetDlgBool(hwnd, IDC_CHECK_TITLE_PAGE);
    settings.writeDiagnosticLog = GetDlgBool(hwnd, IDC_CHECK_WRITE_LOG);
    settings.showSummaryAfterExport = GetDlgBool(hwnd, IDC_CHECK_SHOW_SUMMARY);
    settings.openFileAfterExport = GetDlgBool(hwnd, IDC_CHECK_OPEN_FILE_AFTER_EXPORT);
    settings.openFolderAfterExport = GetDlgBool(hwnd, IDC_CHECK_OPEN_FOLDER_AFTER_EXPORT);
    settings.showPreflightWarnings = GetDlgBool(hwnd, IDC_CHECK_SHOW_PREFLIGHT);
    settings.cssJustifyText = GetDlgBool(hwnd, IDC_CHECK_CSS_JUSTIFY);
    settings.cssFirstLineIndent = GetDlgBool(hwnd, IDC_CHECK_CSS_FIRST_INDENT);
    settings.cssHyphenate = GetDlgBool(hwnd, IDC_CHECK_CSS_HYPHENATE);
    settings.includeCoverInToc = GetDlgBool(hwnd, IDC_CHECK_TOC_COVER);
    settings.includeAnnotationInToc = GetDlgBool(hwnd, IDC_CHECK_TOC_ANNOTATION);
    settings.includeNotesInToc = GetDlgBool(hwnd, IDC_CHECK_TOC_NOTES);
    settings.includeNoteBacklinks = GetDlgBool(hwnd, IDC_CHECK_NOTE_BACKLINKS);
    settings.useFirstImageAsCoverWhenMissing = GetDlgBool(hwnd, IDC_CHECK_COVER_FIRST_IMAGE);
    settings.removeUnusedImages = GetDlgBool(hwnd, IDC_CHECK_REMOVE_UNUSED_IMAGES);
    settings.warnLargeXhtmlKb = GetDlgDword(hwnd, IDC_EDIT_MAX_XHTML_KB, settings.warnLargeXhtmlKb, 64, 8192);
    settings.maxTocDepth = GetDlgDword(hwnd, IDC_EDIT_MAX_TOC_DEPTH, settings.maxTocDepth, 1, 12);
}

INT_PTR CALLBACK ExportOptionsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<ExportOptionsDialogContext*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    ExportPluginSettings* settings = ctx ? ctx->settings : nullptr;

    switch (msg) {
    case WM_INITDIALOG:
        ctx = reinterpret_cast<ExportOptionsDialogContext*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
        settings = ctx ? ctx->settings : nullptr;

        if (settings != nullptr) {
            FillExportOptionsDialog(hwnd, *settings);
        }

        InstallExportOptionsTooltips(hwnd);
        return TRUE;

    case WM_DESTROY:
        DestroyExportOptionsTooltips(hwnd);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (settings != nullptr) {
                ReadExportOptionsDialog(hwnd, *settings);
                SaveExportSettings(*settings);
            }
            EndDialog(hwnd, IDOK);
            return TRUE;

        case IDC_BUTTON_PRESET_DEFAULT:
            if (settings != nullptr) {
                *settings = DefaultExportSettings();
                FillExportOptionsDialog(hwnd, *settings);
            }
            return TRUE;

        case IDC_BUTTON_PRESET_COMPAT:
            if (settings != nullptr) {
                *settings = CompatibilityExportSettings();
                FillExportOptionsDialog(hwnd, *settings);
            }
            return TRUE;

        case IDC_BUTTON_PRESET_RICH:
            if (settings != nullptr) {
                *settings = RichStructureExportSettings();
                FillExportOptionsDialog(hwnd, *settings);
            }
            return TRUE;

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

bool AskExportOptions(HWND owner, ExportPluginSettings& settings, fbe::epub::EpubVersion version)
{
    ExportOptionsDialogContext ctx;
    ctx.settings = &settings;
    ctx.version = version;

    const INT_PTR result = DialogBoxParamW(_AtlBaseModule.GetResourceInstance(),
                                           MAKEINTRESOURCEW(IDD_EXPORT_OPTIONS),
                                           owner,
                                           ExportOptionsDlgProc,
                                           reinterpret_cast<LPARAM>(&ctx));
    return result == IDOK;
}


std::wstring VersionName(fbe::epub::EpubVersion version)
{
    return version == fbe::epub::EpubVersion::Epub2 ? L"EPUB 2.0.1" : L"EPUB 3.3";
}

std::wstring WidePathForLog(const CString& path)
{
    return std::wstring(static_cast<LPCWSTR>(path));
}


std::wstring ToLowerWide(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return s;
}

bool EndsWithNoCase(const std::wstring& value, const std::wstring& suffix)
{
    if (value.size() < suffix.size()) return false;
    return ToLowerWide(value.substr(value.size() - suffix.size())) == ToLowerWide(suffix);
}

bool ResourceLooksLikeImage(const fbe::epub::EpubResource& r)
{
    return r.mediaType.rfind("image/", 0) == 0;
}

std::wstring ExpectedMediaTypeFromHref(const std::wstring& href)
{
    const std::wstring h = ToLowerWide(href);
    if (EndsWithNoCase(h, L".jpg") || EndsWithNoCase(h, L".jpeg") || EndsWithNoCase(h, L".jpe")) return L"image/jpeg";
    if (EndsWithNoCase(h, L".png")) return L"image/png";
    if (EndsWithNoCase(h, L".gif")) return L"image/gif";
    if (EndsWithNoCase(h, L".svg") || EndsWithNoCase(h, L".svgz")) return L"image/svg+xml";
    if (EndsWithNoCase(h, L".webp")) return L"image/webp";
    return {};
}

void AppendCssOptionOverrides(fbe::epub::EpubBook& book, const ExportPluginSettings& settings)
{
    std::wstring css = L"\n/* ExportEPUB user options */\n";
    css += L"p {\n";
    css += settings.cssJustifyText ? L"  text-align: justify;\n" : L"  text-align: left;\n";
    css += settings.cssFirstLineIndent ? L"  text-indent: 1.25em;\n" : L"  text-indent: 0;\n";
    css += L"}\n";
    if (settings.cssHyphenate) {
        css += L"html, body, p { -epub-hyphens: auto; hyphens: auto; }\n";
    } else {
        css += L"html, body, p { -epub-hyphens: none; hyphens: none; }\n";
    }
    book.cssUtf8 += fbe::epub::Utf8FromWide(css);
}

void RemoveNoteBacklinks(fbe::epub::EpubBook& book)
{
    for (auto& chapter : book.chapters) {
        std::wstring& html = chapter.bodyXhtml;
        std::size_t pos = 0;
        while ((pos = html.find(L"class=\"backlinks\"", pos)) != std::wstring::npos) {
            std::size_t start = html.rfind(L"<p", pos);
            std::size_t end = html.find(L"</p>", pos);
            if (start == std::wstring::npos || end == std::wstring::npos) {
                pos += 1;
                continue;
            }
            html.erase(start, (end + 4) - start);
            pos = start;
        }
    }
}

void ApplyTocSettings(fbe::epub::EpubBook& book, const ExportPluginSettings& settings)
{
    for (auto& chapter : book.chapters) {
        if (chapter.epubType == L"frontmatter" && !settings.includeAnnotationInToc) {
            chapter.includeInToc = false;
        }
        if (chapter.epubType == L"endnotes" && !settings.includeNotesInToc) {
            chapter.includeInToc = false;
        }
    }
}

void ApplyCoverFallback(fbe::epub::EpubBook& book, const ExportPluginSettings& settings)
{
    if (!settings.useFirstImageAsCoverWhenMissing || !book.coverImageId.empty()) {
        return;
    }
    for (const auto& r : book.resources) {
        if (ResourceLooksLikeImage(r)) {
            book.coverImageId = r.id;
            break;
        }
    }
}

bool ChapterHtmlReferences(const std::wstring& html, const std::wstring& href)
{
    if (href.empty()) return false;
    if (html.find(href) != std::wstring::npos) return true;
    const std::size_t slash = href.find_last_of(L'/');
    const std::wstring fileOnly = href.substr(slash == std::wstring::npos ? 0 : slash + 1);
    return !fileOnly.empty() && html.find(fileOnly) != std::wstring::npos;
}

void RemoveUnusedImages(fbe::epub::EpubBook& book, const ExportPluginSettings& settings)
{
    if (!settings.removeUnusedImages) return;

    std::vector<fbe::epub::EpubResource> kept;
    kept.reserve(book.resources.size());
    for (auto& r : book.resources) {
        if (!ResourceLooksLikeImage(r) || r.id == book.coverImageId) {
            kept.push_back(std::move(r));
            continue;
        }

        bool used = false;
        for (const auto& chapter : book.chapters) {
            if (ChapterHtmlReferences(chapter.bodyXhtml, r.href)) {
                used = true;
                break;
            }
        }
        if (used) {
            kept.push_back(std::move(r));
        }
    }
    book.resources.swap(kept);
}

std::wstring AttrValueAt(const std::wstring& text, std::size_t attrPos)
{
    std::size_t quote = text.find(L'"', attrPos);
    if (quote == std::wstring::npos) return {};
    std::size_t end = text.find(L'"', quote + 1);
    if (end == std::wstring::npos) return {};
    return text.substr(quote + 1, end - quote - 1);
}

void ScanAttributeValues(const std::wstring& html, const wchar_t* attr, std::vector<std::wstring>& values)
{
    std::wstring needle(attr);
    needle += L"=\"";
    std::size_t pos = 0;
    while ((pos = html.find(needle, pos)) != std::wstring::npos) {
        std::wstring v = AttrValueAt(html, pos + needle.size() - 1);
        if (!v.empty()) values.push_back(std::move(v));
        pos += needle.size();
    }
}

struct PreflightResult {
    std::vector<std::wstring> warnings;
};

PreflightResult RunPreflight(const fbe::epub::EpubBook& book, const ExportPluginSettings& settings)
{
    PreflightResult result;
    std::unordered_map<std::wstring, std::wstring> fragmentOwner;

    for (std::size_t i = 0; i < book.chapters.size(); ++i) {
        const auto& chapter = book.chapters[i];
        std::vector<std::wstring> ids;
        ScanAttributeValues(chapter.bodyXhtml, L"id", ids);
        for (const auto& id : ids) {
            auto inserted = fragmentOwner.try_emplace(id, chapter.title);
            if (!inserted.second) {
                result.warnings.emplace_back(L"Повторяющийся XHTML id: " + id + L" (главы: " + inserted.first->second + L", " + chapter.title + L")");
            }
        }

        const std::size_t kb = fbe::epub::Utf8FromWide(chapter.bodyXhtml).size() / 1024;
        if (settings.warnLargeXhtmlKb > 0 && kb > settings.warnLargeXhtmlKb) {
            std::wstringstream w;
            w << L"Большой XHTML-фрагмент: " << chapter.title << L" — около " << kb << L" КБ";
            result.warnings.emplace_back(w.str());
        }
    }

    for (const auto& chapter : book.chapters) {
        std::vector<std::wstring> hrefs;
        ScanAttributeValues(chapter.bodyXhtml, L"href", hrefs);
        for (const auto& href : hrefs) {
            if (href.rfind(L"http://", 0) == 0 || href.rfind(L"https://", 0) == 0 || href.rfind(L"mailto:", 0) == 0) {
                continue;
            }
            const std::size_t hash = href.find(L'#');
            if (hash != std::wstring::npos) {
                const std::wstring id = href.substr(hash + 1);
                if (!id.empty() && fragmentOwner.find(id) == fragmentOwner.end()) {
                    result.warnings.emplace_back(L"Ссылка указывает на отсутствующий фрагмент: " + href);
                }
            }
        }
    }

    std::unordered_set<std::wstring> manifestIds;
    std::unordered_set<std::wstring> resourceHrefs;
    for (const auto& r : book.resources) {
        if (!manifestIds.insert(r.id).second) {
            result.warnings.emplace_back(L"Повторяющийся manifest id ресурса: " + r.id);
        }
        resourceHrefs.insert(r.href);
        if (r.data.empty()) {
            result.warnings.emplace_back(L"Пустой ресурс изображения/файла: " + r.href);
        }
        const std::wstring expected = ExpectedMediaTypeFromHref(r.href);
        if (!expected.empty() && expected != fbe::epub::WideFromUtf8(r.mediaType)) {
            result.warnings.emplace_back(L"media-type не похож на расширение файла: " + r.href + L" (" + fbe::epub::WideFromUtf8(r.mediaType) + L")");
        }
    }

    for (const auto& chapter : book.chapters) {
        std::vector<std::wstring> srcs;
        ScanAttributeValues(chapter.bodyXhtml, L"src", srcs);
        for (const auto& src : srcs) {
            std::wstring normalized = src;
            while (normalized.rfind(L"../", 0) == 0) normalized.erase(0, 3);
            if (normalized.rfind(L"./", 0) == 0) normalized.erase(0, 2);
            if (resourceHrefs.find(normalized) == resourceHrefs.end()) {
                result.warnings.emplace_back(L"Изображение отсутствует в manifest/resources: " + src);
            }
        }
    }

    return result;
}

std::wstring PreflightWarningsText(const PreflightResult& preflight, std::size_t limit = 12)
{
    if (preflight.warnings.empty()) return L"";
    std::wstringstream w;
    w << L"Встроенная проверка нашла предупреждения: " << preflight.warnings.size() << L"\n\n";
    const std::size_t count = std::min(limit, preflight.warnings.size());
    for (std::size_t i = 0; i < count; ++i) {
        w << L"• " << preflight.warnings[i] << L"\n";
    }
    if (preflight.warnings.size() > count) {
        w << L"…и ещё " << (preflight.warnings.size() - count) << L" предупреждений.\n";
    }
    return w.str();
}



void CopyWideTextToClipboard(HWND owner, const std::wstring& text)
{
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem != nullptr) {
        void* ptr = GlobalLock(mem);
        if (ptr != nullptr) {
            memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
            mem = nullptr;
        }
        if (mem != nullptr) GlobalFree(mem);
    }
    CloseClipboard();
}

std::wstring PreflightWarningsFullText(const PreflightResult& preflight)
{
    std::wstringstream w;
    w << L"Встроенная проверка ExportEPUB\n";
    w << L"Предупреждений: " << preflight.warnings.size() << L"\n\n";
    for (std::size_t i = 0; i < preflight.warnings.size(); ++i) {
        w << L"[W" << std::setw(4) << std::setfill(L'0') << (i + 1) << L"] " << preflight.warnings[i] << L"\n";
    }
    return w.str();
}

void OpenPathWithShell(HWND owner, const CString& path)
{
    ShellExecuteW(owner, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenFolderWithShell(HWND owner, const CString& path)
{
    CString arg;
    arg.Format(L"/select,\"%s\"", static_cast<LPCWSTR>(path));
    ShellExecuteW(owner, L"open", L"explorer.exe", arg, nullptr, SW_SHOWNORMAL);
}

void ShowPreflightWarnings(HWND owner, const PreflightResult& preflight)
{
    if (preflight.warnings.empty()) return;
    CString title;
    title.LoadString(IDS_ERROR_TITLE);
    std::wstring text = PreflightWarningsText(preflight);
    text += L"\nНажмите \"Да\", чтобы скопировать полный отчёт в буфер обмена.";
    const int rc = MessageBoxW(owner, text.c_str(), title, MB_YESNO | MB_ICONWARNING);
    if (rc == IDYES) {
        CopyWideTextToClipboard(owner, PreflightWarningsFullText(preflight));
    }
}

void ShowExportSummary(HWND owner,
                       const CString& outPath,
                       const fbe::epub::EpubBook& book,
                       const fbe::epub::EpubExportOptions& opt,
                       const PreflightResult& preflight)
{
    std::size_t imageCount = 0;
    for (const auto& r : book.resources) {
        if (ResourceLooksLikeImage(r)) ++imageCount;
    }

    std::wstringstream w;
    w << L"EPUB успешно сохранён.\n\n";
    w << L"Файл: " << static_cast<LPCWSTR>(outPath) << L"\n";
    w << L"Формат: " << VersionName(opt.version) << L"\n";
    w << L"Глав: " << book.chapters.size() << L"\n";
    w << L"Ресурсов: " << book.resources.size() << L"\n";
    w << L"Изображений: " << imageCount << L"\n";
    w << L"Предупреждений встроенной проверки: " << preflight.warnings.size() << L"\n\n";
    w << L"Да — открыть EPUB.\nНет — открыть папку с файлом.\nОтмена — закрыть окно.";

    CString title;
    title.LoadString(IDS_ERROR_TITLE);
    const int rc = MessageBoxW(owner, w.str().c_str(), title, MB_YESNOCANCEL | MB_ICONINFORMATION);
    if (rc == IDYES) {
        OpenPathWithShell(owner, outPath);
    } else if (rc == IDNO) {
        OpenFolderWithShell(owner, outPath);
    }
}



void PruneNavChildren(std::vector<fbe::epub::EpubNavPoint>& points, DWORD maxDepth, DWORD currentDepth)
{
    if (maxDepth == 0) return;
    if (currentDepth >= maxDepth) {
        for (auto& p : points) p.children.clear();
        return;
    }
    for (auto& p : points) {
        PruneNavChildren(p.children, maxDepth, currentDepth + 1);
    }
}

void ApplyTocDepthLimit(fbe::epub::EpubBook& book, const ExportPluginSettings& settings)
{
    if (settings.maxTocDepth == 0) return;
    for (auto& chapter : book.chapters) {
        PruneNavChildren(chapter.navChildren, settings.maxTocDepth, 1);
    }
}

void AppendTitlePageLine(std::wstringstream& html, const std::wstring& label, const std::wstring& value)
{
    if (NormalizeWhitespace(value).empty()) return;
    html << L"  <p class=\"titlepage-meta\"><span class=\"titlepage-label\">"
         << fbe::epub::XmlEscape(label) << L":</span> "
         << fbe::epub::XmlEscape(value) << L"</p>\n";
}

std::wstring BuildInlineTitlePageXhtml(const fbe::epub::EpubBook& book)
{
    std::wstringstream html;
    html << L"<div class=\"titlepage\">\n";
    html << L"  <h1 class=\"titlepage-title\">" << fbe::epub::XmlEscape(book.title) << L"</h1>\n";
    if (!book.authors.empty()) {
        html << L"  <p class=\"titlepage-author\">" << fbe::epub::XmlEscape(JoinList(book.authors, L", ")) << L"</p>\n";
    } else if (!book.author.empty()) {
        html << L"  <p class=\"titlepage-author\">" << fbe::epub::XmlEscape(book.author) << L"</p>\n";
    }
    std::wstring series = book.seriesName;
    if (!book.seriesNumber.empty()) {
        if (!series.empty()) series += L" № ";
        series += book.seriesNumber;
    }
    AppendTitlePageLine(html, L"Серия", series);
    AppendTitlePageLine(html, L"Издатель", book.publisher);
    AppendTitlePageLine(html, L"Дата", book.date);
    AppendTitlePageLine(html, L"ISBN", book.isbn);
    if (!NormalizeWhitespace(book.description).empty()) {
        html << L"  <div class=\"titlepage-description\">\n"
             << fbe::epub::MakeSimpleParagraphsXhtml(book.description)
             << L"  </div>\n";
    }
    html << L"</div>\n";
    return html.str();
}

void ApplyInlineTitlePage(fbe::epub::EpubBook& book, const ExportPluginSettings& settings)
{
    if (!settings.includeTitlePage || book.chapters.empty()) return;
    for (auto& chapter : book.chapters) {
        if (chapter.epubType == L"bodymatter") {
            chapter.bodyXhtml = BuildInlineTitlePageXhtml(book) + chapter.bodyXhtml;
            return;
        }
    }
    book.chapters.front().bodyXhtml = BuildInlineTitlePageXhtml(book) + book.chapters.front().bodyXhtml;
}

void ApplyPostBuildSettings(fbe::epub::EpubBook& book, const ExportPluginSettings& settings)
{
    ApplyCoverFallback(book, settings);
    ApplyTocSettings(book, settings);
    if (!settings.includeNoteBacklinks) {
        RemoveNoteBacklinks(book);
    }
    RemoveUnusedImages(book, settings);
    ApplyTocDepthLimit(book, settings);
    ApplyInlineTitlePage(book, settings);
    AppendCssOptionOverrides(book, settings);
}

void WriteExportLog(const CString& outPath,
                    const fbe::epub::EpubBook& book,
                    const fbe::epub::EpubExportOptions& opt)
{
    try {
        std::filesystem::path logPath(static_cast<LPCWSTR>(outPath));
        logPath += L".log";

        std::wstringstream w;
        w << L"FictionBook Editor EPUB export plugin\n";
        w << L"Output: " << WidePathForLog(outPath) << L"\n";
        w << L"Version: " << VersionName(opt.version) << L"\n";
        w << L"NCX fallback in EPUB 3: " << (opt.includeNcxFallbackInEpub3 ? L"yes" : L"no") << L"\n";
        w << L"Title: " << book.title << L"\n";
        w << L"Language: " << book.language << L"\n";
        w << L"Identifier: " << book.identifier << L"\n";
        w << L"Authors: " << JoinList(book.authors, L", ") << L"\n";
        w << L"Translators: " << JoinList(book.translators, L", ") << L"\n";
        w << L"Publisher: " << book.publisher << L"\n";
        w << L"Date: " << book.date << L"\n";
        w << L"ISBN: " << book.isbn << L"\n";
        w << L"Series: " << book.seriesName;
        if (!book.seriesNumber.empty()) w << L" #" << book.seriesNumber;
        w << L"\n";
        w << L"Cover image id: " << book.coverImageId << L"\n";
        w << L"Chapters: " << book.chapters.size() << L"\n";
        for (size_t i = 0; i < book.chapters.size(); ++i) {
            w << L"  " << (i + 1) << L". [" << book.chapters[i].epubType << L"] " << book.chapters[i].title << L"\n";
        }
        w << L"Resources: " << book.resources.size() << L"\n";
        for (const auto& r : book.resources) {
            w << L"  " << r.id << L" -> " << r.href << L" (" << fbe::epub::WideFromUtf8(r.mediaType) << L")";
            if (!r.properties.empty()) w << L" properties=" << r.properties;
            w << L"\n";
        }

        std::ofstream out(logPath, std::ios::binary | std::ios::trunc);
        const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        out.write(reinterpret_cast<const char*>(bom), 3);
        const std::string utf8 = fbe::epub::Utf8FromWide(w.str());
        out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    }
    catch (...) {
        // Export log is diagnostic only. Never fail EPUB export because of it.
        OutputDebugStringW(L"ExportEPUB: failed to write diagnostic log.\n");
    }
}

} // namespace

HRESULT CExportEPUBPlugin::Export(long hWnd, BSTR filename, IDispatch* doc)
{
    const HWND owner = reinterpret_cast<HWND>(static_cast<LONG_PTR>(hWnd));

    try {
        IXMLDOMDocument2Ptr source(doc);
        if (source != nullptr) {
            SetupNamespaces(source);
        }

        ExportPluginSettings settings = LoadExportSettings();

        CString outPath;
        fbe::epub::EpubVersion version = fbe::epub::EpubVersion::Epub3;
        if (!AskOutputFile(owner, filename, source, settings, outPath, version)) {
            return S_FALSE;
        }

        fbe::epub::EpubBook book;
        BuildBookFromFbeDocument(doc, book, version == fbe::epub::EpubVersion::Epub3, settings.includeAnnotationPage);
        ApplyPostBuildSettings(book, settings);
        if (book.chapters.empty()) {
            CString msg;
            msg.LoadString(IDS_ERROR_NO_CHAPTERS);
            ShowError(owner, msg);
            return S_FALSE;
        }

        const PreflightResult preflight = RunPreflight(book, settings);
        if (settings.showPreflightWarnings && !preflight.warnings.empty()) {
            ShowPreflightWarnings(owner, preflight);
        }

        fbe::epub::EpubExportOptions opt;
        opt.version = version;
        opt.includeNcxFallbackInEpub3 = (version == fbe::epub::EpubVersion::Epub3) && settings.includeNcxFallbackInEpub3;
        opt.includeCoverPage = settings.includeCoverPage;
        std::wstring error;
        if (!fbe::epub::EpubExporter().Export(book, std::filesystem::path((LPCWSTR)outPath), opt, &error)) {
            ShowExportError(owner, error);
            return S_FALSE;
        }

        if (settings.writeDiagnosticLog) {
            WriteExportLog(outPath, book, opt);
        }
        if (settings.showSummaryAfterExport) {
            ShowExportSummary(owner, outPath, book, opt, preflight);
        }
        if (settings.openFileAfterExport) {
            OpenPathWithShell(owner, outPath);
        } else if (settings.openFolderAfterExport) {
            OpenFolderWithShell(owner, outPath);
        }
    }
    catch (const _com_error& e) {
        std::wstring msg = e.Description().length() > 0
            ? static_cast<const wchar_t*>(e.Description())
            : std::wstring(e.ErrorMessage());
        ShowExportError(owner, msg);
        return S_FALSE;
    }
    catch (const std::exception& e) {
        ShowExportError(owner, fbe::epub::WideFromUtf8(e.what()));
        return S_FALSE;
    }
    catch (...) {
        ShowExportError(owner, L"Unknown exception");
        return S_FALSE;
    }

    return S_OK;
}


namespace {

void SetExportEpubFileError(wchar_t* buffer, unsigned bufferChars, const std::wstring& message)
{
    if (buffer == nullptr || bufferChars == 0) {
        return;
    }
    buffer[0] = L'\0';
    if (!message.empty()) {
        wcsncpy_s(buffer, bufferChars, message.c_str(), _TRUNCATE);
    }
}

std::wstring ComErrorToStringForBatch(const _com_error& e)
{
    if (e.Description().length() > 0) {
        return static_cast<const wchar_t*>(e.Description());
    }
    return e.ErrorMessage() ? std::wstring(e.ErrorMessage()) : L"COM error";
}


struct BatchComApartment
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool initialized = SUCCEEDED(hr);

    bool IsUsable() const
    {
        // RPC_E_CHANGED_MODE means that COM is already initialized on this
        // thread with another apartment model. MSXML can still be used here;
        // the important part is not to call CoUninitialize() in this case.
        return initialized || hr == RPC_E_CHANGED_MODE;
    }

    ~BatchComApartment()
    {
        if (initialized) {
            CoUninitialize();
        }
    }
};

std::wstring MsxmlParseErrorToString(IXMLDOMDocument2Ptr source)
{
    if (source == nullptr) {
        return L"MSXML could not load the input FB2 file";
    }

    IXMLDOMParseErrorPtr parseError;
    if (FAILED(source->get_parseError(&parseError)) || parseError == nullptr) {
        return L"MSXML could not load the input FB2 file";
    }

    long code = 0;
    parseError->get_errorCode(&code);
    if (code == 0) {
        return L"MSXML could not load the input FB2 file";
    }

    long line = 0;
    long linePos = 0;
    parseError->get_line(&line);
    parseError->get_linepos(&linePos);

    CComBSTR reason;
    parseError->get_reason(&reason);

    std::wstringstream ss;
    ss << L"MSXML parse error";
    if (line > 0) {
        ss << L" at line " << line;
        if (linePos > 0) {
            ss << L", position " << linePos;
        }
    }
    if (reason.Length() > 0) {
        ss << L": " << static_cast<const wchar_t*>(reason);
    }
    return ss.str();
}

} // namespace

extern "C" __declspec(dllexport) int __stdcall ExportEpubFileW(
    const wchar_t* inputFb2Path,
    const wchar_t* outputEpubPath,
    int epubVersion,
    unsigned flags,
    wchar_t* errorBuffer,
    unsigned errorBufferChars)
{
    SetExportEpubFileError(errorBuffer, errorBufferChars, L"");

    const unsigned FLAG_NCX_FALLBACK = 0x0001;
    const unsigned FLAG_COVER_PAGE = 0x0002;
    const unsigned FLAG_ANNOTATION_PAGE = 0x0004;
    const unsigned FLAG_WRITE_LOG = 0x0008;
    const unsigned FLAG_TITLE_PAGE = 0x0010;

    BatchComApartment com;
    if (!com.IsUsable()) {
        SetExportEpubFileError(errorBuffer, errorBufferChars, L"CoInitializeEx failed");
        return 10;
    }

    std::wstring phase = L"initialization";

    try {
        if (inputFb2Path == nullptr || inputFb2Path[0] == L'\0') {
            SetExportEpubFileError(errorBuffer, errorBufferChars, L"Input FB2 path is empty");
            return 1;
        }
        if (outputEpubPath == nullptr || outputEpubPath[0] == L'\0') {
            SetExportEpubFileError(errorBuffer, errorBufferChars, L"Output EPUB path is empty");
            return 2;
        }
        if (epubVersion != 2 && epubVersion != 3) {
            SetExportEpubFileError(errorBuffer, errorBufferChars, L"EPUB version must be 2 or 3");
            return 3;
        }

        phase = L"creating MSXML document";
        IXMLDOMDocument2Ptr source;
        CheckHR(source.CreateInstance(__uuidof(DOMDocument60)));
        CheckHR(source->put_async(VARIANT_FALSE));
        CheckHR(source->put_validateOnParse(VARIANT_FALSE));
        CheckHR(source->put_resolveExternals(VARIANT_FALSE));

        phase = L"loading FB2";
        VARIANT_BOOL loaded = VARIANT_FALSE;
        CheckHR(source->load(_variant_t(inputFb2Path), &loaded));
        if (loaded != VARIANT_TRUE) {
            SetExportEpubFileError(errorBuffer, errorBufferChars, MsxmlParseErrorToString(source));
            return 4;
        }

        phase = L"setting MSXML namespaces";
        SetupNamespaces(source);

        const bool isEpub3 = (epubVersion == 3);

        ExportPluginSettings settings;
        settings.includeNcxFallbackInEpub3 = (flags & FLAG_NCX_FALLBACK) != 0;
        settings.includeCoverPage = (flags & FLAG_COVER_PAGE) != 0;
        settings.includeAnnotationPage = (flags & FLAG_ANNOTATION_PAGE) != 0;
        settings.writeDiagnosticLog = (flags & FLAG_WRITE_LOG) != 0;
        settings.includeTitlePage = (flags & FLAG_TITLE_PAGE) != 0;
        settings.showPreflightWarnings = false;

        phase = L"building EPUB model";
        fbe::epub::EpubBook book;
        BuildBookFromFbeDocument(source.GetInterfacePtr(), book, isEpub3, settings.includeAnnotationPage);
        phase = L"applying export settings";
        ApplyPostBuildSettings(book, settings);
        if (book.chapters.empty()) {
            SetExportEpubFileError(errorBuffer, errorBufferChars, L"No exportable chapters found in FB2");
            return 5;
        }

        // Run lightweight diagnostics for consistency with the GUI exporter.
        // Warnings are not fatal for command-line batch conversion.
        phase = L"running preflight";
        (void)RunPreflight(book, settings);

        fbe::epub::EpubExportOptions opt;
        opt.version = isEpub3 ? fbe::epub::EpubVersion::Epub3 : fbe::epub::EpubVersion::Epub2;
        opt.includeNcxFallbackInEpub3 = isEpub3 && settings.includeNcxFallbackInEpub3;
        opt.includeCoverPage = settings.includeCoverPage;
        phase = L"writing EPUB";
        std::wstring error;
        if (!fbe::epub::EpubExporter().Export(book, std::filesystem::path(outputEpubPath), opt, &error)) {
            SetExportEpubFileError(errorBuffer, errorBufferChars, error.empty() ? L"EPUB export failed" : error);
            return 6;
        }

        if (settings.writeDiagnosticLog) {
            phase = L"writing diagnostic log";
            WriteExportLog(CString(outputEpubPath), book, opt);
        }

        return 0;
    }
    catch (const _com_error& e) {
        SetExportEpubFileError(errorBuffer, errorBufferChars, phase + L": " + ComErrorToStringForBatch(e));
        return 100;
    }
    catch (const std::exception& e) {
        SetExportEpubFileError(errorBuffer, errorBufferChars, phase + L": " + fbe::epub::WideFromUtf8(e.what()));
        return 101;
    }
    catch (...) {
        SetExportEpubFileError(errorBuffer, errorBufferChars, phase + L": Unknown exception");
        return 102;
    }
}
