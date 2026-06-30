// PVS-Studio: WTL message-map macros intentionally assign bHandled inside generated code.
//-V::1048
#include "stdafx.h"
#include "ExportDOCXPlugin.h"

#include "utils.h"

#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace {

struct ZipEntry {
    std::string name;
    DWORD crc;
    DWORD size;
    DWORD localOffset;
};

class CZipStoreWriter {
public:
    CZipStoreWriter() : m_h(INVALID_HANDLE_VALUE) {}
    ~CZipStoreWriter() { if (m_h != INVALID_HANDLE_VALUE) ::CloseHandle(m_h); }

    bool Open(LPCTSTR filename) {
        m_h = ::CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        return m_h != INVALID_HANDLE_VALUE;
    }

    bool AddText(const char* name, const CStringW& xml) {
        return AddText(name, ToUtf8(xml));
    }

    bool AddText(const char* name, const std::string& utf8) {
        return AddBytes(name, utf8.empty() ? NULL : reinterpret_cast<const BYTE*>(utf8.data()), static_cast<DWORD>(utf8.size()));
    }

    bool AddBytes(const char* name, const BYTE* data, DWORD size) {
        if (m_h == INVALID_HANDLE_VALUE) return false;

        ZipEntry e;
        e.name = name;
        e.crc = Crc32(data, size);
        e.size = size;
        e.localOffset = Tell();

        Write32(0x04034b50);       // local file header
        Write16(20);               // version needed
        Write16(0x0800);           // UTF-8 file names
        Write16(0);                // stored, no compression
        Write16(0); Write16(0);    // time/date
        Write32(e.crc);
        Write32(size);
        Write32(size);
        Write16(static_cast<WORD>(e.name.size()));
        Write16(0);
        WriteRaw(e.name.data(), static_cast<DWORD>(e.name.size()));
        if (size) WriteRaw(data, size);
        m_entries.push_back(e);
        return true;
    }

    bool Close() {
        DWORD cdOffset = Tell();
        for (size_t i = 0; i < m_entries.size(); ++i) {
            const ZipEntry& e = m_entries[i];
            Write32(0x02014b50);   // central directory
            Write16(20); Write16(20);
            Write16(0x0800);
            Write16(0);
            Write16(0); Write16(0);
            Write32(e.crc);
            Write32(e.size);
            Write32(e.size);
            Write16(static_cast<WORD>(e.name.size()));
            Write16(0); Write16(0); Write16(0); Write16(0);
            Write32(0);
            Write32(e.localOffset);
            WriteRaw(e.name.data(), static_cast<DWORD>(e.name.size()));
        }
        DWORD cdSize = Tell() - cdOffset;
        Write32(0x06054b50);
        Write16(0); Write16(0);
        Write16(static_cast<WORD>(m_entries.size()));
        Write16(static_cast<WORD>(m_entries.size()));
        Write32(cdSize);
        Write32(cdOffset);
        Write16(0);
        ::CloseHandle(m_h);
        m_h = INVALID_HANDLE_VALUE;
        return true;
    }

private:
    HANDLE m_h;
    std::vector<ZipEntry> m_entries;

    DWORD Tell() const { return ::SetFilePointer(m_h, 0, NULL, FILE_CURRENT); }
    void WriteRaw(const void* p, DWORD cb) {
        DWORD wr = 0;
        if (cb) ::WriteFile(m_h, p, cb, &wr, NULL);
    }
    void Write16(WORD v) { BYTE b[2] = { static_cast<BYTE>(v & 255), static_cast<BYTE>((v >> 8) & 255) }; WriteRaw(b, 2); }
    void Write32(DWORD v) { BYTE b[4] = { static_cast<BYTE>(v & 255), static_cast<BYTE>((v >> 8) & 255), static_cast<BYTE>((v >> 16) & 255), static_cast<BYTE>((v >> 24) & 255) }; WriteRaw(b, 4); }

    static std::string ToUtf8(const CStringW& s) {
        if (s.IsEmpty()) return std::string();
        int cb = ::WideCharToMultiByte(CP_UTF8, 0, s, s.GetLength(), NULL, 0, NULL, NULL);
        std::string out(cb, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, s, s.GetLength(), &out[0], cb, NULL, NULL);
        return out;
    }

    static DWORD Crc32(const BYTE* data, DWORD len) {
        static DWORD table[256];
        static bool ready = false;
        if (!ready) {
            for (DWORD i = 0; i < 256; ++i) {
                DWORD c = i;
                for (int k = 0; k < 8; ++k)
                    c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
                table[i] = c;
            }
            ready = true;
        }
        DWORD crc = 0xFFFFFFFF;
        for (DWORD i = 0; i < len; ++i)
            crc = table[(crc ^ (data ? data[i] : 0)) & 0xff] ^ (crc >> 8);
        return crc ^ 0xFFFFFFFF;
    }
};

std::string ToUtf8(const CStringW& s) {
    if (s.IsEmpty()) return std::string();
    int cb = ::WideCharToMultiByte(CP_UTF8, 0, s, s.GetLength(), NULL, 0, NULL, NULL);
    std::string out(cb, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, s, s.GetLength(), &out[0], cb, NULL, NULL);
    return out;
}


bool WriteUtf8TextFile(const CStringW& fileName, const CStringW& text) {
    HANDLE h = ::CreateFileW(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    static const BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
    DWORD written = 0;
    ::WriteFile(h, bom, 3, &written, NULL);

    std::string utf8 = ToUtf8(text);
    if (!utf8.empty())
        ::WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, NULL);

    ::CloseHandle(h);
    return true;
}

CStringW MakeReportFileName(const CStringW& docxFileName) {
    CStringW report(docxFileName);
    int dot = report.ReverseFind(L'.');
    if (dot >= 0)
        report.Delete(dot, report.GetLength() - dot);
    report += L"_export_report.txt";
    return report;
}

CStringW CleanTextForDocx(CStringW s);

CStringW XmlEscape(const CStringW& s) {
    CStringW r;
    for (int i = 0; i < s.GetLength(); ++i) {
        wchar_t ch = s[i];
        switch (ch) {
        case L'&': r += L"&amp;"; break;
        case L'<': r += L"&lt;"; break;
        case L'>': r += L"&gt;"; break;
        case L'\"': r += L"&quot;"; break;
        case L'\'': r += L"&apos;"; break;
        default:
            if (ch >= 0x20 || ch == L'\t' || ch == L'\n' || ch == L'\r') r += ch;
        }
    }
    return r;
}

CStringW NormalizeText(CStringW s) {
    s = CleanTextForDocx(s);
    U::NormalizeInplace(s);
    s.Replace(wchar_t(0x00A0), L' ');
    s.Replace(wchar_t(0x202F), L' ');
    return s;
}

CStringW NodeName(IXMLDOMNodePtr n) {
    CComBSTR b;
    n->get_baseName(&b);
    if (b.Length()) return CStringW(b);
    n->get_nodeName(&b);
    CStringW s(b);
    int p = s.Find(L':');
    if (p >= 0) s.Delete(0, p + 1);
    return s;
}

CStringW VariantToString(const _variant_t& v) {
    if (V_VT(&v) == VT_BSTR && V_BSTR(&v)) return CStringW(V_BSTR(&v));
    return CStringW();
}

CStringW TextNodeValue(IXMLDOMNodePtr n) {
    if (!n) return CStringW();

    // IXMLDOMNode::text in MSXML normalizes whitespace for XML text nodes.
    // For inline FB2 markup this is harmful: spaces around <emphasis>,
    // <strong>, etc. can disappear and Word then shows glued words
    // (for example: "скрытые<emphasis>численные</emphasis>отношения").
    // nodeValue returns the actual text-node content and preserves leading
    // and trailing spaces/NBSPs.
    VARIANT v;
    VariantInit(&v);
    HRESULT hr = n->get_nodeValue(&v);
    CStringW r;
    if (SUCCEEDED(hr)) {
        if (V_VT(&v) == VT_BSTR && V_BSTR(&v))
            r = V_BSTR(&v);
        else if (V_VT(&v) == (VT_BSTR | VT_BYREF) && V_BSTRREF(&v))
            r = *V_BSTRREF(&v);
    }
    VariantClear(&v);

    if (!r.IsEmpty()) return r;

    CComBSTR b;
    n->get_text(&b);
    return CStringW(b);
}

CStringW Attr(IXMLDOMElementPtr e, const wchar_t* name) {
    if (!e || !name || !*name) return CStringW();

    _variant_t v;
    if (SUCCEEDED(e->getAttribute(bstr_t(name), &v))) {
        CStringW s = VariantToString(v);
        if (!s.IsEmpty()) return s;
    }

    // In MSXML namespace-qualified FB2 attributes (for example l:href) are
    // not always returned reliably by getAttribute("href"). Therefore every
    // attribute is also checked by local name and full node name.
    IXMLDOMNamedNodeMapPtr attrs;
    if (FAILED(e->get_attributes(&attrs)) || !attrs) return CStringW();
    long n = 0;
    attrs->get_length(&n);
    for (long i = 0; i < n; ++i) {
        IXMLDOMNodePtr a;
        attrs->get_item(i, &a);
        if (!a) continue;

        CStringW local = NodeName(a);
        CComBSTR rawName;
        a->get_nodeName(&rawName);
        CStringW full(rawName);

        if (local.CompareNoCase(name) == 0 || full.CompareNoCase(name) == 0) {
            _variant_t av;
            if (SUCCEEDED(a->get_nodeValue(&av))) {
                CStringW s = VariantToString(av);
                if (!s.IsEmpty()) return s;
            }
            CComBSTR b;
            a->get_text(&b);
            return CStringW(b);
        }
    }
    return CStringW();
}

CStringW AttrAny(IXMLDOMElementPtr e, const wchar_t* name1, const wchar_t* name2) {
    CStringW v = Attr(e, name1);
    if (v.IsEmpty() && name2) v = Attr(e, name2);
    if (v.IsEmpty() && (name1 && wcschr(name1, L':'))) {
        const wchar_t* p = wcschr(name1, L':');
        v = Attr(e, p + 1);
    }
    if (v.IsEmpty() && name2 && wcschr(name2, L':')) {
        const wchar_t* p = wcschr(name2, L':');
        v = Attr(e, p + 1);
    }
    return v;
}

CStringW RawAttrFromXml(IXMLDOMNodePtr node, const wchar_t* localName) {
    if (!node || !localName || !*localName) return CStringW();
    CComBSTR xb;
    if (FAILED(node->get_xml(&xb)) || xb.Length() == 0) return CStringW();
    CStringW xml(xb);

    CStringW patterns[4];
    patterns[0].Format(L" %s=\"", localName);
    patterns[1].Format(L" %s='", localName);
    patterns[2].Format(L":%s=\"", localName);
    patterns[3].Format(L":%s='", localName);

    for (int i = 0; i < 4; ++i) {
        int p = xml.Find(patterns[i]);
        if (p < 0) continue;
        p += patterns[i].GetLength();
        wchar_t quote = (i == 1 || i == 3) ? L'\'' : L'\"';
        int e = xml.Find(quote, p);
        if (e > p) return xml.Mid(p, e - p);
    }
    return CStringW();
}

CStringW AttrAnyNode(IXMLDOMNodePtr node, const wchar_t* localName, const wchar_t* qualifiedName = NULL) {
    IXMLDOMElementPtr e;
    if (node && SUCCEEDED(node->QueryInterface(IID_PPV_ARGS(&e)))) {
        CStringW v = AttrAny(e, localName, qualifiedName);
        if (!v.IsEmpty()) return v;
    }
    return RawAttrFromXml(node, localName);
}

IXMLDOMNodePtr RootOf(IXMLDOMDocument2Ptr doc) {
    if (!doc) return NULL;
    IXMLDOMElementPtr root;
    if (FAILED(doc->get_documentElement(&root)) || !root) return NULL;
    return IXMLDOMNodePtr(root);
}

IXMLDOMNodePtr FindDescendantById(IXMLDOMNodePtr node, const wchar_t* tagName, const CStringW& id) {
    if (!node || id.IsEmpty()) return NULL;

    DOMNodeType type = NODE_INVALID;
    if (SUCCEEDED(node->get_nodeType(&type)) && type == NODE_ELEMENT) {
        CStringW nm = NodeName(node);
        IXMLDOMElementPtr e;
        if (SUCCEEDED(node->QueryInterface(IID_PPV_ARGS(&e))) && e) {
            CStringW eid = Attr(e, L"id");
            if (eid.IsEmpty()) eid = RawAttrFromXml(node, L"id");
            if (eid == id && (!tagName || !*tagName || nm.CompareNoCase(tagName) == 0))
                return node;
        }
    }

    IXMLDOMNodeListPtr kids;
    if (FAILED(node->get_childNodes(&kids)) || !kids) return NULL;
    long n = 0;
    kids->get_length(&n);
    for (long i = 0; i < n; ++i) {
        IXMLDOMNodePtr c;
        kids->get_item(i, &c);
        IXMLDOMNodePtr found = FindDescendantById(c, tagName, id);
        if (found) return found;
    }
    return NULL;
}

IXMLDOMNodePtr FindBodyByName(IXMLDOMNodePtr node, const wchar_t* wantedName) {
    if (!node || !wantedName) return NULL;

    DOMNodeType type = NODE_INVALID;
    if (SUCCEEDED(node->get_nodeType(&type)) && type == NODE_ELEMENT) {
        CStringW nm = NodeName(node);
        if (nm.CompareNoCase(L"body") == 0) {
            IXMLDOMElementPtr e;
            if (SUCCEEDED(node->QueryInterface(IID_PPV_ARGS(&e))) && e) {
                CStringW name = Attr(e, L"name");
                if (name.CompareNoCase(wantedName) == 0) return node;
            }
        }
    }

    IXMLDOMNodeListPtr kids;
    if (FAILED(node->get_childNodes(&kids)) || !kids) return NULL;
    long n = 0;
    kids->get_length(&n);
    for (long i = 0; i < n; ++i) {
        IXMLDOMNodePtr c;
        kids->get_item(i, &c);
        IXMLDOMNodePtr found = FindBodyByName(c, wantedName);
        if (found) return found;
    }
    return NULL;
}


CString ExtFromContentType(const CStringW& ct) {
    CStringW l(ct); l.MakeLower();
    if (l.Find(L"png") >= 0) return L"png";
    if (l.Find(L"jpeg") >= 0 || l.Find(L"jpg") >= 0) return L"jpg";
    if (l.Find(L"gif") >= 0) return L"gif";
    if (l.Find(L"bmp") >= 0) return L"bmp";
    if (l.Find(L"tiff") >= 0) return L"tif";
    return L"bin";
}

CStringW ContentTypeFromImageExtension(const CStringW& ext) {
    CStringW l(ext); l.MakeLower();
    if (l == L"png") return L"image/png";
    if (l == L"jpg" || l == L"jpeg") return L"image/jpeg";
    if (l == L"gif") return L"image/gif";
    if (l == L"bmp") return L"image/bmp";
    if (l == L"tif" || l == L"tiff") return L"image/tiff";
    return L"application/octet-stream";
}

CString ExtFromImageSignature(const std::vector<BYTE>& bytes) {
    if (bytes.size() >= 8 &&
        bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47 &&
        bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A) {
        return L"png";
    }
    if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
        return L"jpg";
    }
    if (bytes.size() >= 6 &&
        bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8' &&
        (bytes[4] == '7' || bytes[4] == '9') && bytes[5] == 'a') {
        return L"gif";
    }
    if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') {
        return L"bmp";
    }
    if (bytes.size() >= 4 &&
        ((bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == 0x2A && bytes[3] == 0x00) ||
         (bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0x00 && bytes[3] == 0x2A))) {
        return L"tif";
    }
    return L"bin";
}

CStringW CleanTextForDocx(CStringW s) {
    s.Replace(L"\r\n", L"\n");
    s.Replace(L"\r", L"\n");
    s.Replace(L"\t", L" " );
    // Do not turn ordinary spaces into non-breaking spaces.
    s.Replace(wchar_t(0x00A0), L' ');
    s.Replace(wchar_t(0x202F), L' ');
    return s;
}

CStringW CollapseSpaces(CStringW s) {
    s = CleanTextForDocx(s);
    CStringW out;
    bool wasSpace = false;
    for (int i = 0; i < s.GetLength(); ++i) {
        wchar_t ch = s[i];
        if (ch == L' ' || ch == L'\n') {
            if (!wasSpace) out += L' ';
            wasSpace = true;
        } else {
            out += ch;
            wasSpace = false;
        }
    }
    out.Trim();
    return out;
}

int Base64Value(wchar_t ch) {
    if (ch >= L'A' && ch <= L'Z') return ch - L'A';
    if (ch >= L'a' && ch <= L'z') return ch - L'a' + 26;
    if (ch >= L'0' && ch <= L'9') return ch - L'0' + 52;
    if (ch == L'+') return 62;
    if (ch == L'/') return 63;
    return -1;
}

bool DecodeBase64(const CStringW& input, std::vector<BYTE>& out) {
    out.clear();
    int val = 0;
    int bits = -8;
    for (int i = 0; i < input.GetLength(); ++i) {
        wchar_t ch = input[i];
        if (ch == L'=') break;
        int d = Base64Value(ch);
        if (d < 0) continue;
        val = (val << 6) + d;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<BYTE>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return !out.empty();
}

struct RunFmt {
    bool bold;
    bool italic;
    bool strike;
    bool code;
    int vert; // 0 normal, 1 superscript, -1 subscript
    RunFmt() : bold(false), italic(false), strike(false), code(false), vert(0) {}
};

struct ImagePart {
    CStringW fbId;
    CStringW relId;
    CStringW fileName;
    CStringW contentType;
    std::vector<BYTE> bytes;
    int widthPx;
    int heightPx;
    ImagePart() : widthPx(0), heightPx(0) {}
};

struct FootnotePart {
    CStringW fbId;
    int wordId;
    CStringW xml;
};

struct HyperlinkPart {
    CStringW relId;
    CStringW target;
};

struct DocxExportSettings {
    bool exportImages;
    bool exportCover;
    bool wordFootnotes;       // derived from notesMode, kept for compatibility with older code paths
    bool notesEndFallback;    // derived from notesMode, kept for compatibility with older code paths
    bool justifyText;
    bool firstLineIndent;
    bool chapterPageBreak;
    bool addToc;
    bool exportMetadata;
    bool exportHyperlinks;
    bool createBookmarks;
    bool enhancedFb2Styles;
    bool validateDocx;
    bool createReport;        // create a diagnostic TXT report next to the exported DOCX
    bool limitImageWidth;
    bool customFont;
    bool titlePage;           // create title page from FB2 description
    bool titleIncludeAnnotation; // include FB2 annotation on the generated title page
    bool titleIncludeGenres;     // include FB2 genres on the generated title page
    bool titleIncludeSeries;     // include FB2 sequence/series on the generated title page
    bool titleIncludeFb2Info;    // include technical FB2 document-info on the generated title page
    bool titleIncludeTranslators; // include translators on the title page
    bool titleIncludePublishInfo; // include publish-info on the title page
    bool titleIncludeSrcTitleInfo; // include src-title-info on the title page
    bool appendHistory;       // append document-info/history as a separate section at the end
    bool coverSeparatePage;   // put cover image on a dedicated first page
    bool coverPageBreakAfter; // insert page break after the cover image
    bool imageCaptions;       // detect and style image/table captions
    bool addTocPageBreak;     // insert a page break after Word TOC
    bool addHeaders;          // add book title to header
    bool addPageNumbers;      // add Word PAGE field to footer
    bool noTitlePageNumber;   // keep front matter without page numbers
    bool restartPageNumbering;// restart page numbering from the main text section
    bool autoHyphenation;     // enable Word automatic hyphenation
    bool openAfterExport;     // open the created DOCX after successful export
    int notesMode;            // 0 = Word footnotes, 1 = Word endnotes, 2 = separate notes section
    int exportPreset;         // 0 = book DOCX, 1 = minimal DOCX, 2 = editorial DOCX
    int styleProfile;         // 0 = book, 1 = compact, 2 = minimal
    int pageSize;             // 0 = A4, 1 = A5, 2 = Letter
    int docLanguage;          // 0 = ru-RU, 1 = en-US, 2 = auto from FB2 <lang>
    int tocDepth;             // maximum heading depth included into Word TOC, 1..4
    int emptyLineMode;        // 0 = ignore, 1 = empty paragraph, 2 = add spacing before next paragraph
    int imageMaxWidthCm;      // maximum image width in centimeters when limitImageWidth is enabled
    int fontSizePt;           // custom font size in points
    CStringW fontName;        // custom font name

    DocxExportSettings()
        : exportImages(true), exportCover(true), wordFootnotes(true),
          notesEndFallback(false), justifyText(true), firstLineIndent(true),
          chapterPageBreak(false), addToc(false), exportMetadata(true),
          exportHyperlinks(true), createBookmarks(true), enhancedFb2Styles(true),
          validateDocx(true), createReport(true), limitImageWidth(true), customFont(false),
          titlePage(true), titleIncludeAnnotation(true), titleIncludeGenres(true),
          titleIncludeSeries(true), titleIncludeFb2Info(false), titleIncludeTranslators(true),
          titleIncludePublishInfo(true), titleIncludeSrcTitleInfo(true), appendHistory(false),
          coverSeparatePage(true), coverPageBreakAfter(true), imageCaptions(true), addTocPageBreak(true),
          addHeaders(false), addPageNumbers(true), noTitlePageNumber(true), restartPageNumbering(true),
          autoHyphenation(false), openAfterExport(false),
          notesMode(0), exportPreset(0), styleProfile(0), pageSize(0), docLanguage(0), tocDepth(3), emptyLineMode(2),
          imageMaxWidthCm(14), fontSizePt(12), fontName(L"Times New Roman") {}
};

static const wchar_t* kSettingsRegPath = L"Software\\FBEditor\\ExportDOCX";

DWORD ReadRegDword(HKEY root, const wchar_t* subkey, const wchar_t* name, DWORD defValue) {
    CRegKey key;
    if (key.Open(root, subkey, KEY_READ) != ERROR_SUCCESS) return defValue;
    DWORD value = defValue;
    if (key.QueryDWORDValue(name, value) != ERROR_SUCCESS) return defValue;
    return value;
}

void WriteRegDword(HKEY root, const wchar_t* subkey, const wchar_t* name, DWORD value) {
    CRegKey key;
    if (key.Create(root, subkey) == ERROR_SUCCESS) key.SetDWORDValue(name, value);
}

CStringW ReadRegString(HKEY root, const wchar_t* subkey, const wchar_t* name, const wchar_t* defValue) {
    CRegKey key;
    if (key.Open(root, subkey, KEY_READ) != ERROR_SUCCESS) return CStringW(defValue ? defValue : L"");
    ULONG chars = 0;
    if (key.QueryStringValue(name, NULL, &chars) != ERROR_SUCCESS || chars == 0)
        return CStringW(defValue ? defValue : L"");
    CStringW value;
    wchar_t* buf = value.GetBuffer(static_cast<int>(chars));
    LONG res = key.QueryStringValue(name, buf, &chars);
    value.ReleaseBuffer();
    if (res != ERROR_SUCCESS) return CStringW(defValue ? defValue : L"");
    return value;
}

void WriteRegString(HKEY root, const wchar_t* subkey, const wchar_t* name, const CStringW& value) {
    CRegKey key;
    if (key.Create(root, subkey) == ERROR_SUCCESS) key.SetStringValue(name, value);
}


static bool CStringEqualsNoCase(const CStringW& a, const CStringW& b) {
    return a.CompareNoCase(b) == 0;
}

static bool ContainsFontName(const std::vector<CStringW>& fonts, const CStringW& fontName) {
    for (size_t i = 0; i < fonts.size(); ++i) {
        if (CStringEqualsNoCase(fonts[i], fontName))
            return true;
    }
    return false;
}

static int CALLBACK EnumFontFamilyProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lParam) {
    std::vector<CStringW>* fonts = reinterpret_cast<std::vector<CStringW>*>(lParam);
    if (fonts == NULL || lf == NULL)
        return 1;

    CStringW name(lf->lfFaceName);
    name.Trim();
    if (name.IsEmpty())
        return 1;

    // Vertical CJK fonts are registered with an '@' prefix and should not be shown in a Word-like font picker.
    if (name[0] == L'@')
        return 1;

    if (!ContainsFontName(*fonts, name))
        fonts->push_back(name);
    return 1;
}

static void EnumerateInstalledFonts(std::vector<CStringW>& fonts) {
    fonts.clear();

    HDC hdc = ::GetDC(NULL);
    if (hdc != NULL) {
        LOGFONTW lf = {};
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfFaceName[0] = L'\0';
        ::EnumFontFamiliesExW(hdc, &lf, reinterpret_cast<FONTENUMPROCW>(EnumFontFamilyProc), reinterpret_cast<LPARAM>(&fonts), 0);
        ::ReleaseDC(NULL, hdc);
    }

    std::sort(fonts.begin(), fonts.end(), [](const CStringW& a, const CStringW& b) {
        return a.CompareNoCase(b) < 0;
    });
}

static bool IsFontInstalled(const CStringW& fontName) {
    if (fontName.IsEmpty())
        return false;
    std::vector<CStringW> fonts;
    EnumerateInstalledFonts(fonts);
    return ContainsFontName(fonts, fontName);
}

static CStringW IntToString(int value) {
    CStringW s;
    s.Format(L"%d", value);
    return s;
}

static CStringW PreferredAvailableFont(const wchar_t* first, const wchar_t* second, const wchar_t* third) {
    std::vector<CStringW> fonts;
    EnumerateInstalledFonts(fonts);
    const wchar_t* candidates[] = { first, second, third, L"Times New Roman", L"Calibri", L"Arial" };
    for (int i = 0; i < _countof(candidates); ++i) {
        if (candidates[i] != NULL && ContainsFontName(fonts, CStringW(candidates[i])))
            return CStringW(candidates[i]);
    }
    return fonts.empty() ? CStringW(L"Times New Roman") : fonts[0];
}

static CStringW DefaultFontForProfile(int profile) {
    if (profile == 2)
        return PreferredAvailableFont(L"Aptos", L"Calibri", L"Arial");
    return PreferredAvailableFont(L"Times New Roman", L"Georgia", L"Cambria");
}

static int DefaultFontSizeForProfile(int profile) {
    if (profile == 1) return 11;
    if (profile == 2) return 11;
    return 12;
}


void ApplyExportPreset(DocxExportSettings& s, int preset) {
    if (preset < 0 || preset > 2) preset = 0;
    s.exportPreset = preset;

    // 0: Book DOCX - a balanced reader-friendly document.
    // 1: Minimal DOCX - mostly source text/images with minimal front matter.
    // 2: Editorial DOCX - maximum metadata, diagnostics and navigation for review/editing.
    if (preset == 1) {
        s.exportImages = true;
        s.exportCover = false;
        s.titlePage = false;
        s.titleIncludeAnnotation = false;
        s.titleIncludeGenres = false;
        s.titleIncludeSeries = false;
        s.titleIncludeFb2Info = false;
        s.titleIncludeTranslators = false;
        s.titleIncludePublishInfo = false;
        s.titleIncludeSrcTitleInfo = false;
        s.appendHistory = false;
        s.addToc = false;
        s.exportMetadata = true;
        s.exportHyperlinks = false;
        s.createBookmarks = false;
        s.enhancedFb2Styles = false;
        s.validateDocx = true;
        s.createReport = false;
        s.addHeaders = false;
        s.addPageNumbers = false;
        s.noTitlePageNumber = false;
        s.restartPageNumbering = false;
        s.autoHyphenation = false;
        s.notesMode = 2;
        s.styleProfile = 2;
        s.emptyLineMode = 1;
        s.coverSeparatePage = false;
        s.coverPageBreakAfter = false;
        s.imageCaptions = false;
        s.addTocPageBreak = false;
        s.customFont = false;
        return;
    }

    s.exportImages = true;
    s.exportCover = true;
    s.titlePage = true;
    s.titleIncludeAnnotation = true;
    s.titleIncludeGenres = true;
    s.titleIncludeSeries = true;
    s.titleIncludeTranslators = true;
    s.titleIncludePublishInfo = true;
    s.titleIncludeSrcTitleInfo = (preset == 2);
    s.titleIncludeFb2Info = (preset == 2);
    s.appendHistory = (preset == 2);
    s.addToc = true;
    s.exportMetadata = true;
    s.exportHyperlinks = true;
    s.createBookmarks = true;
    s.enhancedFb2Styles = true;
    s.validateDocx = true;
    s.createReport = true;
    s.addHeaders = (preset == 2);
    s.addPageNumbers = true;
    s.noTitlePageNumber = true;
    s.restartPageNumbering = true;
    s.autoHyphenation = false;
    s.notesMode = 0;
    s.styleProfile = 0;
    s.emptyLineMode = 2;
    s.coverSeparatePage = true;
    s.coverPageBreakAfter = true;
    s.imageCaptions = true;
    s.addTocPageBreak = true;
    s.customFont = false;
}

void NormalizeSettings(DocxExportSettings& s) {
    if (s.notesMode < 0 || s.notesMode > 2) s.notesMode = 0;
    if (s.exportPreset < 0 || s.exportPreset > 2) s.exportPreset = 0;
    if (s.styleProfile < 0 || s.styleProfile > 2) s.styleProfile = 0;
    if (s.pageSize < 0 || s.pageSize > 2) s.pageSize = 0;
    if (s.docLanguage < 0 || s.docLanguage > 2) s.docLanguage = 0;
    if (s.tocDepth < 1 || s.tocDepth > 6) s.tocDepth = 3;
    if (s.emptyLineMode < 0 || s.emptyLineMode > 3) s.emptyLineMode = 2;
    if (s.imageMaxWidthCm < 4) s.imageMaxWidthCm = 4;
    if (s.imageMaxWidthCm > 21) s.imageMaxWidthCm = 21;
    if (s.fontSizePt < 6) s.fontSizePt = 6;
    if (s.fontSizePt > 72) s.fontSizePt = 72;
    s.fontName.Trim();
    if (s.fontName.IsEmpty() || !IsFontInstalled(s.fontName))
        s.fontName = DefaultFontForProfile(s.styleProfile);
    s.wordFootnotes = (s.notesMode == 0);
    s.notesEndFallback = (s.notesMode == 2);
}

void LoadDocxSettings(DocxExportSettings& s) {
    s.exportImages = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportImages", s.exportImages ? 1 : 0) != 0;
    s.exportCover = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportCover", s.exportCover ? 1 : 0) != 0;

    DWORD legacyFootnotes = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"WordFootnotes", s.wordFootnotes ? 1 : 0);
    DWORD legacyEndSection = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"NotesEndFallback", s.notesEndFallback ? 1 : 0);
    s.notesMode = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"NotesMode", legacyFootnotes ? 0 : (legacyEndSection ? 2 : 0)));

    s.justifyText = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"JustifyText", s.justifyText ? 1 : 0) != 0;
    s.firstLineIndent = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"FirstLineIndent", s.firstLineIndent ? 1 : 0) != 0;
    s.chapterPageBreak = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ChapterPageBreak", s.chapterPageBreak ? 1 : 0) != 0;
    s.addToc = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AddToc", s.addToc ? 1 : 0) != 0;
    s.exportMetadata = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportMetadata", s.exportMetadata ? 1 : 0) != 0;
    s.exportHyperlinks = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportHyperlinks", s.exportHyperlinks ? 1 : 0) != 0;
    s.createBookmarks = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CreateBookmarks", s.createBookmarks ? 1 : 0) != 0;
    s.enhancedFb2Styles = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"EnhancedFb2Styles", s.enhancedFb2Styles ? 1 : 0) != 0;
    s.validateDocx = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ValidateDocx", s.validateDocx ? 1 : 0) != 0;
    s.createReport = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CreateReport", s.createReport ? 1 : 0) != 0;
    s.limitImageWidth = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"LimitImageWidth", s.limitImageWidth ? 1 : 0) != 0;
    s.customFont = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CustomFont", s.customFont ? 1 : 0) != 0;
    s.titlePage = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitlePage", s.titlePage ? 1 : 0) != 0;
    s.titleIncludeAnnotation = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeAnnotation", s.titleIncludeAnnotation ? 1 : 0) != 0;
    s.titleIncludeGenres = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeGenres", s.titleIncludeGenres ? 1 : 0) != 0;
    s.titleIncludeSeries = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeSeries", s.titleIncludeSeries ? 1 : 0) != 0;
    s.titleIncludeFb2Info = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeFb2Info", s.titleIncludeFb2Info ? 1 : 0) != 0;
    s.titleIncludeTranslators = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeTranslators", s.titleIncludeTranslators ? 1 : 0) != 0;
    s.titleIncludePublishInfo = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludePublishInfo", s.titleIncludePublishInfo ? 1 : 0) != 0;
    s.titleIncludeSrcTitleInfo = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeSrcTitleInfo", s.titleIncludeSrcTitleInfo ? 1 : 0) != 0;
    s.appendHistory = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AppendHistory", s.appendHistory ? 1 : 0) != 0;
    s.coverSeparatePage = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CoverSeparatePage", s.coverSeparatePage ? 1 : 0) != 0;
    s.coverPageBreakAfter = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CoverPageBreakAfter", s.coverPageBreakAfter ? 1 : 0) != 0;
    s.imageCaptions = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ImageCaptions", s.imageCaptions ? 1 : 0) != 0;
    s.addTocPageBreak = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AddTocPageBreak", s.addTocPageBreak ? 1 : 0) != 0;
    s.addHeaders = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AddHeaders", s.addHeaders ? 1 : 0) != 0;
    s.addPageNumbers = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AddPageNumbers", s.addPageNumbers ? 1 : 0) != 0;
    s.noTitlePageNumber = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"NoTitlePageNumber", s.noTitlePageNumber ? 1 : 0) != 0;
    s.restartPageNumbering = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"RestartPageNumbering", s.restartPageNumbering ? 1 : 0) != 0;
    s.autoHyphenation = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AutoHyphenation", s.autoHyphenation ? 1 : 0) != 0;
    s.openAfterExport = ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"OpenAfterExport", s.openAfterExport ? 1 : 0) != 0;
    s.exportPreset = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportPreset", static_cast<DWORD>(s.exportPreset)));
    s.styleProfile = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"StyleProfile", static_cast<DWORD>(s.styleProfile)));
    s.pageSize = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"PageSize", static_cast<DWORD>(s.pageSize)));
    s.docLanguage = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"DocLanguage", static_cast<DWORD>(s.docLanguage)));
    s.tocDepth = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TocDepth", static_cast<DWORD>(s.tocDepth)));
    s.emptyLineMode = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"EmptyLineMode", static_cast<DWORD>(s.emptyLineMode)));
    s.imageMaxWidthCm = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ImageMaxWidthCm", static_cast<DWORD>(s.imageMaxWidthCm)));
    s.fontSizePt = static_cast<int>(ReadRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"FontSizePt", static_cast<DWORD>(s.fontSizePt)));
    s.fontName = ReadRegString(HKEY_CURRENT_USER, kSettingsRegPath, L"FontName", s.fontName);
    NormalizeSettings(s);
}

void SaveDocxSettings(DocxExportSettings s) {
    NormalizeSettings(s);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportImages", s.exportImages ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportCover", s.exportCover ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"NotesMode", static_cast<DWORD>(s.notesMode));
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"WordFootnotes", s.wordFootnotes ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"NotesEndFallback", s.notesEndFallback ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"JustifyText", s.justifyText ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"FirstLineIndent", s.firstLineIndent ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ChapterPageBreak", s.chapterPageBreak ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AddToc", s.addToc ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportMetadata", s.exportMetadata ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportHyperlinks", s.exportHyperlinks ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CreateBookmarks", s.createBookmarks ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"EnhancedFb2Styles", s.enhancedFb2Styles ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ValidateDocx", s.validateDocx ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CreateReport", s.createReport ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"LimitImageWidth", s.limitImageWidth ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CustomFont", s.customFont ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitlePage", s.titlePage ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeAnnotation", s.titleIncludeAnnotation ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeGenres", s.titleIncludeGenres ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeSeries", s.titleIncludeSeries ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeFb2Info", s.titleIncludeFb2Info ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeTranslators", s.titleIncludeTranslators ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludePublishInfo", s.titleIncludePublishInfo ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TitleIncludeSrcTitleInfo", s.titleIncludeSrcTitleInfo ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AppendHistory", s.appendHistory ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CoverSeparatePage", s.coverSeparatePage ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"CoverPageBreakAfter", s.coverPageBreakAfter ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ImageCaptions", s.imageCaptions ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AddTocPageBreak", s.addTocPageBreak ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AddHeaders", s.addHeaders ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AddPageNumbers", s.addPageNumbers ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"NoTitlePageNumber", s.noTitlePageNumber ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"RestartPageNumbering", s.restartPageNumbering ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"AutoHyphenation", s.autoHyphenation ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"OpenAfterExport", s.openAfterExport ? 1 : 0);
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ExportPreset", static_cast<DWORD>(s.exportPreset));
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"StyleProfile", static_cast<DWORD>(s.styleProfile));
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"PageSize", static_cast<DWORD>(s.pageSize));
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"DocLanguage", static_cast<DWORD>(s.docLanguage));
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"TocDepth", static_cast<DWORD>(s.tocDepth));
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"EmptyLineMode", static_cast<DWORD>(s.emptyLineMode));
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"ImageMaxWidthCm", static_cast<DWORD>(s.imageMaxWidthCm));
    WriteRegDword(HKEY_CURRENT_USER, kSettingsRegPath, L"FontSizePt", static_cast<DWORD>(s.fontSizePt));
    WriteRegString(HKEY_CURRENT_USER, kSettingsRegPath, L"FontName", s.fontName);
}

class CDocxSettingsDialog : public CDialogImpl<CDocxSettingsDialog> {
public:
    enum { IDD = IDD_DOCX_SETTINGS };

    explicit CDocxSettingsDialog(DocxExportSettings& settings) : m_settings(settings), m_original(settings), m_hToolTip(NULL) {}

    // PVS-Studio: BEGIN_MSG_MAP/MESSAGE_HANDLER are ATL message-map macros.
    // The generated bHandled assignments are intentional and controlled by ATL.
    //-V1048
    BEGIN_MSG_MAP(CDocxSettingsDialog) //-V1048
        //-V1048
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog) //-V1048
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColor)
        MESSAGE_HANDLER(WM_CTLCOLORBTN, OnCtlColor)
        MESSAGE_HANDLER(WM_CTLCOLORDLG, OnCtlColorDlg)
        NOTIFY_HANDLER(IDC_SETTINGS_TAB, TCN_SELCHANGE, OnTabChanged)
        COMMAND_ID_HANDLER(IDOK, OnOK)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
        COMMAND_ID_HANDLER(IDC_RESET_DEFAULTS, OnDefaults)
        COMMAND_ID_HANDLER(IDC_EXPORT_IMAGES, OnOptionChanged)
        COMMAND_ID_HANDLER(IDC_LIMIT_IMAGE_WIDTH, OnOptionChanged)
        COMMAND_ID_HANDLER(IDC_TITLE_PAGE, OnOptionChanged)
        COMMAND_ID_HANDLER(IDC_ADD_TOC, OnOptionChanged)
        COMMAND_ID_HANDLER(IDC_ADD_PAGE_NUMBERS, OnOptionChanged)
        COMMAND_ID_HANDLER(IDC_CUSTOM_FONT, OnCustomFontChanged)
        COMMAND_ID_HANDLER(IDC_PRESET_BOOK, OnPresetButton)
        COMMAND_ID_HANDLER(IDC_PRESET_MINIMAL, OnPresetButton)
        COMMAND_ID_HANDLER(IDC_PRESET_EDITORIAL, OnPresetButton)
        COMMAND_HANDLER(IDC_STYLE_PROFILE, CBN_SELCHANGE, OnProfileChanged)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    END_MSG_MAP()

private:
    DocxExportSettings& m_settings;
    DocxExportSettings m_original;
    HWND m_hToolTip;

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
        NormalizeSettings(m_settings);
        if (::IsThemeActive() && ::IsAppThemed()) {
            ::EnableThemeDialogTexture(m_hWnd, ETDT_ENABLETAB);
        }
        InitTabs();
        CheckDlgButton(IDC_EXPORT_IMAGES, m_settings.exportImages ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_EXPORT_COVER, m_settings.exportCover ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_LIMIT_IMAGE_WIDTH, m_settings.limitImageWidth ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(IDC_IMAGE_MAX_WIDTH_CM, m_settings.imageMaxWidthCm, FALSE);

        SendDlgItemMessage(IDC_NOTES_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Сноски Word внизу страницы"));
        SendDlgItemMessage(IDC_NOTES_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Концевые примечания Word"));
        SendDlgItemMessage(IDC_NOTES_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Раздел «Примечания» в конце документа"));
        SendDlgItemMessage(IDC_NOTES_MODE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.notesMode), 0);

        CheckDlgButton(IDC_JUSTIFY_TEXT, m_settings.justifyText ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_FIRST_LINE_INDENT, m_settings.firstLineIndent ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_CHAPTER_PAGE_BREAK, m_settings.chapterPageBreak ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_ADD_TOC, m_settings.addToc ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_EXPORT_METADATA, m_settings.exportMetadata ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_EXPORT_HYPERLINKS, m_settings.exportHyperlinks ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_CREATE_BOOKMARKS, m_settings.createBookmarks ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_ENHANCED_FB2_STYLES, m_settings.enhancedFb2Styles ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_VALIDATE_DOCX, m_settings.validateDocx ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_CREATE_REPORT, m_settings.createReport ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_CUSTOM_FONT, m_settings.customFont ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_PAGE, m_settings.titlePage ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_INCLUDE_ANNOTATION, m_settings.titleIncludeAnnotation ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_INCLUDE_GENRES, m_settings.titleIncludeGenres ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_INCLUDE_SERIES, m_settings.titleIncludeSeries ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_INCLUDE_FB2_INFO, m_settings.titleIncludeFb2Info ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_ADD_HEADERS, m_settings.addHeaders ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_ADD_PAGE_NUMBERS, m_settings.addPageNumbers ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_NO_TITLE_PAGE_NUMBER, m_settings.noTitlePageNumber ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_RESTART_PAGE_NUMBERING, m_settings.restartPageNumbering ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_AUTO_HYPHENATION, m_settings.autoHyphenation ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_OPEN_AFTER_EXPORT, m_settings.openAfterExport ? BST_CHECKED : BST_UNCHECKED);

        SendDlgItemMessage(IDC_TOC_DEPTH, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1"));
        SendDlgItemMessage(IDC_TOC_DEPTH, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2"));
        SendDlgItemMessage(IDC_TOC_DEPTH, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"3"));
        SendDlgItemMessage(IDC_TOC_DEPTH, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"4"));
        SendDlgItemMessage(IDC_TOC_DEPTH, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"5"));
        SendDlgItemMessage(IDC_TOC_DEPTH, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"6"));
        SendDlgItemMessage(IDC_TOC_DEPTH, CB_SETCURSEL, static_cast<WPARAM>(m_settings.tocDepth - 1), 0);

        SendDlgItemMessage(IDC_EMPTY_LINE_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Игнорировать"));
        SendDlgItemMessage(IDC_EMPTY_LINE_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Пустой абзац"));
        SendDlgItemMessage(IDC_EMPTY_LINE_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Интервал"));
        SendDlgItemMessage(IDC_EMPTY_LINE_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Разделитель * * *"));
        SendDlgItemMessage(IDC_EMPTY_LINE_MODE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.emptyLineMode), 0);

        SendDlgItemMessage(IDC_PAGE_SIZE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"A4"));
        SendDlgItemMessage(IDC_PAGE_SIZE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"A5"));
        SendDlgItemMessage(IDC_PAGE_SIZE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Letter"));
        SendDlgItemMessage(IDC_PAGE_SIZE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.pageSize), 0);

        SendDlgItemMessage(IDC_DOC_LANGUAGE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Русский (ru-RU)"));
        SendDlgItemMessage(IDC_DOC_LANGUAGE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Английский (en-US)"));
        SendDlgItemMessage(IDC_DOC_LANGUAGE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Авто из FB2 (<lang>)"));
        SendDlgItemMessage(IDC_DOC_LANGUAGE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.docLanguage), 0);

        SendDlgItemMessage(IDC_STYLE_PROFILE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Книжный"));
        SendDlgItemMessage(IDC_STYLE_PROFILE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Компактный"));
        SendDlgItemMessage(IDC_STYLE_PROFILE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Минимальный"));
        SendDlgItemMessage(IDC_STYLE_PROFILE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.styleProfile), 0);

        FillFontCombo(m_settings.customFont ? m_settings.fontName : DefaultFontForProfile(m_settings.styleProfile));
        SetDlgItemInt(IDC_FONT_SIZE, m_settings.customFont ? m_settings.fontSizePt : DefaultFontSizeForProfile(m_settings.styleProfile), FALSE);
        UpdateControlStates();
        InitTooltips();
        ShowSettingsTab(0);
        CenterWindow(GetParent());
        return TRUE;
    }

    LRESULT OnCtlColorDlg(UINT, WPARAM, LPARAM, BOOL&) {
        return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_BTNFACE));
    }

    LRESULT OnCtlColor(UINT, WPARAM wParam, LPARAM, BOOL&) {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        if (hdc != NULL) {
            ::SetBkMode(hdc, TRANSPARENT);
        }
        return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_BTNFACE));
    }

    LRESULT OnOK(WORD, WORD, HWND, BOOL&) {
        m_settings.exportImages = IsDlgButtonChecked(IDC_EXPORT_IMAGES) == BST_CHECKED;
        m_settings.exportCover = IsDlgButtonChecked(IDC_EXPORT_COVER) == BST_CHECKED;
        m_settings.limitImageWidth = IsDlgButtonChecked(IDC_LIMIT_IMAGE_WIDTH) == BST_CHECKED;
        m_settings.imageMaxWidthCm = ::GetDlgItemInt(m_hWnd, IDC_IMAGE_MAX_WIDTH_CM, NULL, FALSE);
        LRESULT notesMode = SendDlgItemMessage(IDC_NOTES_MODE, CB_GETCURSEL, 0, 0);
        m_settings.notesMode = (notesMode >= 0 && notesMode <= 2) ? static_cast<int>(notesMode) : 0;
        m_settings.justifyText = IsDlgButtonChecked(IDC_JUSTIFY_TEXT) == BST_CHECKED;
        m_settings.firstLineIndent = IsDlgButtonChecked(IDC_FIRST_LINE_INDENT) == BST_CHECKED;
        m_settings.chapterPageBreak = IsDlgButtonChecked(IDC_CHAPTER_PAGE_BREAK) == BST_CHECKED;
        m_settings.addToc = IsDlgButtonChecked(IDC_ADD_TOC) == BST_CHECKED;
        m_settings.exportMetadata = IsDlgButtonChecked(IDC_EXPORT_METADATA) == BST_CHECKED;
        m_settings.exportHyperlinks = IsDlgButtonChecked(IDC_EXPORT_HYPERLINKS) == BST_CHECKED;
        m_settings.createBookmarks = IsDlgButtonChecked(IDC_CREATE_BOOKMARKS) == BST_CHECKED;
        m_settings.enhancedFb2Styles = IsDlgButtonChecked(IDC_ENHANCED_FB2_STYLES) == BST_CHECKED;
        m_settings.validateDocx = IsDlgButtonChecked(IDC_VALIDATE_DOCX) == BST_CHECKED;
        m_settings.createReport = IsDlgButtonChecked(IDC_CREATE_REPORT) == BST_CHECKED;
        m_settings.customFont = IsDlgButtonChecked(IDC_CUSTOM_FONT) == BST_CHECKED;
        m_settings.titlePage = IsDlgButtonChecked(IDC_TITLE_PAGE) == BST_CHECKED;
        m_settings.titleIncludeAnnotation = IsDlgButtonChecked(IDC_TITLE_INCLUDE_ANNOTATION) == BST_CHECKED;
        m_settings.titleIncludeGenres = IsDlgButtonChecked(IDC_TITLE_INCLUDE_GENRES) == BST_CHECKED;
        m_settings.titleIncludeSeries = IsDlgButtonChecked(IDC_TITLE_INCLUDE_SERIES) == BST_CHECKED;
        m_settings.titleIncludeFb2Info = IsDlgButtonChecked(IDC_TITLE_INCLUDE_FB2_INFO) == BST_CHECKED;
        m_settings.addHeaders = IsDlgButtonChecked(IDC_ADD_HEADERS) == BST_CHECKED;
        m_settings.addPageNumbers = IsDlgButtonChecked(IDC_ADD_PAGE_NUMBERS) == BST_CHECKED;
        m_settings.noTitlePageNumber = IsDlgButtonChecked(IDC_NO_TITLE_PAGE_NUMBER) == BST_CHECKED;
        m_settings.restartPageNumbering = IsDlgButtonChecked(IDC_RESTART_PAGE_NUMBERING) == BST_CHECKED;
        m_settings.autoHyphenation = IsDlgButtonChecked(IDC_AUTO_HYPHENATION) == BST_CHECKED;
        m_settings.openAfterExport = IsDlgButtonChecked(IDC_OPEN_AFTER_EXPORT) == BST_CHECKED;
        m_settings.fontName = GetSelectedFontName();
        m_settings.fontSizePt = ::GetDlgItemInt(m_hWnd, IDC_FONT_SIZE, NULL, FALSE);
        if (m_settings.exportPreset < 0 || m_settings.exportPreset > 2)
            m_settings.exportPreset = 0;
        LRESULT profile = SendDlgItemMessage(IDC_STYLE_PROFILE, CB_GETCURSEL, 0, 0);
        m_settings.styleProfile = (profile >= 0 && profile <= 2) ? static_cast<int>(profile) : 0;
        LRESULT pageSize = SendDlgItemMessage(IDC_PAGE_SIZE, CB_GETCURSEL, 0, 0);
        m_settings.pageSize = (pageSize >= 0 && pageSize <= 2) ? static_cast<int>(pageSize) : 0;
        LRESULT lang = SendDlgItemMessage(IDC_DOC_LANGUAGE, CB_GETCURSEL, 0, 0);
        m_settings.docLanguage = (lang >= 0 && lang <= 2) ? static_cast<int>(lang) : 0;
        LRESULT tocDepth = SendDlgItemMessage(IDC_TOC_DEPTH, CB_GETCURSEL, 0, 0);
        m_settings.tocDepth = (tocDepth >= 0 && tocDepth <= 5) ? static_cast<int>(tocDepth) + 1 : 3;
        LRESULT emptyLineMode = SendDlgItemMessage(IDC_EMPTY_LINE_MODE, CB_GETCURSEL, 0, 0);
        m_settings.emptyLineMode = (emptyLineMode >= 0 && emptyLineMode <= 3) ? static_cast<int>(emptyLineMode) : 2;
        NormalizeSettings(m_settings);
        SaveDocxSettings(m_settings);
        EndDialog(IDOK);
        return 0;
    }

    LRESULT OnCancel(WORD, WORD, HWND, BOOL&) {
        m_settings = m_original;
        EndDialog(IDCANCEL);
        return 0;
    }

    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
        if (m_hToolTip != NULL) {
            ::DestroyWindow(m_hToolTip);
            m_hToolTip = NULL;
        }
        return 0;
    }

    LRESULT OnOptionChanged(WORD, WORD, HWND, BOOL&) {
        UpdateControlStates();
        return 0;
    }

    LRESULT OnCustomFontChanged(WORD, WORD, HWND, BOOL&) {
        if (IsDlgButtonChecked(IDC_CUSTOM_FONT) == BST_CHECKED) {
            CStringW current = GetSelectedFontName();
            if (current.IsEmpty())
                FillFontCombo(DefaultFontForProfile(GetSelectedProfile()));
        } else {
            ApplyProfileDefaultsToFontControls();
        }
        UpdateControlStates();
        return 0;
    }

    LRESULT OnPresetButton(WORD, WORD id, HWND, BOOL&) {
        int p = 0;
        if (id == IDC_PRESET_MINIMAL)
            p = 1;
        else if (id == IDC_PRESET_EDITORIAL)
            p = 2;

        ApplyExportPreset(m_settings, p);
        NormalizeSettings(m_settings);
        ApplySettingsToControls();
        UpdateControlStates();
        return 0;
    }

    LRESULT OnProfileChanged(WORD, WORD, HWND, BOOL&) {
        if (IsDlgButtonChecked(IDC_CUSTOM_FONT) != BST_CHECKED)
            ApplyProfileDefaultsToFontControls();
        return 0;
    }

    void InitTabs() {
        HWND hTab = GetDlgItem(IDC_SETTINGS_TAB);
        if (hTab == NULL) return;
        TCITEM item = { 0 };
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(L"Основное"); TabCtrl_InsertItem(hTab, 0, &item);
        item.pszText = const_cast<LPWSTR>(L"Примечания"); TabCtrl_InsertItem(hTab, 1, &item);
        item.pszText = const_cast<LPWSTR>(L"Оформление"); TabCtrl_InsertItem(hTab, 2, &item);
        item.pszText = const_cast<LPWSTR>(L"Дополнительно"); TabCtrl_InsertItem(hTab, 3, &item);
        TabCtrl_SetCurSel(hTab, 0);
    }

    LRESULT OnTabChanged(int, LPNMHDR, BOOL&) {
        int tab = static_cast<int>(SendDlgItemMessage(IDC_SETTINGS_TAB, TCM_GETCURSEL, 0, 0));
        ShowSettingsTab(tab < 0 ? 0 : tab);
        return 0;
    }

    void ShowIds(const UINT* ids, size_t count, BOOL show) {
        for (size_t i = 0; i < count; ++i) {
            HWND h = GetDlgItem(ids[i]);
            if (h != NULL) ::ShowWindow(h, show ? SW_SHOW : SW_HIDE);
        }
    }

    void ShowSettingsTab(int tab) {
        static const UINT page0[] = { IDC_GRP_IMAGES, IDC_EXPORT_IMAGES, IDC_EXPORT_COVER, IDC_LIMIT_IMAGE_WIDTH, IDC_LBL_IMAGE_WIDTH, IDC_IMAGE_MAX_WIDTH_CM, IDC_TITLE_PAGE, IDC_TITLE_INCLUDE_ANNOTATION, IDC_TITLE_INCLUDE_GENRES, IDC_TITLE_INCLUDE_SERIES, IDC_TITLE_INCLUDE_FB2_INFO, IDC_ADD_TOC, IDC_LBL_TOC_DEPTH, IDC_TOC_DEPTH, IDC_CREATE_BOOKMARKS, IDC_EXPORT_HYPERLINKS };
        static const UINT page1[] = { IDC_GRP_NOTES, IDC_LBL_NOTES_MODE, IDC_NOTES_MODE };
        static const UINT page2[] = { IDC_GRP_FORMAT, IDC_LBL_EXPORT_PRESET, IDC_PRESET_BOOK, IDC_PRESET_MINIMAL, IDC_PRESET_EDITORIAL, IDC_JUSTIFY_TEXT, IDC_FIRST_LINE_INDENT, IDC_CHAPTER_PAGE_BREAK, IDC_ENHANCED_FB2_STYLES, IDC_LBL_STYLE_PROFILE, IDC_STYLE_PROFILE, IDC_CUSTOM_FONT, IDC_LBL_FONT_NAME, IDC_FONT_NAME, IDC_LBL_FONT_SIZE, IDC_FONT_SIZE, IDC_LBL_FONT_PT, IDC_LBL_PAGE_SIZE, IDC_PAGE_SIZE, IDC_LBL_EMPTY_LINE_MODE, IDC_EMPTY_LINE_MODE };
        static const UINT page3[] = { IDC_GRP_PROPS, IDC_EXPORT_METADATA, IDC_VALIDATE_DOCX, IDC_CREATE_REPORT, IDC_ADD_HEADERS, IDC_ADD_PAGE_NUMBERS, IDC_NO_TITLE_PAGE_NUMBER, IDC_RESTART_PAGE_NUMBERING, IDC_AUTO_HYPHENATION, IDC_OPEN_AFTER_EXPORT, IDC_LBL_DOC_LANGUAGE, IDC_DOC_LANGUAGE };
        ShowIds(page0, _countof(page0), tab == 0);
        ShowIds(page1, _countof(page1), tab == 1);
        ShowIds(page2, _countof(page2), tab == 2);
        ShowIds(page3, _countof(page3), tab == 3);
    }

    void ApplySettingsToControls() {
        CheckDlgButton(IDC_EXPORT_IMAGES, m_settings.exportImages ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_EXPORT_COVER, m_settings.exportCover ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_LIMIT_IMAGE_WIDTH, m_settings.limitImageWidth ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(IDC_IMAGE_MAX_WIDTH_CM, m_settings.imageMaxWidthCm, FALSE);
        SendDlgItemMessage(IDC_NOTES_MODE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.notesMode), 0);
        CheckDlgButton(IDC_JUSTIFY_TEXT, m_settings.justifyText ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_FIRST_LINE_INDENT, m_settings.firstLineIndent ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_CHAPTER_PAGE_BREAK, m_settings.chapterPageBreak ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_ADD_TOC, m_settings.addToc ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_EXPORT_METADATA, m_settings.exportMetadata ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_EXPORT_HYPERLINKS, m_settings.exportHyperlinks ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_CREATE_BOOKMARKS, m_settings.createBookmarks ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_ENHANCED_FB2_STYLES, m_settings.enhancedFb2Styles ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_VALIDATE_DOCX, m_settings.validateDocx ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_CREATE_REPORT, m_settings.createReport ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_CUSTOM_FONT, m_settings.customFont ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_PAGE, m_settings.titlePage ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_INCLUDE_ANNOTATION, m_settings.titleIncludeAnnotation ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_INCLUDE_GENRES, m_settings.titleIncludeGenres ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_INCLUDE_SERIES, m_settings.titleIncludeSeries ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_TITLE_INCLUDE_FB2_INFO, m_settings.titleIncludeFb2Info ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_ADD_HEADERS, m_settings.addHeaders ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_ADD_PAGE_NUMBERS, m_settings.addPageNumbers ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_NO_TITLE_PAGE_NUMBER, m_settings.noTitlePageNumber ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_RESTART_PAGE_NUMBERING, m_settings.restartPageNumbering ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_AUTO_HYPHENATION, m_settings.autoHyphenation ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(IDC_OPEN_AFTER_EXPORT, m_settings.openAfterExport ? BST_CHECKED : BST_UNCHECKED);
        SendDlgItemMessage(IDC_STYLE_PROFILE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.styleProfile), 0);
        SendDlgItemMessage(IDC_PAGE_SIZE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.pageSize), 0);
        SendDlgItemMessage(IDC_DOC_LANGUAGE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.docLanguage), 0);
        SendDlgItemMessage(IDC_TOC_DEPTH, CB_SETCURSEL, static_cast<WPARAM>(m_settings.tocDepth - 1), 0);
        SendDlgItemMessage(IDC_EMPTY_LINE_MODE, CB_SETCURSEL, static_cast<WPARAM>(m_settings.emptyLineMode), 0);
        FillFontCombo(m_settings.customFont ? m_settings.fontName : DefaultFontForProfile(m_settings.styleProfile));
        SetDlgItemInt(IDC_FONT_SIZE, m_settings.customFont ? m_settings.fontSizePt : DefaultFontSizeForProfile(m_settings.styleProfile), FALSE);
    }

    LRESULT OnDefaults(WORD, WORD, HWND, BOOL&) {
        DocxExportSettings def;
        NormalizeSettings(def);
        m_settings = def;
        ApplySettingsToControls();
        UpdateControlStates();
        return 0;
    }

    int GetSelectedProfile() const {
        LRESULT profile = ::SendDlgItemMessage(m_hWnd, IDC_STYLE_PROFILE, CB_GETCURSEL, 0, 0);
        return (profile >= 0 && profile <= 2) ? static_cast<int>(profile) : 0;
    }

    CStringW GetSelectedFontName() const {
        HWND hCombo = GetDlgItem(IDC_FONT_NAME);
        if (hCombo == NULL)
            return DefaultFontForProfile(GetSelectedProfile());

        LRESULT sel = ::SendMessage(hCombo, CB_GETCURSEL, 0, 0);
        if (sel != CB_ERR) {
            wchar_t buf[LF_FACESIZE * 2] = {0};
            ::SendMessage(hCombo, CB_GETLBTEXT, static_cast<WPARAM>(sel), reinterpret_cast<LPARAM>(buf));
            CStringW value(buf);
            value.Trim();
            if (!value.IsEmpty())
                return value;
        }

        wchar_t text[LF_FACESIZE * 2] = {0};
        ::GetWindowText(hCombo, text, _countof(text));
        CStringW value(text);
        value.Trim();
        return value.IsEmpty() ? DefaultFontForProfile(GetSelectedProfile()) : value;
    }

    void FillFontCombo(const CStringW& selectedFont) {
        HWND hCombo = GetDlgItem(IDC_FONT_NAME);
        if (hCombo == NULL)
            return;

        std::vector<CStringW> fonts;
        EnumerateInstalledFonts(fonts);
        CStringW desired = selectedFont;
        desired.Trim();
        if (desired.IsEmpty())
            desired = DefaultFontForProfile(GetSelectedProfile());

        ::SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < fonts.size(); ++i)
            ::SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(fonts[i])));

        if (fonts.empty())
            ::SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(desired)));

        LRESULT sel = ::SendMessage(hCombo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(desired)));
        if (sel == CB_ERR) {
            CStringW fallback = DefaultFontForProfile(GetSelectedProfile());
            sel = ::SendMessage(hCombo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(fallback)));
        }
        if (sel == CB_ERR)
            sel = 0;
        ::SendMessage(hCombo, CB_SETCURSEL, static_cast<WPARAM>(sel), 0);
        ::SendMessage(hCombo, CB_SETDROPPEDWIDTH, 260, 0);
    }

    void ApplyProfileDefaultsToFontControls() {
        int profile = GetSelectedProfile();
        FillFontCombo(DefaultFontForProfile(profile));
        SetDlgItemInt(IDC_FONT_SIZE, DefaultFontSizeForProfile(profile), FALSE);
    }

    void UpdateControlStates() {
        BOOL exportImages = IsDlgButtonChecked(IDC_EXPORT_IMAGES) == BST_CHECKED;
        BOOL limitImageWidth = exportImages && IsDlgButtonChecked(IDC_LIMIT_IMAGE_WIDTH) == BST_CHECKED;
        ::EnableWindow(GetDlgItem(IDC_EXPORT_COVER), exportImages);
        ::EnableWindow(GetDlgItem(IDC_LIMIT_IMAGE_WIDTH), exportImages);
        ::EnableWindow(GetDlgItem(IDC_IMAGE_MAX_WIDTH_CM), limitImageWidth);
        BOOL customFont = IsDlgButtonChecked(IDC_CUSTOM_FONT) == BST_CHECKED;
        ::EnableWindow(GetDlgItem(IDC_FONT_NAME), customFont);
        ::EnableWindow(GetDlgItem(IDC_FONT_SIZE), customFont);
        BOOL titlePage = IsDlgButtonChecked(IDC_TITLE_PAGE) == BST_CHECKED;
        ::EnableWindow(GetDlgItem(IDC_TITLE_INCLUDE_ANNOTATION), titlePage);
        ::EnableWindow(GetDlgItem(IDC_TITLE_INCLUDE_GENRES), titlePage);
        ::EnableWindow(GetDlgItem(IDC_TITLE_INCLUDE_SERIES), titlePage);
        ::EnableWindow(GetDlgItem(IDC_TITLE_INCLUDE_FB2_INFO), titlePage);
        BOOL addToc = IsDlgButtonChecked(IDC_ADD_TOC) == BST_CHECKED;
        ::EnableWindow(GetDlgItem(IDC_LBL_TOC_DEPTH), addToc);
        ::EnableWindow(GetDlgItem(IDC_TOC_DEPTH), addToc);
        BOOL addPageNumbers = IsDlgButtonChecked(IDC_ADD_PAGE_NUMBERS) == BST_CHECKED;
        ::EnableWindow(GetDlgItem(IDC_NO_TITLE_PAGE_NUMBER), addPageNumbers);
        ::EnableWindow(GetDlgItem(IDC_RESTART_PAGE_NUMBERING), addPageNumbers);
    }

    void InitTooltips() {
        m_hToolTip = ::CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, m_hWnd, NULL, g_hInstance, NULL);
        if (m_hToolTip == NULL)
            return;

        ::SendMessage(m_hToolTip, TTM_SETMAXTIPWIDTH, 0, 440);
        AddTooltip(IDC_EXPORT_IMAGES, L"Включает изображения из секций <image> и связанных блоков <binary> в DOCX-пакет.");
        AddTooltip(IDC_EXPORT_COVER, L"Добавляет изображение из description/title-info/coverpage в начало документа, если обложка есть в FB2.");
        AddTooltip(IDC_LIMIT_IMAGE_WIDTH, L"Масштабирует слишком широкие изображения, чтобы они не выходили за заданную ширину страницы.");
        AddTooltip(IDC_IMAGE_MAX_WIDTH_CM, L"Максимальная ширина изображения в сантиметрах. Обычно удобно 12–16 см для страницы A4.");
        AddTooltip(IDC_TITLE_PAGE, L"Создаёт титульную страницу из описания FB2: обложка, название, авторы и выбранные дополнительные сведения.");
        AddTooltip(IDC_TITLE_INCLUDE_ANNOTATION, L"Добавляет аннотацию из description/title-info/annotation на титульную страницу.");
        AddTooltip(IDC_TITLE_INCLUDE_GENRES, L"Добавляет жанры FB2 на титульную страницу.");
        AddTooltip(IDC_TITLE_INCLUDE_SERIES, L"Добавляет сведения о серии FB2 из элементов sequence.");
        AddTooltip(IDC_TITLE_INCLUDE_FB2_INFO, L"Добавляет технические сведения из document-info: ID, версию FB2 и программу, если они есть в файле.");
        AddTooltip(IDC_NOTES_MODE,
            L"Режим примечаний:\n"
            L"Сноски Word — примечания выводятся внизу страницы возле соответствующего слова.\n"
            L"Концевые примечания Word — примечания собираются в стандартный блок endnotes Word.\n"
            L"Раздел «Примечания» — body name=notes добавляется отдельным разделом в конец документа.");
        AddTooltip(IDC_ADD_TOC, L"Добавляет заголовок «Оглавление», автоматическое поле TOC и стили TOC. После открытия документа поле может потребовать обновления.");
        AddTooltip(IDC_TOC_DEPTH, L"Максимальная глубина заголовков, попадающих в автоматическое оглавление Word: от 1 до 4.");
        AddTooltip(IDC_CREATE_BOOKMARKS, L"Создаёт закладки Word для разделов FB2 с атрибутом id. Нужны для внутренних переходов и ссылок внутри документа.");
        AddTooltip(IDC_EXPORT_HYPERLINKS, L"Преобразует внешние ссылки http/https/mailto и внутренние ссылки #id в гиперссылки Word. Ссылки на примечания обрабатываются выбранным режимом примечаний.");
        AddTooltip(IDC_EXPORT_METADATA, L"Записывает название, автора, жанры, язык, дату и аннотацию из description/title-info в свойства DOCX.");
        AddTooltip(IDC_VALIDATE_DOCX, L"После формирования DOCX выполняет базовую и расширенную проверку: обязательные части, связи, изображения, сноски/концевые примечания, гиперссылки, таблицы и служебные XML-части.");
        AddTooltip(IDC_CREATE_REPORT, L"Создаёт рядом с DOCX диагностический TXT-отчёт: количество глав, абзацев, таблиц, изображений, сносок, гиперссылок и предупреждения проверки.");
        AddTooltip(IDC_JUSTIFY_TEXT, L"Выравнивает основной текст по ширине. Не влияет на заголовки, стихи, изображения и служебные стили.");
        AddTooltip(IDC_FIRST_LINE_INDENT, L"Добавляет отступ первой строки для обычных абзацев, как в книжной вёрстке.");
        AddTooltip(IDC_CHAPTER_PAGE_BREAK, L"Начинает разделы и главы FB2 с новой страницы. Работает для заголовков первого и второго уровня.");
        AddTooltip(IDC_ENHANCED_FB2_STYLES, L"Применяет отдельные стили Word для стихов, цитат, эпиграфов, авторов эпиграфов и подписей к иллюстрациям.");
        AddTooltip(IDC_CUSTOM_FONT, L"Позволяет переопределить шрифт и размер основного текста вручную, независимо от выбранного профиля.");
        AddTooltip(IDC_FONT_NAME, L"Список установленных в Windows шрифтов. Плагин использует выбранный шрифт как основной шрифт DOCX.");
        AddTooltip(IDC_FONT_SIZE, L"Размер основного текста в пунктах. Для книг обычно 11–12 пт, для компактного варианта 10–11 пт.");
        AddTooltip(IDC_PAGE_SIZE, L"Размер страницы итогового документа: A4 для обычной печати, A5 для книжного макета, Letter для американского формата.");
        AddTooltip(IDC_EMPTY_LINE_MODE, L"Обработка FB2-тега empty-line: игнорировать, вставлять пустой абзац или добавлять увеличенный интервал перед следующим абзацем.");
        AddTooltip(IDC_ADD_HEADERS, L"Добавляет верхний колонтитул с названием книги. Полезно для больших документов и печати.");
        AddTooltip(IDC_ADD_PAGE_NUMBERS, L"Добавляет номер страницы в нижний колонтитул стандартным полем PAGE Word.");
        AddTooltip(IDC_NO_TITLE_PAGE_NUMBER, L"Создаёт отдельную секцию Word для титульной страницы и оглавления без номера страницы.");
        AddTooltip(IDC_RESTART_PAGE_NUMBERING, L"Начинает нумерацию страниц основного текста с 1 после титульной страницы/оглавления.");
        AddTooltip(IDC_AUTO_HYPHENATION, L"Включает автоматические переносы Word. Особенно полезно при выравнивании текста по ширине.");
        AddTooltip(IDC_OPEN_AFTER_EXPORT, L"После успешного экспорта открывает созданный DOCX системным приложением по умолчанию.");
        AddTooltip(IDC_DOC_LANGUAGE, L"Язык документа для проверки орфографии и переносов. Режим «Авто из FB2» берёт значение из description/title-info/lang и преобразует ru/en/de/fr/uk и другие короткие коды в язык Word.");
        AddTooltip(IDC_RESET_DEFAULTS, L"Сбрасывает параметры в текущем окне к безопасным значениям по умолчанию. Окно не закрывается, изменения сохраняются после нажатия «ОК».");
        AddTooltip(IDC_PRESET_BOOK,
            L"Книжный DOCX: сбалансированный вариант для чтения и печати. Включает обложку, титульную страницу, оглавление, стили FB2, ссылки, сноски, метаданные и нумерацию страниц без лишних технических блоков.");
        AddTooltip(IDC_PRESET_MINIMAL,
            L"Минимальный DOCX: максимально простой документ. Оставляет основной текст и изображения, отключает обложку, титульную страницу, оглавление, гиперссылки, закладки, расширенные стили, отчёт и нумерацию страниц.");
        AddTooltip(IDC_PRESET_EDITORIAL,
            L"Редакторский DOCX: вариант для проверки и доработки. Включает всё из книжного режима плюс src-title-info, сведения FB2, history, колонтитулы, закладки, отчёты и расширенную проверку DOCX.");
        AddTooltip(IDC_STYLE_PROFILE,
            L"Профиль оформления DOCX:\n"
            L"Книжный — Times New Roman 12 пт, умеренные поля, выравнивание по ширине и отступ первой строки.\n"
            L"Компактный — Times New Roman 10,5 пт, уменьшенные поля, меньший межстрочный интервал и более короткий отступ.\n"
            L"Минимальный — простой документ: Calibri 11 пт, стандартные поля, выравнивание слева и без книжных отступов.");
    }

    void AddTooltip(UINT id, LPCWSTR text) {
        HWND h = GetDlgItem(id);
        if (h == NULL || m_hToolTip == NULL || text == NULL)
            return;

        TOOLINFO ti = { 0 };
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = m_hWnd;
        ti.uId = reinterpret_cast<UINT_PTR>(h);
        ti.lpszText = const_cast<LPWSTR>(text);
        ::SendMessage(m_hToolTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
    }
};

class CDocxSaveDialog : public CFileDialog {
public:
    CDocxSaveDialog(LPCTSTR defaultName, LPCTSTR filter, DocxExportSettings& settings)
        : CFileDialog(FALSE, _T("docx"), defaultName,
            OFN_HIDEREADONLY | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT,
            filter),
          m_settings(settings)
    {
        m_ofn.Flags |= OFN_EXPLORER | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE;
        m_ofn.hInstance = g_hInstance;
        m_ofn.lpTemplateName = MAKEINTRESOURCE(IDD_DOCX_FILE_OPTIONS);
        m_ofn.lpfnHook = HookProc;
        m_ofn.lCustData = reinterpret_cast<LPARAM>(this);
    }

private:
    DocxExportSettings& m_settings;

    static void RestoreSaveButtonText(HWND hDlg) {
        HWND hParent = ::GetParent(hDlg);
        if (hParent != NULL)
            ::SetDlgItemText(hParent, IDOK, L"&Сохранить");
    }

    static UINT_PTR CALLBACK HookProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        CDocxSaveDialog* self = reinterpret_cast<CDocxSaveDialog*>(::GetWindowLongPtr(hDlg, GWLP_USERDATA));

        if (uMsg == WM_INITDIALOG) {
            OPENFILENAME* ofn = reinterpret_cast<OPENFILENAME*>(lParam);
            self = ofn ? reinterpret_cast<CDocxSaveDialog*>(ofn->lCustData) : NULL;
            ::SetWindowLongPtr(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            RestoreSaveButtonText(hDlg);
            return TRUE;
        }

        if (self != NULL && uMsg == WM_COMMAND && LOWORD(wParam) == IDC_FILEDLG_SETTINGS) {
            CDocxSettingsDialog dlg(self->m_settings);
            dlg.DoModal(::GetParent(hDlg));
            RestoreSaveButtonText(hDlg);
            return TRUE;
        }

        return FALSE;
    }
};

WORD ReadBE16(const BYTE* p) { return static_cast<WORD>((p[0] << 8) | p[1]); }
DWORD ReadBE32(const BYTE* p) { return (static_cast<DWORD>(p[0]) << 24) | (static_cast<DWORD>(p[1]) << 16) | (static_cast<DWORD>(p[2]) << 8) | p[3]; }

void DetectImageSize(ImagePart& part) {
    const BYTE* d = part.bytes.empty() ? NULL : &part.bytes[0];
    size_t n = part.bytes.size();
    part.widthPx = 0;
    part.heightPx = 0;
    if (!d || n < 10) return;

    // PNG: signature + IHDR width/height.
    static const BYTE pngSig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (n >= 24 && memcmp(d, pngSig, 8) == 0) {
        part.widthPx = static_cast<int>(ReadBE32(d + 16));
        part.heightPx = static_cast<int>(ReadBE32(d + 20));
        return;
    }

    // GIF.
    if (memcmp(d, "GIF", 3) == 0) {
        part.widthPx = d[6] | (d[7] << 8);
        part.heightPx = d[8] | (d[9] << 8);
        return;
    }

    // BMP.
    if (n >= 26 && d[0] == 'B' && d[1] == 'M') {
        part.widthPx = static_cast<int>(*reinterpret_cast<const LONG*>(d + 18));
        part.heightPx = abs(static_cast<int>(*reinterpret_cast<const LONG*>(d + 22)));
        return;
    }

    // JPEG SOF markers.
    if (d[0] == 0xFF && d[1] == 0xD8) {
        size_t p = 2;
        while (p + 9 < n) {
            while (p < n && d[p] != 0xFF) ++p;
            while (p < n && d[p] == 0xFF) ++p;
            if (p >= n) break;
            BYTE marker = d[p++];
            if (marker == 0xD9 || marker == 0xDA) break;
            if (p + 2 > n) break;
            WORD len = ReadBE16(d + p);
            if (len < 2 || p + len > n) break;
            if ((marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7) ||
                (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF)) {
                part.heightPx = ReadBE16(d + p + 3);
                part.widthPx = ReadBE16(d + p + 5);
                return;
            }
            p += len;
        }
    }
}

class CDocxBuilder {
public:
    CDocxBuilder(IXMLDOMDocument2Ptr doc, const DocxExportSettings& settings)
        : m_doc(doc), m_opt(settings), m_nextRel(10), m_nextImage(1),
          m_docPr(1), m_nextFootnote(1), m_nextEndnote(1), m_sectionDepth(0),
          m_nextBookmark(1), m_pendingBookmarks(), m_pendingPageBreakBefore(false),
          m_pendingEmptyLineBefore(false), m_headerRelId(L"rIdHeader1"), m_footerRelId(L"rIdFooter1"),
          m_sectionCount(0), m_paragraphCount(0), m_tableCount(0),
          m_imageReferences(0), m_missingImages(0), m_coverImagesInserted(0),
          m_noteReferences(0), m_missingNotes(0), m_externalHyperlinkCount(0),
          m_internalHyperlinkCount(0), m_brokenInternalHyperlinks(0),
          m_cssStylesheetCount(0), m_smallImageCount(0), m_heightLimitedImages(0),
          m_captionCount(0), m_poemCount(0), m_epigraphCount(0), m_citeCount(0),
          m_emptyLineCount(0), m_tableRowspanCount(0), m_tableColspanCount(0),
          m_duplicateBookmarkIdsFixed(0), m_noteBodyDepth(0) {
        CollectBookmarkTargets();
    }


    CStringW TextOf(IXMLDOMNodePtr node) {
        if (!node) return CStringW();
        CComBSTR b;
        node->get_text(&b);
        return CStringW(b);
    }

    void AppendAuthorPart(IXMLDOMNodePtr author, const wchar_t* tagName, CStringW& line) {
        if (!author || !tagName || !*tagName) return;
        CStringW xpath;
        xpath.Format(L"./*[local-name()='%s']", tagName);
        IXMLDOMNodePtr n;
        author->selectSingleNode(bstr_t(static_cast<LPCWSTR>(xpath)), &n);
        if (!n) return;
        CStringW value = NormalizeText(TextOf(n));
        value.Trim();
        if (value.IsEmpty()) return;
        if (!line.IsEmpty()) line += L" ";
        line += value;
    }

    void AddDescription() {
        CStringW title = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='book-title']");
        CStringW authors = AuthorsText();
        CStringW translators = TranslatorsText();
        CStringW annotation = AnnotationText();
        CStringW genres = JoinTextsByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='genre']", L"; ");
        CStringW sequence = SequenceText();
        CStringW date = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='date']");
        CStringW lang = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='lang']");
        CStringW fb2id = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='id']");
        CStringW version = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='version']");
        CStringW program = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='program-used']");
        CStringW publishInfo = PublishInfoText();
        CStringW srcInfo = SourceTitleInfoText();
        CStringW history = DocumentHistoryText();

        if (!authors.IsEmpty())
            m_body += ParagraphFromText(authors, L"BookAuthor");
        if (!title.IsEmpty())
            m_body += ParagraphFromText(title, L"BookTitle");
        if (m_opt.titleIncludeTranslators && !translators.IsEmpty())
            m_body += ParagraphFromText(CStringW(L"Перевод: ") + translators, L"TitleInfo");

        if (m_opt.titleIncludeAnnotation && !annotation.IsEmpty()) {
            m_body += ParagraphFromText(L"Аннотация", L"ChapterTitle2");
            m_body += ParagraphFromText(annotation, L"Annotation");
        }
        if (m_opt.titleIncludeGenres && !genres.IsEmpty())
            m_body += ParagraphFromText(CStringW(L"Жанры: ") + genres, L"Fb2Info");
        if (m_opt.titleIncludeSeries && !sequence.IsEmpty())
            m_body += ParagraphFromText(CStringW(L"Серия: ") + sequence, L"Fb2Info");
        if (!date.IsEmpty())
            m_body += ParagraphFromText(CStringW(L"Дата: ") + date, L"Fb2Info");
        if (!lang.IsEmpty())
            m_body += ParagraphFromText(CStringW(L"Язык: ") + lang, L"Fb2Info");
        if (m_opt.titleIncludePublishInfo && !publishInfo.IsEmpty())
            m_body += ParagraphFromText(CStringW(L"Публикация: ") + publishInfo, L"Fb2Info");
        if (m_opt.titleIncludeSrcTitleInfo && !srcInfo.IsEmpty())
            m_body += ParagraphFromText(CStringW(L"Оригинал: ") + srcInfo, L"Fb2Info");
        if (m_opt.titleIncludeFb2Info) {
            if (!fb2id.IsEmpty()) m_body += ParagraphFromText(CStringW(L"FB2 ID: ") + fb2id, L"Fb2Info");
            if (!version.IsEmpty()) m_body += ParagraphFromText(CStringW(L"Версия FB2: ") + version, L"Fb2Info");
            if (!program.IsEmpty()) m_body += ParagraphFromText(CStringW(L"Программа: ") + program, L"Fb2Info");
            if (!history.IsEmpty() && !m_opt.appendHistory) {
                m_body += ParagraphFromText(L"История документа", L"ChapterTitle2");
                m_body += ParagraphFromText(history, L"Annotation");
            }
        }

        if (!m_body.IsEmpty())
            m_body += PageBreakParagraph();
    }


    void AddCoverPage() {
        if (!m_opt.exportImages || !m_opt.exportCover) return;
        IXMLDOMNodePtr image;
        m_doc->selectSingleNode(bstr_t(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='coverpage']/*[local-name()='image']"), &image);
        if (!image) return;
        if (m_opt.createBookmarks) AddPendingImageHrefBookmark(image);
        CStringW drawing = ImageDrawingXml(image, true);
        if (drawing.IsEmpty()) return;
        if (!m_body.IsEmpty() && m_opt.coverSeparatePage)
            m_body += PageBreakParagraph();
        m_body += ParagraphFromRuns(drawing, L"CoverImage");
        ++m_coverImagesInserted;
        if (m_opt.coverPageBreakAfter)
            m_body += PageBreakParagraph();
    }

    bool Build(CZipStoreWriter& zip) {
        m_body.Empty();
        m_frontMatter.Empty();
        m_pendingEmptyLineBefore = false;

        if (m_opt.exportCover) AddCoverPage();
        if (m_opt.titlePage) AddDescription();
        if (m_opt.addToc) {
            if (!m_body.IsEmpty()) {
                CStringW pb = PageBreakParagraph();
                if (m_body.Right(pb.GetLength()) != pb) m_body += pb;
            }
            m_body += TocXml();
            if (m_opt.addTocPageBreak) m_body += PageBreakParagraph();
        }
        if (!m_body.IsEmpty()) {
            m_frontMatter = m_body;
            m_body.Empty();
            m_pendingEmptyLineBefore = false;
        }

        IXMLDOMNodeListPtr bodies;
        m_doc->selectNodes(bstr_t(L"/*[local-name()='FictionBook']/*[local-name()='body']"), &bodies);
        long n = 0;
        if (bodies) bodies->get_length(&n);
        for (long i = 0; i < n; ++i) {
            IXMLDOMNodePtr body;
            bodies->get_item(i, &body);
            ConvertBody(body);
        }

        AppendEndNotesIfNeeded();
        AppendDocumentHistoryIfNeeded();

        if (m_body.IsEmpty()) {
            CComBSTR txt;
            m_doc->get_text(&txt);
            m_body += ParagraphFromText(CStringW(txt), L"Normal");
        }
        if (m_body.IsEmpty()) m_body += L"<w:p/>";

        zip.AddText("[Content_Types].xml", ContentTypesXml());
        zip.AddText("_rels/.rels", RootRelsXml());
        zip.AddText("docProps/core.xml", CorePropsXml());
        zip.AddText("docProps/app.xml", AppPropsXml());
        zip.AddText("docProps/custom.xml", CustomPropsXml());
        zip.AddText("word/document.xml", DocumentXml());
        zip.AddText("word/_rels/document.xml.rels", DocumentRelsXml());
        zip.AddText("word/styles.xml", StylesXml());
        zip.AddText("word/settings.xml", SettingsXml());
        zip.AddText("word/theme/theme1.xml", ThemeXml());
        if (m_opt.addHeaders) zip.AddText("word/header1.xml", HeaderXml());
        if (m_opt.addPageNumbers) zip.AddText("word/footer1.xml", FooterXml());
        if (!m_footnotes.empty() && m_opt.notesMode == 0) {
            zip.AddText("word/footnotes.xml", FootnotesXml());
            zip.AddText("word/_rels/footnotes.xml.rels", NotesPartRelsXml());
        }
        if (!m_footnotes.empty() && m_opt.notesMode == 1) {
            zip.AddText("word/endnotes.xml", EndnotesXml());
            zip.AddText("word/_rels/endnotes.xml.rels", NotesPartRelsXml());
        }

        for (size_t i = 0; i < m_images.size(); ++i) {
            CStringW path = L"word/media/" + m_images[i].fileName;
            zip.AddBytes(ToUtf8(path).c_str(), m_images[i].bytes.empty() ? NULL : &m_images[i].bytes[0], static_cast<DWORD>(m_images[i].bytes.size()));
        }
        ValidateDocxPackage();
        return true;
    }

    CStringW ValidationWarnings() const { return m_validationWarnings; }

    CStringW ReportText(const CStringW& docxFileName) {
        CStringW report;
        report += L"ExportDOCX diagnostic report\r\n";
        report += L"============================\r\n\r\n";
        report += L"Файл DOCX: " + docxFileName + L"\r\n";
        report += L"Название: " + FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='book-title']") + L"\r\n";
        report += L"Авторы: " + AuthorsText() + L"\r\n";
        report += L"Язык Word: " + LanguageCode() + L"\r\n";
        report += L"Исходный <lang>: " + SourceLanguageRaw() + L"\r\n\r\n";

        CStringW line;
        line.Format(L"Разделов FB2: %d\r\n", m_sectionCount); report += line;
        line.Format(L"Абзацев DOCX: %d\r\n", m_paragraphCount); report += line;
        line.Format(L"Таблиц: %d\r\n", m_tableCount); report += line;
        line.Format(L"Изображений в тексте/обложке: %d\r\n", m_imageReferences); report += line;
        line.Format(L"Изображений встроено в DOCX: %d\r\n", static_cast<int>(m_images.size())); report += line;
        line.Format(L"Обложек вставлено: %d\r\n", m_coverImagesInserted); report += line;
        line.Format(L"Изображений не найдено/не декодировано: %d\r\n", m_missingImages); report += line;
        line.Format(L"Ссылок на примечания: %d\r\n", m_noteReferences); report += line;
        line.Format(L"Сносок/концевых примечаний создано: %d\r\n", static_cast<int>(m_footnotes.size())); report += line;
        line.Format(L"Примечаний не найдено: %d\r\n", m_missingNotes); report += line;
        line.Format(L"Внешних гиперссылок: %d\r\n", m_externalHyperlinkCount); report += line;
        line.Format(L"Внутренних гиперссылок всего: %d\r\n", m_internalHyperlinkCount); report += line;
        line.Format(L"Внутренних гиперссылок разрешено: %d\r\n", m_internalHyperlinkCount - m_brokenInternalHyperlinks); report += line;
        line.Format(L"Битых внутренних гиперссылок: %d\r\n", m_brokenInternalHyperlinks); report += line;
        line.Format(L"Целей внутренних ссылок собрано: %d\r\n", static_cast<int>(m_bookmarkTargets.size())); report += line;
        line.Format(L"Дубликатов закладок пропущено: %d\r\n", m_duplicateBookmarkIdsFixed); report += line;
        line.Format(L"FB2 stylesheet обнаружено: %d\r\n", m_cssStylesheetCount); report += line;
        line.Format(L"Маленьких изображений оставлено без растягивания: %d\r\n", m_smallImageCount); report += line;
        line.Format(L"Изображений ограничено по высоте страницы: %d\r\n", m_heightLimitedImages); report += line;
        line.Format(L"Закладок Word: %d\r\n", m_nextBookmark - 1); report += line;

        report += L"\r\nНастройки:\r\n";
        report += L"- Титульная страница: " + CStringW(m_opt.titlePage ? L"да" : L"нет") + L"\r\n";
        report += L"- Оглавление: " + CStringW(m_opt.addToc ? L"да" : L"нет") + L"\r\n";
        report += L"- Изображения: " + CStringW(m_opt.exportImages ? L"да" : L"нет") + L"\r\n";
        report += L"- Примечания: " + NotesModeText() + L"\r\n";
        report += L"- Размер страницы: " + PageSizeText() + L"\r\n";
        { CStringW line2; line2.Format(L"- Глубина оглавления: %d\r\n", m_opt.tocDepth); report += line2; }
        report += L"- Обработка empty-line: " + EmptyLineModeText() + L"\r\n";
        report += L"- Открывать после экспорта: " + CStringW(m_opt.openAfterExport ? L"да" : L"нет") + L"\r\n";

        report += L"\r\nПредупреждения:\r\n";
        report += m_validationWarnings.IsEmpty() ? L"нет\r\n" : (m_validationWarnings + L"\r\n");
        return report;
    }

private:
    IXMLDOMDocument2Ptr m_doc;
    DocxExportSettings m_opt;
    CStringW m_body;
    CStringW m_frontMatter;
    std::vector<ImagePart> m_images;
    std::vector<FootnotePart> m_footnotes;
    std::vector<HyperlinkPart> m_hyperlinks;
    int m_nextRel;
    int m_nextImage;
    int m_docPr;
    int m_nextFootnote;
    int m_nextEndnote;
    int m_sectionDepth;
    int m_nextBookmark;
    CStringW m_pendingBookmarks;
    bool m_pendingPageBreakBefore;
    bool m_pendingEmptyLineBefore;
    CStringW m_validationWarnings;
    CStringW m_headerRelId;
    CStringW m_footerRelId;
    int m_sectionCount;
    int m_paragraphCount;
    int m_tableCount;
    int m_imageReferences;
    int m_missingImages;
    int m_coverImagesInserted;
    int m_noteReferences;
    int m_missingNotes;
    int m_externalHyperlinkCount;
    int m_internalHyperlinkCount;
    int m_brokenInternalHyperlinks;
    int m_cssStylesheetCount;
    int m_smallImageCount;
    int m_heightLimitedImages;
    int m_captionCount;
    int m_poemCount;
    int m_epigraphCount;
    int m_citeCount;
    int m_emptyLineCount;
    int m_tableRowspanCount;
    int m_tableColspanCount;
    int m_duplicateBookmarkIdsFixed;
    int m_noteBodyDepth;
    std::set<std::wstring> m_bookmarkTargets;
    std::set<std::wstring> m_noteTargets;
    std::set<std::wstring> m_emittedBookmarkNames;
    std::set<std::wstring> m_buildingFootnotes;
    std::set<std::wstring> m_recursiveFootnoteWarnings;


    bool IsBookmarkableNode(IXMLDOMNodePtr node) const {
        CStringW nm = NodeName(node);
        return nm == L"section" || nm == L"title" || nm == L"subtitle" ||
               nm == L"p" || nm == L"image" || nm == L"table" ||
               nm == L"poem" || nm == L"stanza" || nm == L"v" ||
               nm == L"cite" || nm == L"epigraph" || nm == L"text-author" ||
               nm == L"code";
    }

    bool IsInsideNotesBody(const IXMLDOMNodePtr& node) {
        IXMLDOMNodePtr cur = node;
        while (cur) {
            if (NodeName(cur) == L"body" && IsNotesBody(cur)) return true;
            IXMLDOMNode* parent = nullptr;
            cur->get_parentNode(&parent);
            cur.Attach(parent);
        }
        return false;
    }

    void AddBookmarkTargetId(const CStringW& id) {
        CStringW clean = id;
        clean.Trim();
        if (!clean.IsEmpty())
            m_bookmarkTargets.emplace(static_cast<LPCWSTR>(clean));
    }

    void CollectBookmarkTargets() {
        m_bookmarkTargets.clear();
        m_noteTargets.clear();
        IXMLDOMNodeListPtr nodes;
        if (m_doc)
            m_doc->selectNodes(bstr_t(L"//*[@id]"), &nodes);
        long n = 0;
        if (nodes) nodes->get_length(&n);
        for (long i = 0; i < n; ++i) {
            IXMLDOMNodePtr node;
            nodes->get_item(i, &node);
            CStringW id = AttrAnyNode(node, L"id", L"id");
            id.Trim();
            if (id.IsEmpty()) continue;

            if (IsInsideNotesBody(node)) {
                if (NodeName(node) == L"section")
                    m_noteTargets.emplace(static_cast<LPCWSTR>(id));
                continue;
            }

            if (IsBookmarkableNode(node))
                AddBookmarkTargetId(id);
        }

        // Some FB2 files link directly to the binary/image id rather than to
        // the surrounding <image id="..."> element. Treat image href targets
        // as bookmarkable as well; the bookmark is emitted near the image.
        IXMLDOMNodeListPtr images;
        if (m_doc)
            m_doc->selectNodes(bstr_t(L"//*[local-name()='image']"), &images);
        long imgCount = 0;
        if (images) images->get_length(&imgCount);
        for (long i = 0; i < imgCount; ++i) {
            IXMLDOMNodePtr image;
            images->get_item(i, &image);
            if (!image || IsInsideNotesBody(image)) continue;
            CStringW href = AttrAnyNode(image, L"href", L"l:href");
            href.Trim();
            if (!href.IsEmpty() && href[0] == L'#') href.Delete(0, 1);
            AddBookmarkTargetId(href);
        }
    }

    bool HasBookmarkTarget(const CStringW& fbId) const {
        return m_bookmarkTargets.find(std::wstring(static_cast<LPCWSTR>(fbId))) != m_bookmarkTargets.end();
    }

    bool HasNoteTarget(const CStringW& fbId) const {
        return m_noteTargets.find(std::wstring(static_cast<LPCWSTR>(fbId))) != m_noteTargets.end();
    }

    bool IsFootnoteBuildInProgress(const CStringW& fbId) const {
        if (fbId.IsEmpty()) return false;
        return m_buildingFootnotes.find(std::wstring(static_cast<LPCWSTR>(fbId))) != m_buildingFootnotes.end();
    }

    void AddRecursiveFootnoteWarning(const CStringW& fbId) {
        if (fbId.IsEmpty()) return;
        std::wstring key(static_cast<LPCWSTR>(fbId));
        if (!m_recursiveFootnoteWarnings.insert(key).second) return;

        CStringW warn;
        warn.Format(L"Обнаружена циклическая ссылка между примечаниями около #%s; рекурсивная ссылка оставлена обычным текстом, чтобы избежать переполнения стека.",
            static_cast<LPCWSTR>(fbId));
        AddValidationWarning(warn);
    }

    bool IsNotesBody(IXMLDOMNodePtr body) {
        IXMLDOMElementPtr e;
        if (SUCCEEDED(body->QueryInterface(IID_PPV_ARGS(&e))) && e) {
            CStringW name = Attr(e, L"name");
            return !name.IsEmpty() && name.CompareNoCase(L"notes") == 0;
        }
        return false;
    }

    void ConvertBody(IXMLDOMNodePtr body) {
        // In FB2 footnotes are usually stored in a separate body name="notes".
        // The normal pass never appends that body to the end of the document.
        // It is either used as a source for real Word footnotes or appended later
        // only when the corresponding setting asks for end notes.
        if (IsNotesBody(body)) return;
        ConvertChildren(body, L"Normal");
    }

    void AppendEndNotesIfNeeded() {
        if (m_opt.notesMode != 2) return;

        IXMLDOMNodePtr notesBody = FindBodyByName(RootOf(m_doc), L"notes");
        if (!notesBody) return;

        m_body += ParagraphFromText(L"\x041F\x0440\x0438\x043C\x0435\x0447\x0430\x043D\x0438\x044F", L"Heading1");
        ConvertChildren(notesBody, L"Normal");
    }



    void AppendDocumentHistoryIfNeeded() {
        if (!m_opt.appendHistory) return;
        CStringW history = DocumentHistoryText();
        if (history.IsEmpty()) return;
        m_body += PageBreakParagraph();
        m_body += ParagraphFromText(L"История документа", L"Heading1");
        m_body += ParagraphFromText(history, L"Annotation");
    }

    void ConvertChildren(IXMLDOMNodePtr node, const wchar_t* defaultStyle) {
        IXMLDOMNodeListPtr kids;
        node->get_childNodes(&kids);
        long n = 0;
        if (kids) kids->get_length(&n);
        for (long i = 0; i < n; ++i) {
            IXMLDOMNodePtr c;
            kids->get_item(i, &c);
            ConvertNode(c, defaultStyle);
        }
    }


    void ConvertNode(IXMLDOMNodePtr node, const wchar_t* defaultStyle) {
        DOMNodeType type;
        node->get_nodeType(&type);
        if (type != NODE_ELEMENT) return;

        CStringW nm = NodeName(node);
        if (m_opt.createBookmarks && nm != L"section" && IsBookmarkableNode(node))
            AddPendingBookmark(node);

        if (nm == L"section") {
            ++m_sectionCount;
            if (m_opt.createBookmarks) AddPendingBookmark(node);
            ++m_sectionDepth;
            ConvertChildren(node, L"Normal");
            --m_sectionDepth;
        }
        else if (nm == L"body") {
            ConvertBody(node);
        }
        else if (nm == L"title") {
            ConvertTitle(node, defaultStyle);
        }
        else if (nm == L"subtitle") {
            CStringW runs;
            CollectInline(node, RunFmt(), runs);
            m_body += ParagraphFromRuns(runs, L"Subtitle");
        }
        else if (nm == L"p") {
            CStringW runs;
            CollectInline(node, RunFmt(), runs);
            const wchar_t* pStyle = defaultStyle && *defaultStyle ? defaultStyle : L"Normal";
            if (m_opt.enhancedFb2Styles && m_opt.imageCaptions && IsCaptionParagraph(node)) { pStyle = L"Caption"; ++m_captionCount; }
            m_body += ParagraphFromRuns(runs, pStyle);
        }
        else if (nm == L"empty-line") {
            ++m_emptyLineCount;
            if (m_opt.emptyLineMode == 1) {
                m_body += EmptyLineParagraph();
            } else if (m_opt.emptyLineMode == 2) {
                m_pendingEmptyLineBefore = true;
            } else if (m_opt.emptyLineMode == 3) {
                m_body += ParagraphFromText(L"* * *", L"SceneSeparator");
            }
        }
        else if (nm == L"image") {
            if (m_opt.createBookmarks) AddPendingImageHrefBookmark(node);
            CStringW drawing = ImageDrawingXml(node);
            if (!drawing.IsEmpty()) m_body += ParagraphFromRuns(drawing, L"Image");
        }
        else if (nm == L"epigraph") {
            ++m_epigraphCount;
            ConvertChildren(node, m_opt.enhancedFb2Styles ? L"Epigraph" : L"Normal");
        }
        else if (nm == L"text-author") {
            CStringW runs;
            CollectInline(node, RunFmt(), runs);
            m_body += ParagraphFromRuns(runs, m_opt.enhancedFb2Styles ? L"TextAuthor" : L"Normal");
        }
        else if (nm == L"date") {
            CStringW runs;
            CollectInline(node, RunFmt(), runs);
            m_body += ParagraphFromRuns(runs, L"Date");
        }
        else if (nm == L"annotation") {
            ConvertChildren(node, L"Annotation");
        }
        else if (nm == L"cite") {
            ++m_citeCount;
            ConvertChildren(node, m_opt.enhancedFb2Styles ? L"Quote" : L"Normal");
        }
        else if (nm == L"poem") {
            ++m_poemCount;
            ConvertChildren(node, m_opt.enhancedFb2Styles ? L"Verse" : L"Normal");
        }
        else if (nm == L"stanza") {
            ConvertChildren(node, m_opt.enhancedFb2Styles ? L"Verse" : L"Normal");
            if (m_opt.enhancedFb2Styles)
                m_body += L"<w:p><w:pPr><w:pStyle w:val=\"VerseSpace\"/></w:pPr></w:p>";
        }
        else if (nm == L"v") {
            CStringW runs;
            CollectInline(node, RunFmt(), runs);
            m_body += ParagraphFromRuns(runs, m_opt.enhancedFb2Styles ? L"Verse" : L"Normal");
        }
        else if (nm == L"code") {
            CStringW runs;
            RunFmt f;
            f.code = true;
            CollectInline(node, f, runs);
            m_body += ParagraphFromRuns(runs.IsEmpty() ? RunXml(L" ", f) : runs, L"CodeBlock");
        }
        else if (nm == L"table") {
            ConvertTable(node);
        }
        else if (nm == L"stylesheet") {
            ++m_cssStylesheetCount;
            AddValidationWarning(L"Обнаружен FB2 stylesheet; CSS не применяется напрямую к DOCX, но текст книги сохранён.");
        }
        else {
            ConvertChildren(node, defaultStyle);
        }
    }

    void ConvertTitle(IXMLDOMNodePtr node, const wchar_t* contextStyle = L"") {
        CStringW ctx(contextStyle ? contextStyle : L"");
        const wchar_t* style = L"Heading1";
        if (ctx == L"Verse") style = L"PoemTitle";
        else if (ctx == L"Quote" || ctx == L"Epigraph") style = L"Subtitle";
        else if (m_sectionDepth == 2) style = L"Heading2";
        else if (m_sectionDepth == 3) style = L"Heading3";
        else if (m_sectionDepth == 4) style = L"Heading4";
        else if (m_sectionDepth == 5) style = L"Heading5";
        else if (m_sectionDepth >= 6) style = L"Heading6";

        // The user option is named "Start chapters on a new page". In many real FB2 files
        // (including Sagan's Cosmos) the actual chapters are stored as depth-2 sections
        // under one wrapper section with the book title. Generate an explicit page-break
        // paragraph before chapter-like section titles; this is more reliable than only
        // setting w:pageBreakBefore on Heading styles.
        if (ctx != L"Verse" && ctx != L"Quote" && ctx != L"Epigraph" && ShouldStartTitleOnNewPage(node)) {
            m_body += PageBreakParagraph();
        }

        IXMLDOMNodeListPtr ps;
        node->selectNodes(bstr_t(L"./*[local-name()='p']"), &ps);
        long n = 0;
        if (ps) ps->get_length(&n);
        if (n == 0) {
            if (m_opt.createBookmarks) AddPendingBookmark(node);
            CStringW runs;
            CollectInline(node, RunFmt(), runs);
            m_body += ParagraphFromRuns(runs, style);
        }
        for (long i = 0; i < n; ++i) {
            IXMLDOMNodePtr p;
            ps->get_item(i, &p);
            if (m_opt.createBookmarks) AddPendingBookmark(p);
            CStringW runs;
            CollectInline(p, RunFmt(), runs);
            m_body += ParagraphFromRuns(runs, style);
        }
    }

    CStringW PageBreakParagraph() const {
        return L"<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>";
    }

    CStringW EmptyLineParagraph() {
        ++m_paragraphCount;
        return L"<w:p><w:pPr><w:pStyle w:val=\"EmptyLine\"/></w:pPr></w:p>";
    }

    bool HasDirectSectionChild(IXMLDOMNodePtr node) {
        if (!node) return false;
        IXMLDOMNodeListPtr kids;
        node->get_childNodes(&kids);
        long count = 0;
        if (kids) kids->get_length(&count);
        for (long i = 0; i < count; ++i) {
            IXMLDOMNodePtr c;
            kids->get_item(i, &c);
            if (NodeName(c) == L"section") return true;
        }
        return false;
    }

    bool HasOwnContentBeforeFirstSubsection(IXMLDOMNodePtr sectionNode) {
        if (!sectionNode) return false;
        IXMLDOMNodeListPtr kids;
        sectionNode->get_childNodes(&kids);
        long count = 0;
        if (kids) kids->get_length(&count);
        for (long i = 0; i < count; ++i) {
            IXMLDOMNodePtr c;
            kids->get_item(i, &c);
            CStringW nm = NodeName(c);
            if (nm == L"section") return false;
            if (nm == L"p" || nm == L"subtitle" || nm == L"epigraph" || nm == L"cite" ||
                nm == L"poem" || nm == L"image" || nm == L"table" || nm == L"empty-line") {
                return true;
            }
        }
        return false;
    }

    bool ShouldStartTitleOnNewPage(IXMLDOMNodePtr titleNode) {
        if (!m_opt.chapterPageBreak || m_body.IsEmpty() || m_sectionDepth <= 0) return false;

        IXMLDOMNodePtr sectionNode;
        titleNode->get_parentNode(&sectionNode);
        if (!sectionNode || NodeName(sectionNode) != L"section") return false;

        // A common FB2 pattern is one top-level wrapper section containing only
        // book title plus many chapter subsections. Do not insert a page break
        // before that wrapper title, but do insert it before the nested chapters.
        if (m_sectionDepth == 1 && HasDirectSectionChild(sectionNode) && !HasOwnContentBeforeFirstSubsection(sectionNode))
            return false;

        // Normal top-level chapters and nested chapters are both supported.
        return m_sectionDepth <= 2;
    }

    bool IsPageMarkerText(CStringW text) {
        text = CollapseSpaces(text);
        if (text.GetLength() < 3) return false;
        if (text[0] != L'{' || text[text.GetLength() - 1] != L'}') return false;
        bool hasDigit = false;
        for (int i = 1; i < text.GetLength() - 1; ++i) {
            if (text[i] >= L'0' && text[i] <= L'9') { hasDigit = true; continue; }
            return false;
        }
        return hasDigit;
    }

    bool IsOriginalPageMarkerElement(IXMLDOMNodePtr node) {
        if (!node) return false;
        CStringW nm = NodeName(node);
        // In many scanned FB2 books page-ending numbers from the printed edition
        // are encoded as inline bold fragments like <strong>{15}</strong>.
        // They are not user notes and should not appear in DOCX text.
        if (nm != L"strong" && nm != L"b") return false;
        return IsPageMarkerText(TextOf(node));
    }

    void RemoveTrailingHyphenBeforePageMarker(CStringW& runs) {
        // Page markers can split a word: "Повест-<strong>{43}</strong>вуя".
        // After removing the marker, also remove the artificial line-break hyphen.
        CStringW marker = L"-</w:t></w:r>";
        int pos = runs.ReverseFind(L'-');
        if (pos < 0) return;
        CStringW tail = runs.Mid(pos);
        if (tail == marker) runs.Delete(pos, 1);
    }

    void CollectInline(IXMLDOMNodePtr node, RunFmt fmt, CStringW& runs) {
        IXMLDOMNodeListPtr kids;
        node->get_childNodes(&kids);
        long n = 0;
        if (kids) kids->get_length(&n);
        for (long i = 0; i < n; ++i) {
            IXMLDOMNodePtr c;
            kids->get_item(i, &c);
            DOMNodeType type;
            c->get_nodeType(&type);
            if (type == NODE_TEXT || type == NODE_CDATA_SECTION) {
                runs += RunXml(CleanTextForDocx(TextNodeValue(c)), fmt);
            }
            else if (type == NODE_ELEMENT) {
                CStringW nm = NodeName(c);
                if (IsOriginalPageMarkerElement(c)) {
                    RemoveTrailingHyphenBeforePageMarker(runs);
                    continue;
                }

                RunFmt f = fmt;
                if (nm == L"strong" || nm == L"b") f.bold = true;
                else if (nm == L"emphasis" || nm == L"i") f.italic = true;
                else if (nm == L"strikethrough") f.strike = true;
                else if (nm == L"code") f.code = true;
                else if (nm == L"sup") f.vert = 1;
                else if (nm == L"sub") f.vert = -1;

                if (nm == L"image") {
                    CStringW drawing = ImageDrawingXml(c);
                    if (!drawing.IsEmpty()) runs += drawing;
                }
                else if (nm == L"a") {
                    if (m_noteBodyDepth > 0) {
                        // Word footnotes/endnotes must not contain nested footnoteReference
                        // elements. Keep note-to-note links as plain visible text inside
                        // the note body; this also avoids cycles such as note81 -> note84 -> note81.
                        CollectInline(c, f, runs);
                    } else {
                        CStringW ref = NoteReferenceXml(c);
                        if (!ref.IsEmpty()) {
                            runs += ref;
                        } else {
                            CStringW link = HyperlinkXml(c, f);
                            if (!link.IsEmpty())
                                runs += link;
                            else {
                                f.code = fmt.code;
                                CollectInline(c, f, runs);
                            }
                        }
                    }
                }
                else {
                    CollectInline(c, f, runs);
                }
            }
        }
    }

    CStringW RunXml(CStringW text, const RunFmt& fmt) {
        if (text.IsEmpty()) return L"";
        text = CleanTextForDocx(text);

        CStringW result;
        CStringW part;
        for (int i = 0; i < text.GetLength(); ++i) {
            wchar_t ch = text[i];
            if (ch == L'\n') {
                // Newlines inside FB2 <p> are usually source-file wrapping, not semantic
                // line breaks. Emitting <w:br/> for them freezes the layout and makes
                // Word show a manual line break at almost every original source line.
                if (part.IsEmpty() || part[part.GetLength() - 1] != L' ')
                    part += L' ';
            }
            else {
                part += ch;
            }
        }
        if (!part.IsEmpty()) result += OneRunXml(part, fmt);
        return result;
    }

    CStringW OneRunXml(CStringW text, const RunFmt& fmt) {
        if (text.IsEmpty()) return L"";
        CStringW x = L"<w:r>";
        CStringW rpr;
        if (fmt.bold) rpr += L"<w:b/>";
        if (fmt.italic) rpr += L"<w:i/>";
        if (fmt.strike) rpr += L"<w:strike/>";
        if (fmt.code) rpr += L"<w:rStyle w:val=\"CodeChar\"/>";
        if (fmt.vert > 0) rpr += L"<w:vertAlign w:val=\"superscript\"/>";
        if (fmt.vert < 0) rpr += L"<w:vertAlign w:val=\"subscript\"/>";
        if (!rpr.IsEmpty()) x += L"<w:rPr>" + rpr + L"</w:rPr>";
        x += L"<w:t xml:space=\"preserve\">";
        x += XmlEscape(text);
        x += L"</w:t></w:r>";
        return x;
    }

    CStringW ParagraphFromText(const CStringW& text, const wchar_t* style) {
        RunFmt f;
        CStringW runs = RunXml(NormalizeText(text), f);
        return ParagraphFromRuns(runs, style);
    }

    CStringW ParagraphFromRuns(const CStringW& runs, const wchar_t* style) {
        if (runs.IsEmpty()) return L"";
        ++m_paragraphCount;
        CStringW styleName(style ? style : L"");
        CStringW x = L"<w:p>";
        if (!styleName.IsEmpty() || m_pendingPageBreakBefore || m_pendingEmptyLineBefore) {
            x += L"<w:pPr>";
            if (!styleName.IsEmpty()) {
                x += L"<w:pStyle w:val=\"";
                x += styleName;
                x += L"\"/>";
            }
            if (m_pendingEmptyLineBefore) {
                x += L"<w:spacing w:before=\"240\"/>";
                m_pendingEmptyLineBefore = false;
            }
            if (m_pendingPageBreakBefore) {
                x += L"<w:pageBreakBefore/>";
                m_pendingPageBreakBefore = false;
            }
            x += L"</w:pPr>";
        }
        if (!m_pendingBookmarks.IsEmpty()) {
            x += m_pendingBookmarks;
            m_pendingBookmarks.Empty();
        }
        x += runs;
        x += L"</w:p>";
        return x;
    }

    bool IsCaptionParagraph(IXMLDOMNodePtr node) {
        CStringW l = CollapseSpaces(TextOf(node));
        l.MakeLower();
        return l.Find(L"илл.") == 0 || l.Find(L"рис.") == 0 || l.Find(L"табл.") == 0 ||
               l.Find(L"fig.") == 0 || l.Find(L"figure ") == 0 || l.Find(L"таблица ") == 0;
    }

    CStringW TocXml() {
        CStringW x;
        x += ParagraphFromText(L"Оглавление", L"TOCTitle");
        x += L"<w:p><w:pPr><w:pStyle w:val=\"TOC1\"/></w:pPr>";
        x += L"<w:r><w:fldChar w:fldCharType=\"begin\" w:dirty=\"true\"/></w:r>";
        x += CStringW(L"<w:r><w:instrText xml:space=\"preserve\"> TOC \\o \"1-") + TocDepthString() + L"\" \\h \\z \\u </w:instrText></w:r>";
        x += L"<w:r><w:fldChar w:fldCharType=\"separate\"/></w:r>";
        x += L"<w:r><w:t>Оглавление будет заполнено Word после обновления поля.</w:t></w:r>";
        x += L"<w:r><w:fldChar w:fldCharType=\"end\"/></w:r></w:p>";
        x += ParagraphFromText(L"Чтобы обновить: щёлкните правой кнопкой мыши по оглавлению и выберите «Обновить поле».", L"TOCInstruction");
        return x;
    }

    void AddPendingBookmark(IXMLDOMNodePtr node) {
        CStringW id = AttrAnyNode(node, L"id", L"id");
        AddPendingBookmarkForId(id);
    }

    void AddPendingImageHrefBookmark(IXMLDOMNodePtr imageNode) {
        CStringW href = AttrAnyNode(imageNode, L"href", L"l:href");
        href.Trim();
        if (!href.IsEmpty() && href[0] == L'#') href.Delete(0, 1);
        AddPendingBookmarkForId(href);
    }

    void AddPendingBookmarkForId(const CStringW& rawId) {
        CStringW id = rawId;
        id.Trim();
        if (id.IsEmpty()) return;

        CStringW name = BookmarkNameFromFbId(id);
        std::wstring key(static_cast<LPCWSTR>(name));
        if (m_emittedBookmarkNames.find(key) != m_emittedBookmarkNames.end()) {
            ++m_duplicateBookmarkIdsFixed;
            return;
        }
        m_emittedBookmarkNames.insert(key);

        CStringW x;
        x.Format(L"<w:bookmarkStart w:id=\"%d\" w:name=\"%s\"/><w:bookmarkEnd w:id=\"%d\"/>",
            m_nextBookmark, static_cast<LPCWSTR>(XmlEscape(name)), m_nextBookmark);
        ++m_nextBookmark;
        m_pendingBookmarks += x;
    }

    CStringW TakePendingBookmarksParagraph() {
        if (m_pendingBookmarks.IsEmpty()) return L"";
        ++m_paragraphCount;
        CStringW x = L"<w:p>" + m_pendingBookmarks + L"</w:p>";
        m_pendingBookmarks.Empty();
        return x;
    }

    CStringW BookmarkNameFromFbId(const CStringW& fbId) const {
        DWORD h = 2166136261u;
        for (int i = 0; i < fbId.GetLength(); ++i) {
            h ^= static_cast<DWORD>(fbId[i]);
            h *= 16777619u;
        }
        CStringW name;
        name.Format(L"fb2_%08X", h);
        return name;
    }

    CStringW CreateExternalHyperlinkRel(const CStringW& target) {
        for (size_t i = 0; i < m_hyperlinks.size(); ++i)
            if (m_hyperlinks[i].target == target) return m_hyperlinks[i].relId;
        HyperlinkPart hp;
        hp.relId.Format(L"rId%d", m_nextRel++);
        hp.target = target;
        m_hyperlinks.push_back(hp);
        ++m_externalHyperlinkCount;
        return hp.relId;
    }

    bool IsExternalHyperlink(const CStringW& href) const {
        CStringW h = href;
        h.MakeLower();
        return h.Find(L"http://") == 0 || h.Find(L"https://") == 0 || h.Find(L"mailto:") == 0;
    }

    CStringW HyperlinkXml(IXMLDOMNodePtr linkNode, RunFmt fmt) {
        if (!m_opt.exportHyperlinks) return L"";
        CStringW href = AttrAnyNode(linkNode, L"href", L"l:href");
        href.Trim();
        if (href.IsEmpty()) return L"";

        bool internal = false;
        if (href[0] == L'#') {
            href.Delete(0, 1);
            internal = true;
        }

        CStringW linkRuns;
        CollectInline(linkNode, fmt, linkRuns);
        if (linkRuns.IsEmpty()) linkRuns = RunXml(href, fmt);

        if (internal) {
            ++m_internalHyperlinkCount;
            if (!m_opt.createBookmarks) {
                AddValidationWarning(L"Внутренняя гиперссылка оставлена обычным текстом, потому что создание закладок отключено.");
                return linkRuns;
            }
            if (!HasBookmarkTarget(href)) {
                ++m_brokenInternalHyperlinks;
                CStringW warn;
                warn.Format(L"Внутренняя ссылка #%s не имеет найденной id-цели в основном тексте; ссылка оставлена обычным текстом.", static_cast<LPCWSTR>(href));
                AddValidationWarning(warn);
                return linkRuns;
            }
            CStringW anchor = BookmarkNameFromFbId(href);
            return L"<w:hyperlink w:anchor=\"" + XmlEscape(anchor) + L"\" w:history=\"1\">" + linkRuns + L"</w:hyperlink>";
        }
        if (IsExternalHyperlink(href)) {
            CStringW relId = CreateExternalHyperlinkRel(href);
            return L"<w:hyperlink r:id=\"" + relId + L"\" w:history=\"1\">" + linkRuns + L"</w:hyperlink>";
        }
        return L"";
    }

    CStringW ImageDrawingXml(IXMLDOMNodePtr imageNode, bool isCover = false) {
        ++m_imageReferences;
        if (!m_opt.exportImages) return L"";
        IXMLDOMElementPtr im;
        if (FAILED(imageNode->QueryInterface(IID_PPV_ARGS(&im)))) return L"";
        CStringW href = AttrAnyNode(imageNode, L"href", L"l:href");
        if (!href.IsEmpty() && href[0] == L'#') href.Delete(0, 1);
        if (href.IsEmpty()) return L"";

        ImagePart* part = FindOrCreateImage(href);
        if (!part) {
            ++m_missingImages;
            CStringW warn;
            warn.Format(L"Не найдено или не декодировано изображение binary id=\"%s\".", static_cast<LPCWSTR>(href));
            AddValidationWarning(warn);
            return L"";
        }

        int wpx = part->widthPx > 0 ? part->widthPx : 480;
        int hpx = part->heightPx > 0 ? part->heightPx : 360;
        __int64 cx = static_cast<__int64>(wpx) * 9525; // 96 DPI pixel to EMU
        __int64 cy = static_cast<__int64>(hpx) * 9525;
        if (wpx <= 320 && hpx <= 320) ++m_smallImageCount;
        if (isCover) {
            LimitCoverImageSize(cx, cy);
        } else {
            if (m_opt.limitImageWidth) {
                __int64 maxCx = static_cast<__int64>(m_opt.imageMaxWidthCm) * 360000; // 1 cm = 360000 EMU
                if (maxCx < 1440000) maxCx = 1440000; // safety: 4 cm
                if (cx > maxCx) {
                    cy = cy * maxCx / cx;
                    cx = maxCx;
                }
            }
            LimitRegularImageHeight(cx, cy);
        }

        CStringW xml;
        xml.Format(
            L"<w:r><w:drawing>"
            L"<wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
            L"<wp:extent cx=\"%I64d\" cy=\"%I64d\"/>"
            L"<wp:docPr id=\"%d\" name=\"Picture %d\"/>"
            L"<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
            L"<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
            L"<pic:pic><pic:nvPicPr><pic:cNvPr id=\"0\" name=\"%s\"/><pic:cNvPicPr/></pic:nvPicPr>"
            L"<pic:blipFill><a:blip r:embed=\"%s\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
            L"<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"%I64d\" cy=\"%I64d\"/></a:xfrm>"
            L"<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr></pic:pic>"
            L"</a:graphicData></a:graphic></wp:inline></w:drawing></w:r>",
            cx, cy, m_docPr, m_docPr, static_cast<LPCWSTR>(XmlEscape(part->fileName)), static_cast<LPCWSTR>(part->relId), cx, cy);
        ++m_docPr;
        return xml;
    }

    ImagePart* FindOrCreateImage(const CStringW& fbId) {
        for (size_t i = 0; i < m_images.size(); ++i)
            if (m_images[i].fbId == fbId) return &m_images[i];

        CStringW safeId = fbId;
        safeId.Replace(L"'", L"&apos;");
        IXMLDOMNodePtr bin;
        CStringW xp;
        xp.Format(L"/*[local-name()='FictionBook']/*[local-name()='binary'][@id='%s']", static_cast<LPCWSTR>(safeId));
        m_doc->selectSingleNode(bstr_t(xp), &bin);
        if (!bin) bin = FindDescendantById(RootOf(m_doc), L"binary", fbId);
        if (!bin) return NULL;

        IXMLDOMElementPtr be;
        if (FAILED(bin->QueryInterface(IID_PPV_ARGS(&be)))) return NULL;
        CStringW ct = Attr(be, L"content-type");

        CComBSTR b64;
        bin->get_text(&b64);

        ImagePart part;
        part.fbId = fbId;
        if (!DecodeBase64(CStringW(b64), part.bytes)) {
            CStringW warn;
            warn.Format(L"Не удалось декодировать base64 изображения binary id=\"%s\".", static_cast<LPCWSTR>(fbId));
            AddValidationWarning(warn);
            return NULL;
        }

        CString ext = ExtFromContentType(ct);
        if (ext.CompareNoCase(L"bin") == 0) {
            // Many real-world FB2 files declare images as application/octet-stream.
            // In that case identify the actual image type by magic bytes instead of
            // producing a noisy warning for a perfectly usable JPEG/PNG/GIF/etc.
            CString sigExt = ExtFromImageSignature(part.bytes);
            if (sigExt.CompareNoCase(L"bin") != 0) {
                ext = sigExt;
                ct = ContentTypeFromImageExtension(CStringW(ext));
            }
        }

        if (ext.CompareNoCase(L"bin") == 0) {
            CStringW warn;
            warn.Format(L"Неизвестный content-type изображения binary id=\"%s\": %s.", static_cast<LPCWSTR>(fbId), static_cast<LPCWSTR>(ct));
            AddValidationWarning(warn);
        }

        part.contentType = ct.IsEmpty() ? L"application/octet-stream" : ct;
        part.relId.Format(L"rId%d", m_nextRel++);
        part.fileName.Format(L"image%d.%s", m_nextImage++, static_cast<LPCWSTR>(ext));
        DetectImageSize(part);
        if (part.widthPx <= 0 || part.heightPx <= 0) {
            CStringW warn;
            warn.Format(L"Не удалось определить размеры изображения binary id=\"%s\"; применён размер по умолчанию.", static_cast<LPCWSTR>(fbId));
            AddValidationWarning(warn);
        }
        m_images.push_back(part);
        return &m_images.back();
    }

    CStringW NoteReferenceXml(IXMLDOMNodePtr linkNode) {
        if (m_opt.notesMode == 2) return L"";
        IXMLDOMElementPtr e;
        if (FAILED(linkNode->QueryInterface(IID_PPV_ARGS(&e)))) return L"";

        CStringW type = AttrAnyNode(linkNode, L"type", L"type");
        CStringW rawHref = AttrAnyNode(linkNode, L"href", L"l:href");
        type.Trim();
        rawHref.Trim();

        bool explicitNote = (type.CompareNoCase(L"note") == 0);
        bool hadHash = (!rawHref.IsEmpty() && rawHref[0] == L'#');

        // External links and ordinary non-anchor URLs are never FB2 notes.
        if (!rawHref.IsEmpty() && IsExternalHyperlink(rawHref)) return L"";
        if (!rawHref.IsEmpty() && !hadHash && !explicitNote) return L"";

        CStringW href = rawHref;
        if (!href.IsEmpty() && href[0] == L'#') href.Delete(0, 1);

        // Treat a link as a note only when the target really exists inside
        // body name="notes". Some real-world FB2 files incorrectly mark normal
        // table-of-contents links as type="note"; those must remain internal links.
        if (!href.IsEmpty() && !HasNoteTarget(href) && !FindNoteSectionById(href)) return L"";

        // If MSXML lost l:href, use visible [N] only when nN exists in notes.
        if (href.IsEmpty()) {
            CStringW visible = CollapseSpaces(TextOf(linkNode));
            CStringW digits;
            for (int i = 0; i < visible.GetLength(); ++i) {
                if (visible[i] >= L'0' && visible[i] <= L'9') digits += visible[i];
            }
            if (!digits.IsEmpty()) {
                CStringW candidate(L"n");
                candidate += digits;
                if (FindNoteSectionById(candidate)) href = candidate;
            }
        }
        if (href.IsEmpty()) return L"";

        if (IsFootnoteBuildInProgress(href)) {
            AddRecursiveFootnoteWarning(href);
            return L"";
        }

        ++m_noteReferences;
        FootnotePart* fn = FindOrCreateFootnote(href);
        if (!fn) {
            if (explicitNote) {
                ++m_missingNotes;
                CStringW warn;
                warn.Format(L"Ссылка на примечание #%s найдена, но соответствующий section в body name=notes не найден.", static_cast<LPCWSTR>(href));
                AddValidationWarning(warn);
            }
            return L"";
        }

        CStringW x;
        const wchar_t* refName = (m_opt.notesMode == 1) ? L"endnoteReference" : L"footnoteReference";
        const wchar_t* styleName = (m_opt.notesMode == 1) ? L"EndnoteReference" : L"FootnoteReference";
        x.Format(L"<w:r><w:rPr><w:rStyle w:val=\"%s\"/></w:rPr><w:%s w:id=\"%d\"/></w:r>", styleName, refName, fn->wordId);
        return x;
    }

    IXMLDOMNodePtr FindNoteSectionById(const CStringW& fbId) {
        if (fbId.IsEmpty()) return NULL;
        CStringW safeId = fbId;
        safeId.Replace(L"'", L"&apos;");
        IXMLDOMNodePtr note;
        CStringW xp;
        xp.Format(L"/*[local-name()='FictionBook']/*[local-name()='body' and @name='notes']/*[local-name()='section' and @id='%s']", static_cast<LPCWSTR>(safeId));
        m_doc->selectSingleNode(bstr_t(xp), &note);
        if (!note) {
            IXMLDOMNodePtr notesBody = FindBodyByName(RootOf(m_doc), L"notes");
            if (notesBody) note = FindDescendantById(notesBody, L"section", fbId);
        }
        return note;
    }

    FootnotePart* FindOrCreateFootnote(const CStringW& fbId) {
        for (size_t i = 0; i < m_footnotes.size(); ++i)
            if (m_footnotes[i].fbId == fbId) return &m_footnotes[i];

        IXMLDOMNodePtr note = FindNoteSectionById(fbId);
        if (!note) return NULL;

        if (NodeName(note).CompareNoCase(L"section") != 0) return NULL;

        std::wstring key(static_cast<LPCWSTR>(fbId));
        if (!m_buildingFootnotes.insert(key).second) {
            AddRecursiveFootnoteWarning(fbId);
            return NULL;
        }

        FootnotePart fn;
        fn.fbId = fbId;
        fn.wordId = (m_opt.notesMode == 1) ? m_nextEndnote++ : m_nextFootnote++;
        fn.xml = BuildNoteXml(note, fn.wordId);
        m_buildingFootnotes.erase(key);
        m_footnotes.push_back(fn);
        return &m_footnotes.back();
    }

    bool IsLikelyFootnoteLabelTitle(IXMLDOMNodePtr titleNode) {
        CStringW text = CollapseSpaces(TextOf(titleNode));
        if (text.IsEmpty()) return true;
        if (text.GetLength() > 16) return false;
        bool hasMeaningful = false;
        for (int i = 0; i < text.GetLength(); ++i) {
            wchar_t ch = text[i];
            if ((ch >= L'0' && ch <= L'9') || ch == L'*' || ch == L'[' || ch == L']' ||
                ch == L'(' || ch == L')' || ch == L'.' || ch == L',' || ch == L'-' ||
                ch == L'—' || ch == L' ' || ch == L'№') {
                continue;
            }
            hasMeaningful = true;
            break;
        }
        return !hasMeaningful;
    }

    CStringW BuildNoteXml(IXMLDOMNodePtr note, int wordId) {
        CStringW content;
        ++m_noteBodyDepth;
        bool firstParagraph = true;
        IXMLDOMNodeListPtr kids;
        note->get_childNodes(&kids);
        long n = 0;
        if (kids) kids->get_length(&n);
        for (long i = 0; i < n; ++i) {
            IXMLDOMNodePtr child;
            kids->get_item(i, &child);
            DOMNodeType nt;
            child->get_nodeType(&nt);
            if (nt != NODE_ELEMENT) continue;
            CStringW nm = NodeName(child);
            if (nm == L"p" || nm == L"subtitle" || nm == L"text-author" || nm == L"date") {
                CStringW runs;
                CollectInline(child, RunFmt(), runs);
                content += NoteParagraph(runs.IsEmpty() ? RunXml(L" ", RunFmt()) : runs, firstParagraph);
                firstParagraph = false;
            } else if (nm == L"title") {
                // Individual FB2 note sections often store the printed note number
                // in <title><p>11</p></title>. Word already generates the real
                // footnote number, so such titles must be skipped to avoid "1211".
                if (IsLikelyFootnoteLabelTitle(child)) continue;

                IXMLDOMNodeListPtr ps;
                child->selectNodes(bstr_t(L"./*[local-name()='p']"), &ps);
                long pc = 0;
                if (ps) ps->get_length(&pc);
                for (long p = 0; p < pc; ++p) {
                    IXMLDOMNodePtr para;
                    ps->get_item(p, &para);
                    RunFmt f; f.bold = true;
                    CStringW runs;
                    CollectInline(para, f, runs);
                    content += NoteParagraph(runs.IsEmpty() ? RunXml(L" ", f) : runs, firstParagraph);
                    firstParagraph = false;
                }
            } else if (nm == L"image") {
                CStringW drawing = ImageDrawingXml(child);
                if (!drawing.IsEmpty()) {
                    content += NoteParagraph(drawing, firstParagraph);
                    firstParagraph = false;
                }
            } else if (nm == L"empty-line") {
                content += NoteParagraph(RunXml(L" ", RunFmt()), firstParagraph);
                firstParagraph = false;
            } else {
                CStringW runs;
                CollectInline(child, RunFmt(), runs);
                if (!runs.IsEmpty()) {
                    content += NoteParagraph(runs, firstParagraph);
                    firstParagraph = false;
                }
            }
        }
        if (content.IsEmpty()) {
            CStringW text = CollapseSpaces(TextOf(note));
            content += NoteParagraph(text.IsEmpty() ? RunXml(L" ", RunFmt()) : RunXml(text, RunFmt()), true);
        }

        --m_noteBodyDepth;

        CStringW x;
        const wchar_t* tag = (m_opt.notesMode == 1) ? L"endnote" : L"footnote";
        x.Format(L"<w:%s w:id=\"%d\">", tag, wordId);
        x += content;
        x += L"</w:";
        x += tag;
        x += L">";
        return x;
    }

    CStringW NoteParagraph(const CStringW& runs, bool includeRef) {
        const bool endnote = (m_opt.notesMode == 1);
        CStringW x = L"<w:p><w:pPr><w:pStyle w:val=\"";
        x += endnote ? L"EndnoteText" : L"FootnoteText";
        x += L"\"/></w:pPr>";
        if (includeRef) {
            x += L"<w:r><w:rPr><w:rStyle w:val=\"";
            x += endnote ? L"EndnoteReference" : L"FootnoteReference";
            x += L"\"/></w:rPr>";
            x += endnote ? L"<w:endnoteRef/>" : L"<w:footnoteRef/>";
            x += L"</w:r>";
        }
        x += runs;
        x += L"</w:p>";
        return x;
    }

    int PositiveAttrInt(IXMLDOMNodePtr node, const wchar_t* name, int defValue = 0) {
        CStringW v = AttrAnyNode(node, name, name);
        v.Trim();
        int n = _wtoi(v);
        return n > 0 ? n : defValue;
    }

    CStringW TableCellXml(IXMLDOMNodePtr cell) {
        CStringW cellXml;
        IXMLDOMNodeListPtr ps;
        cell->selectNodes(bstr_t(L"./*[local-name()='p']"), &ps);
        long pn = 0;
        if (ps) ps->get_length(&pn);
        if (pn > 0) {
            for (long i = 0; i < pn; ++i) {
                IXMLDOMNodePtr p;
                ps->get_item(i, &p);
                CStringW runs;
                RunFmt f;
                if (NodeName(cell) == L"th") f.bold = true;
                CollectInline(p, f, runs);
                cellXml += ParagraphFromRuns(runs.IsEmpty() ? RunXml(L" ", f) : runs, NodeName(cell) == L"th" ? L"TableHeader" : L"TableText");
            }
        } else {
            CStringW runs;
            RunFmt f;
            if (NodeName(cell) == L"th") f.bold = true;
            CollectInline(cell, f, runs);
            cellXml += ParagraphFromRuns(runs.IsEmpty() ? RunXml(L" ", f) : runs, NodeName(cell) == L"th" ? L"TableHeader" : L"TableText");
        }
        return cellXml.IsEmpty() ? L"<w:p/>" : cellXml;
    }

    void ConvertTable(IXMLDOMNodePtr table) {
        ++m_tableCount;
        if (!m_pendingBookmarks.IsEmpty())
            m_body += TakePendingBookmarksParagraph();
        CStringW x;
        x += L"<w:tbl><w:tblPr><w:tblStyle w:val=\"TableGrid\"/><w:tblW w:w=\"5000\" w:type=\"pct\"/>"
             L"<w:tblBorders><w:top w:val=\"single\" w:sz=\"4\"/><w:left w:val=\"single\" w:sz=\"4\"/>"
             L"<w:bottom w:val=\"single\" w:sz=\"4\"/><w:right w:val=\"single\" w:sz=\"4\"/>"
             L"<w:insideH w:val=\"single\" w:sz=\"4\"/><w:insideV w:val=\"single\" w:sz=\"4\"/></w:tblBorders>"
             L"<w:tblLook w:val=\"04A0\" w:firstRow=\"1\" w:lastRow=\"0\" w:firstColumn=\"0\" w:lastColumn=\"0\" w:noHBand=\"0\" w:noVBand=\"1\"/></w:tblPr>";

        IXMLDOMNodeListPtr rows;
        table->selectNodes(bstr_t(L"./*[local-name()='tr']"), &rows);
        long rn = 0;
        if (rows) rows->get_length(&rn);
        if (rn == 0) {
            AddValidationWarning(L"Найдена таблица FB2 без строк tr; таблица пропущена.");
            return;
        }

        std::vector<int> activeRowspans;
        for (long r = 0; r < rn; ++r) {
            IXMLDOMNodePtr row;
            rows->get_item(r, &row);
            IXMLDOMNodeListPtr cells;
            row->selectNodes(bstr_t(L"./*[local-name()='th' or local-name()='td']"), &cells);
            long cn = 0;
            if (cells) cells->get_length(&cn);
            bool headerRow = (r == 0 && cn > 0);
            x += L"<w:tr>";
            if (headerRow) x += L"<w:trPr><w:tblHeader/></w:trPr>";
            int logicalCol = 0;
            if (cn == 0) {
                x += L"<w:tc><w:tcPr><w:tcW w:w=\"0\" w:type=\"auto\"/></w:tcPr><w:p/></w:tc>";
            }
            for (long c = 0; c < cn; ++c) {
                while (logicalCol < static_cast<int>(activeRowspans.size()) && activeRowspans[logicalCol] > 0) {
                    x += L"<w:tc><w:tcPr><w:tcW w:w=\"0\" w:type=\"auto\"/><w:vMerge/></w:tcPr><w:p/></w:tc>";
                    --activeRowspans[logicalCol];
                    ++logicalCol;
                }
                IXMLDOMNodePtr cell;
                cells->get_item(c, &cell);
                int colspan = PositiveAttrInt(cell, L"colspan", 1);
                int rowspan = PositiveAttrInt(cell, L"rowspan", 1);
                if (colspan > 1) ++m_tableColspanCount;
                if (rowspan > 1) ++m_tableRowspanCount;
                if (rowspan > 1 && colspan > 1)
                    AddValidationWarning(L"Таблица содержит ячейку с одновременными rowspan и colspan; вертикальное объединение применено только к основной колонке.");
                CStringW tcPr = L"<w:tcPr><w:tcW w:w=\"0\" w:type=\"auto\"/>";
                if (colspan > 1) {
                    CStringW span;
                    span.Format(L"<w:gridSpan w:val=\"%d\"/>", colspan);
                    tcPr += span;
                }
                if (rowspan > 1)
                    tcPr += L"<w:vMerge w:val=\"restart\"/>";
                if (NodeName(cell) == L"th")
                    tcPr += L"<w:shd w:fill=\"F2F2F2\"/>";
                tcPr += L"</w:tcPr>";
                x += L"<w:tc>" + tcPr + TableCellXml(cell) + L"</w:tc>";
                if (logicalCol >= static_cast<int>(activeRowspans.size()))
                    activeRowspans.resize(logicalCol + 1, 0);
                if (rowspan > 1)
                    activeRowspans[logicalCol] = rowspan - 1;
                logicalCol += colspan;
            }
            while (logicalCol < static_cast<int>(activeRowspans.size()) && activeRowspans[logicalCol] > 0) {
                x += L"<w:tc><w:tcPr><w:tcW w:w=\"0\" w:type=\"auto\"/><w:vMerge/></w:tcPr><w:p/></w:tc>";
                --activeRowspans[logicalCol];
                ++logicalCol;
            }
            x += L"</w:tr>";
        }
        x += L"</w:tbl>";
        m_body += x;
    }

    void AddValidationWarning(const wchar_t* text) {
        AddValidationWarning(CStringW(text ? text : L""));
    }

    void AddValidationWarning(const CStringW& text) {
        if (text.IsEmpty()) return;
        if (!m_validationWarnings.IsEmpty()) m_validationWarnings += L"\r\n";
        m_validationWarnings += L"- ";
        m_validationWarnings += text;
    }

    void ValidateDocxPackage() {
        if (!m_opt.validateDocx) return;
        if (m_body.IsEmpty()) AddValidationWarning(L"document.xml не содержит тела документа.");
        if (m_opt.addToc) {
            CStringW tocMarker = CStringW(L"TOC \\o \"1-") + TocDepthString();
            if (m_frontMatter.Find(tocMarker) < 0 && m_body.Find(tocMarker) < 0)
                AddValidationWarning(L"Не найдено поле оглавления TOC.");
        }
        for (size_t i = 0; i < m_images.size(); ++i) {
            if (m_images[i].bytes.empty()) AddValidationWarning(L"Найдено изображение с пустыми бинарными данными.");
            if (m_images[i].relId.IsEmpty() || m_images[i].fileName.IsEmpty())
                AddValidationWarning(L"Найдено изображение без relationship id или имени файла.");
        }
        for (size_t i = 0; i < m_footnotes.size(); ++i) {
            if (m_footnotes[i].xml.IsEmpty())
                AddValidationWarning(m_opt.notesMode == 1 ? L"Найдено пустое концевое примечание." : L"Найдена пустая сноска.");
        }
        for (size_t i = 0; i < m_hyperlinks.size(); ++i) {
            if (m_hyperlinks[i].relId.IsEmpty() || m_hyperlinks[i].target.IsEmpty())
                AddValidationWarning(L"Найдена гиперссылка без relationship id или адреса.");
        }
        if (m_opt.addHeaders && FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='book-title']").IsEmpty())
            AddValidationWarning(L"Включён верхний колонтитул, но название книги в FB2 не найдено.");
        if (m_tableCount > 0 && m_body.Find(L"<w:tbl>") < 0)
            AddValidationWarning(L"Таблицы были обнаружены, но в document.xml не найден блок w:tbl.");
        if (m_imageReferences > 0 && m_opt.exportImages && m_images.empty())
            AddValidationWarning(L"В тексте есть ссылки на изображения, но ни одно изображение не встроено в DOCX.");
        if (m_opt.docLanguage == 2 && LanguageCode().IsEmpty())
            AddValidationWarning(L"Язык документа выбран автоматически, но поле FB2 <lang> пустое или не распознано.");
    }

    CStringW ContentTypesXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                     L"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                     L"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                     L"<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                     L"<Default Extension=\"png\" ContentType=\"image/png\"/>"
                     L"<Default Extension=\"jpg\" ContentType=\"image/jpeg\"/>"
                     L"<Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>"
                     L"<Default Extension=\"gif\" ContentType=\"image/gif\"/>"
                     L"<Default Extension=\"bmp\" ContentType=\"image/bmp\"/>"
                     L"<Default Extension=\"tif\" ContentType=\"image/tiff\"/>"
                     L"<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
                     L"<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
                     L"<Override PartName=\"/docProps/custom.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.custom-properties+xml\"/>"
                     L"<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
                     L"<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
                     L"<Override PartName=\"/word/settings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml\"/>"
                     L"<Override PartName=\"/word/theme/theme1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.theme+xml\"/>";
        if (m_opt.addHeaders)
            x += L"<Override PartName=\"/word/header1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml\"/>";
        if (m_opt.addPageNumbers)
            x += L"<Override PartName=\"/word/footer1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml\"/>";
        if (!m_footnotes.empty() && m_opt.notesMode == 0)
            x += L"<Override PartName=\"/word/footnotes.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.footnotes+xml\"/>";
        if (!m_footnotes.empty() && m_opt.notesMode == 1)
            x += L"<Override PartName=\"/word/endnotes.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.endnotes+xml\"/>";
        x += L"</Types>";
        return x;
    }

    CStringW RootRelsXml() {
        return L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
               L"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
               L"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
               L"<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
               L"<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
               L"<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/custom-properties\" Target=\"docProps/custom.xml\"/>"
               L"</Relationships>";
    }

    CStringW DocumentRelsXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                     L"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                     L"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
                     L"<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings\" Target=\"settings.xml\"/>"
                     L"<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" Target=\"theme/theme1.xml\"/>";
        if (!m_footnotes.empty() && m_opt.notesMode == 0)
            x += L"<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/footnotes\" Target=\"footnotes.xml\"/>";
        if (!m_footnotes.empty() && m_opt.notesMode == 1)
            x += L"<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/endnotes\" Target=\"endnotes.xml\"/>";
        if (m_opt.addHeaders)
            x += L"<Relationship Id=\"" + m_headerRelId + L"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/header\" Target=\"header1.xml\"/>";
        if (m_opt.addPageNumbers)
            x += L"<Relationship Id=\"" + m_footerRelId + L"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer\" Target=\"footer1.xml\"/>";
        for (size_t i = 0; i < m_images.size(); ++i) {
            x += L"<Relationship Id=\"" + m_images[i].relId + L"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/" + m_images[i].fileName + L"\"/>";
        }
        for (size_t i = 0; i < m_hyperlinks.size(); ++i) {
            x += L"<Relationship Id=\"" + m_hyperlinks[i].relId +
                 L"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" Target=\"" +
                 XmlEscape(m_hyperlinks[i].target) + L"\" TargetMode=\"External\"/>";
        }
        x += L"</Relationships>";
        return x;
    }

    CStringW NotesRootNamespaces() const {
        return L" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\""
               L" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\""
               L" xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\""
               L" xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\""
               L" xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"";
    }

    CStringW NotesPartRelsXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                     L"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
        for (size_t i = 0; i < m_images.size(); ++i) {
            x += L"<Relationship Id=\"" + m_images[i].relId +
                 L"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/" +
                 m_images[i].fileName + L"\"/>";
        }
        for (size_t i = 0; i < m_hyperlinks.size(); ++i) {
            x += L"<Relationship Id=\"" + m_hyperlinks[i].relId +
                 L"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" Target=\"" +
                 XmlEscape(m_hyperlinks[i].target) + L"\" TargetMode=\"External\"/>";
        }
        x += L"</Relationships>";
        return x;
    }

    CStringW FootnotesXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
        x += L"<w:footnotes";
        x += NotesRootNamespaces();
        x += L">";
        x += L"<w:footnote w:type=\"separator\" w:id=\"-1\"><w:p><w:r><w:separator/></w:r></w:p></w:footnote>"
             L"<w:footnote w:type=\"continuationSeparator\" w:id=\"-2\"><w:p><w:r><w:continuationSeparator/></w:r></w:p></w:footnote>";
        for (size_t i = 0; i < m_footnotes.size(); ++i) x += m_footnotes[i].xml;
        x += L"</w:footnotes>";
        return x;
    }

    CStringW EndnotesXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
        x += L"<w:endnotes";
        x += NotesRootNamespaces();
        x += L">";
        x += L"<w:endnote w:type=\"separator\" w:id=\"-1\"><w:p><w:r><w:separator/></w:r></w:p></w:endnote>"
             L"<w:endnote w:type=\"continuationSeparator\" w:id=\"-2\"><w:p><w:r><w:continuationSeparator/></w:r></w:p></w:endnote>";
        for (size_t i = 0; i < m_footnotes.size(); ++i) x += m_footnotes[i].xml;
        x += L"</w:endnotes>";
        return x;
    }

    CStringW HeaderXml() {
        CStringW title = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='book-title']");
        if (title.IsEmpty()) title = L"ExportDOCX";
        CStringW runs = RunXml(title, RunFmt());
        return L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
               L"<w:hdr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
               L"<w:p><w:pPr><w:pStyle w:val=\"Header\"/><w:jc w:val=\"center\"/></w:pPr>" + runs + L"</w:p>"
               L"</w:hdr>";
    }

    CStringW FooterXml() {
        return L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
               L"<w:ftr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
               L"<w:p><w:pPr><w:pStyle w:val=\"Footer\"/><w:jc w:val=\"center\"/></w:pPr>"
               L"<w:r><w:fldChar w:fldCharType=\"begin\"/></w:r>"
               L"<w:r><w:instrText xml:space=\"preserve\"> PAGE </w:instrText></w:r>"
               L"<w:r><w:fldChar w:fldCharType=\"separate\"/></w:r>"
               L"<w:r><w:t>1</w:t></w:r>"
               L"<w:r><w:fldChar w:fldCharType=\"end\"/></w:r>"
               L"</w:p></w:ftr>";
    }

    CStringW PageSizeXml() const {
        if (m_opt.pageSize == 1) return L"<w:pgSz w:w=\"8391\" w:h=\"11906\"/>";  // A5
        if (m_opt.pageSize == 2) return L"<w:pgSz w:w=\"12240\" w:h=\"15840\"/>"; // Letter
        return L"<w:pgSz w:w=\"11906\" w:h=\"16838\"/>"; // A4
    }

    CStringW SourceLanguageRaw() {
        return FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='lang']");
    }

    CStringW MapFb2LanguageToWord(CStringW lang) {
        lang.Trim();
        lang.MakeLower();
        if (lang.IsEmpty()) return CStringW();
        int dash = lang.Find(L'-');
        if (dash < 0) dash = lang.Find(L'_');
        CStringW base = dash > 0 ? lang.Left(dash) : lang;
        if (base == L"ru") return L"ru-RU";
        if (base == L"en") return L"en-US";
        if (base == L"de") return L"de-DE";
        if (base == L"fr") return L"fr-FR";
        if (base == L"uk" || base == L"ua") return L"uk-UA";
        if (base == L"be") return L"be-BY";
        if (base == L"es") return L"es-ES";
        if (base == L"it") return L"it-IT";
        if (base == L"pl") return L"pl-PL";
        if (base == L"cs") return L"cs-CZ";
        if (base == L"tr") return L"tr-TR";
        if (base == L"zh") return L"zh-CN";
        if (base == L"ja") return L"ja-JP";
        return CStringW();
    }

    CStringW LanguageCode() {
        if (m_opt.docLanguage == 1) return L"en-US";
        if (m_opt.docLanguage == 2) return MapFb2LanguageToWord(SourceLanguageRaw());
        return L"ru-RU";
    }

    CStringW NotesModeText() const {
        if (m_opt.notesMode == 1) return L"концевые примечания Word";
        if (m_opt.notesMode == 2) return L"раздел «Примечания» в конце";
        return L"сноски Word";
    }

    CStringW PageSizeText() const {
        if (m_opt.pageSize == 1) return L"A5";
        if (m_opt.pageSize == 2) return L"Letter";
        return L"A4";
    }

    CStringW TocDepthString() const {
        CStringW v;
        int depth = m_opt.tocDepth;
        if (depth < 1 || depth > 6) depth = 3;
        v.Format(L"%d", depth);
        return v;
    }

    CStringW EmptyLineModeText() const {
        if (m_opt.emptyLineMode == 0) return L"игнорировать";
        if (m_opt.emptyLineMode == 1) return L"пустой абзац";
        if (m_opt.emptyLineMode == 3) return L"декоративный разделитель * * *";
        return L"интервал перед следующим абзацем";
    }

    CStringW ExportPresetText() const {
        if (m_opt.exportPreset == 1) return L"Минимальный DOCX";
        if (m_opt.exportPreset == 2) return L"Редакторский DOCX";
        return L"Книжный DOCX";
    }

    void LimitCoverImageSize(__int64& cx, __int64& cy) const {
        __int64 maxCx = 15LL * 360000; // usable width for A4/Letter with book margins
        __int64 maxCy = 23LL * 360000;
        if (m_opt.pageSize == 1) {      // A5
            maxCx = 10LL * 360000;
            maxCy = 16LL * 360000;
        } else if (m_opt.pageSize == 2) { // Letter
            maxCy = 22LL * 360000;
        }
        if (cx <= 0 || cy <= 0) return;
        // Cover images should fill the available page width. Previously the
        // cover was only downscaled, so low-resolution covers stayed tiny.
        cy = cy * maxCx / cx;
        cx = maxCx;
        if (cy > maxCy) {
            cx = cx * maxCy / cy;
            cy = maxCy;
        }
    }


    void LimitRegularImageHeight(__int64& cx, __int64& cy) {
        __int64 maxCy = 0;
        switch (m_opt.pageSize) {
        case 1: maxCy = 6120000; break;  // A5 working height, approx 17 cm
        case 2: maxCy = 8280000; break;  // Letter working height, approx 23 cm
        default: maxCy = 9000000; break; // A4 working height, approx 25 cm
        }
        if (cy > maxCy && cy > 0) {
            cx = cx * maxCy / cy;
            cy = maxCy;
            ++m_heightLimitedImages;
        }
    }

    CStringW MarginsXml() const {
        if (m_opt.styleProfile == 1) {
            return L"<w:pgMar w:top=\"720\" w:right=\"720\" w:bottom=\"720\" w:left=\"720\" w:header=\"567\" w:footer=\"567\" w:gutter=\"0\"/>";
        } else if (m_opt.styleProfile == 2) {
            return L"<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\" w:header=\"720\" w:footer=\"720\" w:gutter=\"0\"/>";
        }
        return L"<w:pgMar w:top=\"1134\" w:right=\"850\" w:bottom=\"1134\" w:left=\"1134\" w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/>";
    }

    CStringW SectionRefsXml(bool includeRefs) const {
        if (!includeRefs) return CStringW();
        CStringW refs;
        if (m_opt.addHeaders) refs += CStringW(L"<w:headerReference w:type=\"default\" r:id=\"") + m_headerRelId + L"\"/>";
        if (m_opt.addPageNumbers) refs += CStringW(L"<w:footerReference w:type=\"default\" r:id=\"") + m_footerRelId + L"\"/>";
        return refs;
    }

    CStringW SectionPrXml(bool includeRefs, bool restartNumbering, bool nextPage) const {
        CStringW x = L"<w:sectPr>";
        if (nextPage) x += L"<w:type w:val=\"nextPage\"/>";
        x += SectionRefsXml(includeRefs);
        x += PageSizeXml();
        x += MarginsXml();
        if (restartNumbering && m_opt.addPageNumbers) x += L"<w:pgNumType w:start=\"1\"/>";
        x += L"</w:sectPr>";
        return x;
    }

    CStringW SectionBreakParagraph(const CStringW& sectPr) const {
        return CStringW(L"<w:p><w:pPr>") + sectPr + L"</w:pPr></w:p>";
    }

    CStringW DocumentXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                     L"<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
                     L"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
                     L"xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
                     L"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
                     L"xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
                     L"<w:body>";
        if (!m_frontMatter.IsEmpty()) {
            x += m_frontMatter;
            bool includeFrontRefs = !m_opt.noTitlePageNumber;
            x += SectionBreakParagraph(SectionPrXml(includeFrontRefs, false, true));
        }
        x += m_body;
        bool restart = !m_frontMatter.IsEmpty() && m_opt.restartPageNumbering;
        x += SectionPrXml(true, restart, false);
        x += L"</w:body></w:document>";
        return x;
    }

    CStringW StylesXml() {
        CStringW fontName = L"Times New Roman";
        int fontSize = 24;
        int line = 276;
        int firstLineTwips = 567;
        CStringW jc = m_opt.justifyText ? L"both" : L"left";

        if (m_opt.styleProfile == 1) {
            // Компактный профиль: меньше шрифт и интервалы, но сохраняется книжная типографика.
            fontName = L"Times New Roman";
            fontSize = 21;
            line = 220;
            firstLineTwips = 426;
        } else if (m_opt.styleProfile == 2) {
            // Минимальный профиль: максимально простой Word-подобный вид без книжных отступов.
            fontName = L"Calibri";
            fontSize = 22;
            line = 240;
            firstLineTwips = 0;
            jc = L"left";
        }

        if (m_opt.customFont) {
            fontName = m_opt.fontName;
            fontName.Trim();
            if (fontName.IsEmpty()) fontName = L"Times New Roman";
            int pt = m_opt.fontSizePt;
            if (pt < 6) pt = 6;
            if (pt > 72) pt = 72;
            fontSize = pt * 2;
        }

        CStringW sz; sz.Format(L"%d", fontSize);
        CStringW lang = LanguageCode();
        CStringW langXml = lang.IsEmpty() ? CStringW(L"") : (CStringW(L"<w:lang w:val=\"") + lang + L"\"/>");
        CStringW lineVal; lineVal.Format(L"%d", line);
        CStringW firstLineVal; firstLineVal.Format(L"%d", m_opt.firstLineIndent ? firstLineTwips : 0);
        CStringW ind = CStringW(L"<w:ind w:firstLine=\"") + firstLineVal + L"\"/>";

        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
               L"<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">";
        x += L"<w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii=\"" + fontName + L"\" w:hAnsi=\"" + fontName + L"\" w:cs=\"" + fontName + L"\"/><w:sz w:val=\"" + sz + L"\"/><w:szCs w:val=\"" + sz + L"\"/>" + langXml + L"</w:rPr></w:rPrDefault>";
        x += L"<w:pPrDefault><w:pPr><w:spacing w:after=\"0\" w:line=\"" + lineVal + L"\" w:lineRule=\"auto\"/></w:pPr></w:pPrDefault></w:docDefaults>";
        x += L"<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/><w:qFormat/><w:pPr><w:jc w:val=\"" + jc + L"\"/><w:spacing w:after=\"0\" w:line=\"" + lineVal + L"\" w:lineRule=\"auto\"/>" + ind + L"</w:pPr><w:rPr><w:rFonts w:ascii=\"" + fontName + L"\" w:hAnsi=\"" + fontName + L"\" w:cs=\"" + fontName + L"\"/><w:sz w:val=\"" + sz + L"\"/>" + langXml + L"</w:rPr></w:style>";
        x += 
               L"<w:style w:type=\"paragraph\" w:styleId=\"BookTitle\"><w:name w:val=\"Book Title\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"240\" w:after=\"120\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"36\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"BookAuthor\"><w:name w:val=\"Book Author\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:after=\"120\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:i/><w:sz w:val=\"24\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TitleInfo\"><w:name w:val=\"Title Info\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:after=\"60\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:sz w:val=\"20\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/><w:basedOn w:val=\"Normal\"/><w:next w:val=\"Normal\"/><w:qFormat/><w:pPr><w:keepNext/><w:jc w:val=\"center\"/><w:spacing w:before=\"360\" w:after=\"160\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"32\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Heading2\"><w:name w:val=\"heading 2\"/><w:basedOn w:val=\"Normal\"/><w:next w:val=\"Normal\"/><w:qFormat/><w:pPr><w:keepNext/><w:jc w:val=\"center\"/><w:spacing w:before=\"240\" w:after=\"120\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"28\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Heading3\"><w:name w:val=\"heading 3\"/><w:basedOn w:val=\"Normal\"/><w:next w:val=\"Normal\"/><w:qFormat/><w:pPr><w:keepNext/><w:jc w:val=\"left\"/><w:spacing w:before=\"160\" w:after=\"80\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:i/><w:sz w:val=\"26\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Heading4\"><w:name w:val=\"heading 4\"/><w:basedOn w:val=\"Normal\"/><w:next w:val=\"Normal\"/><w:qFormat/><w:pPr><w:keepNext/><w:jc w:val=\"left\"/><w:spacing w:before=\"120\" w:after=\"60\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"24\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Subtitle\"><w:name w:val=\"Subtitle\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"120\" w:after=\"120\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:i/><w:sz w:val=\"24\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Annotation\"><w:name w:val=\"Annotation\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"both\"/><w:spacing w:after=\"80\"/><w:ind w:left=\"567\" w:right=\"567\" w:firstLine=\"567\"/></w:pPr><w:rPr><w:i/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Epigraph\"><w:name w:val=\"Epigraph\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"right\"/><w:spacing w:after=\"80\"/><w:ind w:left=\"2268\" w:firstLine=\"0\"/></w:pPr><w:rPr><w:i/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TextAuthor\"><w:name w:val=\"Text Author\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"right\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:i/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Date\"><w:name w:val=\"Date\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"right\"/><w:ind w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Quote\"><w:name w:val=\"Quote\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"both\"/><w:ind w:left=\"567\" w:right=\"567\" w:firstLine=\"567\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Verse\"><w:name w:val=\"Verse\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"left\"/><w:spacing w:after=\"0\" w:line=\"240\" w:lineRule=\"auto\"/><w:ind w:left=\"567\" w:hanging=\"0\" w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"PoemTitle\"><w:name w:val=\"Poem Title\"/><w:basedOn w:val=\"Verse\"/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"120\" w:after=\"80\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:i/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"VerseSpace\"><w:name w:val=\"Verse Space\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:after=\"80\"/><w:ind w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Image\"><w:name w:val=\"Image\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"120\" w:after=\"120\"/><w:ind w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Caption\"><w:name w:val=\"Caption\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"0\" w:after=\"120\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:i/><w:sz w:val=\"20\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TOCTitle\"><w:name w:val=\"TOC Title\"/><w:basedOn w:val=\"Normal\"/><w:qFormat/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:before=\"240\" w:after=\"160\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"32\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TOC1\"><w:name w:val=\"TOC 1\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:tabs><w:tab w:val=\"right\" w:leader=\"dot\" w:pos=\"9350\"/></w:tabs><w:spacing w:after=\"40\"/><w:ind w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TOC2\"><w:name w:val=\"TOC 2\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:tabs><w:tab w:val=\"right\" w:leader=\"dot\" w:pos=\"9350\"/></w:tabs><w:spacing w:after=\"20\"/><w:ind w:left=\"283\" w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TOC3\"><w:name w:val=\"TOC 3\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:tabs><w:tab w:val=\"right\" w:leader=\"dot\" w:pos=\"9350\"/></w:tabs><w:spacing w:after=\"20\"/><w:ind w:left=\"567\" w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TOC4\"><w:name w:val=\"TOC 4\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:tabs><w:tab w:val=\"right\" w:leader=\"dot\" w:pos=\"9350\"/></w:tabs><w:spacing w:after=\"20\"/><w:ind w:left=\"850\" w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TOCInstruction\"><w:name w:val=\"TOC Instruction\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"left\"/><w:spacing w:before=\"80\" w:after=\"160\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:i/><w:sz w:val=\"18\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Header\"><w:name w:val=\"header\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:after=\"0\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:sz w:val=\"18\"/><w:i/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"Footer\"><w:name w:val=\"footer\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:after=\"0\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:sz w:val=\"18\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"EmptyLine\"><w:name w:val=\"Empty Line\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:after=\"120\"/><w:ind w:firstLine=\"0\"/></w:pPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TableText\"><w:name w:val=\"Table Text\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"left\"/><w:spacing w:after=\"0\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:sz w:val=\"22\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"TableHeader\"><w:name w:val=\"Table Header\"/><w:basedOn w:val=\"TableText\"/><w:pPr><w:jc w:val=\"center\"/><w:spacing w:after=\"0\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"22\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"CodeBlock\"><w:name w:val=\"Code Block\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:before=\"80\" w:after=\"80\"/><w:ind w:left=\"567\" w:firstLine=\"0\"/></w:pPr><w:rPr><w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\" w:cs=\"Consolas\"/><w:sz w:val=\"20\"/></w:rPr></w:style>"
               L"<w:style w:type=\"character\" w:styleId=\"CodeChar\"><w:name w:val=\"Code Char\"/><w:rPr><w:rFonts w:ascii=\"Consolas\" w:hAnsi=\"Consolas\" w:cs=\"Consolas\"/><w:sz w:val=\"22\"/></w:rPr></w:style>"
               L"<w:style w:type=\"character\" w:styleId=\"Hyperlink\"><w:name w:val=\"Hyperlink\"/><w:rPr><w:color w:val=\"0563C1\"/><w:u w:val=\"single\"/></w:rPr></w:style>"
               L"<w:style w:type=\"table\" w:styleId=\"TableGrid\"><w:name w:val=\"Table Grid\"/><w:tblPr><w:tblBorders><w:top w:val=\"single\" w:sz=\"4\"/><w:left w:val=\"single\" w:sz=\"4\"/><w:bottom w:val=\"single\" w:sz=\"4\"/><w:right w:val=\"single\" w:sz=\"4\"/><w:insideH w:val=\"single\" w:sz=\"4\"/><w:insideV w:val=\"single\" w:sz=\"4\"/></w:tblBorders></w:tblPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"FootnoteText\"><w:name w:val=\"footnote text\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:after=\"0\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:sz w:val=\"18\"/><w:szCs w:val=\"18\"/></w:rPr></w:style>"
               L"<w:style w:type=\"character\" w:styleId=\"FootnoteReference\"><w:name w:val=\"footnote reference\"/><w:semiHidden/><w:rPr><w:vertAlign w:val=\"superscript\"/></w:rPr></w:style>"
               L"<w:style w:type=\"paragraph\" w:styleId=\"EndnoteText\"><w:name w:val=\"endnote text\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:after=\"0\"/><w:ind w:firstLine=\"0\"/></w:pPr><w:rPr><w:sz w:val=\"18\"/><w:szCs w:val=\"18\"/></w:rPr></w:style>"
               L"<w:style w:type=\"character\" w:styleId=\"EndnoteReference\"><w:name w:val=\"endnote reference\"/><w:semiHidden/><w:rPr><w:vertAlign w:val=\"superscript\"/></w:rPr></w:style>"
               L"</w:styles>";
        return x;
    }

    CStringW SettingsXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            L"<w:settings xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
            L"<w:zoom w:percent=\"100\"/><w:defaultTabStop w:val=\"708\"/><w:characterSpacingControl w:val=\"doNotCompress\"/>";
        if (m_opt.addToc)
            x += L"<w:updateFields w:val=\"true\"/>";
        if (m_opt.autoHyphenation)
            x += L"<w:autoHyphenation/><w:hyphenationZone w:val=\"360\"/>";
        { CStringW lang = LanguageCode(); if (!lang.IsEmpty()) x += CStringW(L"<w:themeFontLang w:val=\"") + lang + L"\"/>"; }
        x += L"<w:compat><w:compatSetting w:name=\"compatibilityMode\" w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"15\"/></w:compat>";
        x += L"</w:settings>";
        return x;
    }

    CStringW FirstTextByXPath(const wchar_t* xpath) {
        IXMLDOMNodePtr n;
        m_doc->selectSingleNode(bstr_t(xpath), &n);
        if (!n) return CStringW();
        return NormalizeText(TextOf(n));
    }

    CStringW JoinTextsByXPath(const wchar_t* xpath, const wchar_t* sep) {
        CStringW out;
        IXMLDOMNodeListPtr nodes;
        m_doc->selectNodes(bstr_t(xpath), &nodes);
        long count = 0;
        if (nodes) nodes->get_length(&count);
        for (long i = 0; i < count; ++i) {
            IXMLDOMNodePtr n;
            nodes->get_item(i, &n);
            CStringW t = NormalizeText(TextOf(n));
            if (t.IsEmpty()) continue;
            if (!out.IsEmpty()) out += sep;
            out += t;
        }
        return out;
    }

    CStringW AuthorLine(IXMLDOMNodePtr author) {
        CStringW line;
        AppendAuthorPart(author, L"first-name", line);
        AppendAuthorPart(author, L"middle-name", line);
        AppendAuthorPart(author, L"last-name", line);
        if (line.IsEmpty()) AppendAuthorPart(author, L"nickname", line);
        return line;
    }

    CStringW AuthorsTextByXPath(const wchar_t* xpath) {
        CStringW out;
        IXMLDOMNodeListPtr authors;
        m_doc->selectNodes(bstr_t(xpath), &authors);
        long count = 0;
        if (authors) authors->get_length(&count);
        for (long i = 0; i < count; ++i) {
            IXMLDOMNodePtr a;
            authors->get_item(i, &a);
            CStringW line = AuthorLine(a);
            if (line.IsEmpty()) continue;
            if (!out.IsEmpty()) out += L"; ";
            out += line;
        }
        return out;
    }

    CStringW AuthorsText() {
        return AuthorsTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='author']");
    }

    CStringW TranslatorsText() {
        return AuthorsTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='translator']");
    }

    CStringW SequenceText() {
        CStringW name = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='sequence']/@name");
        CStringW number = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='sequence']/@number");
        CStringW out = name;
        if (!number.IsEmpty()) {
            if (!out.IsEmpty()) out += L" №";
            out += number;
        }
        return out;
    }

    CStringW AnnotationText() {
        IXMLDOMNodePtr n;
        m_doc->selectSingleNode(bstr_t(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='annotation']"), &n);
        if (!n) return CStringW();
        return CollapseSpaces(TextOf(n));
    }

    CStringW PublishInfoText() {
        CStringW bookName = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='publish-info']/*[local-name()='book-name']");
        CStringW publisher = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='publish-info']/*[local-name()='publisher']");
        CStringW city = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='publish-info']/*[local-name()='city']");
        CStringW year = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='publish-info']/*[local-name()='year']");
        CStringW isbn = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='publish-info']/*[local-name()='isbn']");
        CStringW out;
        if (!bookName.IsEmpty()) out += bookName;
        if (!publisher.IsEmpty()) { if (!out.IsEmpty()) out += L"; "; out += publisher; }
        if (!city.IsEmpty()) { if (!out.IsEmpty()) out += L", "; out += city; }
        if (!year.IsEmpty()) { if (!out.IsEmpty()) out += L", "; out += year; }
        if (!isbn.IsEmpty()) { if (!out.IsEmpty()) out += L", "; out += L"ISBN "; out += isbn; }
        return out;
    }

    CStringW SourceTitleInfoText() {
        CStringW title = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='src-title-info']/*[local-name()='book-title']");
        CStringW authors = AuthorsTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='src-title-info']/*[local-name()='author']");
        CStringW lang = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='src-title-info']/*[local-name()='lang']");
        CStringW out;
        if (!title.IsEmpty()) out += title;
        if (!authors.IsEmpty()) { if (!out.IsEmpty()) out += L" — "; out += authors; }
        if (!lang.IsEmpty()) { if (!out.IsEmpty()) out += L"; "; out += L"lang="; out += lang; }
        return out;
    }

    CStringW DocumentHistoryText() {
        IXMLDOMNodePtr n;
        m_doc->selectSingleNode(bstr_t(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='history']"), &n);
        if (!n) return CStringW();
        return CollapseSpaces(TextOf(n));
    }

    CStringW CorePropsXml() {
        CStringW title = FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='book-title']");
        if (title.IsEmpty()) title = L"FB2 document";

        CStringW creator = m_opt.exportMetadata ? AuthorsText() : CStringW();
        if (creator.IsEmpty()) creator = L"FB Editor ExportDOCX";

        CStringW genres = m_opt.exportMetadata ? JoinTextsByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='genre']", L"; ") : CStringW();
        CStringW lang = m_opt.exportMetadata ? FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='lang']") : CStringW();
        CStringW annotation = m_opt.exportMetadata ? AnnotationText() : CStringW();

        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            L"<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">";
        x += L"<dc:title>" + XmlEscape(title) + L"</dc:title>";
        x += L"<dc:creator>" + XmlEscape(creator) + L"</dc:creator>";
        x += L"<cp:lastModifiedBy>FB Editor ExportDOCX</cp:lastModifiedBy>";
        if (!genres.IsEmpty()) {
            x += L"<cp:keywords>" + XmlEscape(genres) + L"</cp:keywords>";
            x += L"<dc:subject>" + XmlEscape(genres) + L"</dc:subject>";
        }
        if (!annotation.IsEmpty()) x += L"<dc:description>" + XmlEscape(annotation) + L"</dc:description>";
        if (!lang.IsEmpty()) x += L"<dc:language>" + XmlEscape(lang) + L"</dc:language>";
        x += L"</cp:coreProperties>";
        return x;
    }

    void AppendCustomStringProperty(CStringW& x, int& pid, const wchar_t* name, const CStringW& value) {
        if (!name || !*name || value.IsEmpty()) return;
        CStringW prop;
        prop.Format(L"<property fmtid=\"{D5CDD505-2E9C-101B-9397-08002B2CF9AE}\" pid=\"%d\" name=\"%s\"><vt:lpwstr>%s</vt:lpwstr></property>",
            pid++, name, static_cast<LPCWSTR>(XmlEscape(value)));
        x += prop;
    }

    CStringW CustomPropsXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            L"<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/custom-properties\" xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">";
        int pid = 2;
        AppendCustomStringProperty(x, pid, L"FB2 ID", FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='id']"));
        AppendCustomStringProperty(x, pid, L"FB2 Version", FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='version']"));
        AppendCustomStringProperty(x, pid, L"FB2 Program", FirstTextByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='program-used']"));
        AppendCustomStringProperty(x, pid, L"FB2 Genres", JoinTextsByXPath(L"/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='title-info']/*[local-name()='genre']", L"; "));
        AppendCustomStringProperty(x, pid, L"FB2 Translators", TranslatorsText());
        AppendCustomStringProperty(x, pid, L"FB2 Sequence", SequenceText());
        AppendCustomStringProperty(x, pid, L"FB2 Source Title", SourceTitleInfoText());
        AppendCustomStringProperty(x, pid, L"FB2 Publish Info", PublishInfoText());
        x += L"</Properties>";
        return x;
    }

    CStringW AppPropsXml() {
        CStringW x = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            L"<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
            L"<Application>FB Editor ExportDOCX</Application><DocSecurity>0</DocSecurity><ScaleCrop>false</ScaleCrop><LinksUpToDate>false</LinksUpToDate><SharedDoc>false</SharedDoc><HyperlinksChanged>false</HyperlinksChanged>";
        x += CStringW(L"<Paragraphs>") + IntToString(m_paragraphCount) + L"</Paragraphs>";
        x += L"<AppVersion>1.0000</AppVersion></Properties>";
        return x;
    }

    CStringW ThemeXml() {
        return L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><a:theme xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" name=\"Тема Office\"><a:themeElements><a:clrScheme name=\"Стандартная\"><a:dk1><a:sysClr val=\"windowText\" lastClr=\"000000\"/></a:dk1><a:lt1><a:sysClr val=\"window\" lastClr=\"FFFFFF\"/></a:lt1><a:dk2><a:srgbClr val=\"0E2841\"/></a:dk2><a:lt2><a:srgbClr val=\"E8E8E8\"/></a:lt2><a:accent1><a:srgbClr val=\"156082\"/></a:accent1><a:accent2><a:srgbClr val=\"E97132\"/></a:accent2><a:accent3><a:srgbClr val=\"196B24\"/></a:accent3><a:accent4><a:srgbClr val=\"0F9ED5\"/></a:accent4><a:accent5><a:srgbClr val=\"A02B93\"/></a:accent5><a:accent6><a:srgbClr val=\"4EA72E\"/></a:accent6><a:hlink><a:srgbClr val=\"467886\"/></a:hlink><a:folHlink><a:srgbClr val=\"96607D\"/></a:folHlink></a:clrScheme><a:fontScheme name=\"Стандартная\"><a:majorFont><a:latin typefac"
               L"e=\"Aptos Display\" panose=\"02110004020202020204\"/><a:ea typeface=\"\"/><a:cs typeface=\"\"/><a:font script=\"Jpan\" typeface=\"游ゴシック Light\"/><a:font script=\"Hang\" typeface=\"맑은 고딕\"/><a:font script=\"Hans\" typeface=\"等线 Light\"/><a:font script=\"Hant\" typeface=\"新細明體\"/><a:font script=\"Arab\" typeface=\"Times New Roman\"/><a:font script=\"Hebr\" typeface=\"Times New Roman\"/><a:font script=\"Thai\" typeface=\"Angsana New\"/><a:font script=\"Ethi\" typeface=\"Nyala\"/><a:font script=\"Beng\" typeface=\"Vrinda\"/><a:font script=\"Gujr\" typeface=\"Shruti\"/><a:font script=\"Khmr\" typeface=\"MoolBoran\"/><a:font script=\"Knda\" typeface=\"Tunga\"/><a:font script=\"Guru\" typeface=\"Raavi\"/><a:font script=\"Cans\" typeface=\"Euphemia\"/><a:font script=\"Cher\" typeface=\"Plantagenet Cherokee\"/><a:font script=\"Yiii\" typeface=\"Microsoft Yi Baiti\"/><a:font script=\"Tibt\" typeface=\"Microsoft Himalaya\"/"
               L"><a:font script=\"Thaa\" typeface=\"MV Boli\"/><a:font script=\"Deva\" typeface=\"Mangal\"/><a:font script=\"Telu\" typeface=\"Gautami\"/><a:font script=\"Taml\" typeface=\"Latha\"/><a:font script=\"Syrc\" typeface=\"Estrangelo Edessa\"/><a:font script=\"Orya\" typeface=\"Kalinga\"/><a:font script=\"Mlym\" typeface=\"Kartika\"/><a:font script=\"Laoo\" typeface=\"DokChampa\"/><a:font script=\"Sinh\" typeface=\"Iskoola Pota\"/><a:font script=\"Mong\" typeface=\"Mongolian Baiti\"/><a:font script=\"Viet\" typeface=\"Times New Roman\"/><a:font script=\"Uigh\" typeface=\"Microsoft Uighur\"/><a:font script=\"Geor\" typeface=\"Sylfaen\"/><a:font script=\"Armn\" typeface=\"Arial\"/><a:font script=\"Bugi\" typeface=\"Leelawadee UI\"/><a:font script=\"Bopo\" typeface=\"Microsoft JhengHei\"/><a:font script=\"Java\" typeface=\"Javanese Text\"/><a:font script=\"Lisu\" typeface=\"Segoe UI\"/><a:font script=\"Mymr\" typeface=\""
               L"Myanmar Text\"/><a:font script=\"Nkoo\" typeface=\"Ebrima\"/><a:font script=\"Olck\" typeface=\"Nirmala UI\"/><a:font script=\"Osma\" typeface=\"Ebrima\"/><a:font script=\"Phag\" typeface=\"Phagspa\"/><a:font script=\"Syrn\" typeface=\"Estrangelo Edessa\"/><a:font script=\"Syrj\" typeface=\"Estrangelo Edessa\"/><a:font script=\"Syre\" typeface=\"Estrangelo Edessa\"/><a:font script=\"Sora\" typeface=\"Nirmala UI\"/><a:font script=\"Tale\" typeface=\"Microsoft Tai Le\"/><a:font script=\"Talu\" typeface=\"Microsoft New Tai Lue\"/><a:font script=\"Tfng\" typeface=\"Ebrima\"/></a:majorFont><a:minorFont><a:latin typeface=\"Aptos\" panose=\"02110004020202020204\"/><a:ea typeface=\"\"/><a:cs typeface=\"\"/><a:font script=\"Jpan\" typeface=\"游明朝\"/><a:font script=\"Hang\" typeface=\"맑은 고딕\"/><a:font script=\"Hans\" typeface=\"等线\"/><a:font script=\"Hant\" typeface=\"新細明體\"/><a:font script=\"Arab\" typeface=\"Arial\"/><"
               L"a:font script=\"Hebr\" typeface=\"Arial\"/><a:font script=\"Thai\" typeface=\"Cordia New\"/><a:font script=\"Ethi\" typeface=\"Nyala\"/><a:font script=\"Beng\" typeface=\"Vrinda\"/><a:font script=\"Gujr\" typeface=\"Shruti\"/><a:font script=\"Khmr\" typeface=\"DaunPenh\"/><a:font script=\"Knda\" typeface=\"Tunga\"/><a:font script=\"Guru\" typeface=\"Raavi\"/><a:font script=\"Cans\" typeface=\"Euphemia\"/><a:font script=\"Cher\" typeface=\"Plantagenet Cherokee\"/><a:font script=\"Yiii\" typeface=\"Microsoft Yi Baiti\"/><a:font script=\"Tibt\" typeface=\"Microsoft Himalaya\"/><a:font script=\"Thaa\" typeface=\"MV Boli\"/><a:font script=\"Deva\" typeface=\"Mangal\"/><a:font script=\"Telu\" typeface=\"Gautami\"/><a:font script=\"Taml\" typeface=\"Latha\"/><a:font script=\"Syrc\" typeface=\"Estrangelo Edessa\"/><a:font script=\"Orya\" typeface=\"Kalinga\"/><a:font script=\"Mlym\" typeface=\"Kartika\"/><a:font script=\""
               L"Laoo\" typeface=\"DokChampa\"/><a:font script=\"Sinh\" typeface=\"Iskoola Pota\"/><a:font script=\"Mong\" typeface=\"Mongolian Baiti\"/><a:font script=\"Viet\" typeface=\"Arial\"/><a:font script=\"Uigh\" typeface=\"Microsoft Uighur\"/><a:font script=\"Geor\" typeface=\"Sylfaen\"/><a:font script=\"Armn\" typeface=\"Arial\"/><a:font script=\"Bugi\" typeface=\"Leelawadee UI\"/><a:font script=\"Bopo\" typeface=\"Microsoft JhengHei\"/><a:font script=\"Java\" typeface=\"Javanese Text\"/><a:font script=\"Lisu\" typeface=\"Segoe UI\"/><a:font script=\"Mymr\" typeface=\"Myanmar Text\"/><a:font script=\"Nkoo\" typeface=\"Ebrima\"/><a:font script=\"Olck\" typeface=\"Nirmala UI\"/><a:font script=\"Osma\" typeface=\"Ebrima\"/><a:font script=\"Phag\" typeface=\"Phagspa\"/><a:font script=\"Syrn\" typeface=\"Estrangelo Edessa\"/><a:font script=\"Syrj\" typeface=\"Estrangelo Edessa\"/><a:font script=\"Syre\" typeface=\"Estrange"
               L"lo Edessa\"/><a:font script=\"Sora\" typeface=\"Nirmala UI\"/><a:font script=\"Tale\" typeface=\"Microsoft Tai Le\"/><a:font script=\"Talu\" typeface=\"Microsoft New Tai Lue\"/><a:font script=\"Tfng\" typeface=\"Ebrima\"/></a:minorFont></a:fontScheme><a:fmtScheme name=\"Стандартная\"><a:fillStyleLst><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill><a:gradFill rotWithShape=\"1\"><a:gsLst><a:gs pos=\"0\"><a:schemeClr val=\"phClr\"><a:lumMod val=\"110000\"/><a:satMod val=\"105000\"/><a:tint val=\"67000\"/></a:schemeClr></a:gs><a:gs pos=\"50000\"><a:schemeClr val=\"phClr\"><a:lumMod val=\"105000\"/><a:satMod val=\"103000\"/><a:tint val=\"73000\"/></a:schemeClr></a:gs><a:gs pos=\"100000\"><a:schemeClr val=\"phClr\"><a:lumMod val=\"105000\"/><a:satMod val=\"109000\"/><a:tint val=\"81000\"/></a:schemeClr></a:gs></a:gsLst><a:lin ang=\"5400000\" scaled=\"0\"/></a:gradFill><a:gradFill rotWithShape="
               L"\"1\"><a:gsLst><a:gs pos=\"0\"><a:schemeClr val=\"phClr\"><a:satMod val=\"103000\"/><a:lumMod val=\"102000\"/><a:tint val=\"94000\"/></a:schemeClr></a:gs><a:gs pos=\"50000\"><a:schemeClr val=\"phClr\"><a:satMod val=\"110000\"/><a:lumMod val=\"100000\"/><a:shade val=\"100000\"/></a:schemeClr></a:gs><a:gs pos=\"100000\"><a:schemeClr val=\"phClr\"><a:lumMod val=\"99000\"/><a:satMod val=\"120000\"/><a:shade val=\"78000\"/></a:schemeClr></a:gs></a:gsLst><a:lin ang=\"5400000\" scaled=\"0\"/></a:gradFill></a:fillStyleLst><a:lnStyleLst><a:ln w=\"6350\" cap=\"flat\" cmpd=\"sng\" algn=\"ctr\"><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill><a:prstDash val=\"solid\"/><a:miter lim=\"800000\"/></a:ln><a:ln w=\"12700\" cap=\"flat\" cmpd=\"sng\" algn=\"ctr\"><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill><a:prstDash val=\"solid\"/><a:miter lim=\"800000\"/></a:ln><a:ln w=\"19050\" cap=\"flat\" cmpd=\"sng\" a"
               L"lgn=\"ctr\"><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill><a:prstDash val=\"solid\"/><a:miter lim=\"800000\"/></a:ln></a:lnStyleLst><a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle><a:effectStyle><a:effectLst/></a:effectStyle><a:effectStyle><a:effectLst><a:outerShdw blurRad=\"57150\" dist=\"19050\" dir=\"5400000\" algn=\"ctr\" rotWithShape=\"0\"><a:srgbClr val=\"000000\"><a:alpha val=\"63000\"/></a:srgbClr></a:outerShdw></a:effectLst></a:effectStyle></a:effectStyleLst><a:bgFillStyleLst><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill><a:solidFill><a:schemeClr val=\"phClr\"><a:tint val=\"95000\"/><a:satMod val=\"170000\"/></a:schemeClr></a:solidFill><a:gradFill rotWithShape=\"1\"><a:gsLst><a:gs pos=\"0\"><a:schemeClr val=\"phClr\"><a:tint val=\"93000\"/><a:satMod val=\"150000\"/><a:shade val=\"98000\"/><a:lumMod val=\"102000\"/></a:schemeClr></a:gs><a:gs"
               L" pos=\"50000\"><a:schemeClr val=\"phClr\"><a:tint val=\"98000\"/><a:satMod val=\"130000\"/><a:shade val=\"90000\"/><a:lumMod val=\"103000\"/></a:schemeClr></a:gs><a:gs pos=\"100000\"><a:schemeClr val=\"phClr\"><a:shade val=\"63000\"/><a:satMod val=\"120000\"/></a:schemeClr></a:gs></a:gsLst><a:lin ang=\"5400000\" scaled=\"0\"/></a:gradFill></a:bgFillStyleLst></a:fmtScheme></a:themeElements><a:objectDefaults/><a:extraClrSchemeLst/><a:extLst><a:ext uri=\"{05A4C25C-085E-4340-85A3-A5531E510DB2}\"><thm15:themeFamily xmlns:thm15=\"http://schemas.microsoft.com/office/thememl/2012/main\" name=\"Office Theme\" id=\"{2E142A2C-CD16-42D6-873A-C26D2A0506FA}\" vid=\"{1BDDFF52-6CD6-40A5-AB3C-68EB2F1E4D0A}\"/></a:ext></a:extLst></a:theme>";
    }
};


} // namespace

extern "C" __declspec(dllexport) HRESULT WINAPI ExportFB2FileToDOCX(LPCWSTR fb2Path, LPCWSTR docxPath)
{
    if (!fb2Path || !*fb2Path || !docxPath || !*docxPath)
        return E_INVALIDARG;

    HRESULT hrCo = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hrCo);
    if (FAILED(hrCo) && hrCo != RPC_E_CHANGED_MODE)
        return hrCo;

    HRESULT result = E_FAIL;

    try {
        // Keep all COM smart pointers inside this inner scope so that they are
        // released before CoUninitialize(). This is important for the batch EXE:
        // releasing MSXML smart pointers after COM uninitialization may raise an
        // access violation at the function exit.
        {
            IXMLDOMDocument2Ptr source;

            // Do not use CLSID_DOMDocument60 here: in some Visual Studio/Windows SDK
            // configurations it is declared as an external GUID and requires an
            // additional import library, which may cause LNK2001. Resolving the
            // MSXML6 ProgID at runtime keeps the batch-export entry point link-safe.
            HRESULT hr = source.CreateInstance(L"Msxml2.DOMDocument.6.0");
            if (FAILED(hr) || source == NULL) {
                result = FAILED(hr) ? hr : E_FAIL;
            } else {
                source->put_async(VARIANT_FALSE);
                source->put_validateOnParse(VARIANT_FALSE);
                source->put_resolveExternals(VARIANT_FALSE);

                VARIANT_BOOL ok = VARIANT_FALSE;
                hr = source->load(_variant_t(fb2Path), &ok);
                if (FAILED(hr) || ok != VARIANT_TRUE) {
                    result = FAILED(hr) ? hr : E_FAIL;
                } else {
                    DocxExportSettings settings;
                    LoadDocxSettings(settings);

                    CZipStoreWriter zip;
                    if (!zip.Open(docxPath)) {
                        DWORD err = ::GetLastError();
                        result = HRESULT_FROM_WIN32(err);
                    } else {
                        CDocxBuilder builder(source, settings);
                        if (!builder.Build(zip)) {
                            zip.Close();
                            result = E_FAIL;
                        } else {
                            zip.Close();

                            if (settings.createReport) {
                                CStringW reportName = MakeReportFileName(CStringW(docxPath));
                                WriteUtf8TextFile(reportName, builder.ReportText(CStringW(docxPath)));
                            }

                            result = S_OK;
                        }
                    }
                }
            }
        }
    }
    catch (...) {
        result = E_FAIL;
    }

    if (needUninit) ::CoUninitialize();
    return result;
}

HRESULT CExportDOCXPlugin::Export(long hWnd, BSTR filename, IDispatch *doc)
{
    HWND hwndParent = static_cast<HWND>(LongToHandle(hWnd));
    try {
        IXMLDOMDocument2Ptr source(doc);
        if (source == NULL) return E_INVALIDARG;

        DocxExportSettings settings;
        LoadDocxSettings(settings);

        CString outName(filename ? filename : L"");
        int dot = outName.ReverseFind(_T('.'));
        if (dot >= 0) outName.Delete(dot, outName.GetLength() - dot);
        outName += _T(".docx");

        CString strFilter;
        strFilter.LoadString(IDS_SAVE_FILE_FILTER);
        strFilter.Replace(_T('|'), _T('\0'));
        CDocxSaveDialog dlg(outName, strFilter, settings);
        dlg.m_ofn.nFilterIndex = 1;
        if (dlg.DoModal(hwndParent) != IDOK)
            return S_FALSE;

        CZipStoreWriter zip;
        if (!zip.Open(dlg.m_szFileName)) {
            CString msg;
            msg.Format(IDS_ERROR_OPEN_FILE, dlg.m_szFileName, static_cast<LPCTSTR>(U::Win32ErrMsg(::GetLastError())));
            AtlTaskDialog(hwndParent, static_cast<UINT>(IDR_EXPORTDOCX), static_cast<LPCTSTR>(msg), static_cast<LPCTSTR>(NULL), TDCBF_OK_BUTTON, TD_ERROR_ICON);
            return S_FALSE;
        }

        CDocxBuilder builder(source, settings);
        builder.Build(zip);
        zip.Close();
        if (settings.createReport) {
            CStringW reportName = MakeReportFileName(CStringW(dlg.m_szFileName));
            WriteUtf8TextFile(reportName, builder.ReportText(CStringW(dlg.m_szFileName)));
        }
        if (settings.validateDocx) {
            CStringW warnings = builder.ValidationWarnings();
            if (!warnings.IsEmpty()) {
                CStringW msg = L"DOCX создан, но внутренняя проверка нашла предупреждения:\r\n";
                msg += warnings;
                AtlTaskDialog(hwndParent, static_cast<UINT>(IDR_EXPORTDOCX), static_cast<LPCTSTR>(msg), static_cast<LPCTSTR>(NULL), TDCBF_OK_BUTTON, TD_WARNING_ICON);
            }
        }
        if (settings.openAfterExport) {
            ::ShellExecuteW(hwndParent, L"open", dlg.m_szFileName, NULL, NULL, SW_SHOWNORMAL);
        }
    }
    catch (_com_error& e) {
        U::ReportError(e);
        return S_FALSE;
    }
    catch (...) {
        AtlTaskDialog(hwndParent, static_cast<UINT>(IDR_EXPORTDOCX), _T("Unexpected DOCX export error."), static_cast<LPCTSTR>(NULL), TDCBF_OK_BUTTON, TD_ERROR_ICON);
        return S_FALSE;
    }
    return S_OK;
}
