#include "stdafx.h"
#include "MsxmlImport.h"
#include "EpubImport.h"
#include "ImportEPUBApi.h"
#include "resource.h"
#include "RuntimeLocalization.h"
#include "SecureEpubZip.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <set>
#include <algorithm>
#include <cwctype>
#include <new>
#include <zlib.h>
#include <shlwapi.h>
#include <gdiplus.h>

namespace
{
    void CheckError(HRESULT result) {if (FAILED(result)) _com_issue_error(result);}
    EpubImportOptions g_options;
    EpubImportRuntimeStats g_runtimeStats;
    thread_local CStringW g_bookRoot;
    thread_local LPCWSTR g_importStage = L"initialization";
    thread_local wchar_t g_imageStageDetail[256] = {};
    struct BookRootScope {
        CStringW previous;
        explicit BookRootScope(const CStringW& root) : previous(g_bookRoot) { g_bookRoot=root; }
        ~BookRootScope() { g_bookRoot=previous; }
    };

    CStringW LoadImportRuntimeString(UINT stringId, LPCWSTR fallback)
    {
        return LoadImportEpubString(stringId, fallback);
    }

    struct ManifestItem
    {
        CStringW id;
        CStringW href;
        CStringW mediaType;
        CStringW properties;
        CStringW absPath;
    };

    struct NavigationEntry
    {
        CStringW title, path, fragment;
        int parent = -1, spineIndex = -1;
        std::vector<size_t> children;
    };

    struct EpubPackage
    {
        CStringW opfPath;
        CStringW opfDir;
        CStringW title;
        CStringW titleSort;
        CStringW language;
        CStringW originalLanguage;
        CStringW description;
        CStringW identifier;
        CStringW isbn;
        CStringW date;
        CStringW dateEvent;
        std::vector<std::pair<CStringW, CStringW>> datedEvents;
        CStringW publisher;
        CStringW source;
        CStringW rights;
        CStringW modifiedDate;
        CStringW accessibilitySummary;
        CStringW seriesName;
        CStringW seriesNumber;
        CStringW coverId;
        std::vector<CStringW> creators;
        std::vector<CStringW> contributors;
        std::vector<CStringW> subjects;
        std::map<std::wstring, ManifestItem> manifest;
        std::vector<CStringW> spine;
        std::set<std::wstring> mandatorySpine;
        std::map<std::wstring, CStringW> navTitlesByAbsPath;
        std::vector<NavigationEntry> navigation;
        std::set<std::wstring> guideServiceDocuments;
        int guideReferenceCount;

        EpubPackage()
            : guideReferenceCount(0)
        {
        }
    };

    // Per-import bookkeeping; none of these details cross the plug-in ABI.
    struct ContentInventory
    {
        std::map<std::wstring, CStringW> binaryIds;
        CStringW coverImageId;
        unsigned nextImageNumber = 1;
        std::map<std::wstring, ManifestItem> extraImages;
        std::set<std::wstring> reservedIds, emittedIds, noteTargets, requestedNotes, navigationTargets;
        std::set<std::wstring> mainDocuments, noteContentAnchors, noteRootDocuments, residualDocuments;
        std::map<std::wstring, ManifestItem> linkedNonSpineDocuments;
        std::set<std::wstring> excludedServiceDocuments;
        std::map<std::wstring, unsigned> spineDocumentTitleCounts;
        unsigned spineXhtmlDocuments = 0;
        std::set<std::wstring> warnedInlineCss;
        std::set<std::wstring> warnedCssAtRules;
        std::set<std::wstring> warnedOrphanClosingTags;
        std::set<std::wstring> warnedUnmappedPoems;
        std::set<std::wstring> repairedXhtmlDocuments, unrecoveredXhtmlDocuments;
        std::set<std::wstring> mappedLinkElements, removedBacklinkElements, retainedBacklinkElements, textOnlyBacklinkElements, fidelityLossElements;
        CComPtr<IUnknown> diagnosticDocument;
        std::unordered_map<IUnknown*, unsigned> diagnosticSiblingPositions;
        std::set<IUnknown*> diagnosticIndexedParents;
        std::vector<CComPtr<IUnknown>> diagnosticNodeReferences;
        std::map<std::wstring, bool> imageOnlyCoverPages;
        std::set<std::wstring> fb2GenreIds;
        bool fb2GenresLoaded = false;
        std::map<std::wstring, CStringW> noteLinks;
        std::map<std::wstring, CStringW> noteRoots;
        CStringW warnings;
        int noteReferences = 0, importedNoteReferences = 0, lostNoteReferences = 0;
        int skippedServiceLinks = 0, missingSourceResources = 0, missingSourceAnchors = 0, missingGeneratedTargets = 0;
        int imageReferences = 0, importedImages = 0, skippedImages = 0;
        unsigned indexedElements = 0, structuralLookups = 0;
        unsigned residualFragments = 0, recoveredFragments = 0;
        std::map<std::wstring, unsigned> xhtmlLoads;
        std::map<std::wstring, MSXML2::IXMLDOMDocument2Ptr> xhtmlCache;
        unsigned long long xhtmlCacheBytes = 0;
        unsigned xhtmlCacheHits = 0;
        int generatedBookTitleOffset = -1, generatedBookTitleLength = 0;
    };
    thread_local ContentInventory g_content;

    struct XhtmlCacheScope
    {
        ~XhtmlCacheScope() {
            g_content.xhtmlCache.clear(); g_content.xhtmlCacheBytes = 0;
            g_content.diagnosticSiblingPositions.clear(); g_content.diagnosticIndexedParents.clear();
            g_content.diagnosticNodeReferences.clear(); g_content.diagnosticDocument.Release();
        }
    };

    CStringW UniqueContentId(const CStringW& base)
    {
        CStringW id(base);
        for (unsigned suffix = 2; !g_content.reservedIds.emplace(id.GetString()).second; ++suffix)
            id.Format(L"%s_%u", base.GetString(), suffix);
        return id;
    }

    HRESULT CreateMsxmlDom(MSXML2::IXMLDOMDocument2Ptr& dom);

    CStringW XmlEscape(const CStringW& text)
    {
        CStringW s(text);
        s.Replace(L"&",  L"&amp;");
        s.Replace(L"<",  L"&lt;");
        s.Replace(L">",  L"&gt;");
        s.Replace(L"\"", L"&quot;");
        s.Replace(L"'",  L"&apos;");
        return s;
    }

    CStringW TrimString(const CStringW& text)
    {
        CStringW s(text);
        s.Trim();
        return s;
    }

    int CountMojibakeMarkers(const CStringW& text)
    {
        // Typical UTF-8-as-Windows-1251 mojibake for Russian contains many
        // pairs beginning with mojibake Cyrillic marker letters.
        int count = 0;
        for (int i = 0; i < text.GetLength(); ++i)
        {
            const wchar_t ch = text[i];
            // Use numeric Unicode code points instead of non-ASCII character
            // literals. This keeps the file safe for static analyzers and for
            // build environments where source encoding may be interpreted
            // differently.
            if (ch == 0x0420 || ch == 0x0421 || ch == 0x00D0 || ch == 0x00D1)
                ++count;
            if (ch == 0xFFFD)
                count += 2;
        }
        return count;
    }

    bool ContainsCyrillic(const CStringW& text)
    {
        for (int i = 0; i < text.GetLength(); ++i)
        {
            wchar_t ch = text[i];
            if ((ch >= 0x0400 && ch <= 0x04FF) || ch == 0x2116)
                return true;
        }
        return false;
    }

    CStringW TryRepairUtf8DecodedAsCp1251(const CStringW& text)
    {
        if (text.IsEmpty())
            return text;

        const int badBefore = CountMojibakeMarkers(text);
        if (badBefore < 2)
            return text;

        BOOL usedDefaultChar = FALSE;
        int byteCount = ::WideCharToMultiByte(1251, WC_NO_BEST_FIT_CHARS,
            text.GetString(), text.GetLength(), nullptr, 0, nullptr, &usedDefaultChar);
        if (byteCount <= 0 || usedDefaultChar)
            return text;

        std::vector<char> bytes(static_cast<size_t>(byteCount));
        ::WideCharToMultiByte(1251, WC_NO_BEST_FIT_CHARS,
            text.GetString(), text.GetLength(), bytes.data(), byteCount, nullptr, &usedDefaultChar);
        if (usedDefaultChar)
            return text;

        int wideCount = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            bytes.data(), byteCount, nullptr, 0);
        if (wideCount <= 0)
            return text;

        CStringW decoded;
        LPWSTR buffer = decoded.GetBuffer(wideCount);
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            bytes.data(), byteCount, buffer, wideCount);
        decoded.ReleaseBuffer(wideCount);

        const int badAfter = CountMojibakeMarkers(decoded);
        if (badAfter < badBefore && ContainsCyrillic(decoded))
            return decoded;

        return text;
    }

    CStringW CleanTypographyText(const CStringW& text)
    {
        if (!g_options.cleanTypography || text.IsEmpty())
            return text;

        CStringW out;
        out.Preallocate(text.GetLength());

        for (int i = 0; i < text.GetLength(); ++i)
        {
            wchar_t ch = text[i];

            // U+00AD SOFT HYPHEN is often inserted by EPUB generators for
            // line wrapping. In FB2 it usually becomes visual garbage, so drop it.
            if (ch == 0x00AD)
                continue;

            // Normalize common non-breaking and technical spaces to plain spaces.
            if (ch == 0x00A0 || ch == 0x2007 || ch == 0x202F || ch == L'\t' || ch == L'\r' || ch == L'\n')
                ch = L' ';

            // ZWNJ/ZWJ affect spelling and emoji composition, so they are not
            // disposable layout marks even when typography cleanup is enabled.
            if (ch == 0x200B || ch == 0xFEFF)
                continue;

            // Visible hyphens and dashes carry distinct typography and line-breaking
            // semantics; unlike a discretionary soft hyphen, preserve them verbatim.
            if (ch == L' ' && !out.IsEmpty() && out[out.GetLength() - 1] == L' ')
                continue;
            out += ch;
        }
        return out;
    }

    CStringW NormalizeImportedText(const CStringW& text)
    {
        CStringW s = g_options.repairEncoding ? TryRepairUtf8DecodedAsCp1251(text) : text;
        return CleanTypographyText(s);
    }

    bool IsEmptyText(const CStringW& text)
    {
        CStringW s(text);
        s.Trim();
        return s.IsEmpty() != FALSE;
    }

    CStringW FileNameOnly(const CStringW& path)
    {
        int slash1 = path.ReverseFind(L'\\');
        int slash2 = path.ReverseFind(L'/');
        int slash = max(slash1, slash2);
        CStringW name = slash >= 0 ? path.Mid(slash + 1) : path;
        int dot = name.ReverseFind(L'.');
        if (dot > 0)
            name = name.Left(dot);
        return name;
    }

    CStringW DirectoryOf(const CStringW& path)
    {
        int slash1 = path.ReverseFind(L'\\');
        int slash2 = path.ReverseFind(L'/');
        int slash = max(slash1, slash2);
        return slash >= 0 ? path.Left(slash) : CStringW();
    }

    CStringW CombinePathLocal(const CStringW& dir, const CStringW& name)
    {
        if (dir.IsEmpty())
            return name;
        CStringW out(dir);
        if (out.Right(1) != L"\\" && out.Right(1) != L"/")
            out += L"\\";
        out += name;
        return out;
    }

    CStringW ChangeExtension(const CStringW& path, LPCWSTR newExt)
    {
        CStringW out(path);
        int slash1 = out.ReverseFind(L'\\');
        int slash2 = out.ReverseFind(L'/');
        int slash = max(slash1, slash2);
        int dot = out.ReverseFind(L'.');
        if (dot > slash)
            out = out.Left(dot);
        out += newExt;
        return out;
    }

    bool WriteUtf8TextFile(const CStringW& path, const CStringW& text)
    {
        int bytesRequired = WideCharToMultiByte(CP_UTF8, 0, text.GetString(), text.GetLength(), nullptr, 0, nullptr, nullptr);
        if (bytesRequired < 0)
            return false;

        std::vector<char> bytes(static_cast<size_t>(bytesRequired) + 3);
        bytes[0] = static_cast<char>(0xEF);
        bytes[1] = static_cast<char>(0xBB);
        bytes[2] = static_cast<char>(0xBF);
        if (bytesRequired > 0)
            WideCharToMultiByte(CP_UTF8, 0, text.GetString(), text.GetLength(), bytes.data() + 3, bytesRequired, nullptr, nullptr);

        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        DWORD written = 0;
        BOOL ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
        CloseHandle(h);
        return ok && written == bytes.size();
    }


    bool IsLikelyHtmlXmlDocumentPath(const CStringW& path)
    {
        CStringW lower(path);
        lower.MakeLower();
        return lower.Right(6) == L".xhtml" || lower.Right(5) == L".html" || lower.Right(4) == L".htm";
    }

    bool ReadTextFileBytes(const CStringW& path, std::vector<unsigned char>& bytes)
    {
        bytes.clear();
        HANDLE h = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER size = {};
        if (!::GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > 64LL * 1024LL * 1024LL)
        {
            ::CloseHandle(h);
            return false;
        }

        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        BOOL ok = TRUE;
        if (!bytes.empty())
            ok = ::ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        ::CloseHandle(h);
        if (!ok || read != bytes.size())
        {
            bytes.clear();
            return false;
        }
        return true;
    }

    UINT DeclaredMarkupCodePage(const std::vector<unsigned char>& bytes)
    {
        // Only inspect XML declarations and HTML meta tags, not arbitrary text.
        const size_t count = (std::min)(bytes.size(), static_cast<size_t>(8192));
        std::string head; head.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const unsigned char ch = bytes[i];
            head.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : static_cast<char>(ch));
        }
        const size_t body = head.find("<body");
        if (body != std::string::npos) head.resize(body);
        auto attribute = [](const std::string& tag, const char* name) -> UINT {
            size_t pos = tag.find(name);
            while (pos != std::string::npos && pos > 0 &&
                   ((tag[pos - 1] >= 'a' && tag[pos - 1] <= 'z') || (tag[pos - 1] >= '0' && tag[pos - 1] <= '9') || tag[pos - 1] == '-' || tag[pos - 1] == '_'))
                pos = tag.find(name, pos + 1);
            if (pos == std::string::npos) return 0;
            pos += strlen(name);
            while (pos < tag.size() && (tag[pos] == ' ' || tag[pos] == '\t' || tag[pos] == '\r' || tag[pos] == '\n')) ++pos;
            if (pos == tag.size() || tag[pos++] != '=') return 0;
            while (pos < tag.size() && (tag[pos] == ' ' || tag[pos] == '\t' || tag[pos] == '\r' || tag[pos] == '\n' || tag[pos] == '\'' || tag[pos] == '"')) ++pos;
            const size_t start = pos;
            while (pos < tag.size() && ((tag[pos] >= 'a' && tag[pos] <= 'z') || (tag[pos] >= '0' && tag[pos] <= '9') || tag[pos] == '-' || tag[pos] == '_')) ++pos;
            const std::string encoding = tag.substr(start, pos - start);
            if (encoding == "utf-8" || encoding == "utf8") return CP_UTF8;
            if (encoding == "windows-1251" || encoding == "cp1251") return 1251;
            if (encoding == "windows-1252" || encoding == "cp1252") return 1252;
            if (encoding == "iso-8859-1" || encoding == "iso_8859-1") return 28591;
            return 0;
        };
        size_t pos = head.find("<?xml");
        if (pos != std::string::npos) {
            const size_t end = head.find("?>", pos);
            if (end != std::string::npos) {
                const UINT cp = attribute(head.substr(pos, end - pos), "encoding");
                if (cp) return cp;
            }
        }
        pos = 0;
        while ((pos = head.find("<meta", pos)) != std::string::npos) {
            const size_t end = head.find('>', pos);
            if (end == std::string::npos) break;
            const UINT cp = attribute(head.substr(pos, end - pos), "charset");
            if (cp) return cp;
            pos = end + 1;
        }
        return 0;
    }

    bool DecodeTextFileBytes(const std::vector<unsigned char>& bytes, CStringW& text, UINT* decodedCodePage = nullptr)
    {
        text.Empty();
        if (decodedCodePage) *decodedCodePage = 0;
        if (bytes.empty())
            return true;

        if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
        {
            const int charCount = static_cast<int>((bytes.size() - 2) / 2);
            if (charCount > 0)
                text.SetString(reinterpret_cast<LPCWSTR>(&bytes[2]), charCount);
            return true;
        }

        if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
        {
            const size_t charCount = (bytes.size() - 2) / 2;
            CStringW out;
            LPWSTR buffer = out.GetBuffer(static_cast<int>(charCount));
            for (size_t i = 0; i < charCount; ++i)
                buffer[i] = static_cast<wchar_t>((bytes[2 + i * 2] << 8) | bytes[3 + i * 2]);
            out.ReleaseBuffer(static_cast<int>(charCount));
            text = out;
            return true;
        }

        int offset = 0;
        if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
            offset = 3;

        const char* data = reinterpret_cast<const char*>(bytes.data() + offset);
        const int dataSize = static_cast<int>(bytes.size() - offset);
        UINT codePage = offset ? CP_UTF8 : DeclaredMarkupCodePage(bytes);
        if (!codePage) codePage = CP_UTF8;
        DWORD flags = codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
        int wideCount = ::MultiByteToWideChar(codePage, flags, data, dataSize, nullptr, 0);
        if (wideCount <= 0)
        {
            codePage = 1252;
            flags = 0;
            wideCount = ::MultiByteToWideChar(codePage, flags, data, dataSize, nullptr, 0);
        }
        if (wideCount <= 0)
            return false;

        LPWSTR buffer = text.GetBuffer(wideCount);
        ::MultiByteToWideChar(codePage, flags, data, dataSize, buffer, wideCount);
        text.ReleaseBuffer(wideCount);
        if (decodedCodePage) *decodedCodePage = codePage;
        return true;
    }

    bool IsXmlPredefinedEntity(const CStringW& name)
    {
        return name == L"amp" || name == L"lt" || name == L"gt" || name == L"quot" || name == L"apos";
    }

    bool IsValidNumericEntity(const CStringW& s, int ampPos, int semicolonPos)
    {
        if (ampPos + 2 >= semicolonPos || s[ampPos + 1] != L'#')
            return false;
        int i = ampPos + 2;
        bool hex = false;
        if (i < semicolonPos && (s[i] == L'x' || s[i] == L'X'))
        {
            hex = true;
            ++i;
        }
        if (i >= semicolonPos)
            return false;
        for (; i < semicolonPos; ++i)
        {
            wchar_t ch = s[i];
            const bool ok = hex
                ? ((ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F'))
                : (ch >= L'0' && ch <= L'9');
            if (!ok)
                return false;
        }
        return true;
    }

    CStringW ReplaceKnownHtmlEntities(CStringW s)
    {
        struct EntityReplacement { LPCWSTR name; LPCWSTR value; };
        static const EntityReplacement entities[] =
        {
            { L"&nbsp;", L"&#160;" }, { L"&ensp;", L"&#8194;" }, { L"&emsp;", L"&#8195;" }, { L"&thinsp;", L"&#8201;" },
            { L"&shy;", L"&#173;" }, { L"&copy;", L"&#169;" }, { L"&reg;", L"&#174;" }, { L"&trade;", L"&#8482;" },
            { L"&ndash;", L"&#8211;" }, { L"&mdash;", L"&#8212;" }, { L"&minus;", L"&#8722;" },
            { L"&lsquo;", L"&#8216;" }, { L"&rsquo;", L"&#8217;" }, { L"&ldquo;", L"&#8220;" }, { L"&rdquo;", L"&#8221;" },
            { L"&sbquo;", L"&#8218;" }, { L"&bdquo;", L"&#8222;" }, { L"&laquo;", L"&#171;" }, { L"&raquo;", L"&#187;" },
            { L"&lsaquo;", L"&#8249;" }, { L"&rsaquo;", L"&#8250;" }, { L"&hellip;", L"&#8230;" }, { L"&bull;", L"&#8226;" },
            { L"&middot;", L"&#183;" }, { L"&sect;", L"&#167;" }, { L"&para;", L"&#182;" }, { L"&dagger;", L"&#8224;" }, { L"&Dagger;", L"&#8225;" },
            { L"&prime;", L"&#8242;" }, { L"&Prime;", L"&#8243;" }, { L"&permil;", L"&#8240;" },
            { L"&deg;", L"&#176;" }, { L"&plusmn;", L"&#177;" }, { L"&times;", L"&#215;" }, { L"&divide;", L"&#247;" },
            { L"&cent;", L"&#162;" }, { L"&pound;", L"&#163;" }, { L"&yen;", L"&#165;" }, { L"&euro;", L"&#8364;" },
            { L"&OElig;", L"&#338;" }, { L"&oelig;", L"&#339;" }, { L"&Scaron;", L"&#352;" }, { L"&scaron;", L"&#353;" }, { L"&Yuml;", L"&#376;" },
            { L"&zwnj;", L"&#8204;" }, { L"&zwj;", L"&#8205;" }, { L"&lrm;", L"&#8206;" }, { L"&rlm;", L"&#8207;" }
        };

        static const EntityReplacement html4Entities[] = {
#include "Html4Entities.inl"
        };
        CStringW result;
        int start = 0;
        while (start < s.GetLength())
        {
            const int cdata = s.Find(L"<![CDATA[", start);
            const int comment = s.Find(L"<!--", start);
            const int protectedStart = cdata < 0 ? comment : comment < 0 ? cdata : min(cdata, comment);
            const int segmentEnd = protectedStart < 0 ? s.GetLength() : protectedStart;
            CStringW segment = s.Mid(start, segmentEnd - start);
            for (const auto& entity : entities) segment.Replace(entity.name, entity.value);
            for (const auto& entity : html4Entities) segment.Replace(entity.name, entity.value);
            result += segment;
            if (protectedStart < 0) break;
            const bool isCdata = protectedStart == cdata;
            const int end = s.Find(isCdata ? L"]]>" : L"-->", protectedStart + (isCdata ? 9 : 4));
            if (end < 0) { result += s.Mid(protectedStart); break; }
            result += s.Mid(protectedStart, end + 3 - protectedStart);
            start = end + 3;
        }
        return result;
    }

    CStringW EscapeUnsupportedAmpersands(CStringW s)
    {
        CStringW out;
        out.Preallocate(s.GetLength());
        for (int i = 0; i < s.GetLength(); ++i)
        {
            const bool cdata = s[i] == L'<' && s.Mid(i, 9) == L"<![CDATA[";
            const bool comment = s[i] == L'<' && s.Mid(i, 4) == L"<!--";
            if (cdata || comment)
            {
                const int end = s.Find(cdata ? L"]]>" : L"-->", i + (cdata ? 9 : 4));
                if (end < 0) { out += s.Mid(i); break; }
                out += s.Mid(i, end + 3 - i);
                i = end + 2;
                continue;
            }
            if (s[i] != L'&')
            {
                out += s[i];
                continue;
            }

            const int semicolon = s.Find(L';', i + 1);
            if (semicolon > i + 1 && semicolon - i <= 32)
            {
                CStringW name = s.Mid(i + 1, semicolon - i - 1);
                if (IsXmlPredefinedEntity(name) || IsValidNumericEntity(s, i, semicolon))
                {
                    out += s.Mid(i, semicolon - i + 1);
                    i = semicolon;
                    continue;
                }
            }

            out += L"&amp;";
        }
        return out;
    }

    bool IsHtmlVoidElementName(const CStringW& name)
    {
        CStringW n(name);
        n.MakeLower();
        return n == L"area" || n == L"base" || n == L"br" || n == L"col" ||
            n == L"embed" || n == L"hr" || n == L"img" || n == L"input" ||
            n == L"link" || n == L"meta" || n == L"param" || n == L"source" ||
            n == L"track" || n == L"wbr";
    }

    int FindHtmlTagEnd(const CStringW& text, int tagStart)
    {
        bool inQuote = false;
        wchar_t quote = 0;
        for (int i = tagStart + 1; i < text.GetLength(); ++i)
        {
            wchar_t ch = text[i];
            if (inQuote)
            {
                if (ch == quote)
                    inQuote = false;
                continue;
            }

            if (ch == L'\'' || ch == L'"')
            {
                inQuote = true;
                quote = ch;
                continue;
            }

            if (ch == L'>')
                return i;
        }
        return -1;
    }

    CStringW SelfCloseHtmlVoidTagsForXml(const CStringW& html)
    {
        CStringW out;
        out.Preallocate(html.GetLength() + 64);

        int pos = 0;
        while (pos < html.GetLength())
        {
            int tagStart = html.Find(L'<', pos);
            if (tagStart < 0)
            {
                out += html.Mid(pos);
                break;
            }

            out += html.Mid(pos, tagStart - pos);

            if (tagStart + 1 >= html.GetLength())
            {
                out += html.Mid(tagStart);
                break;
            }

            wchar_t next = html[tagStart + 1];
            if (next == L'/' || next == L'!' || next == L'?')
            {
                int tagEnd = FindHtmlTagEnd(html, tagStart);
                if (tagEnd < 0)
                {
                    out += html.Mid(tagStart);
                    break;
                }
                // The fallback self-closes HTML void elements below. Some
                // XHTML producers also emit explicit closing tags for them;
                // retaining those closers would make the normalized XML invalid.
                CStringW closingName;
                if (next == L'/')
                {
                    closingName = html.Mid(tagStart + 2, tagEnd - tagStart - 2);
                    closingName.Trim();
                }
                if (next != L'/' || !IsHtmlVoidElementName(closingName))
                    out += html.Mid(tagStart, tagEnd - tagStart + 1);
                pos = tagEnd + 1;
                continue;
            }

            int nameStart = tagStart + 1;
            int nameEnd = nameStart;
            while (nameEnd < html.GetLength())
            {
                wchar_t ch = html[nameEnd];
                const bool ok = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                    (ch >= L'0' && ch <= L'9') || ch == L':' || ch == L'-' || ch == L'_';
                if (!ok)
                    break;
                ++nameEnd;
            }

            CStringW name = html.Mid(nameStart, nameEnd - nameStart);
            int tagEnd = FindHtmlTagEnd(html, tagStart);
            if (tagEnd < 0)
            {
                out += html.Mid(tagStart);
                break;
            }

            CStringW tag = html.Mid(tagStart, tagEnd - tagStart + 1);
            if (IsHtmlVoidElementName(name))
            {
                int last = tag.GetLength() - 2;
                while (last >= 0 && iswspace(tag[last]))
                    --last;
                if (last >= 0 && tag[last] != L'/')
                {
                    tag = tag.Left(last + 1) + L" />";
                }
            }

            out += tag;
            pos = tagEnd + 1;
        }

        return out;
    }

    CStringW NormalizeXmlDeclarationVersion(CStringW xml)
    {
        CStringW head = xml.Left(min(xml.GetLength(), 128));
        CStringW lower(head);
        lower.MakeLower();
        if (lower.Find(L"<?xml") < 0 || lower.Find(L"version") < 0)
            return xml;

        xml.Replace(L"version=\"1.1\"", L"version=\"1.0\"");
        xml.Replace(L"version='1.1'", L"version='1.0'");
        return xml;
    }

    CStringW RemoveInvalidXmlControlChars(const CStringW& xml)
    {
        CStringW out;
        out.Preallocate(xml.GetLength());
        for (int i = 0; i < xml.GetLength(); ++i)
        {
            wchar_t ch = xml[i];
            if ((ch >= 0x20) || ch == L'\t' || ch == L'\r' || ch == L'\n')
                out += ch;
            else
                out += L' ';
        }
        return out;
    }

    CStringW SanitizeMarkupForXml(CStringW xml, bool htmlMode)
    {
        xml = NormalizeXmlDeclarationVersion(xml);
        xml = RemoveInvalidXmlControlChars(xml);
        xml = ReplaceKnownHtmlEntities(xml);
        xml = EscapeUnsupportedAmpersands(xml);
        if (htmlMode)
            xml = SelfCloseHtmlVoidTagsForXml(xml);
        return xml;
    }

    // Repair only unmatched closing a/div tags when every other tag is
    // properly nested. Do not infer missing open tags or restructure content.
    bool RemoveOrphanHtmlClosingTags(const CStringW& input, CStringW& output)
    {
        std::vector<CStringW> stack;
        std::vector<std::pair<int, int>> removals;
        int pos = 0;
        while (pos < input.GetLength())
        {
            const int start = input.Find(L'<', pos);
            if (start < 0) break;
            if (input.Mid(start, 4) == L"<!--" || input.Mid(start, 9) == L"<![CDATA[")
            {
                const bool comment = input.Mid(start, 4) == L"<!--";
                const int end = input.Find(comment ? L"-->" : L"]]>", start + (comment ? 4 : 9));
                if (end < 0) return false;
                pos = end + 3; continue;
            }
            if (input.Mid(start, 2) == L"<?")
            {
                const int end = input.Find(L"?>", start + 2);
                if (end < 0) return false;
                pos = end + 2; continue;
            }
            const int end = FindHtmlTagEnd(input, start);
            if (end < 0 || start + 1 >= end) return false;
            if (input[start + 1] == L'!')
            {
                // Internal DTD subsets may contain nested markup and quotes.
                if (input.Mid(start, end - start + 1).Find(L'[') >= 0) return false;
                pos = end + 1; continue;
            }
            const bool closing = input[start + 1] == L'/';
            const int nameStart = start + (closing ? 2 : 1);
            int nameEnd = nameStart;
            while (nameEnd < end)
            {
                const wchar_t ch = input[nameEnd];
                if (!((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                      (ch >= L'0' && ch <= L'9') || ch == L':' || ch == L'-' || ch == L'_' || ch == L'.')) break;
                ++nameEnd;
            }
            if (nameEnd == nameStart) return false;
            const CStringW name = input.Mid(nameStart, nameEnd - nameStart);
            if (closing)
            {
                for (int i = nameEnd; i < end; ++i) if (!iswspace(input[i])) return false;
                if (!stack.empty() && stack.back() == name) stack.pop_back();
                else
                {
                    if (name != L"a" && name != L"div") return false;
                    if (std::find(stack.begin(), stack.end(), name) != stack.end() || removals.size() >= 32) return false;
                    removals.emplace_back(start, end + 1);
                }
            }
            else
            {
                int last = end - 1;
                while (last > nameEnd && iswspace(input[last])) --last;
                if (input[last] != L'/')
                {
                    if (stack.size() >= 1024) return false;
                    stack.push_back(name);
                }
            }
            pos = end + 1;
        }
        if (!stack.empty() || removals.empty()) return false;
        output.Empty(); output.Preallocate(input.GetLength());
        pos = 0;
        for (const auto& range : removals) { output += input.Mid(pos, range.first - pos); pos = range.second; }
        output += input.Mid(pos);
        return true;
    }

    bool LoadXmlFileAfterMarkupSanitizing(const CStringW& path, bool htmlMode, MSXML2::IXMLDOMDocument2Ptr& dom, CStringW& errorText)
    {
        std::vector<unsigned char> bytes;
        CStringW xml;
        UINT decodedCodePage = 0;
        if (!ReadTextFileBytes(path, bytes) || !DecodeTextFileBytes(bytes, xml, &decodedCodePage))
            return false;

        xml = SanitizeMarkupForXml(xml, htmlMode);

        HRESULT hr = CreateMsxmlDom(dom);
        if (FAILED(hr))
            return false;

        VARIANT_BOOL loaded = VARIANT_FALSE;
        CComBSTR xmlText(static_cast<LPCWSTR>(xml));
        HRESULT loadHr = dom->loadXML(xmlText, &loaded);
        if ((FAILED(loadHr) || loaded != VARIANT_TRUE) && htmlMode)
        {
            CStringW repaired;
            if (RemoveOrphanHtmlClosingTags(xml, repaired))
            {
                CComBSTR repairedText(static_cast<LPCWSTR>(repaired));
                loaded = VARIANT_FALSE;
                loadHr = dom->loadXML(repairedText, &loaded);
                if (SUCCEEDED(loadHr) && loaded == VARIANT_TRUE &&
                    g_content.warnedOrphanClosingTags.emplace(path.GetString()).second)
                    g_content.warnings += L"Ignored unmatched closing HTML tag in: " + path + L"\r\n";
            }
        }
        if (FAILED(loadHr) || loaded != VARIANT_TRUE)
        {
            if (htmlMode && !g_content.repairedXhtmlDocuments.count(path.GetString())) g_content.unrecoveredXhtmlDocuments.emplace(path.GetString());
            CStringW reason = MsxmlGetParseErrorReason(dom);
            if (reason.IsEmpty())
                reason = htmlMode ? LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XML_UNKNOWN_AFTER_HTML, L"unknown XML error after HTML fallback") : LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XML_UNKNOWN_AFTER_XML, L"unknown XML error after XML fallback");
            errorText.Format(htmlMode
                ? static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_HTML_FALLBACK_FAILED, L"MSXML could not read the HTML/XHTML file even after HTML markup normalization:\r\n%s\r\n%s"))
                : static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XML_FALLBACK_FAILED, L"MSXML could not read the XML file even after safe normalization:\r\n%s\r\n%s")),
                path.GetString(), reason.GetString());
            return false;
        }
        if (htmlMode) {
            g_content.unrecoveredXhtmlDocuments.erase(path.GetString());
            if (g_content.repairedXhtmlDocuments.emplace(path.GetString()).second) {
                g_content.warnings += L"Recovered malformed XHTML by conservative normalization: " + path + L"\r\n";
                if (decodedCodePage && decodedCodePage != CP_UTF8) {
                    CStringW detail;
                    detail.Format(L"Decoded HTML using code page %u (declared or fallback): %s\r\n", decodedCodePage, path.GetString());
                    g_content.warnings += detail;
                }
            }
        }
        return true;
    }

    struct ImportLog
    {
        CStringW text;

        void Add(const CStringW& line)
        {
            text += line;
            text += L"\r\n";
        }

        void AddFormat(LPCWSTR format, const CStringW& value)
        {
            CStringW line;
            line.Format(format, value.GetString());
            Add(line);
        }

        void AddFormatInt(LPCWSTR format, int value)
        {
            CStringW line;
            line.Format(format, value);
            Add(line);
        }

        void SaveIfRequested(const CStringW& epubPath, bool force = false) const
        {
            if (!g_options.writeImportLog && !force)
                return;
            CStringW logPath = ChangeExtension(epubPath, L".ImportEPUB.log");
            WriteUtf8TextFile(logPath, text);
        }
    };

    CStringW NormalizeZipRelativePath(const CStringW& path)
    {
        CStringW s(path);
        s.Replace(L'/', L'\\');
        while (s.Find(L"\\\\") >= 0)
            s.Replace(L"\\\\", L"\\");
        return s;
    }

    int HexValue(wchar_t ch)
    {
        if (ch >= L'0' && ch <= L'9') return ch - L'0';
        if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
        if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
        return -1;
    }

    CStringW UrlDecodeUtf8(const CStringW& value)
    {
        // EPUB href values are URI-like. Decode only percent-encoded UTF-8.
        // Plain non-ASCII characters are preserved as-is.
        CStringW out;
        std::vector<unsigned char> bytes;

        auto FlushBytes = [&]()
        {
            if (bytes.empty())
                return;

            int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                reinterpret_cast<LPCSTR>(&bytes[0]), static_cast<int>(bytes.size()), nullptr, 0);
            if (required > 0)
            {
                CStringW decoded;
                LPWSTR buffer = decoded.GetBuffer(required);
                MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                    reinterpret_cast<LPCSTR>(&bytes[0]), static_cast<int>(bytes.size()), buffer, required);
                decoded.ReleaseBuffer(required);
                out += decoded;
            }
            else
            {
                for (size_t i = 0; i < bytes.size(); ++i)
                    out += static_cast<wchar_t>(bytes[i]);
            }
            bytes.clear();
        };

        for (int i = 0; i < value.GetLength(); ++i)
        {
            wchar_t ch = value[i];
            if (ch == L'%' && i + 2 < value.GetLength())
            {
                int hi = HexValue(value[i + 1]);
                int lo = HexValue(value[i + 2]);
                if (hi >= 0 && lo >= 0)
                {
                    bytes.push_back(static_cast<unsigned char>((hi << 4) | lo));
                    i += 2;
                    continue;
                }
            }

            FlushBytes();
            out += ch;
        }

        FlushBytes();
        return out;
    }

    // Resolve dot segments before applying the extended-length Windows prefix.
    CStringW CanonicalExtendedPath(const CStringW& input)
    {
        CStringW plain(input);
        const bool unc = plain.Left(8).CompareNoCase(L"\\\\?\\UNC\\") == 0;
        const bool extended = unc || plain.Left(4) == L"\\\\?\\";
        const bool volumeGuid = plain.Left(11).CompareNoCase(L"\\\\?\\Volume{") == 0;
        if (unc) plain = CStringW(L"\\\\") + plain.Mid(8);
        else if (extended && !volumeGuid) plain = plain.Mid(4);
        const DWORD required = ::GetFullPathNameW(plain, 0, nullptr, nullptr);
        if (!required || required > 32768) return CStringW();
        std::vector<wchar_t> buffer(required, L'\0');
        const DWORD length = ::GetFullPathNameW(plain, static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
        if (!length || length >= buffer.size()) return CStringW();
        CStringW result(buffer.data());
        if (volumeGuid) return result;
        if (extended && result.Left(2) == L"\\\\") return CStringW(L"\\\\?\\UNC\\") + result.Mid(2);
        if (extended) return CStringW(L"\\\\?\\") + result;
        return result;
    }

    CStringW CombineAndCanonicalizePath(const CStringW& baseDir, const CStringW& relative)
    {
        CStringW rel = UrlDecodeUtf8(relative);
        rel.Replace(L'/', L'\\');
        if (g_bookRoot.IsEmpty() || rel.IsEmpty() || rel[0]==L'\\' || rel.Find(L':')>=0 ||
            wcslen(rel.GetString())!=static_cast<size_t>(rel.GetLength())) return CStringW();
        int segmentStart=0;
        while(segmentStart<rel.GetLength()) {
            int end=rel.Find(L'\\',segmentStart); if(end<0) end=rel.GetLength();
            const CStringW segment=rel.Mid(segmentStart,end-segmentStart);
            if(!segment.IsEmpty() && segment!=L"." && segment!=L".." && (segment.Right(1)==L" " || segment.Right(1)==L".")) return CStringW();
            segmentStart=end+1;
        }
        const CStringW path=CanonicalExtendedPath(baseDir+L"\\"+rel);
        CStringW prefix=CanonicalExtendedPath(g_bookRoot); prefix.TrimRight(L"\\");
        if(path.IsEmpty() || prefix.IsEmpty()) return CStringW();
        if(path.GetLength()<=prefix.GetLength() || path.Left(prefix.GetLength()).CompareNoCase(prefix)!=0 || path[prefix.GetLength()]!=L'\\') return CStringW();
        // Reject redirections at the extraction root and at every descendant,
        // including the final resource. Never fall back to an unchecked path.
        for(int i=prefix.GetLength(); i<=path.GetLength(); ++i) {
            if(i!=path.GetLength() && path[i]!=L'\\') continue;
            const DWORD attributes=::GetFileAttributesW(path.Left(i));
            if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) return CStringW();
        }
        return path;
    }

    bool FileExists(const CStringW& path)
    {
        DWORD attrs = GetFileAttributesW(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool DirectoryExists(const CStringW& path)
    {
        DWORD attrs = GetFileAttributesW(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool CreateDirectoryRecursive(const CStringW& path)
    {
        if (path.IsEmpty())
            return false;
        if (DirectoryExists(path))
            return true;

        CStringW parent = DirectoryOf(path);
        if (!parent.IsEmpty() && parent != path && !DirectoryExists(parent))
            CreateDirectoryRecursive(parent);

        if (::CreateDirectoryW(path, nullptr))
            return true;
        return GetLastError() == ERROR_ALREADY_EXISTS;
    }

    CStringW GuidString()
    {
        GUID guid = {};
        if (FAILED(::CoCreateGuid(&guid)))
            return L"00000000-0000-0000-0000-000000000000";

        wchar_t buffer[64] = L"";
        ::StringFromGUID2(guid, buffer, _countof(buffer));
        CStringW s(buffer);
        s.Trim(L"{}");
        return s;
    }

    CStringW CompactGuidString()
    {
        GUID guid = {};
        if (FAILED(::CoCreateGuid(&guid)))
            return CStringW();
        static const wchar_t alphabet[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&guid);
        CStringW result;
        unsigned int bits = 0, value = 0;
        for (size_t i = 0; i < sizeof(guid); ++i)
        {
            value = (value << 8) | bytes[i];
            bits += 8;
            while (bits >= 6)
            {
                bits -= 6;
                result.AppendChar(alphabet[(value >> bits) & 63]);
            }
        }
        if (bits)
            result.AppendChar(alphabet[(value << (6 - bits)) & 63]);
        return result;
    }

    bool DeleteDirectoryTree(const CStringW& dir)
    {
        if (!DirectoryExists(dir))
            return true;
        if(::GetFileAttributesW(dir)&FILE_ATTRIBUTE_REPARSE_POINT) return ::RemoveDirectoryW(dir)!=FALSE;

        CStringW mask = dir;
        if (!mask.IsEmpty() && mask.Right(1) != L"\\")
            mask += L"\\";
        mask += L"*";

        WIN32_FIND_DATAW fd = {};
        HANDLE h = ::FindFirstFileW(mask, &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                    continue;

                CStringW child = dir;
                if (!child.IsEmpty() && child.Right(1) != L"\\")
                    child += L"\\";
                child += fd.cFileName;

                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    DeleteDirectoryTree(child);
                }
                else
                {
                    if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) ::SetFileAttributesW(child, FILE_ATTRIBUTE_NORMAL);
                    ::DeleteFileW(child);
                }
            }
            while (::FindNextFileW(h, &fd));
            ::FindClose(h);
        }

        ::SetFileAttributesW(dir, FILE_ATTRIBUTE_NORMAL);
        return ::RemoveDirectoryW(dir) != FALSE;
    }

    class TempDirectory
    {
    public:
        TempDirectory()
            : m_keep(false)
        {
            std::vector<wchar_t> temp(32768, L'\0');
            const DWORD tempLength = ::GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
            if (tempLength == 0 || tempLength >= temp.size()) return;
            CStringW selectedTemp(temp.data());
            // GetTempPathW may silently fall back when TMP/TEMP exceeds MAX_PATH.
            // Honor an existing long directory, retaining TMP's precedence.
            for (const wchar_t* name : { L"TMP", L"TEMP" }) {
                std::vector<wchar_t> value(32768, L'\0');
                const DWORD n = ::GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
                if (!n) continue;
                if (n >= value.size()) break;
                if (n >= MAX_PATH) {
                    CStringW candidate = CanonicalExtendedPath(CStringW(value.data()));
                    if (!candidate.IsEmpty() && candidate.Left(4) != L"\\\\?\\") {
                        if (candidate.Left(2) == L"\\\\") candidate = CStringW(L"\\\\?\\UNC\\") + candidate.Mid(2);
                        else candidate = CStringW(L"\\\\?\\") + candidate;
                    }
                    const DWORD attrs = ::GetFileAttributesW(candidate);
                    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) selectedTemp = candidate;
                }
                break;
            }
            CStringW extendedTemp = CanonicalExtendedPath(selectedTemp);
            if (extendedTemp.IsEmpty()) return;
            if (extendedTemp.Right(1) != L"\\") extendedTemp += L"\\";
            if (extendedTemp.Left(4) != L"\\\\?\\") {
                if (extendedTemp.Left(2) == L"\\\\") extendedTemp = CStringW(L"\\\\?\\UNC\\") + extendedTemp.Mid(2);
                else extendedTemp = CStringW(L"\\\\?\\") + extendedTemp;
            }
            // Keep the extraction path short while retaining full GUID entropy.
            for (int attempt = 0; attempt < 4; ++attempt)
            {
                const CStringW id = CompactGuidString();
                if (id.IsEmpty())
                    break;
                CStringW candidate;
                candidate.Format(L"%sE_%s", extendedTemp.GetString(), id.GetString());
                if (::CreateDirectoryW(candidate, nullptr))
                {
                    m_path = candidate;
                    break;
                }
                if (::GetLastError() != ERROR_ALREADY_EXISTS)
                    break;
            }
        }

        ~TempDirectory()
        {
            if (!m_keep)
                DeleteDirectoryTree(m_path);
        }

        const CStringW& Path() const { return m_path; }
        void Keep() { m_keep = true; }

    private:
        CStringW m_path;
        bool m_keep;
    };

    bool HasPathPrefixNoCase(const CStringW& path, const CStringW& prefix)
    {
        CStringW p(path);
        CStringW r(prefix);
        p.MakeLower();
        r.MakeLower();
        if (!r.IsEmpty() && r.Right(1) != L"\\")
            r += L"\\";
        return p.Left(r.GetLength()) == r;
    }

    bool ValidateExtractedDirectoryTree(const CStringW& root, CStringW& errorText)
    {
        CStringW canonicalRoot = CanonicalExtendedPath(root);
        if (canonicalRoot.IsEmpty()) { errorText = L"Cannot canonicalize EPUB extraction root."; return false; }

        std::vector<CStringW> stack;
        stack.push_back(canonicalRoot);

        while (!stack.empty())
        {
            CStringW dir = stack.back();
            stack.pop_back();

            CStringW mask = dir;
            if (!mask.IsEmpty() && mask.Right(1) != L"\\")
                mask += L"\\";
            mask += L"*";

            WIN32_FIND_DATAW fd = {};
            HANDLE h = FindFirstFileW(mask, &fd);
            if (h == INVALID_HANDLE_VALUE)
                continue;

            do
            {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                    continue;

                CStringW child = dir;
                if (!child.IsEmpty() && child.Right(1) != L"\\")
                    child += L"\\";
                child += fd.cFileName;

                CStringW checkedPath = CanonicalExtendedPath(child);
                if (checkedPath.IsEmpty()) { FindClose(h); errorText = L"Cannot canonicalize EPUB resource."; return false; }
                if (!HasPathPrefixNoCase(checkedPath, canonicalRoot))
                {
                    FindClose(h);
                    errorText = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_ZIP_UNSAFE_PATH, L"ZIP contains an unsafe path outside the temporary folder: ") + child;
                    return false;
                }

                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                {
                    FindClose(h);
                    errorText = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_ZIP_REPARSE_POINT, L"ZIP contains a reparse point/symlink; import was stopped: ") + child;
                    return false;
                }

                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    stack.push_back(child);
            }
            while (FindNextFileW(h, &fd));

            FindClose(h);
        }

        return true;
    }

    bool ExpandEpubToDirectory(const CStringW& epubPath, const CStringW& tempRoot, CStringW& extractDir, CStringW& errorText)
    {
        // Keep the explicit diagnostic for 7z files misnamed as EPUB.
        HANDLE input = ::CreateFileW(epubPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (input != INVALID_HANDLE_VALUE)
        {
            BYTE signature[6] = {};
            DWORD read = 0;
            const bool isSevenZip = ::ReadFile(input, signature, sizeof(signature), &read, nullptr) &&
                read == sizeof(signature) && signature[0] == 0x37 && signature[1] == 0x7a &&
                signature[2] == 0xbc && signature[3] == 0xaf && signature[4] == 0x27 && signature[5] == 0x1c;
            ::CloseHandle(input);
            if (isSevenZip)
            {
                errorText = L"7z: " + LoadImportRuntimeString(IDS_IMPORT_RUNTIME_ZIP_OPEN_FAILED,
                    L"Windows could not open EPUB as a ZIP container. The file may be damaged or blocked.");
                return false;
            }
        }
        extractDir = tempRoot + L"\\epub";
        return ExtractSecureEpubZip(epubPath, extractDir, errorText) && ValidateExtractedDirectoryTree(extractDir, errorText);
    }

    HRESULT CreateMsxmlDom(MSXML2::IXMLDOMDocument2Ptr& dom)
    {
        dom = nullptr;

        // Do not use CLSID_DOMDocument60 here. In some Windows SDK/MSVC
        // combinations msxml6.h declares this CLSID as an external symbol,
        // and the console project may fail to link with LNK2001. Resolving
        // the ProgID at runtime keeps the project independent from generated
        // #import files and from MSXML import-library quirks, which is also
        // friendlier for PVS-Studio analysis from a clean checkout.
        CLSID clsid = CLSID_NULL;
        HRESULT hr = ::CLSIDFromProgID(L"Msxml2.DOMDocument.6.0", &clsid);
        if (FAILED(hr))
            return hr;

        MSXML2::IXMLDOMDocument2* raw = nullptr;
        hr = ::CoCreateInstance(
            clsid,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(MSXML2::IXMLDOMDocument2),
            reinterpret_cast<void**>(&raw));
        if (FAILED(hr))
            return hr;

        dom = MSXML2::IXMLDOMDocument2Ptr(raw, false);

        dom->put_async(VARIANT_FALSE);
        dom->put_validateOnParse(VARIANT_FALSE);
        dom->put_resolveExternals(VARIANT_FALSE);
        dom->put_preserveWhiteSpace(VARIANT_TRUE);
        dom->setProperty(_bstr_t(L"SelectionLanguage"), _variant_t(L"XPath"));

        // MSXML 6.0 по умолчанию запрещает любой <!DOCTYPE ...>.
        // Многие EPUB/XHTML-файлы содержат безопасный XHTML/HTML doctype:
        //
        //   <!DOCTYPE html>
        //
        // Из-за этого DOMDocument.6.0 возвращает ошибку загрузки даже для
        // корректных XHTML-файлов из spine. Внешние DTD/ресурсы при этом всё
        // равно не загружаются, потому что выше выставлен resolveExternals=false.
        try
        {
            dom->setProperty(_bstr_t(L"ProhibitDTD"), _variant_t(VARIANT_FALSE, VT_BOOL));
        }
        catch (const _com_error& e)
        {
            CStringW debugMessage;
            debugMessage.Format(L"ImportEPUB: MSXML ProhibitDTD property is unavailable, hr=0x%08lX.\r\n", e.Error());
            ::OutputDebugStringW(debugMessage);
            // Свойство есть в MSXML 6.0. Если в нестандартной системе оно
            // недоступно, продолжаем работу: container.xml/OPF всё равно
            // будут читаться, а ошибка XHTML попадёт в диагностический FB2.
        }

        return S_OK;
    }

    bool LoadXmlFile(const CStringW& path, MSXML2::IXMLDOMDocument2Ptr& dom, CStringW& errorText)
    {
        const bool htmlMode = IsLikelyHtmlXmlDocumentPath(path);
        CStringW key(path); key.MakeLower();
        const std::wstring cacheKey(key.GetString());
        if (htmlMode) {
            const auto cached = g_content.xhtmlCache.find(cacheKey);
            if (cached != g_content.xhtmlCache.end()) { dom = cached->second; ++g_content.xhtmlCacheHits; return true; }
            ++g_content.xhtmlLoads[cacheKey];
        }
        auto remember = [&]() {
            // The inventory, note and rendering passes only read source DOMs.
            // Bound both the number of COM documents and their input bytes;
            // over-budget documents keep the original load-on-each-pass path.
            const unsigned long long limit = 512ULL * 1024;
            if (!htmlMode || !dom || g_content.xhtmlCache.size() >= 128) return;
            WIN32_FILE_ATTRIBUTE_DATA attributes = {};
            if (!::GetFileAttributesExW(path, GetFileExInfoStandard, &attributes)) return;
            const unsigned long long size = (static_cast<unsigned long long>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
            if (size > limit || g_content.xhtmlCacheBytes > limit - size) return;
            if (g_content.xhtmlCache.emplace(cacheKey, dom).second) g_content.xhtmlCacheBytes += size;
        };
        HRESULT hr = CreateMsxmlDom(dom);
        if (FAILED(hr))
        {
            errorText.Format(static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_MSXML_CREATE_FAILED, L"Failed to create MSXML DOMDocument.6.0. HRESULT: 0x%08X")), static_cast<unsigned int>(hr));
            return false;
        }

        VARIANT_BOOL loaded = VARIANT_FALSE;
        CStringW msxmlPath(path);
        const bool volumeGuid = msxmlPath.Left(11).CompareNoCase(L"\\\\?\\Volume{") == 0;
        if (msxmlPath.Left(8).CompareNoCase(L"\\\\?\\UNC\\") == 0) msxmlPath = CStringW(L"\\\\") + msxmlPath.Mid(8);
        else if (!volumeGuid && msxmlPath.Left(4) == L"\\\\?\\") msxmlPath = msxmlPath.Mid(4);
        HRESULT loadHr = E_FAIL;
        if (!volumeGuid && msxmlPath.GetLength() < MAX_PATH) {
            _variant_t fileName(static_cast<LPCWSTR>(msxmlPath));
            loadHr = dom->load(fileName, &loaded);
        } else {
            CComPtr<IStream> stream;
            loadHr = ::SHCreateStreamOnFileEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
            CComQIPtr<IPersistStream> persist(dom);
            if (SUCCEEDED(loadHr) && stream && persist) {
                loadHr = persist->Load(stream);
                loaded = loadHr == S_OK ? VARIANT_TRUE : VARIANT_FALSE;
            } else {
                if (SUCCEEDED(loadHr)) loadHr = E_NOINTERFACE;
            }
        }
        if (FAILED(loadHr) || loaded != VARIANT_TRUE)
        {
            CStringW reason = MsxmlGetParseErrorReason(dom);
            if (reason.IsEmpty())
                reason = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XML_UNKNOWN, L"unknown XML error");

            // Some real-world EPUB files use .htm/.html spine items that are HTML,
            // not strict XHTML: undeclared &nbsp;, <link>, <meta>, <br>, <img>
            // and other void elements are often not closed as XML. Retry by
            // loading the file as text and applying a conservative XML-safe
            // normalization. For OPF/container XML the same fallback also fixes
            // common XML 1.1 declarations that MSXML 6.0 rejects as "bad version".
            CStringW fallbackError;
            if (LoadXmlFileAfterMarkupSanitizing(path, htmlMode, dom, fallbackError)) { remember(); return true; }

            errorText.Format(static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XML_LOAD_FAILED, L"MSXML could not read the XML file:\r\n%s\r\n%s")), path.GetString(), reason.GetString());
            if (!fallbackError.IsEmpty())
                errorText += CStringW(htmlMode ? L"\r\nHTML fallback: " : L"\r\nXML fallback: ") + fallbackError;
            return false;
        }

        remember();
        return true;
    }

    CStringW LocalName(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node)
            return CStringW();

        CStringW s = MsxmlGetNodeBaseName(node);
        if (!s.IsEmpty())
            return s;

        s = MsxmlGetNodeName(node);
        int colon = s.Find(L':');
        if (colon >= 0)
            s = s.Mid(colon + 1);
        return s;
    }

    CStringW NodeText(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node)
            return CStringW();
        return NormalizeImportedText(TrimString(MsxmlGetNodeText(node)));
    }

    CStringW Attr(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR name)
    {
        if (!node)
            return CStringW();

        MSXML2::IXMLDOMNamedNodeMapPtr attrs = MsxmlGetAttributes(node);
        if (!attrs)
            return CStringW();

        MSXML2::IXMLDOMNodePtr attr = MsxmlGetNamedItem(attrs, name);
        if (!attr)
            return CStringW();

        return MsxmlGetNodeText(attr);
    }

    CStringW ToLowerString(const CStringW& text)
    {
        CStringW s(text);
        s.MakeLower();
        return s;
    }

    bool ContainsNoCase(const CStringW& text, LPCWSTR part)
    {
        CStringW lowerText = ToLowerString(text);
        CStringW lowerPart(part);
        lowerPart.MakeLower();
        return lowerText.Find(lowerPart) >= 0;
    }

    bool LooksLikeIsbnValue(const CStringW& value)
    {
        CStringW compact;
        for (int i = 0; i < value.GetLength(); ++i)
        {
            wchar_t ch = value[i];
            if (ch >= L'0' && ch <= L'9')
                compact += ch;
            else if ((ch == L'X' || ch == L'x') && compact.GetLength() == 9)
                compact += L'X';
        }

        if (compact.GetLength() == 13)
            return compact.Left(3) == L"978" || compact.Left(3) == L"979";
        return compact.GetLength() == 10;
    }

    CStringW NormalizeIsbnValue(const CStringW& value)
    {
        CStringW out;
        for (int i = 0; i < value.GetLength(); ++i)
        {
            wchar_t ch = value[i];
            if ((ch >= L'0' && ch <= L'9') || ch == L'X' || ch == L'x' || ch == L'-')
                out += ch;
        }
        out.Trim(L"-");
        return out;
    }

    bool HasCssClass(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR className)
    {
        CStringW classes = Attr(node, L"class");
        if (classes.IsEmpty())
            return false;

        CStringW needle(className);
        classes.MakeLower();
        needle.MakeLower();

        int start = 0;
        CStringW token = classes.Tokenize(L" \t\r\n", start);
        while (start != -1 || !token.IsEmpty())
        {
            if (token == needle)
                return true;
            if (start == -1)
                break;
            token = classes.Tokenize(L" \t\r\n", start);
        }
        return false;
    }


    bool HasSemanticCssClass(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR className)
    {
        return g_options.useCssSemanticClasses && HasCssClass(node, className);
    }

    bool SemanticClassContains(const CStringW& classes, LPCWSTR token)
    {
        return g_options.useCssSemanticClasses && ContainsNoCase(classes, token);
    }

    bool CssComputedHidden(const MSXML2::IXMLDOMNodePtr& node);
    bool IsTokenInList(CStringW value, LPCWSTR token);

    bool IsInteractiveTermDefinition(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || !IsTokenInList(Attr(node, L"class"), L"keytermdef")) return false;
        MSXML2::IXMLDOMNode* rawParent = nullptr;
        node->get_parentNode(&rawParent);
        MSXML2::IXMLDOMNodePtr parent(rawParent, false);
        if (!parent || !IsTokenInList(Attr(parent, L"class"), L"keytermentry")) return false;
        const auto children = MsxmlGetChildNodes(parent);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            const auto child = MsxmlGetNodeListItem(children, i);
            if (child && child != node && IsTokenInList(Attr(child, L"class"), L"keyterm")) return true;
        }
        return false;
    }

    bool IsHiddenElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!g_options.skipHiddenElements || !node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;

        // HTML boolean attributes are active by presence, including hidden="".
        const auto attributes = MsxmlGetAttributes(node);
        if (attributes && MsxmlGetNamedItem(attributes, L"hidden"))
            return true;

        // aria-hidden affects the accessibility tree, not visual rendering.
        // Treating it as CSS visibility discards visible text and images.
        return CssComputedHidden(node) && !IsInteractiveTermDefinition(node);
    }

    CStringW AttrByLocalName(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR localName)
    {
        if (!node)
            return CStringW();

        MSXML2::IXMLDOMNamedNodeMapPtr attrs = MsxmlGetAttributes(node);
        if (!attrs)
            return CStringW();

        long count = MsxmlGetNamedNodeMapLength(attrs);
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr attr = MsxmlGetNamedNodeMapItem(attrs, i);
            if (LocalName(attr).CompareNoCase(localName) == 0)
                return MsxmlGetNodeText(attr);
        }
        return CStringW();
    }

    CStringW EpubType(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW type = Attr(node, L"epub:type");
        if (type.IsEmpty())
            type = AttrByLocalName(node, L"type");
        return type;
    }

    bool IsTokenInList(CStringW value, LPCWSTR token)
    {
        if (value.IsEmpty())
            return false;
        CStringW lowerToken(token);
        lowerToken.MakeLower();
        value.MakeLower();

        int start = 0;
        CStringW part = value.Tokenize(L" \t\r\n", start);
        while (start != -1 || !part.IsEmpty())
        {
            if (part == lowerToken)
                return true;
            if (start == -1)
                break;
            part = value.Tokenize(L" \t\r\n", start);
        }
        return false;
    }

    bool IsEpubType(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR token)
    {
        return IsTokenInList(EpubType(node), token);
    }

    bool IsRoleToken(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR token)
    {
        CStringW role = Attr(node, L"role");
        if (IsTokenInList(role, token))
            return true;

        // DPUB-ARIA often stores values such as doc-noteref/doc-footnote.
        CStringW lowerRole(role);
        lowerRole.MakeLower();
        CStringW lowerToken(token);
        lowerToken.MakeLower();
        return !lowerRole.IsEmpty() && lowerRole.Find(lowerToken) >= 0;
    }

    bool IsMarkedBacklinkElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;
        return IsEpubType(node, L"backlink") || IsRoleToken(node, L"doc-backlink") ||
            HasSemanticCssClass(node, L"backlink") || HasSemanticCssClass(node, L"backlinks") ||
            HasSemanticCssClass(node, L"fn-backref");
    }

    bool IsBacklinkElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        return g_options.removeFootnoteBacklinks && IsMarkedBacklinkElement(node);
    }

    bool IsNoteReferenceElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;
        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"a") != 0)
            return false;
        return IsEpubType(node, L"noteref") || IsRoleToken(node, L"doc-noteref") ||
            HasSemanticCssClass(node, L"noteref") || HasSemanticCssClass(node, L"footnote-ref") || HasSemanticCssClass(node, L"footnote-reference") ||
            HasSemanticCssClass(node, L"fnref") || HasSemanticCssClass(node, L"footnote_reference");
    }

    bool IsEndnoteElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;
        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"aside") != 0 && name.CompareNoCase(L"section") != 0 && name.CompareNoCase(L"div") != 0 &&
            name.CompareNoCase(L"p") != 0 && name.CompareNoCase(L"li") != 0)
            return false;
        return IsEpubType(node, L"endnote") || IsEpubType(node, L"footnote") ||
            IsRoleToken(node, L"doc-endnote") || IsRoleToken(node, L"doc-footnote") ||
            HasSemanticCssClass(node, L"note") || HasSemanticCssClass(node, L"endnote") || HasSemanticCssClass(node, L"footnote");
    }

    bool IsEndnotesContainerElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;

        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"body") != 0 && name.CompareNoCase(L"section") != 0 &&
            name.CompareNoCase(L"aside") != 0 && name.CompareNoCase(L"div") != 0 &&
            name.CompareNoCase(L"ol") != 0 && name.CompareNoCase(L"ul") != 0)
        {
            return false;
        }

        CStringW id = Attr(node, L"id");
        id.MakeLower();

        return IsEpubType(node, L"endnotes") || IsEpubType(node, L"footnotes") ||
            IsRoleToken(node, L"doc-endnotes") || IsRoleToken(node, L"doc-footnotes") ||
            HasSemanticCssClass(node, L"notes") || HasSemanticCssClass(node, L"endnotes") ||
            HasSemanticCssClass(node, L"footnotes") || id == L"endnotes" || id == L"footnotes" ||
            id.Find(L"endnotes") >= 0 || id.Find(L"footnotes") >= 0;
    }

    bool IsEndnotesBody(const MSXML2::IXMLDOMNodePtr& body)
    {
        return body && IsEndnotesContainerElement(body);
    }

    CStringW HrefFragment(const CStringW& href)
    {
        int hash = href.Find(L'#');
        if (hash < 0 || hash + 1 >= href.GetLength())
            return CStringW();
        return UrlDecodeUtf8(href.Mid(hash + 1));
    }

    CStringW NormalizeFb2Id(const CStringW& value)
    {
        CStringW out = value;
        out.Trim();
        // Empty EPUB ids/names must not be converted to generated "note_*" ids.
        // Otherwise ordinary paragraphs without an id attribute receive confusing
        // FB2 ids such as note_3125 even when the book contains no notes.
        if (out.IsEmpty())
            return CStringW();

        for (int i = 0; i < out.GetLength(); ++i)
        {
            wchar_t ch = out[i];
            const bool ok = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'-' || ch == L'.';
            if (!ok)
                out.SetAt(i, L'_');
        }

        wchar_t first = out[0];
        if (!((first >= L'a' && first <= L'z') || (first >= L'A' && first <= L'Z') || first == L'_'))
            out = L"id_" + out;
        return out;
    }

    void CollectElementsByLocalName(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR localName, std::vector<MSXML2::IXMLDOMNodePtr>& out)
    {
        if (!node)
            return;

        if (LocalName(node).CompareNoCase(localName) == 0)
            out.push_back(node);

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return;

        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
            CollectElementsByLocalName(MsxmlGetNodeListItem(children, i), localName, out);
    }

    bool LocateOpfFile(const CStringW& extractDir, CStringW& opfPath, CStringW& errorText)
    {
        CStringW containerPath = extractDir + L"\\META-INF\\container.xml";
        if (!FileExists(containerPath))
        {
            errorText = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_CONTAINER_MISSING, L"META-INF/container.xml was not found in EPUB.");
            return false;
        }

        MSXML2::IXMLDOMDocument2Ptr doc;
        if (!LoadXmlFile(containerPath, doc, errorText))
            return false;

        std::vector<MSXML2::IXMLDOMNodePtr> rootfiles;
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"rootfile", rootfiles);
        for (size_t i = 0; i < rootfiles.size(); ++i)
        {
            CStringW mediaType = Attr(rootfiles[i], L"media-type");
            CStringW fullPath = Attr(rootfiles[i], L"full-path");
            if (!fullPath.IsEmpty() && (mediaType.IsEmpty() || mediaType.CompareNoCase(L"application/oebps-package+xml") == 0))
            {
                opfPath = CombineAndCanonicalizePath(extractDir, fullPath);
                if (FileExists(opfPath))
                    return true;
            }
        }

        errorText = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_CONTAINER_ROOTFILE_MISSING, L"No rootfile with OPF package was found in container.xml.");
        return false;
    }

    bool ParseOpf(const CStringW& opfPath, EpubPackage& package, CStringW& errorText)
    {
        package = EpubPackage();
        package.opfPath = opfPath;
        package.opfDir = DirectoryOf(opfPath);

        MSXML2::IXMLDOMDocument2Ptr doc;
        if (!LoadXmlFile(opfPath, doc, errorText))
            return false;

        std::vector<MSXML2::IXMLDOMNodePtr> nodes;
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"title", nodes);
        if (!nodes.empty())
            package.title = NodeText(nodes[0]);

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"creator", nodes);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            CStringW creator = NodeText(nodes[i]);
            if (!creator.IsEmpty())
                package.creators.push_back(creator);
        }

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"contributor", nodes);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            CStringW contributor = NodeText(nodes[i]);
            if (!contributor.IsEmpty())
                package.contributors.push_back(contributor);
        }

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"language", nodes);
        if (!nodes.empty())
            package.language = NodeText(nodes[0]);

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"description", nodes);
        if (!nodes.empty())
            package.description = NodeText(nodes[0]);

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"identifier", nodes);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            CStringW identifier = NodeText(nodes[i]);
            if (identifier.IsEmpty())
                continue;
            if (package.identifier.IsEmpty())
                package.identifier = identifier;
            if (package.isbn.IsEmpty() && (ContainsNoCase(identifier, L"isbn") || LooksLikeIsbnValue(identifier)))
                package.isbn = NormalizeIsbnValue(identifier);
        }

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"date", nodes);
        int dateRank = -1;
        for (const auto& node : nodes)
        {
            const CStringW date = NodeText(node);
            if (date.IsEmpty()) continue;
            CStringW event = AttrByLocalName(node, L"event"); event.Trim(); event.MakeLower();
            package.datedEvents.emplace_back(event, date);
            const int rank = event == L"publication" ? 3 : event.IsEmpty() ? 2 : event == L"creation" ? 1 : 0;
            if (rank > dateRank) { package.date = date; package.dateEvent = event; dateRank = rank; }
        }

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"publisher", nodes);
        if (!nodes.empty())
            package.publisher = NodeText(nodes[0]);

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"source", nodes);
        if (!nodes.empty())
            package.source = NodeText(nodes[0]);

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"rights", nodes);
        if (!nodes.empty())
            package.rights = NodeText(nodes[0]);

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"subject", nodes);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            CStringW subject = NodeText(nodes[i]);
            if (!subject.IsEmpty())
                package.subjects.push_back(subject);
        }

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"meta", nodes);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            CStringW name = Attr(nodes[i], L"name");
            CStringW property = Attr(nodes[i], L"property");
            CStringW content = Attr(nodes[i], L"content");
            if (content.IsEmpty())
                content = NodeText(nodes[i]);

            if (name.CompareNoCase(L"cover") == 0 && !content.IsEmpty())
                package.coverId = content;
            else if ((name.CompareNoCase(L"calibre:title_sort") == 0 || property.CompareNoCase(L"file-as") == 0) && !content.IsEmpty() && package.titleSort.IsEmpty())
                package.titleSort = NormalizeImportedText(content);
            else if (name.CompareNoCase(L"calibre:series") == 0 && !content.IsEmpty())
                package.seriesName = NormalizeImportedText(content);
            else if (name.CompareNoCase(L"calibre:series_index") == 0 && !content.IsEmpty())
                package.seriesNumber = NormalizeImportedText(content);
            else if (property.CompareNoCase(L"belongs-to-collection") == 0 && package.seriesName.IsEmpty() && !content.IsEmpty())
                package.seriesName = NormalizeImportedText(content);
            else if (property.CompareNoCase(L"group-position") == 0 && package.seriesNumber.IsEmpty() && !content.IsEmpty())
                package.seriesNumber = NormalizeImportedText(content);
            else if (property.CompareNoCase(L"dcterms:modified") == 0 && package.modifiedDate.IsEmpty() && !content.IsEmpty())
                package.modifiedDate = NormalizeImportedText(content);
            else if ((property.CompareNoCase(L"schema:accessibilitySummary") == 0 || property.CompareNoCase(L"accessibilitySummary") == 0) && package.accessibilitySummary.IsEmpty() && !content.IsEmpty())
                package.accessibilitySummary = NormalizeImportedText(content);
        }

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"item", nodes);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            ManifestItem item;
            item.id = Attr(nodes[i], L"id");
            item.href = Attr(nodes[i], L"href");
            item.mediaType = Attr(nodes[i], L"media-type");
            item.properties = Attr(nodes[i], L"properties");
            if (!item.href.IsEmpty())
                item.absPath = CombineAndCanonicalizePath(package.opfDir, item.href);
            if(!item.href.IsEmpty() && item.absPath.IsEmpty()) { errorText=L"Unsafe EPUB manifest resource: "+item.href; return false; }
            if (!item.id.IsEmpty() && !item.href.IsEmpty())
                package.manifest[std::wstring(item.id.GetString())] = item;

            if (package.coverId.IsEmpty() && item.properties.Find(L"cover-image") >= 0)
                package.coverId = item.id;
        }

        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"itemref", nodes);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            CStringW idref = Attr(nodes[i], L"idref");
            if (!idref.IsEmpty())
            {
                package.spine.push_back(idref);
                if (Attr(nodes[i], L"linear").CompareNoCase(L"no") != 0)
                    package.mandatorySpine.insert(std::wstring(idref.GetString()));
            }
        }

        // EPUB 2 guide is still common in real books. It often contains better
        // labels for title/cover/toc/notes pages than the XHTML itself. Use it
        // as an additional navigation-title source and as a hint for service
        // pages that should not become ordinary FB2 chapters.
        nodes.clear();
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"reference", nodes);
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            CStringW href = Attr(nodes[i], L"href");
            CStringW title = NormalizeImportedText(TrimString(Attr(nodes[i], L"title")));
            CStringW type = Attr(nodes[i], L"type");
            type.MakeLower();
            if (href.IsEmpty())
                continue;

            CStringW pathOnly(href);
            int hash = pathOnly.Find(L'#');
            if (hash >= 0)
                pathOnly = pathOnly.Left(hash);
            CStringW absPath = CombineAndCanonicalizePath(package.opfDir, pathOnly);
            if (!absPath.IsEmpty())
            {
                ++package.guideReferenceCount;
                if (!title.IsEmpty() && package.navTitlesByAbsPath.find(std::wstring(absPath.GetString())) == package.navTitlesByAbsPath.end())
                    package.navTitlesByAbsPath[std::wstring(absPath.GetString())] = title;

                if (type == L"cover" || type == L"title-page" || type == L"toc" ||
                    type == L"loi" || type == L"lot" || type == L"notes" ||
                    type == L"copyright-page")
                {
                    package.guideServiceDocuments.emplace(absPath.GetString());
                }

                if (type == L"cover" && package.coverId.IsEmpty())
                {
                    for (std::map<std::wstring, ManifestItem>::const_iterator mi = package.manifest.begin(); mi != package.manifest.end(); ++mi)
                    {
                        if (mi->second.absPath.CompareNoCase(absPath) == 0)
                        {
                            package.coverId = mi->second.id;
                            break;
                        }
                    }
                }
            }
        }

        if (package.title.IsEmpty())
            package.title = FileNameOnly(opfPath);

        if (package.spine.empty())
        {
            errorText = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_OPF_SPINE_MISSING, L"No spine/itemref list was found in OPF, so chapter order cannot be detected.");
            return false;
        }

        return true;
    }

    CStringW HrefWithoutFragment(const CStringW& href)
    {
        CStringW s(href);
        int hash = s.Find(L'#');
        if (hash >= 0)
            s = s.Left(hash);
        return s;
    }

    CStringW TocKeyForHref(const CStringW& baseDir, const CStringW& href)
    {
        CStringW pathOnly = HrefWithoutFragment(href);
        if (pathOnly.IsEmpty())
            return CStringW();
        return CombineAndCanonicalizePath(baseDir, pathOnly);
    }

    void AddNavigationTitle(EpubPackage& package, const CStringW& baseDir, const CStringW& href, const CStringW& title)
    {
        CStringW key = TocKeyForHref(baseDir, href);
        CStringW normalizedTitle = NormalizeImportedText(TrimString(title));
        if (key.IsEmpty() || normalizedTitle.IsEmpty())
            return;
        if (package.navTitlesByAbsPath.find(std::wstring(key.GetString())) == package.navTitlesByAbsPath.end())
            package.navTitlesByAbsPath[std::wstring(key.GetString())] = normalizedTitle;
    }

    bool IsExternalHref(CStringW href);
    MSXML2::IXMLDOMNodePtr FindFirstElementByLocalName(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR localName);

    void ReadNavigationTree(EpubPackage& package, const MSXML2::IXMLDOMNodePtr& node,
        const CStringW& baseDir, int parent, bool ncx)
    {
        if (!node) return;
        const CStringW name = LocalName(node);
        int nextParent = parent;
        if (name == (ncx ? L"navPoint" : L"li"))
        {
            MSXML2::IXMLDOMNodePtr link, label;
            const auto children = MsxmlGetChildNodes(node);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i)
            {
                const auto child = MsxmlGetNodeListItem(children, i);
                if (LocalName(child) == (ncx ? L"content" : L"a")) link = child;
                if (LocalName(child) == L"navLabel") label = child;
            }
            NavigationEntry entry;
            entry.parent = parent;
            entry.title = NormalizeImportedText(TrimString(NodeText(ncx ? label : link)));
            const CStringW href = Attr(link, ncx ? L"src" : L"href");
            if (!href.IsEmpty() && !IsExternalHref(href))
            {
                entry.path = TocKeyForHref(baseDir, href);
                entry.fragment = HrefFragment(href);
                AddNavigationTitle(package, baseDir, href, entry.title);
                for (size_t i = 0; i < package.spine.size(); ++i)
                {
                    const auto item = package.manifest.find(std::wstring(package.spine[i]));
                    if (item != package.manifest.end() && item->second.absPath.CompareNoCase(entry.path) == 0)
                    { entry.spineIndex = static_cast<int>(i);break; }
                }
            }
            nextParent = static_cast<int>(package.navigation.size());
            package.navigation.push_back(entry);
            if (parent >= 0) package.navigation[static_cast<size_t>(parent)].children.push_back(static_cast<size_t>(nextParent));
        }
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            ReadNavigationTree(package, MsxmlGetNodeListItem(children, i), baseDir, nextParent, ncx);
    }

    void ParseNavigationXhtml(EpubPackage& package, const ManifestItem& navItem, ImportLog& log)
    {
        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (!LoadXmlFile(navItem.absPath, doc, error))
        {
            log.Add(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_NAV_READ_FAILED, L"nav.xhtml: failed to read the table of contents."));
            log.Add(error);
            return;
        }

        std::vector<MSXML2::IXMLDOMNodePtr> navs;
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"nav", navs);
        for (const auto& nav : navs)
        {
            if (!IsEpubType(nav, L"toc") && !IsRoleToken(nav, L"doc-toc")) continue;
            ReadNavigationTree(package, nav, DirectoryOf(navItem.absPath), -1, false);
        }
        log.AddFormatInt(static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_NAV_HEADINGS_COUNT, L"nav.xhtml: headings found: %d")), static_cast<int>(package.navTitlesByAbsPath.size()));
    }

    MSXML2::IXMLDOMNodePtr FindFirstDescendantByLocalNameForToc(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR localName)
    {
        if (!node)
            return nullptr;
        if (LocalName(node).CompareNoCase(localName) == 0)
            return node;
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return nullptr;
        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr found = FindFirstDescendantByLocalNameForToc(MsxmlGetNodeListItem(children, i), localName);
            if (found)
                return found;
        }
        return nullptr;
    }

    void ParseNcxToc(EpubPackage& package, const ManifestItem& ncxItem, ImportLog& log)
    {
        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (!LoadXmlFile(ncxItem.absPath, doc, error))
        {
            log.Add(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_NCX_READ_FAILED, L"toc.ncx: failed to read the table of contents."));
            log.Add(error);
            return;
        }

        std::vector<MSXML2::IXMLDOMNodePtr> navPoints;
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"navPoint", navPoints);
        // EPUB 3 nav wins when both navigation formats are present.
        if (package.navigation.empty())
            ReadNavigationTree(package, FindFirstDescendantByLocalNameForToc(MsxmlGetDocumentElement(doc), L"navMap"),
                DirectoryOf(ncxItem.absPath), -1, true);
        log.AddFormatInt(static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_NCX_NAVPOINT_COUNT, L"toc.ncx: navPoint found: %d")), static_cast<int>(navPoints.size()));
    }

    void ParsePackageNavigation(EpubPackage& package, ImportLog& log)
    {
        if (!g_options.useNavigationTitles)
            return;

        if (package.guideReferenceCount > 0)
            log.AddFormatInt(L"OPF guide references: %d", package.guideReferenceCount);

        for (std::map<std::wstring, ManifestItem>::const_iterator it = package.manifest.begin(); it != package.manifest.end(); ++it)
        {
            const ManifestItem& item = it->second;
            CStringW props = item.properties;
            props.MakeLower();
            if (IsTokenInList(props, L"nav") || item.id.CompareNoCase(L"nav") == 0)
                ParseNavigationXhtml(package, item, log);
        }

        for (std::map<std::wstring, ManifestItem>::const_iterator it = package.manifest.begin(); it != package.manifest.end(); ++it)
        {
            const ManifestItem& item = it->second;
            if (item.mediaType.CompareNoCase(L"application/x-dtbncx+xml") == 0 || item.href.Right(4).CompareNoCase(L".ncx") == 0)
                ParseNcxToc(package, item, log);
        }
    }

    CStringW NavigationTitleForItem(const EpubPackage& package, const ManifestItem& item)
    {
        CStringW key = item.absPath;
        std::map<std::wstring, CStringW>::const_iterator it = package.navTitlesByAbsPath.find(std::wstring(key.GetString()));
        if (it != package.navTitlesByAbsPath.end())
            return it->second;
        return CStringW();
    }

    bool IsImageMediaType(const CStringW& mediaType)
    {
        return mediaType.Left(6).CompareNoCase(L"image/") == 0;
    }

    bool IsImageHrefByExtension(const CStringW& href)
    {
        CStringW h = href;
        int hash = h.Find(L'#');
        if (hash >= 0)
            h = h.Left(hash);
        h.MakeLower();
        return h.Right(4) == L".jpg" || h.Right(5) == L".jpeg" ||
            h.Right(4) == L".png" || h.Right(4) == L".gif" ||
            h.Right(4) == L".svg" || h.Right(5) == L".webp" ||
            h.Right(4) == L".bmp";
    }

    CStringW GuessImageMediaTypeFromHref(const CStringW& href)
    {
        CStringW h = href;
        h.MakeLower();
        if (h.Right(4) == L".jpg" || h.Right(5) == L".jpeg") return L"image/jpeg";
        if (h.Right(4) == L".png") return L"image/png";
        if (h.Right(4) == L".gif") return L"image/gif";
        if (h.Right(4) == L".svg") return L"image/svg+xml";
        if (h.Right(5) == L".webp") return L"image/webp";
        if (h.Right(4) == L".bmp") return L"image/bmp";
        return L"application/octet-stream";
    }

    CStringW SniffRasterImageMediaType(const CStringW& path)
    {
        HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return CStringW();
        unsigned char bytes[12] = {}; DWORD read = 0;
        const BOOL ok = ::ReadFile(file, bytes, sizeof(bytes), &read, nullptr);
        ::CloseHandle(file);
        if (!ok) return CStringW();
        const unsigned char png[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        if (read >= 8 && memcmp(bytes, png, sizeof(png)) == 0) return L"image/png";
        if (read >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff) return L"image/jpeg";
        if (read >= 6 && (memcmp(bytes, "GIF87a", 6) == 0 || memcmp(bytes, "GIF89a", 6) == 0)) return L"image/gif";
        if (read >= 12 && memcmp(bytes, "RIFF", 4) == 0 && memcmp(bytes + 8, "WEBP", 4) == 0) return L"image/webp";
        if (read >= 2 && bytes[0] == 'B' && bytes[1] == 'M') return L"image/bmp";
        return CStringW();
    }

    CStringW NormalizeBinaryId(const CStringW& id, const CStringW& href);

    bool IsSvgImage(const ManifestItem& item)
    {
        CStringW media(item.mediaType);
        media.MakeLower();
        CStringW href(item.href);
        href.MakeLower();
        return media == L"image/svg+xml" || href.Right(4) == L".svg" || href.Find(L".svg#") >= 0;
    }

    CStringW ChangeFileExtensionForId(const CStringW& value, const CStringW& extension)
    {
        CStringW out(value);
        int slash = max(out.ReverseFind(L'/'), out.ReverseFind(L'\\'));
        int dot = out.ReverseFind(L'.');
        if (dot > slash)
            out = out.Left(dot);
        out += extension;
        return out;
    }

    CStringW BinaryIdForImageItem(const ManifestItem& item)
    {
        const auto found = g_content.binaryIds.find(std::wstring(item.id));
        if (found != g_content.binaryIds.end()) return found->second;
        if (g_options.unifyImageNames)
        {
            CStringW extension = item.href;
            const int fragment = extension.Find(L'#'); if (fragment >= 0) extension = extension.Left(fragment);
            const int query = extension.Find(L'?'); if (query >= 0) extension = extension.Left(query);
            const int slash = max(extension.ReverseFind(L'/'), extension.ReverseFind(L'\\'));
            const int dot = extension.ReverseFind(L'.');
            extension = dot > slash ? extension.Mid(dot) : CStringW();
            if (IsSvgImage(item))
            {
                if (g_options.svgConversionMode == SVG_IMPORT_CONVERT_PNG) extension = L".png";
                else if (g_options.svgConversionMode == SVG_IMPORT_CONVERT_JPEG) extension = L".jpg";
                else if (extension.CompareNoCase(L".svg") != 0) extension = L".svg";
            }
            bool safeExtension = extension.GetLength() >= 2 && extension.GetLength() <= 8;
            for (int i = 1; safeExtension && i < extension.GetLength(); ++i)
                safeExtension = (extension[i] >= L'a' && extension[i] <= L'z') || (extension[i] >= L'A' && extension[i] <= L'Z') || (extension[i] >= L'0' && extension[i] <= L'9');
            if (!safeExtension)
            {
                CStringW media(item.mediaType); media.MakeLower();
                if (media == L"image/jpeg") extension = L".jpg";
                else if (media == L"image/png") extension = L".png";
                else if (media == L"image/gif") extension = L".gif";
                else if (media == L"image/webp") extension = L".webp";
                else extension = L".bin";
            }
            CStringW base;
            if (g_options.separateCoverName && !g_content.coverImageId.IsEmpty() && item.id == g_content.coverImageId)
                base = L"cover";
            else base.Format(L"i_%03u", g_content.nextImageNumber++);
            CStringW unique = base + extension;
            for (unsigned suffix = 2; !g_content.reservedIds.emplace(unique.GetString()).second; ++suffix)
            {
                CStringW numbered;
                numbered.Format(L"%s_%u", base.GetString(), suffix);
                unique = numbered + extension;
            }
            g_content.binaryIds.emplace(std::wstring(item.id), unique);
            return unique;
        }
        CStringW id = NormalizeBinaryId(item.id, item.href);
        if (IsSvgImage(item))
        {
            if (g_options.svgConversionMode == SVG_IMPORT_CONVERT_PNG)
                id = ChangeFileExtensionForId(id, L".png");
            else if (g_options.svgConversionMode == SVG_IMPORT_CONVERT_JPEG)
                id = ChangeFileExtensionForId(id, L".jpg");
        }
        return id;
    }

    CStringW QuoteArg(const CStringW& value)
    {
        CStringW out = L"\"";
        for (int i = 0; i < value.GetLength(); ++i)
        {
            if (value[i] == L'\"')
                out += L"\\\"";
            else
                out += value[i];
        }
        out += L"\"";
        return out;
    }

    CStringW SearchExecutablePath(LPCWSTR exeName)
    {
        wchar_t buffer[MAX_PATH] = L"";
        DWORD len = ::SearchPathW(nullptr, exeName, nullptr, _countof(buffer), buffer, nullptr);
        if (len > 0 && len < _countof(buffer) && FileExists(buffer))
            return CStringW(buffer);
        return CStringW();
    }

    CStringW EnvironmentPath(LPCWSTR name)
    {
        wchar_t buffer[4096] = L"";
        DWORD len = ::GetEnvironmentVariableW(name, buffer, _countof(buffer));
        if (len == 0 || len >= _countof(buffer))
            return CStringW();
        return CStringW(buffer);
    }

    void AddIfFileExists(std::vector<CStringW>& values, const CStringW& path)
    {
        if (!path.IsEmpty() && FileExists(path))
            values.push_back(path);
    }

    CStringW FindEdgeExecutable()
    {
        CStringW path = SearchExecutablePath(L"msedge.exe");
        if (!path.IsEmpty())
            return path;

        std::vector<CStringW> candidates;
        CStringW pf = EnvironmentPath(L"ProgramFiles");
        CStringW pfx86 = EnvironmentPath(L"ProgramFiles(x86)");
        AddIfFileExists(candidates, CombinePathLocal(pf, L"Microsoft\\Edge\\Application\\msedge.exe"));
        AddIfFileExists(candidates, CombinePathLocal(pfx86, L"Microsoft\\Edge\\Application\\msedge.exe"));
        return candidates.empty() ? CStringW() : candidates[0];
    }

    CStringW FindInkscapeExecutable()
    {
        CStringW path = SearchExecutablePath(L"inkscape.exe");
        if (!path.IsEmpty())
            return path;

        std::vector<CStringW> candidates;
        CStringW pf = EnvironmentPath(L"ProgramFiles");
        CStringW pfx86 = EnvironmentPath(L"ProgramFiles(x86)");
        AddIfFileExists(candidates, CombinePathLocal(pf, L"Inkscape\\bin\\inkscape.exe"));
        AddIfFileExists(candidates, CombinePathLocal(pfx86, L"Inkscape\\bin\\inkscape.exe"));
        return candidates.empty() ? CStringW() : candidates[0];
    }

    bool RunCommandLine(const CStringW& commandLine, DWORD timeoutMs)
    {
        CStringW mutableCmd(commandLine);
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        if (!::CreateProcessW(nullptr, mutableCmd.GetBuffer(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            mutableCmd.ReleaseBuffer();
            return false;
        }
        mutableCmd.ReleaseBuffer();

        DWORD wait = ::WaitForSingleObject(pi.hProcess, timeoutMs);
        DWORD exitCode = 1;
        if (wait == WAIT_TIMEOUT)
        {
            ::TerminateProcess(pi.hProcess, 1);
            ::WaitForSingleObject(pi.hProcess, 3000);
        }
        else
        {
            ::GetExitCodeProcess(pi.hProcess, &exitCode);
        }
        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);
        return wait != WAIT_TIMEOUT && exitCode == 0;
    }

    CStringW FileUriFromPath(const CStringW& path)
    {
        CStringW uri(path);
        uri.Replace(L'\\', L'/');
        if (uri.GetLength() >= 2 && uri[1] == L':')
            uri = L"/" + uri;
        uri.Replace(L"%", L"%25");
        uri.Replace(L" ", L"%20");
        return L"file://" + uri;
    }

    CStringW DirectoryOfCurrentModule()
    {
        HMODULE module = nullptr;
        if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&DirectoryOfCurrentModule), &module))
        {
            module = ::GetModuleHandleW(nullptr);
        }

        wchar_t path[MAX_PATH] = L"";
        DWORD len = module ? ::GetModuleFileNameW(module, path, _countof(path)) : 0;
        if (len == 0 || len >= _countof(path))
            return CStringW();
        return DirectoryOf(CStringW(path));
    }

    bool TryRenderSvgToPngWithLunaSvgAdapter(const CStringW& svgPath, const CStringW& pngPath, CStringW& warning)
    {
        typedef int(__stdcall *ImportEPUBLunaSvgRenderPngFn)(
            const wchar_t* svgPath,
            const wchar_t* pngPath,
            unsigned int maxWidth,
            unsigned int maxHeight,
            wchar_t* errorBuffer,
            unsigned int errorBufferChars);

        const CStringW moduleDir = DirectoryOfCurrentModule();
        CStringW adapterPath = CombinePathLocal(moduleDir, L"ImportEPUBLunaSVG.dll");
        // ImportEPUB.dll is shipped in Plugins, where the first probe finds
        // its helper.  ImportEPUBBatch.exe intentionally remains in the
        // program directory, so its identical importer code also probes the
        // packaged Plugins directory without relying on the current directory.
        if (!FileExists(adapterPath))
            adapterPath = CombinePathLocal(CombinePathLocal(moduleDir, L"Plugins"), L"ImportEPUBLunaSVG.dll");
        if (!FileExists(adapterPath))
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_ADAPTER_MISSING, L"SVG not converted: ImportEPUBLunaSVG.dll is missing near ImportEPUB.dll/ImportEPUBBatch.exe. A bitmap placeholder will be created: ") + svgPath + L"\r\n";
            return false;
        }

        HMODULE adapter = ::LoadLibraryW(adapterPath);
        if (!adapter)
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_ADAPTER_LOAD_FAILED, L"SVG not converted: ImportEPUBLunaSVG.dll could not be loaded. Check Win32/x86 bitness and LunaSVG dependencies. A bitmap placeholder will be created: ") + adapterPath + L"\r\n";
            return false;
        }

        ImportEPUBLunaSvgRenderPngFn render = reinterpret_cast<ImportEPUBLunaSvgRenderPngFn>(
            ::GetProcAddress(adapter, "ImportEPUBLunaSvgRenderPng"));
        if (!render)
        {
            ::FreeLibrary(adapter);
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_ADAPTER_EXPORT_MISSING, L"SVG not converted: ImportEPUBLunaSVG.dll does not export ImportEPUBLunaSvgRenderPng. A bitmap placeholder will be created.\r\n");
            return false;
        }

        wchar_t errorText[1024] = L"";
        const int ok = render(svgPath, pngPath, 2200, 3200, errorText, _countof(errorText));
        ::FreeLibrary(adapter);

        if (ok != 0 && FileExists(pngPath))
            return true;

        CStringW details(errorText);
        details.Trim();
        warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_ADAPTER_RENDER_FAILED, L"SVG was not converted through ImportEPUBLunaSVG.dll");
        if (!details.IsEmpty())
            warning += L": " + details;
        warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_PLACEHOLDER_SUFFIX, L". A bitmap placeholder will be created: ") + svgPath + L"\r\n";
        return false;
    }

    bool RenderSvgToPngFile(const CStringW& svgPath, const CStringW& pngPath, CStringW& warning)
    {
        // SVG rasterization is intentionally optional. ImportEPUB does not link to LunaSVG directly:
        // it looks for ImportEPUBLunaSVG.dll next to ImportEPUB.dll / ImportEPUBBatch.exe.
        // If the helper DLL is absent or fails, the importer creates a visible placeholder bitmap.
        return TryRenderSvgToPngWithLunaSvgAdapter(svgPath, pngPath, warning);
    }

    int GetGdiplusEncoderClsid(const WCHAR* mimeType, CLSID* clsid)
    {
        UINT count = 0;
        UINT size = 0;
        if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || size == 0)
            return -1;
        std::vector<BYTE> buffer(size);
        Gdiplus::ImageCodecInfo* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
        if (Gdiplus::GetImageEncoders(count, size, info) != Gdiplus::Ok)
            return -1;
        for (UINT i = 0; i < count; ++i)
        {
            if (wcscmp(info[i].MimeType, mimeType) == 0)
            {
                *clsid = info[i].Clsid;
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool ConvertPngToJpegFile(const CStringW& pngPath, const CStringW& jpegPath, CStringW& warning)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken = 0;
        if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_GDIPLUS_PNG_TO_JPEG_FAILED, L"Could not start GDI+ to convert PNG to JPEG.\r\n");
            return false;
        }

        bool ok = false;
        {
            Gdiplus::Image image(pngPath);
            CLSID clsid = {};
            if (image.GetLastStatus() == Gdiplus::Ok && GetGdiplusEncoderClsid(L"image/jpeg", &clsid) >= 0)
                ok = (image.Save(jpegPath, &clsid, nullptr) == Gdiplus::Ok);
        }
        Gdiplus::GdiplusShutdown(gdiplusToken);
        if (!ok)
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_SAVE_JPEG_FAILED, L"Could not save SVG as JPEG: ") + jpegPath + L"\r\n";
        return ok && FileExists(jpegPath);
    }

    bool CreateSvgPlaceholderImageFile(const CStringW& outputPath, bool jpeg, const CStringW& svgName, CStringW& warning)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken = 0;
        if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_GDIPLUS_PLACEHOLDER_FAILED, L"Could not start GDI+ to create SVG placeholder.\r\n");
            return false;
        }

        bool ok = false;
        {
            Gdiplus::Bitmap bitmap(1200, 700, PixelFormat24bppRGB);
            Gdiplus::Graphics graphics(&bitmap);
            graphics.Clear(Gdiplus::Color(255, 255, 255, 255));

            Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 0, 0, 0));
            Gdiplus::SolidBrush hintBrush(Gdiplus::Color(255, 80, 80, 80));
            Gdiplus::Pen borderPen(Gdiplus::Color(255, 160, 160, 160), 3.0f);

            graphics.DrawRectangle(&borderPen, 20, 20, 1160, 660);

            Gdiplus::FontFamily fontFamily(L"Segoe UI");
            Gdiplus::Font titleFont(&fontFamily, 34.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font textFont(&fontFamily, 26.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

            CStringW title = L"SVG image was not rasterized";
            CStringW line1 = L"ImportEPUB could not convert this SVG to a bitmap image.";
            CStringW line2 = L"Place ImportEPUBLunaSVG.dll near the EXE/DLL, or use --svg keep/skip.";
            CStringW line3 = L"Source: " + svgName;

            Gdiplus::StringFormat format;
            format.SetAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF titleRect(40.0f, 170.0f, 1120.0f, 60.0f);
            graphics.DrawString(title, -1, &titleFont, titleRect, &format, &titleBrush);

            Gdiplus::RectF textRect1(60.0f, 280.0f, 1080.0f, 44.0f);
            Gdiplus::RectF textRect2(60.0f, 330.0f, 1080.0f, 44.0f);
            Gdiplus::RectF textRect3(60.0f, 410.0f, 1080.0f, 120.0f);
            graphics.DrawString(line1, -1, &textFont, textRect1, &format, &hintBrush);
            graphics.DrawString(line2, -1, &textFont, textRect2, &format, &hintBrush);
            graphics.DrawString(line3, -1, &textFont, textRect3, &format, &hintBrush);

            CLSID clsid = {};
            const WCHAR* mimeType = jpeg ? L"image/jpeg" : L"image/png";
            if (GetGdiplusEncoderClsid(mimeType, &clsid) >= 0)
                ok = (bitmap.Save(outputPath, &clsid, nullptr) == Gdiplus::Ok);
        }

        Gdiplus::GdiplusShutdown(gdiplusToken);
        if (!ok)
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_PLACEHOLDER_CREATE_FAILED, L"Could not create SVG placeholder image: ") + outputPath + L"\r\n";
        return ok && FileExists(outputPath);
    }

    bool ReadBinaryFile(const CStringW& path, std::vector<unsigned char>& data);

    CStringW Base64Encode(const std::vector<unsigned char>& data);

    bool WriteSvgUtf8File(const CStringW& path, const CStringW& text)
    {
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.GetString(), text.GetLength(), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return false;
        std::vector<char> bytes(static_cast<size_t>(size));
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.GetString(), text.GetLength(), bytes.data(), size, nullptr, nullptr) != size)
            return false;
        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        const BOOL ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
        CloseHandle(file);
        return ok && written == bytes.size();
    }

    bool PrepareSvgImageResources(const CStringW& svgPath, const CStringW& preparedPath, CStringW& renderPath, CStringW& warning)
    {
        renderPath = svgPath;
        std::vector<unsigned char> source;
        CStringW sourceText;
        if (!ReadBinaryFile(svgPath, source) || !DecodeTextFileBytes(source, sourceText))
            return false;
        if (!ContainsNoCase(sourceText, L"<image") && !ContainsNoCase(sourceText, L":image"))
            return true;

        MSXML2::IXMLDOMDocument2Ptr dom;
        CStringW error;
        if (!LoadXmlFile(svgPath, dom, error))
        {
            warning += L"SVG image resources could not be inspected safely.\r\n";
            return false;
        }
        std::vector<MSXML2::IXMLDOMNodePtr> images;
        CollectElementsByLocalName(MsxmlGetDocumentElement(dom), L"image", images);
        bool changed = false;
        size_t totalBytes = 0;
        for (const auto& image : images)
        {
            const auto attributes = MsxmlGetAttributes(image);
            const long count = attributes ? MsxmlGetNamedNodeMapLength(attributes) : 0;
            for (long i = 0; i < count; ++i)
            {
                const auto attr = MsxmlGetNamedNodeMapItem(attributes, i);
                if (LocalName(attr).CompareNoCase(L"href") != 0) continue;
                CStringW href = MsxmlGetNodeText(attr);
                href.Trim();
                if (href.IsEmpty() || href.Left(5).CompareNoCase(L"data:") == 0) continue;
                for (MSXML2::IXMLDOMNodePtr current = image; current; )
                {
                    if (!Attr(current, L"xml:base").IsEmpty())
                    {
                        warning += L"SVG raster resource with xml:base could not be embedded safely.\r\n";
                        return false;
                    }
                    MSXML2::IXMLDOMNode* parent = nullptr;
                    current->get_parentNode(&parent);
                    current = MSXML2::IXMLDOMNodePtr(parent, false);
                }
                const int fragment = href.Find(L'#');
                const int query = href.Find(L'?');
                if (fragment >= 0 || query >= 0)
                {
                    warning += L"SVG raster resource with query or fragment could not be embedded safely.\r\n";
                    return false;
                }
                const CStringW path = CombineAndCanonicalizePath(DirectoryOf(svgPath), href);
                const CStringW media = GuessImageMediaTypeFromHref(href);
                if (path.IsEmpty() || (media != L"image/png" && media != L"image/jpeg" && media != L"image/gif" && media != L"image/webp" && media != L"image/bmp"))
                {
                    warning += L"SVG raster resource is external, unsupported or outside the EPUB.\r\n";
                    return false;
                }
                std::vector<unsigned char> bytes;
                WIN32_FILE_ATTRIBUTE_DATA info = {};
                if (!GetFileAttributesExW(path, GetFileExInfoStandard, &info))
                {
                    warning += L"SVG raster resource could not be read or exceeds the embedding limit.\r\n";
                    return false;
                }
                const unsigned long long resourceSize = (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
                const unsigned long long limit = 16ULL * 1024 * 1024;
                if (!resourceSize || resourceSize > limit || totalBytes > limit - resourceSize ||
                    !ReadBinaryFile(path, bytes) || bytes.size() != resourceSize)
                {
                    warning += L"SVG raster resource could not be read or exceeds the embedding limit.\r\n";
                    return false;
                }
                totalBytes += bytes.size();
                const CStringW uri = L"data:" + media + L";base64," + Base64Encode(bytes);
                if (FAILED(attr->put_nodeValue(CComVariant(uri)))) return false;
                changed = true;
            }
        }
        if (!changed) return true;
        CComBSTR serialized;
        if (FAILED(dom->get_xml(&serialized)) || !serialized) return false;
        CStringW xml(serialized);
        xml.TrimLeft();
        if (xml.Left(5).CompareNoCase(L"<?xml") == 0)
        {
            const int end = xml.Find(L"?>");
            if (end < 0) return false;
            xml = xml.Mid(end + 2);
        }
        if (!WriteSvgUtf8File(preparedPath, xml)) return false;
        renderPath = preparedPath;
        return true;
    }

    bool ReadSvgImageData(const ManifestItem& item, std::vector<unsigned char>& data, CStringW& contentType, CStringW& warning)
    {
        if (!IsSvgImage(item))
        {
            contentType = IsImageMediaType(item.mediaType) ? item.mediaType : GuessImageMediaTypeFromHref(item.href);
            return ReadBinaryFile(item.absPath, data);
        }

        ++g_runtimeStats.svgImages;

        if (g_options.svgConversionMode == SVG_IMPORT_SKIP)
        {
            ++g_runtimeStats.svgSkipped;
            g_runtimeStats.svgBackend = L"skip";
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_SKIPPED_BY_OPTION, L"SVG skipped by import option: ") + item.href + L"\r\n";
            return false;
        }

        if (g_options.svgConversionMode == SVG_IMPORT_KEEP)
        {
            g_runtimeStats.svgBackend = L"keep";
            contentType = L"image/svg+xml";
            return ReadBinaryFile(item.absPath, data);
        }

        wchar_t temp[MAX_PATH] = L"";
        if (!::GetTempPathW(_countof(temp), temp))
            return false;
        CStringW tempDir;
        tempDir.Format(L"%sImportEPUB_svg_%s", temp, GuidString().GetString());
        CreateDirectoryRecursive(tempDir);

        CStringW pngPath = CombinePathLocal(tempDir, L"rendered.png");
        CStringW renderPath;
        bool rendered = PrepareSvgImageResources(item.absPath, CombinePathLocal(tempDir, L"prepared.svg"), renderPath, warning) &&
            RenderSvgToPngFile(renderPath, pngPath, warning);
        bool ok = false;

        if (!rendered)
        {
            // Do not leave a dangling <image l:href="#..."> without a matching <binary>.
            // If no SVG renderer is available, create a visible placeholder bitmap instead.
            // The original SVG is not embedded in this mode because many FB2/FBE consumers do not display SVG.
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_PLACEHOLDER_REASON, L"A bitmap placeholder will be created for SVG because ImportEPUBLunaSVG.dll is missing or failed to render: ") + item.href + L"\r\n";
            if (g_options.svgConversionMode == SVG_IMPORT_CONVERT_JPEG)
            {
                CStringW jpgPlaceholder = CombinePathLocal(tempDir, L"svg-placeholder.jpg");
                if (CreateSvgPlaceholderImageFile(jpgPlaceholder, true, item.href, warning))
                {
                    contentType = L"image/jpeg";
                    ok = ReadBinaryFile(jpgPlaceholder, data);
                    if (ok)
                    {
                        ++g_runtimeStats.svgPlaceholders;
                        g_runtimeStats.svgBackend = L"placeholder";
                    }
                }
            }
            else
            {
                CStringW pngPlaceholder = CombinePathLocal(tempDir, L"svg-placeholder.png");
                if (CreateSvgPlaceholderImageFile(pngPlaceholder, false, item.href, warning))
                {
                    contentType = L"image/png";
                    ok = ReadBinaryFile(pngPlaceholder, data);
                    if (ok)
                    {
                        ++g_runtimeStats.svgPlaceholders;
                        g_runtimeStats.svgBackend = L"placeholder";
                    }
                }
            }
        }
        else if (g_options.svgConversionMode == SVG_IMPORT_CONVERT_JPEG)
        {
            CStringW jpgPath = CombinePathLocal(tempDir, L"rendered.jpg");
            if (ConvertPngToJpegFile(pngPath, jpgPath, warning))
            {
                contentType = L"image/jpeg";
                ok = ReadBinaryFile(jpgPath, data);
                if (ok)
                {
                    ++g_runtimeStats.svgConverted;
                    g_runtimeStats.svgBackend = L"LunaSVG";
                }
            }
        }
        else
        {
            contentType = L"image/png";
            ok = ReadBinaryFile(pngPath, data);
            if (ok)
            {
                ++g_runtimeStats.svgConverted;
                g_runtimeStats.svgBackend = L"LunaSVG";
            }
        }

        DeleteDirectoryTree(tempDir);
        if (!ok)
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_CONVERTED_READ_FAILED, L"Could not read SVG conversion result: ") + item.href + L"\r\n";
        return ok;
    }

    CStringW NormalizeBinaryId(const CStringW& id, const CStringW& href)
    {
        CStringW out = id;
        if (out.IsEmpty())
            out = FileNameOnly(href);
        if (out.IsEmpty())
            out = L"image";

        for (int i = 0; i < out.GetLength(); ++i)
        {
            wchar_t ch = out[i];
            const bool ok = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'-' || ch == L'.';
            if (!ok)
                out.SetAt(i, L'_');
        }

        if (!(out[0] >= L'a' && out[0] <= L'z') && !(out[0] >= L'A' && out[0] <= L'Z') && out[0] != L'_')
            out = L"img_" + out;
        return out;
    }

    struct EpubNote
    {
        CStringW id;
        CStringW title;
        CStringW content;
    };

    struct ConvertContext
    {
        const EpubPackage* package;
        CStringW currentXhtmlDir;
        CStringW currentXhtmlPath;
        std::set<std::wstring>* usedImageIds;
        const std::set<std::wstring>* noteIds;
        std::set<std::wstring>* usedFb2Ids;
        std::map<std::wstring, std::wstring>* anchorIdMap;
        bool collectingNotes;
        bool inlinePageBreaks = false;
        bool inlineLineBreaks = false;
    };

    // Stable source position even if an XHTML document is loaded again. Cache
    // sibling ordinals only for the current DOM and retain their COM identities
    // until that DOM changes; no document-wide quadratic sibling scans.
    std::wstring DiagnosticElementKey(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        if (!node || ctx.currentXhtmlPath.IsEmpty()) return std::wstring();
        std::vector<MSXML2::IXMLDOMNodePtr> chain;
        MSXML2::IXMLDOMNodePtr current(node);
        for (unsigned depth = 0; current && depth < 1024; ++depth) {
            IXMLDOMNode* rawParent = nullptr;
            if (FAILED(current->get_parentNode(&rawParent)) || !rawParent) break;
            chain.push_back(current); current.Attach(rawParent);
        }
        if (!current || chain.empty() || MsxmlGetNodeType(current) != MSXML2::NODE_DOCUMENT) return std::wstring();
        CComPtr<IUnknown> document;
        if (FAILED(current->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&document.p)))) return std::wstring();
        if (g_content.diagnosticDocument != document) {
            g_content.diagnosticSiblingPositions.clear();
            g_content.diagnosticIndexedParents.clear();
            g_content.diagnosticNodeReferences.clear();
            g_content.diagnosticDocument = document;
        }
        CStringW key(ctx.currentXhtmlPath); key.MakeLower();
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            CComPtr<IUnknown> identity;
            if (FAILED((*it)->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity.p)))) return std::wstring();
            auto position = g_content.diagnosticSiblingPositions.find(identity);
            if (position == g_content.diagnosticSiblingPositions.end()) {
                IXMLDOMNode* rawParent = nullptr;
                if (FAILED((*it)->get_parentNode(&rawParent)) || !rawParent) return std::wstring();
                MSXML2::IXMLDOMNodePtr parent(rawParent, false);
                CComPtr<IUnknown> parentIdentity;
                if (FAILED(parent->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&parentIdentity.p)))) return std::wstring();
                if (g_content.diagnosticIndexedParents.emplace(parentIdentity).second) {
                    g_content.diagnosticNodeReferences.push_back(parentIdentity);
                    IXMLDOMNode* rawChild = nullptr;
                    if (FAILED(parent->get_firstChild(&rawChild))) return std::wstring();
                    MSXML2::IXMLDOMNodePtr child(rawChild, false);
                    unsigned ordinal = 0;
                    while (child) {
                        CComPtr<IUnknown> childIdentity;
                        if (FAILED(child->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&childIdentity.p)))) return std::wstring();
                        g_content.diagnosticSiblingPositions.emplace(childIdentity, ordinal++);
                        g_content.diagnosticNodeReferences.push_back(childIdentity);
                        IXMLDOMNode* rawNext = nullptr;
                        if (FAILED(child->get_nextSibling(&rawNext))) return std::wstring();
                        child.Attach(rawNext);
                    }
                }
                position = g_content.diagnosticSiblingPositions.find(identity);
                if (position == g_content.diagnosticSiblingPositions.end()) return std::wstring();
            }
            CStringW part; part.Format(L"/%u", position->second); key += part;
        }
        return std::wstring(key.GetString());
    }

    void RecordDiagnosticElement(std::set<std::wstring>& destination, const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        const std::wstring key = DiagnosticElementKey(node, ctx);
        if (!key.empty()) destination.emplace(key);
    }

    void RecordBacklink(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        if (!IsMarkedBacklinkElement(node)) return;
        auto& target = g_options.removeFootnoteBacklinks ? g_content.removedBacklinkElements :
            (g_options.preserveLinks ? g_content.retainedBacklinkElements : g_content.textOnlyBacklinkElements);
        RecordDiagnosticElement(target, node, ctx);
    }

    void RecordMappedLink(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, const CStringW& source, const CStringW& target)
    {
        if (!IsExternalHref(source) && !target.IsEmpty() && target[0] == L'#' && source != target)
            RecordDiagnosticElement(g_content.mappedLinkElements, node, ctx);
    }

    const ManifestItem* FindManifestItemById(const EpubPackage& package, const CStringW& id)
    {
        std::map<std::wstring, ManifestItem>::const_iterator it = package.manifest.find(std::wstring(id.GetString()));
        if (it == package.manifest.end())
            return nullptr;
        return &it->second;
    }

    const ManifestItem* FindManifestImageByHref(const ConvertContext& ctx, const CStringW& href)
    {
        if (!ctx.package || href.IsEmpty())
            return nullptr;

        CStringW noFragment = href;
        int hash = noFragment.Find(L'#');
        if (hash >= 0)
            noFragment = noFragment.Left(hash);
        const int query = noFragment.Find(L'?');
        if (query >= 0) noFragment = noFragment.Left(query);
        if (noFragment.IsEmpty())
            return nullptr;

        CStringW abs = CombineAndCanonicalizePath(ctx.currentXhtmlDir, noFragment);
        for (std::map<std::wstring, ManifestItem>::const_iterator it = ctx.package->manifest.begin(); it != ctx.package->manifest.end(); ++it)
        {
            const ManifestItem& item = it->second;
            if (!IsImageMediaType(item.mediaType) && !IsImageHrefByExtension(item.href))
                continue;
            if (item.absPath.CompareNoCase(abs) == 0)
                return &item;
        }
        for (const auto& entry : g_content.extraImages)
            if (entry.second.absPath.CompareNoCase(abs) == 0) return &entry.second;
        // EPUBs in the wild can omit an otherwise valid, referenced image from
        // the manifest. Only admit that particular file through the same path guard.
        if (!abs.IsEmpty() && FileExists(abs))
        {
            const bool knownExtension = IsImageHrefByExtension(noFragment);
            const CStringW detected = knownExtension ? CStringW() : SniffRasterImageMediaType(abs);
            if (!knownExtension && detected.IsEmpty()) return nullptr;
            ManifestItem item;
            item.id = UniqueContentId(NormalizeBinaryId(CStringW(), noFragment));
            item.href = noFragment;item.absPath = abs;
            item.mediaType = knownExtension ? GuessImageMediaTypeFromHref(noFragment) : detected;
            if (!knownExtension)
                g_content.warnings += L"Image type inferred from raster signature: " + noFragment + L"\r\n";
            if (!g_options.unifyImageNames) g_content.binaryIds.emplace(item.id.GetString(), item.id);
            return &g_content.extraImages.emplace(std::wstring(item.id), item).first->second;
        }
        return nullptr;
    }

    CStringW Fb2ImageElementForHref(const ConvertContext& ctx, const CStringW& href)
    {
        if (!g_options.importImages)
            return CStringW();

        ++g_content.imageReferences;
        const ManifestItem* item = FindManifestImageByHref(ctx, href);
        if (!item)
        {
            ++g_content.skippedImages;
            g_content.warnings += L"Unresolved image resource: " + ctx.currentXhtmlPath + L" -> " + href + L"\r\n";
            return CStringW();
        }
        if (IsSvgImage(*item) && g_options.svgConversionMode == SVG_IMPORT_SKIP)
        {
            ++g_content.skippedImages;
            return CStringW();
        }
        if (!FileExists(item->absPath))
        {
            ++g_content.skippedImages;
            g_content.warnings += L"Missing image resource: " + item->href + L"\r\n";
            return CStringW();
        }

        CStringW id = BinaryIdForImageItem(*item);
        if (ctx.usedImageIds)
            ctx.usedImageIds->insert(std::wstring(item->id.GetString()));
        return L"<image l:href=\"#" + XmlEscape(id) + L"\"/>";
    }

    CStringW FirstUrlFromSrcset(const CStringW& srcset)
    {
        CStringW s(srcset);
        s.Trim();
        if (s.IsEmpty())
            return CStringW();

        int comma = s.Find(L',');
        if (comma >= 0)
            s = s.Left(comma);
        s.Trim();

        int start = 0;
        CStringW url = s.Tokenize(L" \t\r\n", start);
        url.Trim();
        return url;
    }

    CStringW ImageHrefFromNode(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW href = Attr(node, L"src");
        if (href.IsEmpty())
            href = Attr(node, L"href");
        if (href.IsEmpty())
            href = AttrByLocalName(node, L"href");
        if (href.IsEmpty())
            href = FirstUrlFromSrcset(Attr(node, L"srcset"));
        return href;
    }

    void ImageCandidates(const MSXML2::IXMLDOMNodePtr& node, std::vector<CStringW>& candidates)
    {
        const CStringW name = LocalName(node);
        if (name == L"picture")
        {
            const auto children = MsxmlGetChildNodes(node);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i) ImageCandidates(MsxmlGetNodeListItem(children, i), candidates);
            return;
        }
        if (name != L"img" && name != L"image" && name != L"source" && name != L"object" && name != L"video") return;
        CStringW primary = name == L"object" ? Attr(node, L"data") : name == L"video" ? Attr(node, L"poster") : Attr(node, L"src");
        if (primary.IsEmpty()) primary = AttrByLocalName(node, L"href");
        if (!primary.IsEmpty()) candidates.push_back(primary);
        const CStringW srcset = Attr(node, L"srcset");
        int position = 0;
        CStringW entry = srcset.Tokenize(L",", position);
        while (!entry.IsEmpty())
        {
            const CStringW url = FirstUrlFromSrcset(entry);
            if (!url.IsEmpty()) candidates.push_back(url);
            if (position < 0) break;
            entry = srcset.Tokenize(L",", position);
        }
    }

    CStringW Fb2ImageElementForNode(const ConvertContext& ctx, const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!g_options.importImages) return CStringW();
        std::vector<CStringW> candidates;
        ImageCandidates(node, candidates);
        for (const auto& href : candidates)
        {
            const auto item = FindManifestImageByHref(ctx, href);
            if (item && FileExists(item->absPath) && !(IsSvgImage(*item) && g_options.svgConversionMode == SVG_IMPORT_SKIP))
                return Fb2ImageElementForHref(ctx, href);
        }
        return candidates.empty() ? CStringW() : Fb2ImageElementForHref(ctx, candidates.front());
    }

    const ManifestItem* FirstImageInXhtml(const ManifestItem& document, const EpubPackage& package)
    {
        if (document.mediaType.CompareNoCase(L"application/xhtml+xml") != 0 &&
            document.mediaType.CompareNoCase(L"text/html") != 0 &&
            document.href.Right(5).CompareNoCase(L".html") != 0 &&
            document.href.Right(6).CompareNoCase(L".xhtml") != 0)
        {
            return nullptr;
        }

        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (!LoadXmlFile(document.absPath, doc, error))
            return nullptr;

        ConvertContext ctx = {};
        ctx.package = &package;
        ctx.currentXhtmlDir = DirectoryOf(document.absPath);
        ctx.usedImageIds = nullptr;
        ctx.noteIds = nullptr;

        std::vector<MSXML2::IXMLDOMNodePtr> images;
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"img", images);
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"image", images);
        for (size_t i = 0; i < images.size(); ++i)
        {
            CStringW href = ImageHrefFromNode(images[i]);
            const ManifestItem* imageItem = FindManifestImageByHref(ctx, href);
            if (imageItem && !(IsSvgImage(*imageItem) && g_options.svgConversionMode == SVG_IMPORT_SKIP))
                return imageItem;
        }

        return nullptr;
    }

    const ManifestItem* ResolveCoverImageItem(const EpubPackage& package)
    {
        if (package.coverId.IsEmpty()) return nullptr;
        const ManifestItem* cover = FindManifestItemById(package, package.coverId);
        if (!cover) return nullptr;
        if (IsImageMediaType(cover->mediaType) || IsImageHrefByExtension(cover->href))
            return IsSvgImage(*cover) && g_options.svgConversionMode == SVG_IMPORT_SKIP ? nullptr : cover;
        return FirstImageInXhtml(*cover, package);
    }

    bool ReadBinaryFile(const CStringW& path, std::vector<unsigned char>& data)
    {
        data.clear();
        HANDLE h = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER size = {};
        if (!::GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > 256LL * 1024LL * 1024LL)
        {
            ::CloseHandle(h);
            return false;
        }

        data.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        BOOL ok = TRUE;
        if (!data.empty())
            ok = ::ReadFile(h, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
        ::CloseHandle(h);
        if (!ok || read != data.size())
        {
            data.clear();
            return false;
        }
        return true;
    }

    CStringW Base64Encode(const std::vector<unsigned char>& data)
    {
        static const wchar_t alphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        CStringW out;
        const size_t len = data.size();
        for (size_t i = 0; i < len; i += 3)
        {
            unsigned int b0 = data[i];
            unsigned int b1 = (i + 1 < len) ? data[i + 1] : 0;
            unsigned int b2 = (i + 2 < len) ? data[i + 2] : 0;
            out += alphabet[(b0 >> 2) & 0x3F];
            out += alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F];
            out += (i + 1 < len) ? alphabet[((b1 << 2) | (b2 >> 6)) & 0x3F] : L'=';
            out += (i + 2 < len) ? alphabet[b2 & 0x3F] : L'=';
        }
        return out;
    }

    CStringW LowerMapKey(CStringW value);
    CStringW ResolveFb2HrefFragment(const ConvertContext& ctx, const CStringW& rawFragment);
    CStringW ResolveFb2Href(const ConvertContext& ctx, const CStringW& rawHref);
    CStringW PathAnchorMapKey(const CStringW& absPath, const CStringW& rawId);
    bool IsExternalHref(CStringW href);

    struct InlineFontState { bool italic = false; bool bold = false; };
    struct CssSelectorPart
    {
        CStringW element;
        std::vector<CStringW> classes;
    };
    struct CssFontRule
    {
        int pseudo = 0; // 1: before, 2: after
        bool contentSet = false, contentImportant = false;
        CStringW content;
        CStringW declarations, visibilityDeclarations, positioningDeclarations;
        CStringW backgroundHref, backgroundDir;
        bool backgroundSet = false, backgroundImportant = false;
        CStringW listImageHref, listImageDir;
        bool listImageSet = false, listImageImportant = false, listImageInherit = false, listImageUnsupported = false;
        bool hasVertical = false, unsupportedFont = false;
        std::vector<CssSelectorPart> parts;
        int classSpecificity = 0, elementSpecificity = 0;
    };
    thread_local const std::vector<CssFontRule>* g_cssFontRules = nullptr;
    thread_local bool g_cssHasVerticalRules = false;
    thread_local bool g_cssHasUnsupportedRules = false;
    CStringW SupportedCssFontDeclarations(CStringW declarations, CStringW& warning, bool* hasFont = nullptr);

    bool ParseCssNumericWeight(const CStringW& value, int& weight)
    {
        if (value.IsEmpty() || value.GetLength() > 4) return false;
        int parsed = 0;
        for (int i = 0; i < value.GetLength(); ++i)
        {
            if (value[i] < L'0' || value[i] > L'9') return false;
            parsed = parsed * 10 + value[i] - L'0';
        }
        if (parsed < 1 || parsed > 1000) return false;
        weight = parsed;
        return true;
    }

    bool ExtractCssImportant(CStringW& value)
    {
        const int bang = value.ReverseFind(L'!');
        if (bang < 0) return false;
        CStringW suffix = value.Mid(bang + 1); suffix.Trim();
        if (suffix != L"important") return false;
        value = value.Left(bang); value.Trim();
        return true;
    }

    void ParseCssBackground(CStringW declarations, bool& set, bool& important, CStringW& href)
    {
        int position = 0;
        while (position != -1)
        {
            CStringW declaration = declarations.Tokenize(L";", position);
            const int colon = declaration.Find(L':');
            if (colon < 0) continue;
            CStringW key = declaration.Left(colon), value = declaration.Mid(colon + 1);
            key.Trim(); key.MakeLower(); value.Trim();
            if (key != L"background-image" && key != L"background") continue;
            CStringW lowered(value); lowered.MakeLower();
            const bool priority = ExtractCssImportant(lowered);
            if (priority)
            {
                const int bang = value.ReverseFind(L'!');
                value = value.Left(bang); value.Trim();
            }
            if (set && important && !priority) continue;
            set = true; important = priority; href.Empty();
            lowered = value; lowered.MakeLower();
            const int start = lowered.Find(L"url(");
            if (start < 0 || lowered.Find(L"url(", start + 4) >= 0) continue;
            const int end = value.Find(L')', start + 4);
            if (end < 0) continue;
            href = value.Mid(start + 4, end - start - 4); href.Trim();
            if (href.GetLength() >= 2 && ((href[0] == L'\'' && href[href.GetLength() - 1] == L'\'') ||
                (href[0] == L'"' && href[href.GetLength() - 1] == L'"')))
                href = href.Mid(1, href.GetLength() - 2);
        }
    }

    struct CssListImage
    {
        CStringW href, directory;
        bool set = false, important = false, inherit = false, unsupported = false;
    };

    void ParseCssListImage(CStringW declarations, CssListImage& result)
    {
        int position = 0;
        while (position != -1)
        {
            CStringW declaration = declarations.Tokenize(L";", position);
            const int colon = declaration.Find(L':');
            if (colon < 0) continue;
            CStringW key = declaration.Left(colon), value = declaration.Mid(colon + 1);
            key.Trim(); key.MakeLower(); value.Trim();
            if (key != L"list-style-image" && key != L"list-style") continue;
            CStringW lowered(value); lowered.MakeLower();
            const bool important = ExtractCssImportant(lowered);
            if (result.set && result.important && !important) continue;
            if (important) { value = value.Left(value.ReverseFind(L'!')); value.Trim(); }
            lowered = value; lowered.MakeLower();
            result.set = true; result.important = important; result.inherit = false;
            result.unsupported = false; result.href.Empty();
            if (lowered == L"inherit" || lowered == L"unset") { result.inherit = true; continue; }
            if (lowered == L"none" || lowered == L"initial") continue;
            if (key == L"list-style")
            {
                const int urlStart = lowered.Find(L"url(");
                if (urlStart < 0)
                {
                    // The shorthand resets list-style-image even when it specifies only a type or position.
                    if (lowered.Find(L"var(") >= 0 || lowered.Find(L"image(") >= 0)
                        result.unsupported = true;
                    continue;
                }
                const int urlEnd = lowered.Find(L')', urlStart + 4);
                if (urlEnd < 0 || lowered.Find(L"url(", urlStart + 4) >= 0)
                { result.unsupported = true; continue; }
                value = value.Mid(urlStart, urlEnd - urlStart + 1);
                lowered = value; lowered.MakeLower();
            }
            if (lowered.Left(4) != L"url(" || lowered.Right(1) != L")" || lowered.Find(L"url(", 4) >= 0)
            { result.unsupported = true; continue; }
            CStringW href = value.Mid(4, value.GetLength() - 5); href.Trim();
            if (href.GetLength() >= 2 && ((href[0] == L'\'' && href[href.GetLength() - 1] == L'\'') ||
                (href[0] == L'"' && href[href.GetLength() - 1] == L'"')))
                href = href.Mid(1, href.GetLength() - 2);
            if (href.IsEmpty()) result.unsupported = true;
            else result.href = href;
        }
    }

    bool HasCssVisibilityDeclaration(CStringW declarations)
    {
        int position = 0;
        while (position != -1)
        {
            CStringW declaration = declarations.Tokenize(L";", position);
            const int colon = declaration.Find(L':');
            if (colon < 0) continue;
            CStringW key = declaration.Left(colon); key.Trim(); key.MakeLower();
            if (key == L"display" || key == L"visibility") return true;
        }
        return false;
    }

    void ApplyCssVisibility(CStringW declarations, bool important, bool& displayHidden, bool& visibilityHidden)
    {
        declarations.MakeLower();
        int position = 0;
        while (position != -1)
        {
            CStringW declaration = declarations.Tokenize(L";", position);
            const int colon = declaration.Find(L':');
            if (colon < 0) continue;
            CStringW key = declaration.Left(colon), value = declaration.Mid(colon + 1);
            key.Trim(); value.Trim();
            if (ExtractCssImportant(value) != important) continue;
            if (key == L"display" && value == L"none") displayHidden = true;
            else if (key == L"display" && !value.IsEmpty() && value != L"inherit" && value != L"unset") displayHidden = false;
            else if (key == L"visibility" && (value == L"hidden" || value == L"collapse")) visibilityHidden = true;
            else if (key == L"visibility" && (value == L"visible" || value == L"initial")) visibilityHidden = false;
        }
    }

    void ApplyFontDeclarations(CStringW style, InlineFontState& font, const InlineFontState& inherited, bool important)
    {
        style.MakeLower();
        int position = 0;
        while (position != -1)
        {
            CStringW declaration = style.Tokenize(L";", position);
            const int colon = declaration.Find(L':');
            if (colon < 0) continue;
            CStringW key = declaration.Left(colon), value = declaration.Mid(colon + 1);
            key.Trim(); value.Trim();
            if (ExtractCssImportant(value) != important) continue;
            if (key == L"font-style")
            {
                if (value == L"italic" || value == L"oblique") font.italic = true;
                else if (value == L"normal" || value == L"initial") font.italic = false;
                else if (value == L"inherit" || value == L"unset") font.italic = inherited.italic;
            }
            else if (key == L"font-weight")
            {
                if (value == L"bold") font.bold = true;
                else if (value == L"normal" || value == L"initial") font.bold = false;
                else if (value == L"inherit" || value == L"unset") font.bold = inherited.bold;
                else { int weight = 0; if (ParseCssNumericWeight(value, weight)) font.bold = weight >= 600; }
            }
        }
    }

    bool MatchesCssClass(const MSXML2::IXMLDOMNodePtr& node, const CStringW& name)
    {
        CStringW classes = Attr(node, L"class");
        int position = 0;
        while (position != -1)
            if (classes.Tokenize(L" \t\r\n", position) == name) return true;
        return false;
    }

    bool MatchesCssPart(const MSXML2::IXMLDOMNodePtr& node, const CssSelectorPart& part)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT) return false;
        if (!part.element.IsEmpty() && part.element.CompareNoCase(LocalName(node)) != 0) return false;
        for (const auto& cssClass : part.classes)
            if (!MatchesCssClass(node, cssClass)) return false;
        return true;
    }

    bool MatchesCssRule(const MSXML2::IXMLDOMNodePtr& node, const CssFontRule& rule, int pseudo = 0)
    {
        if (rule.pseudo != pseudo || rule.parts.empty() || !MatchesCssPart(node, rule.parts.back())) return false;
        MSXML2::IXMLDOMNodePtr ancestor(node);
        for (size_t index = rule.parts.size() - 1; index > 0; --index)
        {
            bool found = false;
            while (ancestor)
            {
                IXMLDOMNode* parent = nullptr;
                if (FAILED(ancestor->get_parentNode(&parent))) return false;
                ancestor = MSXML2::IXMLDOMNodePtr(parent, false);
                if (MatchesCssPart(ancestor, rule.parts[index - 1])) { found = true; break; }
            }
            if (!found) return false;
        }
        return true;
    }

    void ApplyInlineFont(const MSXML2::IXMLDOMNodePtr& node, InlineFontState& font)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT) return;
        const InlineFontState inherited = font;
        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"em") == 0 || name.CompareNoCase(L"i") == 0) font.italic = true;
        if (name.CompareNoCase(L"strong") == 0 || name.CompareNoCase(L"b") == 0) font.bold = true;
        const CStringW inlineStyle = Attr(node, L"style");
        CStringW supported;
        if (!inlineStyle.IsEmpty())
        {
            CStringW warning;
            supported = SupportedCssFontDeclarations(inlineStyle, warning);
            if (!warning.IsEmpty() && g_content.warnedInlineCss.emplace(inlineStyle.GetString()).second)
                g_content.warnings += warning;
        }
        // Important declarations override all normal declarations; within each
        // tier the stylesheet is sorted by specificity and the inline style wins.
        for (int priority = 0; priority != 2; ++priority)
        {
            if (g_cssFontRules) for (const auto& rule : *g_cssFontRules)
            {
                if (!rule.declarations.IsEmpty() && MatchesCssRule(node, rule))
                    ApplyFontDeclarations(rule.declarations, font, inherited, priority != 0);
            }
            ApplyFontDeclarations(supported, font, inherited, priority != 0);
        }
    }

    void ApplyCssVertical(CStringW declarations, int& vertical, bool important, bool& specified)
    {
        declarations.MakeLower();
        int position = 0;
        while (position != -1)
        {
            const CStringW declaration = declarations.Tokenize(L";", position);
            const int colon = declaration.Find(L':');
            if (colon < 0) continue;
            CStringW key = declaration.Left(colon), value = declaration.Mid(colon + 1);
            key.Trim(); value.Trim();
            if (key != L"vertical-align" || ExtractCssImportant(value) != important) continue;
            if (value == L"super") { vertical = 1; specified = true; }
            else if (value == L"sub") { vertical = -1; specified = true; }
            else if (value == L"baseline" || value == L"initial" || value == L"unset") { vertical = 0; specified = true; }
        }
    }

    int CssVerticalForNode(const MSXML2::IXMLDOMNodePtr& node, bool* hasValue = nullptr)
    {
        int vertical = 0;
        bool specified = false;
        const CStringW inlineStyle = Attr(node, L"style");
        if (!g_cssHasVerticalRules && inlineStyle.IsEmpty())
        {
            if (hasValue) *hasValue = false;
            return 0;
        }
        CStringW position, top;
        const auto applyPosition = [&](CStringW declarations, bool important) {
            declarations.MakeLower();
            int cursor = 0;
            while (cursor != -1)
            {
                CStringW declaration = declarations.Tokenize(L";", cursor);
                const int colon = declaration.Find(L':');
                if (colon < 0) continue;
                CStringW key = declaration.Left(colon), value = declaration.Mid(colon + 1);
                key.Trim(); value.Trim();
                if ((key != L"position" && key != L"top") || ExtractCssImportant(value) != important) continue;
                if (key == L"position") position = value; else top = value;
            }
        };
        for (int priority = 0; priority != 2; ++priority)
        {
            if (g_cssFontRules) for (const auto& rule : *g_cssFontRules)
                if ((rule.hasVertical || !rule.positioningDeclarations.IsEmpty()) && MatchesCssRule(node, rule))
                {
                    if (rule.hasVertical) ApplyCssVertical(rule.declarations, vertical, priority != 0, specified);
                    applyPosition(rule.positioningDeclarations, priority != 0);
                }
            ApplyCssVertical(inlineStyle, vertical, priority != 0, specified);
            applyPosition(inlineStyle, priority != 0);
        }
        const CStringW nodeName = LocalName(node);
        if (specified && vertical == 0 && (nodeName == L"sub" || nodeName == L"sup") &&
            position == L"relative" && !top.IsEmpty() && top != L"auto")
        {
            wchar_t* end = nullptr;
            const double offset = wcstod(top.GetString(), &end);
            CStringW unit(end ? end : L""); unit.Trim();
            if (end != top.GetString() && (unit == L"em" || unit == L"ex" || unit == L"px" || unit == L"%") && offset != 0)
            {
                vertical = offset > 0 ? -1 : 1;
                specified = true;
            }
        }
        if (hasValue) *hasValue = specified;
        return vertical;
    }

    bool IsCssIdentifier(const CStringW& value)
    {
        if (value.IsEmpty()) return false;
        for (int i = 0; i < value.GetLength(); ++i)
        {
            const wchar_t c = value[i];
            if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
                (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')) return false;
        }
        return true;
    }

    CStringW SupportedCssFontDeclarations(CStringW declarations, CStringW& warning, bool* hasFont)
    {
        CStringW supported;
        if (hasFont) *hasFont = false;
        int position = 0;
        while (position != -1)
        {
            CStringW declaration = declarations.Tokenize(L";", position);
            const int colon = declaration.Find(L':');
            if (colon < 0) continue;
            CStringW key = declaration.Left(colon), value = declaration.Mid(colon + 1);
            key.Trim(); key.MakeLower(); value.Trim(); value.MakeLower();
            if (key != L"font-style" && key != L"font-weight" && key != L"font" && key != L"vertical-align") continue;
            if (hasFont) *hasFont = true;
            const bool important = ExtractCssImportant(value);
            if (key == L"vertical-align")
            {
                if (value == L"super" || value == L"sub" || value == L"baseline" || value == L"initial" || value == L"unset")
                    supported += key + L":" + value + (important ? L"!important;" : L";");
                else
                    warning += L"Unsupported CSS vertical-align declaration: " + value + L"\r\n";
                continue;
            }
            int weight = 0;
            const bool numericWeight = ParseCssNumericWeight(value, weight);
            const bool inheritedKeyword = value == L"inherit" || value == L"initial" || value == L"unset";
            const bool valid = (key == L"font-style" && (value == L"italic" || value == L"oblique" || value == L"normal" || inheritedKeyword)) ||
                (key == L"font-weight" && (value == L"normal" || value == L"bold" || numericWeight || inheritedKeyword));
            if (!valid) { warning += L"Unsupported CSS font declaration: " + key + L"\r\n"; continue; }
            supported += key + L":" + value + (important ? L"!important;" : L";");
        }
        return supported;
    }

    bool DecodeCssContentString(const CStringW& value, CStringW& text, CStringW& warning)
    {
        if (value.GetLength() < 2 || (value[0] != L'\'' && value[0] != L'"')) return false;
        const wchar_t quote = value[0];
        for (int i = 1; i < value.GetLength(); ++i)
        {
            wchar_t ch = value[i];
            if (ch == quote) return i == value.GetLength() - 1;
            if (ch == L'\r' || ch == L'\n' || ch == L'\f') return false;
            if (ch != L'\\') { text.AppendChar(ch); continue; }
            if (++i >= value.GetLength()) return false;
            ch = value[i];
            if (ch == L'\r' || ch == L'\n' || ch == L'\f')
            {
                if (ch == L'\r' && i + 1 < value.GetLength() && value[i + 1] == L'\n') ++i;
                continue;
            }
            unsigned long code = 0; int digits = 0;
            while (i < value.GetLength() && digits < 6)
            {
                ch = value[i];
                const int digit = ch >= L'0' && ch <= L'9' ? ch - L'0' :
                    ch >= L'a' && ch <= L'f' ? ch - L'a' + 10 :
                    ch >= L'A' && ch <= L'F' ? ch - L'A' + 10 : -1;
                if (digit < 0) break;
                code = code * 16 + static_cast<unsigned long>(digit); ++digits; ++i;
            }
            if (!digits) { text.AppendChar(ch); continue; }
            if (i < value.GetLength() && (value[i] == L' ' || value[i] == L'\t' || value[i] == L'\n' || value[i] == L'\r' || value[i] == L'\f'))
            {
                if (value[i] == L'\r' && i + 1 < value.GetLength() && value[i + 1] == L'\n') ++i;
            }
            else --i;
            if (!code || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
                (code < 0x20 && code != 0x09 && code != 0x0a && code != 0x0d) ||
                code == 0xfffe || code == 0xffff)
            {
                code = 0xfffd;
                warning += L"Invalid CSS generated character was replaced.\r\n";
            }
            if (code <= 0xffff) text.AppendChar(static_cast<wchar_t>(code));
            else { code -= 0x10000; text.AppendChar(static_cast<wchar_t>(0xd800 + (code >> 10))); text.AppendChar(static_cast<wchar_t>(0xdc00 + (code & 0x3ff))); }
        }
        return false;
    }

    void ParseCssGeneratedContent(const CStringW& declarations, bool& set, bool& important, CStringW& content, CStringW& warning)
    {
        int start = 0;
        while (start < declarations.GetLength())
        {
            int end = start; wchar_t quote = 0;
            for (; end < declarations.GetLength(); ++end)
            {
                const wchar_t c = declarations[end];
                if (quote && c == L'\\' && end + 1 < declarations.GetLength()) { ++end; continue; }
                if (quote && c == quote) quote = 0;
                else if (!quote && (c == L'\'' || c == L'"')) quote = c;
                else if (!quote && c == L';') break;
            }
            CStringW item = declarations.Mid(start, end - start); start = end + 1;
            const int colon = item.Find(L':'); if (colon < 0) continue;
            CStringW key = item.Left(colon); key.Trim(); key.MakeLower();
            if (key != L"content") continue;
            CStringW value = item.Mid(colon + 1); value.Trim();
            CStringW lowered(value); lowered.MakeLower();
            const bool priority = ExtractCssImportant(lowered);
            if (priority) { value = value.Left(value.ReverseFind(L'!')); value.Trim(); }
            if (set && important && !priority) continue;
            CStringW text;
            if (lowered != L"none" && lowered != L"normal")
            {
                if (!DecodeCssContentString(value, text, warning))
                {
                    warning += L"Unsupported CSS generated content; it was not imported.\r\n";
                    set = true; important = priority; content.Empty();
                    continue;
                }
            }
            set = true; important = priority; content = text;
        }
    }

    void ParseCssFontRules(CStringW css, const CStringW& baseDir, std::vector<CssFontRule>& rules, CStringW& warning, int nesting = 0)
    {
        wchar_t commentQuote = 0;
        for (int index = 0; index < css.GetLength(); ++index)
        {
            const wchar_t c = css[index];
            if (commentQuote)
            {
                if (c == L'\\' && index + 1 < css.GetLength()) ++index;
                else if (c == commentQuote) commentQuote = 0;
            }
            else if (c == L'\'' || c == L'"') commentQuote = c;
            else if (c == L'/' && index + 1 < css.GetLength() && css[index + 1] == L'*')
            {
                const int end = css.Find(L"*/", index + 2);
                if (end < 0) { warning += L"Unterminated CSS comment.\r\n"; css = css.Left(index); break; }
                css.Delete(index, end + 2 - index);
                css.Insert(index, L' ');
            }
        }
        int position = 0;
        while (position < css.GetLength())
        {
            while (position < css.GetLength() && (css[position] == L' ' || css[position] == L'\t' ||
                css[position] == L'\r' || css[position] == L'\n')) ++position;
            if (position >= css.GetLength()) break;
            if (css[position] == L'@')
            {
                int nameEnd = position + 1;
                while (nameEnd < css.GetLength() &&
                    ((css[nameEnd] >= L'a' && css[nameEnd] <= L'z') ||
                     (css[nameEnd] >= L'A' && css[nameEnd] <= L'Z') || css[nameEnd] == L'-')) ++nameEnd;
                CStringW ruleName = css.Mid(position, nameEnd - position);
                ruleName.MakeLower();
                // Only the exact media type "all" is unconditional. Other
                // conditions remain skipped and explicitly reported.
                int depth = 0;
                int blockOpen = -1;
                wchar_t quote = 0;
                bool finished = false;
                for (int i = position; i < css.GetLength(); ++i)
                {
                    const wchar_t c = css[i];
                    if (quote)
                    {
                        if (c == L'\\' && i + 1 < css.GetLength()) ++i;
                        else if (c == quote) quote = 0;
                        continue;
                    }
                    if (c == L'\'' || c == L'"') { quote = c; continue; }
                    if (c == L'{') { if (depth == 0) blockOpen = i; ++depth; }
                    else if (c == L'}' && depth > 0 && --depth == 0) { position = i + 1; finished = true; break; }
                    else if (c == L';' && depth == 0) { position = i + 1; finished = true; break; }
                }
                CStringW mediaCondition;
                if (blockOpen >= 0) { mediaCondition = css.Mid(nameEnd, blockOpen - nameEnd); mediaCondition.Trim(); }
                if (finished && ruleName == L"@media" && mediaCondition.CompareNoCase(L"all") == 0 && blockOpen >= 0 && nesting < 16)
                {
                    ParseCssFontRules(css.Mid(blockOpen + 1, position - blockOpen - 2), baseDir, rules, warning, nesting + 1);
                    continue;
                }
                if (g_content.warnedCssAtRules.emplace(ruleName.GetString()).second)
                {
                    if (ruleName == L"@page")
                        warning += L"Unsupported CSS at-rule @page: page layout settings cannot be represented in FB2 and were not applied.\r\n";
                    else
                        warning += L"Unsupported CSS at-rule " + ruleName + L"; its contents were not applied.\r\n";
                }
                if (!finished) break;
                continue;
            }
            const int open = css.Find(L'{', position);
            if (open < 0) break;
            int close = -1;
            wchar_t quote = 0;
            for (int index = open + 1; index < css.GetLength(); ++index)
            {
                const wchar_t c = css[index];
                if (quote)
                {
                    if (c == L'\\' && index + 1 < css.GetLength()) ++index;
                    else if (c == quote) quote = 0;
                }
                else if (c == L'\'' || c == L'"') quote = c;
                else if (c == L'}') { close = index; break; }
            }
            if (close < 0) { warning += L"Unsupported or malformed CSS font rule.\r\n"; break; }
            CStringW selectors = css.Mid(position, open - position);
            CStringW declarations = css.Mid(open + 1, close - open - 1);
            bool affectsFont = false;
            CStringW declarationWarning;
            const CStringW supported = SupportedCssFontDeclarations(declarations, declarationWarning, &affectsFont);
            warning += declarationWarning;
            bool backgroundSet = false, backgroundImportant = false;
            CStringW backgroundHref;
            ParseCssBackground(declarations, backgroundSet, backgroundImportant, backgroundHref);
            CssListImage listImage;
            ParseCssListImage(declarations, listImage);
            const bool affectsVisibility = HasCssVisibilityDeclaration(declarations);
            bool contentSet = false, contentImportant = false;
            CStringW generatedContent;
            ParseCssGeneratedContent(declarations, contentSet, contentImportant, generatedContent, warning);
            CStringW positionProbe(declarations); positionProbe.MakeLower();
            const bool affectsPosition = positionProbe.Find(L"position") >= 0 || positionProbe.Find(L"top") >= 0;
            if (affectsFont || backgroundSet || affectsVisibility || listImage.set || affectsPosition || contentSet)
            {
                int selectorPosition = 0;
                while (selectorPosition != -1)
                {
                    CStringW selector = selectors.Tokenize(L",", selectorPosition); selector.Trim();
                    if (selector.IsEmpty()) continue;
                    CssFontRule rule;
                    rule.unsupportedFont = !declarationWarning.IsEmpty();
                    const CStringW originalSelector(selector);
                    CStringW lowerSelector(selector); lowerSelector.MakeLower();
                    if (lowerSelector.Right(8) == L"::before") { rule.pseudo = 1; selector = selector.Left(selector.GetLength() - 8); }
                    else if (lowerSelector.Right(7) == L"::after") { rule.pseudo = 2; selector = selector.Left(selector.GetLength() - 7); }
                    selector.TrimRight();
                    bool valid = true;
                    int partPosition = 0;
                    while (partPosition != -1)
                    {
                        const CStringW token = selector.Tokenize(L" \t\r\n\f", partPosition);
                        if (token.IsEmpty()) continue;
                        CssSelectorPart part;
                        const int dot = token.Find(L'.');
                        part.element = dot >= 0 ? token.Left(dot) : token;
                        if (!part.element.IsEmpty() && !IsCssIdentifier(part.element)) { valid = false; break; }
                        int classPosition = dot;
                        while (classPosition >= 0)
                        {
                            const int next = token.Find(L'.', classPosition + 1);
                            const CStringW cssClass = next >= 0 ? token.Mid(classPosition + 1, next - classPosition - 1) : token.Mid(classPosition + 1);
                            if (!IsCssIdentifier(cssClass)) { valid = false; break; }
                            part.classes.push_back(cssClass);
                            classPosition = next;
                        }
                        if (!valid || (part.element.IsEmpty() && part.classes.empty())) { valid = false; break; }
                        rule.classSpecificity += static_cast<int>(part.classes.size());
                        if (!part.element.IsEmpty()) ++rule.elementSpecificity;
                        rule.parts.push_back(part);
                    }
                    valid = valid && !rule.parts.empty();
                    if (!valid) { warning += L"Unsupported CSS font selector or declaration: " + originalSelector + L"\r\n"; continue; }
                    rule.declarations = supported;
                    if (rule.pseudo)
                    {
                        rule.contentSet = contentSet; rule.contentImportant = contentImportant; rule.content = generatedContent;
                        if (affectsVisibility) rule.visibilityDeclarations = declarations;
                        if (contentSet || !supported.IsEmpty() || affectsVisibility || rule.unsupportedFont) rules.push_back(rule);
                        continue;
                    }
                    if (affectsPosition) rule.positioningDeclarations = declarations;
                    rule.hasVertical = supported.Find(L"vertical-align:") >= 0;
                    if (affectsVisibility) rule.visibilityDeclarations = declarations;
                    rule.backgroundSet = backgroundSet;
                    rule.backgroundImportant = backgroundImportant;
                    rule.backgroundHref = backgroundHref;
                    rule.backgroundDir = baseDir;
                    rule.listImageSet = listImage.set;
                    rule.listImageImportant = listImage.important;
                    rule.listImageInherit = listImage.inherit;
                    rule.listImageUnsupported = listImage.unsupported;
                    rule.listImageHref = listImage.href;
                    rule.listImageDir = baseDir;
                    if (!supported.IsEmpty() || backgroundSet || affectsVisibility || listImage.set || affectsPosition || rule.unsupportedFont) rules.push_back(rule);
                }
            }
            position = close + 1;
        }
    }

    class ScopedCssFontRules
    {
        const std::vector<CssFontRule>* previous_;
        bool previousHasVertical_;
        bool previousHasUnsupported_;
        std::vector<CssFontRule> rules_;
    public:
        ScopedCssFontRules(const MSXML2::IXMLDOMDocument2Ptr& doc, const CStringW& path,
            const ConvertContext& ctx, CStringW& warning) : previous_(g_cssFontRules), previousHasVertical_(g_cssHasVerticalRules), previousHasUnsupported_(g_cssHasUnsupportedRules)
        {
            const auto head = FindFirstElementByLocalName(MsxmlGetDocumentElement(doc), L"head");
            const auto children = MsxmlGetChildNodes(head);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i)
            {
                const auto child = MsxmlGetNodeListItem(children, i);
                const CStringW name = LocalName(child);
                CStringW media = Attr(child, L"media"); media.Trim();
                if ((name.CompareNoCase(L"style") == 0 || name.CompareNoCase(L"link") == 0) &&
                    !media.IsEmpty() && media.CompareNoCase(L"all") != 0)
                {
                    warning += L"Unsupported CSS media condition; stylesheet was not applied: " + media + L"\r\n";
                    continue;
                }
                if (name.CompareNoCase(L"style") == 0) ParseCssFontRules(MsxmlGetNodeText(child), DirectoryOf(path), rules_, warning);
                else if (name.CompareNoCase(L"link") == 0 && ContainsNoCase(Attr(child, L"rel"), L"stylesheet"))
                {
                    const CStringW href = Attr(child, L"href");
                    CStringW resource = href;
                    const int fragment = resource.Find(L'#');
                    if (fragment >= 0) resource = resource.Left(fragment);
                    const int query = resource.Find(L'?');
                    if (query >= 0) resource = resource.Left(query);
                    const CStringW absolute = CombineAndCanonicalizePath(DirectoryOf(path), resource);
                    const ManifestItem* stylesheet = nullptr;
                    if (ctx.package && !absolute.IsEmpty() && !IsExternalHref(href))
                        for (const auto& item : ctx.package->manifest)
                            if (item.second.absPath.CompareNoCase(absolute) == 0 && item.second.mediaType.CompareNoCase(L"text/css") == 0)
                            { stylesheet = &item.second; break; }
                    std::vector<unsigned char> bytes;
                    CStringW text;
                    if (!stylesheet || !ReadTextFileBytes(stylesheet->absPath, bytes) || !DecodeTextFileBytes(bytes, text))
                        warning += L"Stylesheet could not be loaded from the EPUB manifest: " + href + L"\r\n";
                    else ParseCssFontRules(text, DirectoryOf(stylesheet->absPath), rules_, warning);
                }
            }
            std::stable_sort(rules_.begin(), rules_.end(), [](const CssFontRule& a, const CssFontRule& b) {
                return a.classSpecificity != b.classSpecificity ? a.classSpecificity < b.classSpecificity : a.elementSpecificity < b.elementSpecificity;
            });
            g_cssFontRules = &rules_;
            g_cssHasVerticalRules = std::any_of(rules_.begin(), rules_.end(), [](const CssFontRule& rule) { return rule.hasVertical; });
            g_cssHasUnsupportedRules = std::any_of(rules_.begin(), rules_.end(), [](const CssFontRule& rule) { return rule.unsupportedFont; });
        }
        ~ScopedCssFontRules() { g_cssFontRules = previous_; g_cssHasVerticalRules = previousHasVertical_; g_cssHasUnsupportedRules = previousHasUnsupported_; }
    };

    bool CssComputedHidden(const MSXML2::IXMLDOMNodePtr& node)
    {
        bool displayHidden = false, visibilityHidden = false;
        const CStringW inlineStyle = Attr(node, L"style");
        for (int priority = 0; priority != 2; ++priority)
        {
            if (g_cssFontRules) for (const auto& rule : *g_cssFontRules)
            {
                if (!rule.visibilityDeclarations.IsEmpty() && MatchesCssRule(node, rule))
                    ApplyCssVisibility(rule.visibilityDeclarations, priority != 0, displayHidden, visibilityHidden);
            }
            ApplyCssVisibility(inlineStyle, priority != 0, displayHidden, visibilityHidden);
        }
        return displayHidden || visibilityHidden;
    }

    void ResolveCssBackground(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, CStringW& href, CStringW& directory)
    {
        bool inlineSet = false, inlineImportant = false;
        CStringW inlineHref;
        ParseCssBackground(Attr(node, L"style"), inlineSet, inlineImportant, inlineHref);
        href.Empty(); directory.Empty();
        for (int priority = 0; priority != 2; ++priority)
        {
            if (g_cssFontRules) for (const auto& rule : *g_cssFontRules)
            {
                if (rule.backgroundSet && rule.backgroundImportant == (priority != 0) && MatchesCssRule(node, rule))
                {
                    href = rule.backgroundHref;
                    directory = rule.backgroundDir;
                }
            }
            if (inlineSet && inlineImportant == (priority != 0))
            {
                href = inlineHref;
                directory = ctx.currentXhtmlDir;
            }
        }
    }

    CssListImage ResolveCssListImage(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> ancestors;
        for (auto current = node; current; )
        {
            ancestors.push_back(current);
            MSXML2::IXMLDOMNode* rawParent = nullptr;
            if (FAILED(current->get_parentNode(&rawParent))) break;
            current = MSXML2::IXMLDOMNodePtr(rawParent, false);
        }
        CssListImage inherited;
        for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it)
        {
            CssListImage inlineImage;
            ParseCssListImage(Attr(*it, L"style"), inlineImage);
            inlineImage.directory = ctx.currentXhtmlDir;
            CssListImage local;
            for (int priority = 0; priority != 2; ++priority)
            {
                if (g_cssFontRules) for (const auto& rule : *g_cssFontRules)
                {
                    if (!rule.listImageSet || rule.listImageImportant != (priority != 0) || !MatchesCssRule(*it, rule)) continue;
                    local.set = true; local.href = rule.listImageHref; local.directory = rule.listImageDir;
                    local.inherit = rule.listImageInherit; local.unsupported = rule.listImageUnsupported;
                }
                if (inlineImage.set && inlineImage.important == (priority != 0)) local = inlineImage;
            }
            if (local.set && !local.inherit) inherited = local;
        }
        return inherited;
    }

    CStringW CssListMarkerToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        if (!g_options.importLists) return CStringW();
        const CssListImage listImage = ResolveCssListImage(node, ctx);
        if (listImage.unsupported && g_content.warnedInlineCss.emplace(L"unsupported-css-list-image").second)
            g_content.warnings += L"Unsupported CSS list-style-image value; a text marker was used.\r\n";
        if (!g_options.importImages || listImage.href.IsEmpty()) return CStringW();
        const CStringW key = listImage.directory + L"/" + listImage.href;
        if (IsExternalHref(listImage.href))
        {
            if (g_content.warnedInlineCss.emplace(std::wstring(L"external-list-image:") + key.GetString()).second)
                g_content.warnings += L"External CSS list marker image was not imported: " + listImage.href + L"\r\n";
            return CStringW();
        }
        const std::wstring failureKey = std::wstring(L"failed-list-image:") + key.GetString();
        if (g_content.warnedInlineCss.count(failureKey)) return CStringW();
        ConvertContext imageContext = ctx;
        imageContext.currentXhtmlDir = listImage.directory;
        const CStringW marker = Fb2ImageElementForHref(imageContext, listImage.href);
        if (marker.IsEmpty()) g_content.warnedInlineCss.emplace(failureKey);
        return marker;
    }

    CStringW EmptyBackgroundImage(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, bool inlineElement)
    {
        if (!g_options.importImages || !node) return CStringW();
        const CStringW name = LocalName(node);
        if (inlineElement ? (name != L"span" && name != L"a") : (name != L"div" && name != L"section" && name != L"figure")) return CStringW();
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            if (MsxmlGetNodeType(MsxmlGetNodeListItem(children, i)) == MSXML2::NODE_ELEMENT)
                return CStringW();
        if (!TrimString(NodeText(node)).IsEmpty()) return CStringW();

        CStringW href, directory;
        ResolveCssBackground(node, ctx, href, directory);
        if (href.IsEmpty()) return CStringW();
        if (IsExternalHref(href))
        {
            g_content.warnings += L"External CSS background image was not imported.\r\n";
            return CStringW();
        }
        ConvertContext imageContext = ctx;
        imageContext.currentXhtmlDir = directory;
        return Fb2ImageElementForHref(imageContext, href);
    }

    InlineFontState InheritedInlineFont(const MSXML2::IXMLDOMNodePtr& node)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> ancestors;
        for (MSXML2::IXMLDOMNodePtr current = node; current; )
        {
            ancestors.push_back(current);
            MSXML2::IXMLDOMNode* parent = nullptr;
            current->get_parentNode(&parent);
            current = MSXML2::IXMLDOMNodePtr(parent, false);
        }
        InlineFontState font;
        for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) ApplyInlineFont(*it, font);
        return font;
    }

    thread_local unsigned int g_inlineLinkDepth = 0;
    struct InlineLinkScope
    {
        InlineLinkScope() { ++g_inlineLinkDepth; }
        ~InlineLinkScope() { --g_inlineLinkDepth; }
    };
    bool LooksLikePageBreak(const MSXML2::IXMLDOMNodePtr& node);
    bool PageBreakHasContent(const MSXML2::IXMLDOMNodePtr& node);
    bool IsInlineBoundarySpace(wchar_t ch)
    {
        return iswspace(ch) || ch == 0x00A0 || ch == 0x2007 || ch == 0x202F;
    }

    CStringW WrapInlinePreservingBoundarySpace(const CStringW& content, LPCWSTR tag)
    {
        int first = 0, last = content.GetLength();
        while (first < last && IsInlineBoundarySpace(content[first])) ++first;
        while (last > first && IsInlineBoundarySpace(content[last - 1])) --last;
        if (first == last) return content;
        return content.Left(first) + L"<" + tag + L">" + content.Mid(first, last - first) +
            L"</" + tag + L">" + content.Mid(last);
    }

    CStringW InlineToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, InlineFontState font);
    CStringW InlineToFb2Impl(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, InlineFontState font, bool applyVertical);
    void RecordUnsupportedInlineStyle(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        const CStringW style = Attr(node, L"style");
        CStringW warning;
        if (!style.IsEmpty()) SupportedCssFontDeclarations(style, warning);
        if (warning.IsEmpty() && g_cssHasUnsupportedRules && g_cssFontRules)
            for (const auto& rule : *g_cssFontRules)
                if (rule.unsupportedFont && MatchesCssRule(node, rule, rule.pseudo)) { warning = L"unsupported"; break; }
        if (!warning.IsEmpty()) RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
    }
    CStringW InlineToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        MSXML2::IXMLDOMNode* parent = nullptr;
        if (node) node->get_parentNode(&parent);
        return InlineToFb2(node, ctx, InheritedInlineFont(MSXML2::IXMLDOMNodePtr(parent, false)));
    }

    CStringW ChildrenInlineToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, InlineFontState font)
    {
        CStringW out;
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return out;

        const CStringW parentName = LocalName(node);
        const bool heading = parentName.GetLength() == 2 && parentName[0] == L'h' &&
            parentName[1] >= L'1' && parentName[1] <= L'6';
        bool afterBlock = false;
        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
        {
            const auto child = MsxmlGetNodeListItem(children, i);
            const CStringW part = InlineToFb2(child, ctx, font);
            const bool block = heading && LocalName(child) == L"div" && !part.IsEmpty();
            if (part.IsEmpty()) continue;
            if ((block || afterBlock) && !out.IsEmpty() && !iswspace(out[out.GetLength() - 1]) &&
                !iswspace(part[0])) out += L" ";
            out += part;
            afterBlock = block;
        }
        return out;
    }

    CStringW ChildrenInlineToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        return ChildrenInlineToFb2(node, ctx, InheritedInlineFont(node));
    }

    CStringW InlineToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, InlineFontState font)
    {
        CStringW result = InlineToFb2Impl(node, ctx, font, true);
        if (!result.IsEmpty() && node && MsxmlGetNodeType(node) == MSXML2::NODE_ELEMENT)
            RecordUnsupportedInlineStyle(node, ctx);
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT || !g_cssFontRules)
            return result;
        if (result.IsEmpty() && (LocalName(node) != L"span" || IsHiddenElement(node) ||
            (ctx.inlinePageBreaks && LooksLikePageBreak(node) && !PageBreakHasContent(node)) ||
            (g_options.importNotes && !ctx.collectingNotes &&
             g_content.noteTargets.count(std::wstring(PathAnchorMapKey(ctx.currentXhtmlPath, Attr(node, L"id")))))))
            return result;
        ApplyInlineFont(node, font);
        const auto generated = [&](int pseudo) {
            CStringW value;
            InlineFontState generatedFont = font;
            bool displayHidden = false, visibilityHidden = false;
            for (int priority = 0; priority != 2; ++priority)
                for (const auto& rule : *g_cssFontRules)
                    if (MatchesCssRule(node, rule, pseudo))
                    {
                        if (rule.contentSet && rule.contentImportant == (priority != 0)) value = rule.content;
                        ApplyFontDeclarations(rule.declarations, generatedFont, font, priority != 0);
                        ApplyCssVisibility(rule.visibilityDeclarations, priority != 0, displayHidden, visibilityHidden);
                    }
            if (displayHidden || visibilityHidden) return CStringW();
            value = XmlEscape(NormalizeImportedText(value));
            if (generatedFont.italic) value = WrapInlinePreservingBoundarySpace(value, L"emphasis");
            if (generatedFont.bold) value = WrapInlinePreservingBoundarySpace(value, L"strong");
            return value;
        };
        const CStringW before = generated(1), after = generated(2);
        if (LocalName(node) == L"a" && result.Left(3) == L"<a " && result.Right(4) == L"</a>" &&
            result.Find(L"</a>") == result.GetLength() - 4)
        {
            const int start = result.Find(L'>');
            if (start >= 0)
                return result.Left(start + 1) + before + result.Mid(start + 1, result.GetLength() - start - 5) + after + L"</a>";
        }
        return before + result + after;
    }

    CStringW InlineToFb2Impl(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, InlineFontState font, bool applyVertical)
    {
        if (!node)
            return CStringW();

        MSXML2::DOMNodeType type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION)
        {
            CStringW text = XmlEscape(NormalizeImportedText(MsxmlGetNodeText(node)));
            if (text.IsEmpty()) return text;
            if (font.italic) text = WrapInlinePreservingBoundarySpace(text, L"emphasis");
            if (font.bold) text = WrapInlinePreservingBoundarySpace(text, L"strong");
            return text;
        }

        if (type != MSXML2::NODE_ELEMENT)
            return CStringW();

        if (IsHiddenElement(node))
            return CStringW();

        if (ctx.inlinePageBreaks && LooksLikePageBreak(node) && !PageBreakHasContent(node))
        {
            if (!g_options.importPageBreaks) return CStringW();
            CStringW label = NormalizeImportedText(Attr(node, L"title"));
            if (label.IsEmpty()) label = NormalizeImportedText(Attr(node, L"aria-label"));
            if (label.IsEmpty()) label = NodeText(node);
            label.Trim();
            return label.IsEmpty() ? CStringW() : L"<sub>" + XmlEscape(label) + L"</sub>";
        }

        if (g_options.importNotes && !ctx.collectingNotes &&
            g_content.noteTargets.count(std::wstring(PathAnchorMapKey(ctx.currentXhtmlPath, Attr(node, L"id")))))
            return CStringW();

        CStringW name = LocalName(node);
        if (g_options.skipHiddenElements && IsInteractiveTermDefinition(node) && CssComputedHidden(node))
        {
            ApplyInlineFont(node, font);
            CStringW content = ChildrenInlineToFb2(node, ctx, font);
            content.Trim();
            if (content.IsEmpty()) return CStringW();
            if (g_content.warnedInlineCss.emplace(L"interactive-term-definitions").second)
                g_content.warnings += L"Interactive term definitions were retained inline because FB2 cannot display the original popup.\r\n";
            return L" (" + content + L")";
        }
        if (applyVertical && name.CompareNoCase(L"sup") != 0 && name.CompareNoCase(L"sub") != 0)
        {
            const int vertical = CssVerticalForNode(node);
            if (vertical != 0)
            {
                CStringW content = InlineToFb2Impl(node, ctx, font, false);
                return WrapInlinePreservingBoundarySpace(content, vertical > 0 ? L"sup" : L"sub");
            }
        }
        if (name == L"span")
        {
            const CStringW backgroundImage = EmptyBackgroundImage(node, ctx, true);
            if (!backgroundImage.IsEmpty()) return backgroundImage;
        }
        if (g_options.importImages)
        {
            CStringW backgroundHref, backgroundDirectory;
            ResolveCssBackground(node, ctx, backgroundHref, backgroundDirectory);
            const CStringW warningKey = L"background:" + backgroundDirectory + L"/" + backgroundHref;
            if (!backgroundHref.IsEmpty() && !TrimString(NodeText(node)).IsEmpty() &&
                g_content.warnedInlineCss.emplace(warningKey.GetString()).second)
                g_content.warnings += L"CSS background image on non-empty inline content was not imported.\r\n";
        }
        ApplyInlineFont(node, font);
        if (name == L"msub" || name == L"msup" || name == L"msubsup" || name == L"mfrac" || name == L"msqrt" ||
            name == L"munder" || name == L"mover" || name == L"munderover" || name == L"mtable" ||
            name == L"mroot" || name == L"semantics")
        {
            CComBSTR uri;
            if (SUCCEEDED(node->get_namespaceURI(&uri)) && uri &&
                CStringW(uri) == L"http://www.w3.org/1998/Math/MathML")
            {
                std::vector<MSXML2::IXMLDOMNodePtr> operands;
                const auto children = MsxmlGetChildNodes(node);
                for (long i = 0, count = children ? MsxmlGetNodeListLength(children) : 0; i < count; ++i)
                {
                    const auto child = MsxmlGetNodeListItem(children, i);
                    if (MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT) operands.push_back(child);
                }
                if (name == L"semantics")
                {
                    for (const auto& operand : operands)
                    {
                        const CStringW operandName = LocalName(operand);
                        if (operandName != L"annotation" && operandName != L"annotation-xml")
                            return InlineToFb2(operand, ctx, font);
                    }
                }
                if (name == L"mroot" && operands.size() == 2)
                {
                    RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
                    if (g_content.warnedInlineCss.emplace(L"mathml-root-approximation").second)
                        g_content.warnings += L"MathML indexed root was approximated as text.\r\n";
                    return CStringW(L"root(") + InlineToFb2(operands[1], ctx, font) + L", " +
                        InlineToFb2(operands[0], ctx, font) + L")";
                }
                if (name == L"mtable")
                {
                    CStringW table;
                    for (const auto& row : operands)
                    {
                        if (LocalName(row) != L"mtr" && LocalName(row) != L"mlabeledtr") continue;
                        CStringW cells;
                        bool hasCell = false;
                        const auto rowChildren = MsxmlGetChildNodes(row);
                        for (long i = 0, count = rowChildren ? MsxmlGetNodeListLength(rowChildren) : 0; i < count; ++i)
                        {
                            const auto cell = MsxmlGetNodeListItem(rowChildren, i);
                            if (LocalName(cell) != L"mtd") continue;
                            CStringW text = ChildrenInlineToFb2(cell, ctx, font); text.Trim();
                            if (hasCell) cells += L" | ";
                            cells += text;
                            hasCell = true;
                        }
                        if (!hasCell) continue;
                        if (!table.IsEmpty()) table += L"; ";
                        table += cells;
                    }
                    if (!table.IsEmpty())
                    {
                        RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
                        if (g_content.warnedInlineCss.emplace(L"mathml-table-approximation").second)
                            g_content.warnings += L"MathML table layout was approximated as text.\r\n";
                        return table;
                    }
                }
                if ((name == L"munder" || name == L"mover" || name == L"munderover") &&
                    operands.size() == (name == L"munderover" ? 3u : 2u))
                {
                    const CStringW base = InlineToFb2(operands[0], ctx, font);
                    const CStringW under = InlineToFb2(operands[1], ctx, font);
                    CStringW baseText = TrimString(NodeText(operands[0]));
                    const bool largeOperator = baseText == L"∑" || baseText == L"∏" ||
                        baseText == L"∫" || baseText == L"∮";
                    if (largeOperator)
                    {
                        CStringW result = base + WrapInlinePreservingBoundarySpace(under, name == L"mover" ? L"sup" : L"sub");
                        if (name == L"munderover")
                            result += WrapInlinePreservingBoundarySpace(InlineToFb2(operands[2], ctx, font), L"sup");
                        return result;
                    }
                    RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
                    if (g_content.warnedInlineCss.emplace(L"mathml-over-under-approximation").second)
                        g_content.warnings += L"MathML over/under notation was approximated as text.\r\n";
                    if (name == L"mover" && (TrimString(NodeText(operands[1])) == L"¯" ||
                        TrimString(NodeText(operands[1])) == L"‾")) return CStringW(L"overline(") + base + L")";
                    if (name == L"munderover")
                        return CStringW(L"underover(") + base + L", " + under + L", " +
                            InlineToFb2(operands[2], ctx, font) + L")";
                    return CStringW(name == L"mover" ? L"overset(" : L"underset(") + under + L", " + base + L")";
                }
                if (name == L"msqrt")
                {
                    CStringW content = ChildrenInlineToFb2(node, ctx, font);
                    content.Trim();
                    if (!content.IsEmpty()) return CStringW(L"\x221A(") + content + L")";
                }
                const size_t expected = name == L"msubsup" ? 3 : 2;
                if (operands.size() == expected)
                {
                    if (name == L"mfrac")
                    {
                        CStringW thickness = Attr(node, L"linethickness"); thickness.Trim();
                        wchar_t* end = nullptr;
                        const double lineWidth = wcstod(thickness.GetString(), &end);
                        if (!thickness.IsEmpty() && end != thickness.GetString() && lineWidth == 0.0)
                        {
                            RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
                            if (g_content.warnedInlineCss.emplace(L"mathml-barless-stack").second)
                                g_content.warnings += L"MathML stack without a fraction bar was approximated as text.\r\n";
                            return CStringW(L"stack(") + InlineToFb2(operands[0], ctx, font) + L", " +
                                InlineToFb2(operands[1], ctx, font) + L")";
                        }
                        return CStringW(L"(") + InlineToFb2(operands[0], ctx, font) + L")/(" +
                            InlineToFb2(operands[1], ctx, font) + L")";
                    }
                    CStringW result = InlineToFb2(operands[0], ctx, font);
                    result += WrapInlinePreservingBoundarySpace(InlineToFb2(operands[1], ctx, font),
                        name == L"msup" ? L"sup" : L"sub");
                    if (name == L"msubsup")
                        result += WrapInlinePreservingBoundarySpace(InlineToFb2(operands[2], ctx, font), L"sup");
                    return result;
                }
            }
        }
        if (name.CompareNoCase(L"li") == 0)
        {
            // Nested list entries rendered inline are separate blocks in the
            // source. Without a boundary adjacent link labels merge (83 + 105
            // becomes 83105), changing the book's text and index references.
            CStringW text = ChildrenInlineToFb2(node, ctx, font);
            if (text.IsEmpty()) return CStringW();
            const CStringW marker = CssListMarkerToFb2(node, ctx);
            return marker.IsEmpty() ? L" " + text : L" " + marker + L" " + text;
        }
        if (name.CompareNoCase(L"em") == 0 || name.CompareNoCase(L"i") == 0 || name.CompareNoCase(L"strong") == 0 || name.CompareNoCase(L"b") == 0)
            return ChildrenInlineToFb2(node, ctx, font);
        if (name.CompareNoCase(L"sup") == 0 || name.CompareNoCase(L"sub") == 0)
        {
            bool specified = false;
            const int cssVertical = CssVerticalForNode(node, &specified);
            const int vertical = specified ? cssVertical : (name.CompareNoCase(L"sup") == 0 ? 1 : -1);
            const CStringW content = ChildrenInlineToFb2(node, ctx, font);
            if (vertical == 0) return content;
            return WrapInlinePreservingBoundarySpace(content, vertical > 0 ? L"sup" : L"sub");
        }
        if (name.CompareNoCase(L"code") == 0 || name.CompareNoCase(L"kbd") == 0 || name.CompareNoCase(L"samp") == 0 || name.CompareNoCase(L"var") == 0)
        {
            const CStringW content = ChildrenInlineToFb2(node, ctx, font);
            // The editor discards whitespace-only inline elements on save.
            // Keep an unformatted separator so adjacent words do not merge.
            return WrapInlinePreservingBoundarySpace(content, L"code");
        }
        if (name.CompareNoCase(L"del") == 0 || name.CompareNoCase(L"s") == 0 || name.CompareNoCase(L"strike") == 0)
            return WrapInlinePreservingBoundarySpace(ChildrenInlineToFb2(node, ctx, font), L"strikethrough");
        if (name.CompareNoCase(L"q") == 0)
            return L"«" + ChildrenInlineToFb2(node, ctx, font) + L"»";
        if (name.CompareNoCase(L"abbr") == 0 || name.CompareNoCase(L"acronym") == 0)
        {
            CStringW text = ChildrenInlineToFb2(node, ctx, font);
            CStringW title = NormalizeImportedText(Attr(node, L"title"));
            title.Trim();
            if (!title.IsEmpty())
                return text + L" (" + XmlEscape(title) + L")";
            return text;
        }
        if (name.CompareNoCase(L"rt") == 0)
        {
            CStringW text = ChildrenInlineToFb2(node, ctx, font);
            text.Trim();
            return text.IsEmpty() ? CStringW() : L" (" + text + L")";
        }
        if (name.CompareNoCase(L"rp") == 0)
            return CStringW();
        if (name.CompareNoCase(L"br") == 0)
            return ctx.inlineLineBreaks ? L"<!--fbe-dedication-line-break-->" : L" ";
        if (name.CompareNoCase(L"a") == 0)
        {
            RecordBacklink(node, ctx);
            if (IsBacklinkElement(node))
                return CStringW();

            if (g_inlineLinkDepth != 0)
            {
                RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
                g_content.warnings += L"Nested hyperlink was flattened because FB2 cannot nest links.\r\n";
                return ChildrenInlineToFb2(node, ctx, font);
            }
            CStringW href = Attr(node, L"href");
            // HTML can contain nested anchors, but FB2 cannot. Split direct
            // nested links into adjacent links, keeping the outer destination
            // on the text on either side and the inner destination on its text.
            const auto linkChildren = MsxmlGetChildNodes(node);
            const long linkChildCount = linkChildren ? MsxmlGetNodeListLength(linkChildren) : 0;
            bool hasDirectNestedLink = false;
            for (long i = 0; i < linkChildCount; ++i)
            {
                const auto child = MsxmlGetNodeListItem(linkChildren, i);
                if (LocalName(child) == L"a" && !Attr(child, L"href").IsEmpty())
                {
                    hasDirectNestedLink = true;
                    break;
                }
            }
            if (hasDirectNestedLink && !IsNoteReferenceElement(node))
            {
                const int hash = href.Find(L'#');
                const CStringW targetPath = hash == 0 ? ctx.currentXhtmlPath :
                    CombineAndCanonicalizePath(ctx.currentXhtmlDir, hash < 0 ? href : href.Left(hash));
                const CStringW noteKey = IsExternalHref(href) ? CStringW() : PathAnchorMapKey(targetPath, HrefFragment(href));
                const auto note = g_content.noteLinks.find(std::wstring(noteKey));
                const bool excluded = !IsExternalHref(href) && !href.Left(hash >= 0 ? hash : href.GetLength()).IsEmpty() &&
                    g_content.excludedServiceDocuments.count(LowerMapKey(targetPath).GetString());
                if (excluded) ++g_content.skippedServiceLinks;
                const CStringW normalizedHref = href.IsEmpty() ? CStringW() :
                    (g_options.importNotes && note != g_content.noteLinks.end()
                        ? CStringW(L"#") + note->second : ResolveFb2Href(ctx, href));
                if (g_options.preserveLinks && !excluded) RecordMappedLink(node, ctx, href, normalizedHref);
                CStringW result, pending;
                bool outerLinkEmitted = false;
                auto flush = [&]() {
                    if (pending.IsEmpty()) return;
                    if (!normalizedHref.IsEmpty() && g_options.preserveLinks && !excluded && !TrimString(pending).IsEmpty())
                    {
                        result += L"<a l:href=\"" + XmlEscape(normalizedHref) + L"\">" + pending + L"</a>";
                        outerLinkEmitted = true;
                    }
                    else
                        result += pending;
                    pending.Empty();
                };
                for (long i = 0; i < linkChildCount; ++i)
                {
                    const auto child = MsxmlGetNodeListItem(linkChildren, i);
                    if (LocalName(child) == L"a" && !Attr(child, L"href").IsEmpty())
                    {
                        flush();
                        result += InlineToFb2(child, ctx, font);
                    }
                    else
                    {
                        InlineLinkScope linkScope;
                        pending += InlineToFb2(child, ctx, font);
                    }
                }
                flush();
                if (!outerLinkEmitted && !normalizedHref.IsEmpty() && g_options.preserveLinks && !excluded)
                    g_content.warnings += L"Outer hyperlink had no separate text and was omitted while retaining nested links.\r\n";
                return result;
            }
            InlineLinkScope linkScope;
            CStringW text = ChildrenInlineToFb2(node, ctx, font);
            CStringW visibleText = text;
            visibleText.Trim();
            if (visibleText.IsEmpty())
            {
                const CStringW backgroundImage = EmptyBackgroundImage(node, ctx, true);
                if (!backgroundImage.IsEmpty()) text = backgroundImage;
            }
            visibleText = text;
            visibleText.Trim();
            if (visibleText.IsEmpty())
                text = XmlEscape(NormalizeImportedText(Attr(node, L"title")));
            if (text.IsEmpty())
                text = XmlEscape(NormalizeImportedText(Attr(node, L"aria-label")));
            const int hash = href.Find(L'#');
            const CStringW targetPath = hash == 0 ? ctx.currentXhtmlPath :
                CombineAndCanonicalizePath(ctx.currentXhtmlDir, hash < 0 ? href : href.Left(hash));
            const CStringW noteKey = IsExternalHref(href) ? CStringW() : PathAnchorMapKey(targetPath, HrefFragment(href));
            const auto note = g_content.noteLinks.find(std::wstring(noteKey));
            CStringW marker = TrimString(NodeText(node));
            marker.Trim(L"[]() .");
            bool number = !marker.IsEmpty() && marker.GetLength() <= 8;
            for (int i = 0; number && i < marker.GetLength(); ++i) number = iswdigit(marker[i]) != 0;
            bool symbol = !marker.IsEmpty() && marker.GetLength() <= 3;
            for (int i = 0; symbol && i < marker.GetLength(); ++i)
                symbol = marker[i] == L'*' || marker[i] == 0x2020 || marker[i] == 0x2021 || marker[i] == 0x00A7;
            const bool letter = marker.GetLength() == 1 && iswalpha(marker[0]) != 0;
            const bool targetsNavigation = g_content.navigationTargets.count(std::wstring(noteKey)) != 0;
            if (g_options.importNotes && !targetsNavigation && (IsNoteReferenceElement(node) ||
                ((note != g_content.noteLinks.end() || g_content.requestedNotes.count(std::wstring(noteKey))) && (number || symbol || letter))))
            {
                ++g_content.noteReferences;
                if (text.IsEmpty()) text = L"*";
                if (note != g_content.noteLinks.end())
                {
                    ++g_content.importedNoteReferences;
                    RecordMappedLink(node, ctx, href, CStringW(L"#") + note->second);
                    return L"<a l:href=\"#" + XmlEscape(note->second) + L"\" type=\"note\">" + text + L"</a>";
                }
                ++g_content.lostNoteReferences;
                g_content.warnings += L"Unresolved note reference: " + ctx.currentXhtmlPath + L" -> " + href + L"\r\n";
                return L"<sup>" + text + L"</sup>";
            }

            if (!href.IsEmpty() && g_options.preserveLinks)
            {
                // An ordinary link can point to an anchor collected inside a
                // note whose FB2 section uses a different root identifier.
                if (g_options.importNotes && note != g_content.noteLinks.end())
                {
                    RecordMappedLink(node, ctx, href, CStringW(L"#") + note->second);
                    return L"<a l:href=\"#" + XmlEscape(note->second) + L"\">" + text + L"</a>";
                }
                // Missing source fragments cannot become valid FB2 targets;
                // preserve their visible content and report the source defect.
                if (!IsExternalHref(href) && !href.Left(href.Find(L'#') >= 0 ? href.Find(L'#') : href.GetLength()).IsEmpty() &&
                    g_content.excludedServiceDocuments.count(LowerMapKey(targetPath).GetString()))
                {
                    ++g_content.skippedServiceLinks;
                    return text;
                }
                CStringW normalizedHref = ResolveFb2Href(ctx, href);
                if (normalizedHref.IsEmpty()) return text;
                RecordMappedLink(node, ctx, href, normalizedHref);
                return L"<a l:href=\"" + XmlEscape(normalizedHref) + L"\">" + text + L"</a>";
            }
            return text;
        }
        if (name == L"video")
        {
            RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
            g_content.warnings += L"EPUB video is not supported; its poster and fallback content were retained when available.\r\n";
            return Fb2ImageElementForNode(ctx, node) + ChildrenInlineToFb2(node, ctx, font);
        }
        if (name == L"img" || name == L"image" || name == L"picture" || name == L"object")
        {
            CStringW src = ImageHrefFromNode(node);
            CStringW image = Fb2ImageElementForNode(ctx, node);
            if (!image.IsEmpty())
                return image;

            CStringW alt = NormalizeImportedText(Attr(node, L"alt"));
            if (alt.IsEmpty())
                alt = src;
            if (!alt.IsEmpty())
                return LoadImportRuntimeString(IDS_IMPORT_RUNTIME_IMAGE_PLACEHOLDER_OPEN, L"[Image: ") + XmlEscape(alt) + L"]";
            return LoadImportRuntimeString(IDS_IMPORT_RUNTIME_IMAGE_PLACEHOLDER, L"[Image]");
        }

        return ChildrenInlineToFb2(node, ctx, font);
    }

    bool IsHeadingName(const CStringW& name)
    {
        return name.GetLength() == 2 && (name[0] == L'h' || name[0] == L'H') && name[1] >= L'1' && name[1] <= L'6';
    }

    void AppendParagraphIfNotEmpty(CStringW& out, const CStringW& content)
    {
        CStringW tmp(content);
        tmp.Trim();
        if (!tmp.IsEmpty())
            out += L"      <p>" + tmp + L"</p>\r\n";
    }

    CStringW RawAnchorId(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW id = Attr(node, L"id");
        if (id.IsEmpty())
            id = Attr(node, L"name");
        id.Trim();
        return id;
    }

    MSXML2::IXMLDOMNodePtr ParentNode(const MSXML2::IXMLDOMNodePtr& node)
    {
        IXMLDOMNode* parent = nullptr;
        if (node) node->get_parentNode(&parent);
        return MSXML2::IXMLDOMNodePtr(parent, false);
    }

    // Only this scope holds DOM identities. The owner outlives the index and no
    // pointer is retained in ContentInventory after a document pass ends.
    struct SourceNoteIndex;
    thread_local SourceNoteIndex* g_noteIndex = nullptr;
    struct SourceNoteIndex
    {
        MSXML2::IXMLDOMDocument2Ptr owner;
        std::unordered_map<IUnknown*, unsigned> elements;
        std::vector<CComPtr<IUnknown>> identities;
        SourceNoteIndex* previous;
        bool indexed = false;

        explicit SourceNoteIndex(const MSXML2::IXMLDOMDocument2Ptr& doc) : owner(doc), previous(g_noteIndex) {g_noteIndex = this;}
        ~SourceNoteIndex() {g_noteIndex = previous;}
        SourceNoteIndex(const SourceNoteIndex&) = delete;
        SourceNoteIndex& operator=(const SourceNoteIndex&) = delete;

        void Index(const MSXML2::IXMLDOMNodePtr& node)
        {
            if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT) return;
            const unsigned ordinal = static_cast<unsigned>(elements.size());
            CComPtr<IUnknown> identity;
            CheckError(node->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity.p)));
            elements.emplace(identity.p, ordinal);
            identities.push_back(identity.p);
            ++g_content.indexedElements;
            const auto children = MsxmlGetChildNodes(node);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i) Index(MsxmlGetNodeListItem(children, i));
        }
    };

    // Preorder element ordinals are stable across reparses and ignore whitespace.
    // One forward DOM walk per pass; subsequent identity lookups are O(1).
    CStringW NoteRootKey(const MSXML2::IXMLDOMNodePtr& node, const CStringW& path)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT) return CStringW();
        if (!g_noteIndex) _com_issue_error(E_UNEXPECTED);
        if (!g_noteIndex->indexed)
        {
            g_noteIndex->Index(MsxmlGetDocumentElement(g_noteIndex->owner));
            g_noteIndex->indexed = true;
        }
        ++g_content.structuralLookups;
        CComPtr<IUnknown> identity;
        CheckError(node->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity.p)));
        const auto found = g_noteIndex->elements.find(identity.p);
        if (found == g_noteIndex->elements.end()) _com_issue_error(E_UNEXPECTED);
        CStringW key = path;key.MakeLower();
        CStringW ordinal;ordinal.Format(L"/e%u", found->second);
        return key + ordinal;
    }

    CStringW NoteAnchorId(const MSXML2::IXMLDOMNodePtr& node, const CStringW& path)
    {
        const auto found = g_content.noteRoots.find(std::wstring(NoteRootKey(node, path)));
        return found == g_content.noteRoots.end() ? RawAnchorId(node) : found->second;
    }

    bool IsCollectedNoteNode(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        if (g_content.noteRoots.empty()) return false;
        CStringW path = ctx.currentXhtmlPath;path.MakeLower();
        if (!g_content.noteRootDocuments.count(std::wstring(path))) return false;
        return g_content.noteRoots.count(std::wstring(NoteRootKey(node, ctx.currentXhtmlPath))) != 0;
    }

    CStringW FirstDescendantAnchorId(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node)
            return CStringW();

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (!child || MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT)
                continue;

            CStringW id = RawAnchorId(child);
            if (!id.IsEmpty())
                return id;

            id = FirstDescendantAnchorId(child);
            if (!id.IsEmpty())
                return id;
        }
        return CStringW();
    }

    CStringW LowerMapKey(CStringW value)
    {
        value.Trim();
        value.MakeLower();
        return value;
    }

    CStringW NormalizedAnchorMapKey(const CStringW& rawId)
    {
        return LowerMapKey(NormalizeFb2Id(rawId));
    }

    CStringW PathAnchorMapKey(const CStringW& absPath, const CStringW& rawId)
    {
        CStringW idKey(rawId);
        if (absPath.IsEmpty() || idKey.IsEmpty())
            return CStringW();
        return LowerMapKey(absPath) + L"#" + idKey;
    }

    void AddAnchorIdMap(const ConvertContext& ctx, const CStringW& rawId, const CStringW& fb2Id)
    {
        if (!ctx.anchorIdMap || rawId.IsEmpty() || fb2Id.IsEmpty())
            return;

        CStringW rawKey = NormalizedAnchorMapKey(rawId);
        if (!rawKey.IsEmpty() && ctx.anchorIdMap->find(std::wstring(rawKey.GetString())) == ctx.anchorIdMap->end())
            (*ctx.anchorIdMap)[std::wstring(rawKey.GetString())] = std::wstring(fb2Id.GetString());

        CStringW pathKey = PathAnchorMapKey(ctx.currentXhtmlPath, rawId);
        if (!pathKey.IsEmpty() && ctx.anchorIdMap->find(std::wstring(pathKey.GetString())) == ctx.anchorIdMap->end())
            (*ctx.anchorIdMap)[std::wstring(pathKey.GetString())] = std::wstring(fb2Id.GetString());
    }

    void AddDocumentHrefMap(const ConvertContext& ctx, const CStringW& fb2Id)
    {
        if (!ctx.anchorIdMap || ctx.currentXhtmlPath.IsEmpty() || fb2Id.IsEmpty())
            return;

        CStringW key = LowerMapKey(ctx.currentXhtmlPath);
        if (!key.IsEmpty() && ctx.anchorIdMap->find(std::wstring(key.GetString())) == ctx.anchorIdMap->end())
            (*ctx.anchorIdMap)[std::wstring(key.GetString())] = std::wstring(fb2Id.GetString());
    }

    CStringW IdFromAttributeText(const CStringW& idAttr)
    {
        int q1 = idAttr.Find(L"\"");
        int q2 = q1 >= 0 ? idAttr.Find(L"\"", q1 + 1) : -1;
        if (q1 >= 0 && q2 > q1)
            return idAttr.Mid(q1 + 1, q2 - q1 - 1);
        return CStringW();
    }

    bool IsExternalHref(CStringW href)
    {
        href.Trim();
        href.MakeLower();
        return href.Find(L"://") >= 0 || href.Left(7) == L"mailto:" || href.Left(4) == L"tel:" ||
            href.Left(5) == L"data:" || href.Left(11) == L"javascript:";
    }

    bool IsProbablyXhtmlHref(CStringW href)
    {
        int hash = href.Find(L'#');
        if (hash >= 0)
            href = href.Left(hash);
        href.Trim();
        href.MakeLower();
        return href.Right(6) == L".xhtml" || href.Right(5) == L".html" || href.Right(4) == L".htm";
    }

    CStringW RegisterFb2AnchorId(const ConvertContext& ctx, const CStringW& rawId)
    {
        if (!g_options.preserveLinks)
            return CStringW();

        CStringW raw(rawId);
        raw.Trim();
        if (raw.IsEmpty())
            return CStringW();

        const CStringW key = PathAnchorMapKey(ctx.currentXhtmlPath, raw);
        if (ctx.anchorIdMap)
        {
            const auto existing = ctx.anchorIdMap->find(std::wstring(key));
            if (existing != ctx.anchorIdMap->end())
                return g_content.emittedIds.emplace(existing->second).second ? CStringW(existing->second.c_str()) : CStringW();
        }

        CStringW id = NormalizeFb2Id(raw);
        id.Trim();
        if (id.IsEmpty())
            return CStringW();

        id = UniqueContentId(id);
        AddAnchorIdMap(ctx, rawId, id);
        g_content.emittedIds.emplace(id.GetString());
        return id;
    }

    CStringW ResolveFb2HrefFragment(const ConvertContext& ctx, const CStringW& rawFragment)
    {
        CStringW id = NormalizeFb2Id(rawFragment);
        id.Trim();
        if (id.IsEmpty())
            return CStringW();

        if (ctx.anchorIdMap)
        {
            CStringW pathKey = PathAnchorMapKey(ctx.currentXhtmlPath, rawFragment);
            if (!pathKey.IsEmpty())
            {
                std::map<std::wstring, std::wstring>::const_iterator it = ctx.anchorIdMap->find(std::wstring(pathKey.GetString()));
                if (it != ctx.anchorIdMap->end())
                    return CStringW(it->second.c_str());
            }

        }

        ++g_content.missingSourceAnchors;
        g_content.warnings += L"Unresolved internal fragment: " + ctx.currentXhtmlPath + L"#" + rawFragment + L"\r\n";
        return CStringW();
    }

    CStringW ResolveFb2Href(const ConvertContext& ctx, const CStringW& rawHref)
    {
        CStringW href(rawHref);
        href.Trim();
        if (href.IsEmpty() || IsExternalHref(href))
            return href;

        int hash = href.Find(L'#');
        CStringW pathPart = hash >= 0 ? href.Left(hash) : href;
        CStringW fragment = hash >= 0 ? UrlDecodeUtf8(href.Mid(hash + 1)) : CStringW();
        pathPart.Trim();
        fragment.Trim();

        if (hash >= 0)
        {
            if (!fragment.IsEmpty())
            {
                if (ctx.anchorIdMap && !pathPart.IsEmpty())
                {
                    CStringW abs = CombineAndCanonicalizePath(ctx.currentXhtmlDir, pathPart);
                    CStringW pathKey = PathAnchorMapKey(abs, fragment);
                    std::map<std::wstring, std::wstring>::const_iterator it = ctx.anchorIdMap->find(std::wstring(pathKey.GetString()));
                    if (it != ctx.anchorIdMap->end())
                    {
                        if (g_content.mainDocuments.count(LowerMapKey(abs).GetString()) ||
                            g_content.noteContentAnchors.count(pathKey.GetString())) return L"#" + CStringW(it->second.c_str());
                        g_content.warnings += L"Internal link target outside imported content: " + href + L"\r\n";
                        return href;
                    }
                }
                if (pathPart.IsEmpty())
                {
                    const CStringW target = ResolveFb2HrefFragment(ctx, fragment);
                    return target.IsEmpty() ? CStringW() : CStringW(L"#") + target;
                }
                const CStringW unresolvedPath = CombineAndCanonicalizePath(ctx.currentXhtmlDir, pathPart);
                if (FileExists(unresolvedPath)) ++g_content.missingSourceAnchors;
                else ++g_content.missingSourceResources;
                g_content.warnings += L"Unresolved internal link: " + ctx.currentXhtmlPath + L" -> " + href + L"\r\n";
                return href;
            }

            if (!pathPart.IsEmpty() && ctx.anchorIdMap)
            {
                CStringW abs = CombineAndCanonicalizePath(ctx.currentXhtmlDir, pathPart);
                CStringW key = LowerMapKey(abs);
                std::map<std::wstring, std::wstring>::const_iterator it = ctx.anchorIdMap->find(std::wstring(key.GetString()));
                if (it != ctx.anchorIdMap->end())
                {
                    if (g_content.mainDocuments.count(key.GetString()) || g_content.emittedIds.count(it->second))
                        return L"#" + CStringW(it->second.c_str());
                    g_content.warnings += L"Internal document outside imported content: " + href + L"\r\n";
                    return href;
                }
            }
        }
        else if (IsProbablyXhtmlHref(pathPart) && ctx.anchorIdMap)
        {
            CStringW abs = CombineAndCanonicalizePath(ctx.currentXhtmlDir, pathPart);
            CStringW key = LowerMapKey(abs);
            std::map<std::wstring, std::wstring>::const_iterator it = ctx.anchorIdMap->find(std::wstring(key.GetString()));
            if (it != ctx.anchorIdMap->end())
            {
                if (g_content.mainDocuments.count(key.GetString()) || g_content.emittedIds.count(it->second))
                    return L"#" + CStringW(it->second.c_str());
                g_content.warnings += L"Internal document outside imported content: " + href + L"\r\n";
                return href;
            }
        }

        return href;
    }

    CStringW OptionalIdAttributeFromRaw(const CStringW& rawId, const ConvertContext& ctx)
    {
        CStringW id = RegisterFb2AnchorId(ctx, rawId);
        if (id.IsEmpty())
            return CStringW();
        return L" id=\"" + XmlEscape(id) + L"\"";
    }

    CStringW OptionalIdAttribute(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        return OptionalIdAttributeFromRaw(RawAnchorId(node), ctx);
    }

    void CollectRawAnchorIds(const MSXML2::IXMLDOMNodePtr& node, std::vector<CStringW>& ids, std::set<std::wstring>& seenKeys)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return;

        CStringW raw = RawAnchorId(node);
        CStringW key(raw);
        if (!key.IsEmpty() && seenKeys.emplace(key.GetString()).second)
            ids.push_back(raw);

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            CollectRawAnchorIds(MsxmlGetNodeListItem(children, i), ids, seenKeys);
    }

    std::vector<CStringW> CollectRawAnchorIds(const MSXML2::IXMLDOMNodePtr& node)
    {
        std::vector<CStringW> ids;
        std::set<std::wstring> seenKeys;
        CollectRawAnchorIds(node, ids, seenKeys);
        return ids;
    }

    CStringW OptionalIdAttributeIncludingDescendant(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        CStringW id = RawAnchorId(node);
        if (id.IsEmpty())
            id = FirstDescendantAnchorId(node);
        return OptionalIdAttributeFromRaw(id, ctx);
    }

    CStringW AnchorMarkerParagraphsFromRawIds(const std::vector<CStringW>& ids, size_t startIndex, const ConvertContext& ctx)
    {
        CStringW markers;
        for (size_t i = startIndex; i < ids.size(); ++i)
        {
            CStringW idAttr = OptionalIdAttributeFromRaw(ids[i], ctx);
            if (!idAttr.IsEmpty())
                markers += L"      <p" + idAttr + L"></p>\r\n";
        }
        return markers;
    }

    bool IsQuoteTextDiv(const MSXML2::IXMLDOMNodePtr& node);

    CStringW OptionalIdAttributeAndExtraMarkersIncludingDescendant(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, CStringW& extraMarkers)
    {
        extraMarkers.Empty();
        std::vector<CStringW> ids;
        const CStringW name = LocalName(node);
        if (name == L"p" || IsHeadingName(name) || name == L"li" || (name == L"div" && IsQuoteTextDiv(node))) ids = CollectRawAnchorIds(node);
        else if (!RawAnchorId(node).IsEmpty()) ids.push_back(RawAnchorId(node));
        if (ids.empty())
            return CStringW();

        const CStringW primary = OptionalIdAttributeFromRaw(ids.front(), ctx);
        extraMarkers = AnchorMarkerParagraphsFromRawIds(ids, 1, ctx);
        return primary;
    }

    void AppendParagraphNode(CStringW& out, const MSXML2::IXMLDOMNodePtr& sourceNode, const CStringW& content, const ConvertContext& ctx)
    {
        CStringW tmp(content);
        tmp.Trim();
        CStringW extraMarkers;
        CStringW idAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(sourceNode, ctx, extraMarkers);
        out += extraMarkers;
        if (!tmp.IsEmpty() || !idAttr.IsEmpty())
            out += L"      <p" + idAttr + L">" + tmp + L"</p>\r\n";
    }

    // FB2 has no inline br. Preserve direct XHTML line boundaries as separate
    // paragraphs or subtitles, keeping all source anchors available.
    bool AppendDirectBreakLines(const MSXML2::IXMLDOMNodePtr& node, CStringW& out,
        const ConvertContext& ctx, bool subtitle)
    {
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        bool hasBreak = false;
        for (long i = 0; i < count; ++i)
        {
            const auto child = MsxmlGetNodeListItem(children, i);
            if (MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT && LocalName(child).CompareNoCase(L"br") == 0)
                hasBreak = true;
        }
        if (!hasBreak) return false;
        std::vector<CStringW> lines;
        CStringW current;
        for (long i = 0; i < count; ++i)
        {
            const auto child = MsxmlGetNodeListItem(children, i);
            if (MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT && LocalName(child).CompareNoCase(L"br") == 0)
            {
                current.Trim(); lines.push_back(current); current.Empty();
            }
            else current += InlineToFb2(child, ctx);
        }
        current.Trim(); if (!current.IsEmpty()) lines.push_back(current);
        CStringW markers;
        CStringW id = OptionalIdAttributeAndExtraMarkersIncludingDescendant(node, ctx, markers);
        out += markers;
        const CStringW tag = subtitle ? L"subtitle" : L"p";
        for (const auto& line : lines)
        {
            if (line.IsEmpty() && id.IsEmpty()) out += L"      <empty-line/>\r\n";
            else out += L"      <" + tag + id + L">" + line + L"</" + tag + L">\r\n";
            id.Empty();
        }
        return true;
    }

    bool IsStandaloneAsteriskSeparator(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node) return false;
        CStringW text = NodeText(node);
        int stars = 0;
        for (int i = 0; i < text.GetLength(); ++i)
        {
            if (text[i] == L'*') ++stars;
            else if (text[i] != L' ' && text[i] != L'\t' && text[i] != L'\r' && text[i] != L'\n' && text[i] != 0x00a0) return false;
        }
        if (stars != 3) return false;
        std::vector<MSXML2::IXMLDOMNodePtr> images;
        CollectElementsByLocalName(node, L"img", images);
        CollectElementsByLocalName(node, L"image", images);
        CollectElementsByLocalName(node, L"svg", images);
        CollectElementsByLocalName(node, L"picture", images);
        CollectElementsByLocalName(node, L"object", images);
        if (!images.empty()) return false;
        MSXML2::IXMLDOMNode* raw = nullptr;
        node->get_parentNode(&raw);
        for (MSXML2::IXMLDOMNodePtr parent(raw, false); parent; )
        {
            const CStringW name = LocalName(parent);
            if (name == L"blockquote" || name == L"aside" || name == L"li" || name == L"table" || name == L"figure") return false;
            raw = nullptr; parent->get_parentNode(&raw); parent = MSXML2::IXMLDOMNodePtr(raw, false);
        }
        return true;
    }

    bool HasLineBreakElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node)
            return false;

        if (MsxmlGetNodeType(node) == MSXML2::NODE_ELEMENT && LocalName(node).CompareNoCase(L"br") == 0)
            return true;

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            if (HasLineBreakElement(MsxmlGetNodeListItem(children, i)))
                return true;
        }
        return false;
    }

    void CollectInlineLinesPreservingBreaks(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, std::vector<CStringW>& lines)
    {
        CStringW current;
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (!child)
                continue;

            if (MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT && LocalName(child).CompareNoCase(L"br") == 0)
            {
                current.Trim();
                if (!current.IsEmpty())
                    lines.push_back(current);
                current.Empty();
                continue;
            }

            current += InlineToFb2(child, ctx);
        }

        current.Trim();
        if (!current.IsEmpty())
            lines.push_back(current);
    }

    CStringW EscapePreLinePreservingIndent(const CStringW& line)
    {
        CStringW expanded;
        expanded.Preallocate(line.GetLength() + 16);
        for (int i = 0; i < line.GetLength(); ++i)
        {
            if (line[i] == L'\t')
                expanded += L"    ";
            else
                expanded += line[i];
        }

        // In normal FB2 text consecutive leading spaces may be visually collapsed by
        // readers. Keep indentation in <pre>/<code> by converting only the leading
        // spaces to NBSP before XML escaping.
        for (int i = 0; i < expanded.GetLength(); ++i)
        {
            if (expanded[i] == L' ')
                expanded.SetAt(i, 0x00A0);
            else
                break;
        }
        return XmlEscape(expanded);
    }

    bool LooksLikePoem(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW type = EpubType(node);
        CStringW cls = Attr(node, L"class");
        return IsTokenInList(type, L"poem") || IsTokenInList(type, L"verse") ||
            SemanticClassContains(cls, L"poem") || SemanticClassContains(cls, L"stanza") || SemanticClassContains(cls, L"verse");
    }

    bool LooksLikeEpigraph(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW type = EpubType(node);
        CStringW cls = Attr(node, L"class");
        return IsTokenInList(type, L"epigraph") || SemanticClassContains(cls, L"epigraph");
    }

    bool LooksLikeCite(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW type = EpubType(node);
        CStringW cls = Attr(node, L"class");
        if (IsTokenInList(type, L"quote") || IsTokenInList(type, L"citation")) return true;
        int position = 0;
        CStringW item = cls.Tokenize(L" \t\r\n", position);
        while (!item.IsEmpty())
        {
            // Typography classes such as text-transform-smartquotes describe
            // punctuation, not a quotation container.
            if (!ContainsNoCase(item, L"smartquote") &&
                (SemanticClassContains(item, L"quote") || SemanticClassContains(item, L"citation") || SemanticClassContains(item, L"cite")))
                return true;
            item = cls.Tokenize(L" \t\r\n", position);
        }
        return false;
    }

    bool LooksLikeTextAuthor(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;

        CStringW name = LocalName(node);
        CStringW type = EpubType(node);
        CStringW cls = Attr(node, L"class");
        return name.CompareNoCase(L"address") == 0 || name.CompareNoCase(L"footer") == 0 ||
            IsTokenInList(type, L"text-author") || IsTokenInList(type, L"author") ||
            IsTokenInList(type, L"attribution") || IsRoleToken(node, L"doc-credit") ||
            SemanticClassContains(cls, L"text-author") || SemanticClassContains(cls, L"text_author") ||
            SemanticClassContains(cls, L"attribution") || SemanticClassContains(cls, L"signature") ||
            SemanticClassContains(cls, L"source");
    }

    bool LooksLikeSubtitle(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW type = EpubType(node);
        CStringW cls = Attr(node, L"class");
        return IsTokenInList(type, L"subtitle") || IsTokenInList(type, L"subheading") ||
            SemanticClassContains(cls, L"subtitle") || SemanticClassContains(cls, L"sub-title") ||
            SemanticClassContains(cls, L"subhead") || SemanticClassContains(cls, L"lead");
    }

    bool LooksLikeDedication(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW type = EpubType(node);
        CStringW cls = Attr(node, L"class");
        CStringW role = Attr(node, L"role");
        return IsTokenInList(type, L"dedication") || IsTokenInList(type, L"dedication-page") ||
            SemanticClassContains(cls, L"dedication") || SemanticClassContains(cls, L"dedicate") ||
            ContainsNoCase(role, L"doc-dedication");
    }

    bool LooksLikeSidebar(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW name = LocalName(node);
        CStringW type = EpubType(node);
        CStringW cls = Attr(node, L"class");
        CStringW role = Attr(node, L"role");
        return name.CompareNoCase(L"aside") == 0 || IsTokenInList(type, L"sidebar") ||
            SemanticClassContains(cls, L"sidebar") || SemanticClassContains(cls, L"note-box") ||
            ContainsNoCase(role, L"doc-tip") || ContainsNoCase(role, L"doc-pullquote");
    }

    bool LooksLikePageBreak(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW type = EpubType(node);
        CStringW cls = Attr(node, L"class");
        return IsTokenInList(type, L"pagebreak") || IsTokenInList(type, L"page-break") ||
            SemanticClassContains(cls, L"pagebreak") || SemanticClassContains(cls, L"page-break");
    }

    bool PageBreakHasContent(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || IsHiddenElement(node)) return false;
        const CStringW name = LocalName(node);
        if (name == L"img" || name == L"image" || name == L"picture" || name == L"object" || name == L"svg" || name == L"video")
            return true;
        const auto type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION)
        {
            const CStringW text = NodeText(node);
            for (int i = 0; i < text.GetLength(); ++i)
                if (!iswspace(text[i]) && !iswdigit(text[i]) && !iswpunct(text[i])) return true;
        }
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            if (PageBreakHasContent(MsxmlGetNodeListItem(children, i))) return true;
        return false;
    }

    bool IsSimplePoemInline(const MSXML2::IXMLDOMNodePtr& node, unsigned depth = 0)
    {
        if (depth > 64) return false;
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i) {
            const auto child = MsxmlGetNodeListItem(children, i);
            if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT) continue;
            const CStringW name = LocalName(child);
            if (name != L"span" && name != L"br" && name != L"em" && name != L"i" && name != L"strong" && name != L"b") return false;
            if (!IsSimplePoemInline(child, depth + 1)) return false;
        }
        return true;
    }

    // Handle the unambiguous Standard Ebooks pattern: a title header followed
    // by one paragraph per stanza and BR-separated verse lines. Leave complex
    // poems to the existing conversion instead of guessing about their order.
    bool AppendSimpleZ3998Poem(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!IsEpubType(node, L"z3998:poem") || IsEpubType(node, L"epigraph")) return false;
        const CStringW rootRaw = RawAnchorId(node);
        const auto ids = CollectRawAnchorIds(node);
        if (ids.size() > (rootRaw.IsEmpty() ? 0U : 1U)) return false;
        MSXML2::IXMLDOMNodePtr title;
        std::vector<MSXML2::IXMLDOMNodePtr> stanzas;
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i) {
            const auto child = MsxmlGetNodeListItem(children, i);
            if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT) {
                if (!TrimString(NodeText(child)).IsEmpty()) return false;
                continue;
            }
            const CStringW name = LocalName(child);
            if (name == L"header" && !title && stanzas.empty() && IsEpubType(child, L"title")) {
                const auto headerChildren = MsxmlGetChildNodes(child);
                const long headerCount = headerChildren ? MsxmlGetNodeListLength(headerChildren) : 0;
                for (long j = 0; j < headerCount; ++j) {
                    const auto part = MsxmlGetNodeListItem(headerChildren, j);
                    if (MsxmlGetNodeType(part) != MSXML2::NODE_ELEMENT) {
                        if (!TrimString(NodeText(part)).IsEmpty()) return false;
                    } else if (LocalName(part) == L"p" && !title && IsSimplePoemInline(part)) title = part;
                    else return false;
                }
                if (!title) return false;
            } else if (name == L"p" && IsSimplePoemInline(child) && !TrimString(NodeText(child)).IsEmpty()) stanzas.push_back(child);
            else return false;
        }
        if (stanzas.empty()) return false;
        CStringW poem = L"      <poem" + OptionalIdAttributeFromRaw(rootRaw, ctx) + L">\r\n";
        if (title) {
            CStringW text = ChildrenInlineToFb2(title, ctx); text.Trim();
            if (text.IsEmpty()) return false;
            poem += L"        <title><p>" + text + L"</p></title>\r\n";
        }
        for (const auto& stanza : stanzas) {
            std::vector<CStringW> lines;
            CollectInlineLinesPreservingBreaks(stanza, ctx, lines);
            CStringW verses;
            for (auto line : lines) { line.Trim(); if (!line.IsEmpty()) verses += L"          <v>" + line + L"</v>\r\n"; }
            if (verses.IsEmpty()) return false;
            poem += L"        <stanza>\r\n" + verses + L"        </stanza>\r\n";
        }
        out += poem + L"      </poem>\r\n";
        return true;
    }

    void AppendPoemNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!g_options.importPoemsEpigraphs)
            return;

        std::vector<MSXML2::IXMLDOMNodePtr> lines;
        CollectElementsByLocalName(node, L"p", lines);
        if (lines.empty())
            CollectElementsByLocalName(node, L"div", lines);

        CStringW poem;
        CStringW authors;
        poem += L"      <poem>\r\n";
        poem += L"        <stanza>\r\n";
        int lineCount = 0;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            std::vector<CStringW> logicalLines;
            if (HasLineBreakElement(lines[i]))
                CollectInlineLinesPreservingBreaks(lines[i], ctx, logicalLines);
            else
            {
                CStringW singleLine = ChildrenInlineToFb2(lines[i], ctx);
                singleLine.Trim();
                if (!singleLine.IsEmpty())
                    logicalLines.push_back(singleLine);
            }

            for (size_t j = 0; j < logicalLines.size(); ++j)
            {
                CStringW line = logicalLines[j];
                line.Trim();
                if (line.IsEmpty())
                    continue;

                if (LooksLikeTextAuthor(lines[i]))
                    authors += L"        <text-author>" + line + L"</text-author>\r\n";
                else
                {
                    poem += L"          <v>" + line + L"</v>\r\n";
                    ++lineCount;
                }
            }
        }
        poem += L"        </stanza>\r\n";
        poem += authors;
        poem += L"      </poem>\r\n";

        if (lineCount > 0 || !authors.IsEmpty())
        {
            const CStringW rootRaw = RawAnchorId(node);
            const CStringW rootId = OptionalIdAttributeFromRaw(rootRaw, ctx);
            const auto ids = CollectRawAnchorIds(node);
            CStringW markers;
            for (size_t i = !rootRaw.IsEmpty() && !ids.empty() && ids.front() == rootRaw ? 1 : 0; i < ids.size(); ++i)
            {
                const CStringW id = OptionalIdAttributeFromRaw(ids[i], ctx);
                if (!id.IsEmpty()) markers += L"          <v" + id + L"></v>\r\n";
            }
            poem.Replace(L"      <poem>\r\n", L"      <poem" + rootId + L">\r\n");
            poem.Replace(L"        <stanza>\r\n", L"        <stanza>\r\n" + markers);
            out += poem;
        }
    }

    void AppendPreNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx);

    bool LinkContainsBlockContent(const MSXML2::IXMLDOMNodePtr& node);

    bool IsQuoteTextDiv(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (LocalName(node) != L"div") return false;
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            const auto child = MsxmlGetNodeListItem(children, i);
            if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT) continue;
            const CStringW name = LocalName(child);
            if (name == L"p" || name == L"div" || name == L"section" || name == L"article" ||
                name == L"aside" || name == L"ul" || name == L"ol" || name == L"li" ||
                name == L"table" || name == L"pre" || name == L"figure" || name == L"blockquote" ||
                IsHeadingName(name)) return false;
            if (LinkContainsBlockContent(child)) return false;
        }
        return !TrimString(NodeText(node)).IsEmpty();
    }

    void AppendListNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx);

    bool IsSimpleMarkedQuoteList(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        if (!g_options.importLists || !g_options.importImages) return false;
        bool marked = false, complex = false;
        const auto items = MsxmlGetChildNodes(node);
        const long itemCount = items ? MsxmlGetNodeListLength(items) : 0;
        for (long i = 0; i < itemCount; ++i)
        {
            const auto item = MsxmlGetNodeListItem(items, i);
            if (MsxmlGetNodeType(item) != MSXML2::NODE_ELEMENT || LocalName(item) != L"li") continue;
            if (!ResolveCssListImage(item, ctx).href.IsEmpty()) marked = true;
            const auto children = MsxmlGetChildNodes(item);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long j = 0; j < count; ++j)
            {
                const auto child = MsxmlGetNodeListItem(children, j);
                if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT) continue;
                const CStringW name = LocalName(child);
                if (name == L"p" || name == L"div" || name == L"section" || name == L"article" ||
                    name == L"aside" || name == L"ul" || name == L"ol" || name == L"li" ||
                    name == L"table" || name == L"pre" || name == L"figure" || name == L"blockquote" ||
                    IsHeadingName(name)) complex = true;
            }
        }
        return marked && !complex;
    }

    bool IsQuoteInlineNode(const CStringW& name)
    {
        return name == L"a" || name == L"span" || name == L"code" || name == L"em" || name == L"i" ||
            name == L"strong" || name == L"b" || name == L"sup" || name == L"sub" || name == L"s" ||
            name == L"strike" || name == L"del" || name == L"u" || name == L"small" || name == L"big";
    }

    void CollectQuoteBlocks(const MSXML2::IXMLDOMNodePtr& node, std::vector<MSXML2::IXMLDOMNodePtr>& blocks,
        std::vector<CStringW>& prefixes, const ConvertContext& ctx, bool preserveInline = false)
    {
        if (!node)
            return;
        const auto type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION)
        {
            blocks.push_back(node);
            prefixes.emplace_back();
            return;
        }
        if (IsHiddenElement(node)) return;
        const CStringW name = LocalName(node);
        if (name == L"p" || name == L"pre" || name == L"br" || IsHeadingName(name) || IsQuoteTextDiv(node) || name == L"img" || name == L"image" || name == L"picture" || name == L"object" ||
            (preserveInline && IsQuoteInlineNode(name)) ||
            ((name == L"ul" || name == L"ol") && IsSimpleMarkedQuoteList(node, ctx)))
        {
            blocks.push_back(node);
            prefixes.emplace_back();
            return;
        }
        CStringW itemMarker;
        if (name == L"li" && g_options.importLists && g_options.importImages)
            itemMarker = CssListMarkerToFb2(node, ctx);
        const size_t firstBlock = blocks.size();
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            CollectQuoteBlocks(MsxmlGetNodeListItem(children, i), blocks, prefixes, ctx, preserveInline || !itemMarker.IsEmpty());
        if (name == L"li" && g_options.importLists && g_options.importImages)
        {
            MSXML2::IXMLDOMNode* rawParent = nullptr;
            if (SUCCEEDED(node->get_parentNode(&rawParent)) && rawParent)
            {
                const MSXML2::IXMLDOMNodePtr parent(rawParent, false);
                const CStringW parentName = LocalName(parent);
                if (parentName == L"ul" || parentName == L"ol")
                {
                    if (!itemMarker.IsEmpty())
                    {
                        size_t index = firstBlock;
                        while (index < blocks.size())
                        {
                            const auto blockType = MsxmlGetNodeType(blocks[index]);
                            if (blockType != MSXML2::NODE_TEXT && blockType != MSXML2::NODE_CDATA_SECTION) break;
                            if (!TrimString(NodeText(blocks[index])).IsEmpty()) break;
                            ++index;
                        }
                        if (index < blocks.size()) prefixes[index] = itemMarker + L" " + prefixes[index];
                        else if (g_content.warnedInlineCss.emplace(L"empty-sidebar-list-image").second)
                            g_content.warnings += L"CSS list image marker on an empty sidebar item was not preserved.\r\n";
                    }
                }
            }
        }
    }

    void AppendEpigraphOrCiteNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx, bool epigraph)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> paragraphs;
        std::vector<CStringW> prefixes;
        CollectQuoteBlocks(node, paragraphs, prefixes, ctx);

        CStringW xml;
        std::vector<CStringW> blocks;
        bool afterAuthor = false;
        int count = 0;
        bool hasParagraph = false;
        for (const auto& block : paragraphs)
            if (MsxmlGetNodeType(block) == MSXML2::NODE_TEXT || MsxmlGetNodeType(block) == MSXML2::NODE_CDATA_SECTION ||
                LocalName(block) == L"p" || LocalName(block) == L"pre" || LocalName(block) == L"ul" || LocalName(block) == L"ol" || IsQuoteInlineNode(LocalName(block)) ||
                IsHeadingName(LocalName(block)) || IsQuoteTextDiv(block)) { hasParagraph = true; break; }
        if (!hasParagraph)
        {
            CStringW text = ChildrenInlineToFb2(node, ctx);
            text.Trim();
            if (!text.IsEmpty())
            {
                xml += L"        <p>" + text + L"</p>\r\n";
                ++count;
            }
        }
        else
        {
            CStringW pendingInline;
            const auto flushInline = [&]() {
                CStringW text(pendingInline);
                pendingInline.Empty();
                text.Trim();
                if (text.IsEmpty()) return;
                if (afterAuthor) { blocks.push_back(xml); xml.Empty(); }
                xml += L"        <p>" + text + L"</p>\r\n";
                afterAuthor = false;
                ++count;
            };
            for (size_t i = 0; i < paragraphs.size(); ++i)
            {
                const auto type = MsxmlGetNodeType(paragraphs[i]);
                if (type == MSXML2::NODE_ELEMENT && LocalName(paragraphs[i]) == L"br")
                {
                    const bool hasPending = !TrimString(pendingInline).IsEmpty();
                    flushInline();
                    if (!hasPending)
                    {
                        if (afterAuthor) { blocks.push_back(xml); xml.Empty(); afterAuthor = false; }
                        xml += L"        <empty-line/>\r\n";
                        ++count;
                    }
                    continue;
                }
                const bool directText = type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION || IsQuoteInlineNode(LocalName(paragraphs[i]));
                if (directText)
                {
                    if (!prefixes[i].IsEmpty()) flushInline();
                    pendingInline += prefixes[i];
                    pendingInline += InlineToFb2(paragraphs[i], ctx);
                    continue;
                }
                flushInline();
                const CStringW prefix = prefixes[i];
                if (LocalName(paragraphs[i]) == L"pre")
                {
                    if (afterAuthor) { blocks.push_back(xml); xml.Empty(); afterAuthor = false; }
                    if (!prefix.IsEmpty()) { xml += L"        <p>" + prefix + L"</p>\r\n"; ++count; }
                    const int previousLength = xml.GetLength();
                    AppendPreNode(paragraphs[i], xml, ctx);
                    if (xml.GetLength() != previousLength) ++count;
                    continue;
                }
                if (LocalName(paragraphs[i]) == L"ul" || LocalName(paragraphs[i]) == L"ol")
                {
                    if (afterAuthor) { blocks.push_back(xml); xml.Empty(); afterAuthor = false; }
                    if (!prefix.IsEmpty()) { xml += L"        <p>" + prefix + L"</p>\r\n"; ++count; }
                    const int previousLength = xml.GetLength();
                    AppendListNode(paragraphs[i], xml, ctx);
                    if (xml.GetLength() != previousLength) ++count;
                    continue;
                }
                if (!directText && LocalName(paragraphs[i]) != L"p" && !IsHeadingName(LocalName(paragraphs[i])) && !IsQuoteTextDiv(paragraphs[i]))
                {
                    const CStringW image = Fb2ImageElementForNode(ctx, paragraphs[i]);
                    if (!image.IsEmpty())
                    {
                        if (afterAuthor)
                        {
                            blocks.push_back(xml);
                            xml.Empty();
                            afterAuthor = false;
                        }
                        xml += L"        <p>" + prefix + image + L"</p>\r\n";
                        ++count;
                    }
                    continue;
                }
                CStringW text = ChildrenInlineToFb2(paragraphs[i], ctx);
                text.Trim();
                text = prefix + text;
                if (!text.IsEmpty())
                {
                    const bool author = LooksLikeTextAuthor(paragraphs[i]);
                    if (!author && afterAuthor)
                    {
                        blocks.push_back(xml);
                        xml.Empty();
                    }
                    if (author)
                        xml += L"        <text-author>" + text + L"</text-author>\r\n";
                    else
                        xml += L"        <p>" + text + L"</p>\r\n";
                    afterAuthor = author;
                    ++count;
                }
            }
            flushInline();
        }
        if (count > 0)
        {
            blocks.push_back(xml);
            const CStringW rootRaw = RawAnchorId(node);
            const CStringW rootId = OptionalIdAttributeFromRaw(rootRaw, ctx);
            std::vector<CStringW> anchors = CollectRawAnchorIds(node);
            CStringW markers = AnchorMarkerParagraphsFromRawIds(anchors,
                !rootRaw.IsEmpty() && !anchors.empty() && anchors.front() == rootRaw ? 1 : 0, ctx);
            for (size_t i = 0; i < blocks.size(); ++i)
            {
                out += epigraph ? L"      <epigraph" : L"      <cite";
                out += (i == 0 ? rootId : CStringW()) + L">\r\n" + (i == 0 ? markers : CStringW()) + blocks[i];
                out += epigraph ? L"      </epigraph>\r\n" : L"      </cite>\r\n";
            }
        }
        else
        {
            // An otherwise empty quote can still contain a legitimate target.
            out += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(node), 0, ctx);
        }
    }

    CStringW TableCellAttributes(const MSXML2::IXMLDOMNodePtr& node)
    {
        CStringW attrs;
        const LPCWSTR names[] = { L"colspan", L"rowspan", L"align", L"valign" };
        for (int i = 0; i < static_cast<int>(_countof(names)); ++i)
        {
            CStringW value = Attr(node, names[i]);
            if (!value.IsEmpty())
            {
                attrs += L" ";
                attrs += names[i];
                attrs += L"=\"";
                attrs += XmlEscape(value);
                attrs += L"\"";
            }
        }
        return attrs;
    }

    CStringW TableCaptionText(const MSXML2::IXMLDOMNodePtr& tableNode, const ConvertContext& ctx)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> captions;
        CollectElementsByLocalName(tableNode, L"caption", captions);
        if (captions.empty())
            return CStringW();

        CStringW caption = ChildrenInlineToFb2(captions[0], ctx);
        caption.Trim();
        return caption;
    }

    CStringW TableCellContentToFb2(const MSXML2::IXMLDOMNodePtr& cell, const ConvertContext& ctx)
    {
        CStringW out;
        bool blockBoundary = false;
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(cell);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (!child)
                continue;

            CStringW part;
            bool block = false;
            if (MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT)
            {
                CStringW name = LocalName(child);
                if (name.CompareNoCase(L"p") == 0 || name.CompareNoCase(L"div") == 0 || name.CompareNoCase(L"section") == 0)
                {
                    block = true;
                    part = ChildrenInlineToFb2(child, ctx);
                }
                else if (name.CompareNoCase(L"br") == 0)
                {
                    blockBoundary = true;
                    continue;
                }
                else if (name.CompareNoCase(L"ul") == 0 || name.CompareNoCase(L"ol") == 0)
                {
                    block = true;
                    std::vector<MSXML2::IXMLDOMNodePtr> items;
                    CollectElementsByLocalName(child, L"li", items);
                    for (size_t li = 0; li < items.size(); ++li)
                    {
                        CStringW itemText = ChildrenInlineToFb2(items[li], ctx);
                        itemText.Trim();
                        if (!itemText.IsEmpty())
                        {
                            if (!part.IsEmpty())
                                part += L"; ";
                            part += itemText;
                        }
                    }
                }
                else if (name.CompareNoCase(L"caption") == 0)
                    continue;
                else
                    part = InlineToFb2(child, ctx);
            }
            else
            {
                part = InlineToFb2(child, ctx);
            }

            if (block) part.Trim();
            if (TrimString(part).IsEmpty())
            {
                if (!blockBoundary && !block) out += part;
                continue;
            }
            if (blockBoundary || block)
            {
                out.TrimRight();
                part.TrimLeft();
            }
            if (!out.IsEmpty() && (blockBoundary || block))
                out += L"; ";
            out += part;
            blockBoundary = block;
        }

        if (out.IsEmpty())
            out = ChildrenInlineToFb2(cell, ctx);
        out.Trim();
        return out;
    }

    int ParsePositiveInt(const CStringW& value, int fallback)
    {
        CStringW s(value);
        s.Trim();
        if (s.IsEmpty())
            return fallback;
        int result = 0;
        for (int i = 0; i < s.GetLength(); ++i)
        {
            if (s[i] < L'0' || s[i] > L'9')
                return fallback;
            result = result * 10 + (s[i] - L'0');
            if (result > 1000000)
                return fallback;
        }
        return result > 0 ? result : fallback;
    }

    CStringW RomanNumber(int value, bool upper)
    {
        struct RomanPair { int value; LPCWSTR text; };
        static const RomanPair pairs[] =
        {
            {1000, L"M"}, {900, L"CM"}, {500, L"D"}, {400, L"CD"},
            {100, L"C"}, {90, L"XC"}, {50, L"L"}, {40, L"XL"},
            {10, L"X"}, {9, L"IX"}, {5, L"V"}, {4, L"IV"}, {1, L"I"}
        };

        if (value <= 0 || value > 3999)
        {
            CStringW fallback;
            fallback.Format(L"%d", value);
            return fallback;
        }

        CStringW out;
        int n = value;
        for (int i = 0; i < static_cast<int>(_countof(pairs)); ++i)
        {
            while (n >= pairs[i].value)
            {
                out += pairs[i].text;
                n -= pairs[i].value;
            }
        }
        if (!upper)
            out.MakeLower();
        return out;
    }

    CStringW AlphaNumber(int value, bool upper)
    {
        if (value <= 0)
        {
            CStringW fallback;
            fallback.Format(L"%d", value);
            return fallback;
        }

        CStringW out;
        int n = value;
        while (n > 0)
        {
            --n;
            wchar_t ch = static_cast<wchar_t>((upper ? L'A' : L'a') + (n % 26));
            out.Insert(0, ch);
            n /= 26;
        }
        return out;
    }

    CStringW OrderedListPrefix(int number, const CStringW& listType, const CStringW& style)
    {
        CStringW type(listType);
        type.Trim();
        CStringW css(style);
        css.MakeLower();

        if (type == L"A" || ContainsNoCase(css, L"upper-alpha") || ContainsNoCase(css, L"upper-latin"))
            return AlphaNumber(number, true) + L". ";
        if (type == L"a" || ContainsNoCase(css, L"lower-alpha") || ContainsNoCase(css, L"lower-latin"))
            return AlphaNumber(number, false) + L". ";
        if (type == L"I" || ContainsNoCase(css, L"upper-roman"))
            return RomanNumber(number, true) + L". ";
        if (type == L"i" || ContainsNoCase(css, L"lower-roman"))
            return RomanNumber(number, false) + L". ";

        CStringW prefix;
        prefix.Format(L"%d. ", number);
        return prefix;
    }

    void AppendBlockNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx);

    void AppendListNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!g_options.importLists)
            return;

        CStringW name = LocalName(node);
        const bool ordered = name.CompareNoCase(L"ol") == 0;
        int number = ordered ? ParsePositiveInt(Attr(node, L"start"), 1) : 1;
        CStringW listType = Attr(node, L"type");
        CStringW listStyle = Attr(node, L"style");
        const CStringW listId = OptionalIdAttribute(node, ctx);
        if (!listId.IsEmpty()) out += L"      <p" + listId + L"></p>\r\n";

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (!child || MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT)
                continue;
            if (LocalName(child).CompareNoCase(L"li") != 0)
            {
                // Real-world XHTML can place anchors and blocks directly inside a list.
                // Preserve them in document order without assigning list numbering.
                AppendBlockNode(child, out, ctx);
                continue;
            }
            if (!ctx.collectingNotes && g_options.importNotes && IsCollectedNoteNode(child, ctx)) continue;

            int itemNumber = number;
            if (ordered)
                itemNumber = ParsePositiveInt(Attr(child, L"value"), number);

            // A list item may contain multiple real paragraphs. Flattening
            // them as inline children merges adjacent words and loses their
            // boundaries, especially in technical callouts.
            const auto itemChildren = MsxmlGetChildNodes(child);
            const long itemCount = itemChildren ? MsxmlGetNodeListLength(itemChildren) : 0;
            bool hasBlockChildren = false;
            for (long j = 0; j < itemCount; ++j)
            {
                const CStringW childName = LocalName(MsxmlGetNodeListItem(itemChildren, j));
                if (childName == L"p" || childName == L"pre" || childName == L"div" ||
                    childName == L"ul" || childName == L"ol" || childName == L"table") { hasBlockChildren = true; break; }
            }
            if (hasBlockChildren)
            {
                CStringW prefix;
                const CStringW marker = CssListMarkerToFb2(child, ctx);
                if (!marker.IsEmpty()) prefix = marker + L" ";
                if (ordered)
                {
                    if (prefix.IsEmpty()) prefix = OrderedListPrefix(itemNumber, listType, listStyle);
                    number = itemNumber + 1;
                }
                else if (prefix.IsEmpty()) prefix = L"• ";

                CStringW itemContent, pending, inlineMarkers;
                const InlineFontState font = InheritedInlineFont(child);
                const auto flush = [&]() {
                    pending.Trim();
                    if (!pending.IsEmpty()) itemContent += L"      <p>" + pending + L"</p>\r\n";
                    pending.Empty();
                };
                for (long j = 0; j < itemCount; ++j)
                {
                    const auto part = MsxmlGetNodeListItem(itemChildren, j);
                    const CStringW partName = LocalName(part);
                    if (MsxmlGetNodeType(part) == MSXML2::NODE_ELEMENT &&
                        (partName == L"p" || partName == L"pre" || partName == L"div" || partName == L"ul" || partName == L"ol" || partName == L"table"))
                    {
                        flush();
                        AppendBlockNode(part, itemContent, ctx);
                    }
                    else
                    {
                        inlineMarkers += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(part), 0, ctx);
                        pending += InlineToFb2(part, ctx, font);
                    }
                }
                flush();
                const CStringW itemId = OptionalIdAttribute(child, ctx);
                if (!itemId.IsEmpty()) out += L"      <p" + itemId + L"></p>\r\n";
                out += inlineMarkers;
                int scan = 0;
                while ((scan = itemContent.Find(L"<p", scan)) >= 0)
                {
                    const int openEnd = itemContent.Find(L'>', scan);
                    if (openEnd < 0) break;
                    if (itemContent.Mid(openEnd + 1, 4) != L"</p>")
                    {
                        itemContent.Insert(openEnd + 1, prefix);
                        break;
                    }
                    scan = openEnd + 1;
                }
                out += itemContent;
                continue;
            }

            CStringW text = ChildrenInlineToFb2(child, ctx);
            text.Trim();
            CStringW markers;
            const CStringW itemId = OptionalIdAttributeAndExtraMarkersIncludingDescendant(child, ctx, markers);
            out += markers;
            if (text.IsEmpty())
            {
                if (!itemId.IsEmpty()) out += L"      <p" + itemId + L"></p>\r\n";
                continue;
            }

            CStringW prefix;
            const CStringW marker = CssListMarkerToFb2(child, ctx);
            if (!marker.IsEmpty()) prefix = marker + L" ";
            if (ordered)
            {
                if (prefix.IsEmpty()) prefix = OrderedListPrefix(itemNumber, listType, listStyle);
                number = itemNumber + 1;
            }
            else if (prefix.IsEmpty())
            {
                prefix = L"• ";
            }

            out += L"      <p" + itemId + L">" + prefix + text + L"</p>\r\n";
        }
    }

    void AppendTableNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!g_options.importTables)
            return;

        std::vector<MSXML2::IXMLDOMNodePtr> rows;
        CollectElementsByLocalName(node, L"tr", rows);
        if (rows.empty())
            return;

        CStringW table;
        CStringW caption = TableCaptionText(node, ctx);
        if (!caption.IsEmpty())
            table += L"      <subtitle>" + caption + L"</subtitle>\r\n";
        table += L"      <table" + OptionalIdAttribute(node, ctx) + L">\r\n";
        int rowCount = 0;
        for (size_t r = 0; r < rows.size(); ++r)
        {
            CStringW row;
            row += L"        <tr>\r\n";
            MSXML2::IXMLDOMNodeListPtr cells = MsxmlGetChildNodes(rows[r]);
            int cellCount = 0;
            long n = cells ? MsxmlGetNodeListLength(cells) : 0;
            long rowAnchorCell = -1;
            for (long c = 0; c < n; ++c)
            {
                const auto cell = MsxmlGetNodeListItem(cells, c);
                const CStringW name = LocalName(cell);
                if ((name == L"td" || name == L"th") && CollectRawAnchorIds(cell).empty())
                {
                    rowAnchorCell = c;
                    break;
                }
            }
            const CStringW rowId = rowAnchorCell >= 0 ? OptionalIdAttribute(rows[r], ctx) : CStringW();
            for (long c = 0; c < n; ++c)
            {
                MSXML2::IXMLDOMNodePtr cell = MsxmlGetNodeListItem(cells, c);
                CStringW name = LocalName(cell);
                if (MsxmlGetNodeType(cell) != MSXML2::NODE_ELEMENT ||
                    (name.CompareNoCase(L"td") != 0 && name.CompareNoCase(L"th") != 0))
                    continue;

                CStringW text = TableCellContentToFb2(cell, ctx);
                text.Trim();
                CStringW cellName = name;
                cellName.MakeLower();
                const auto ids = CollectRawAnchorIds(cell);
                const CStringW cellId = c == rowAnchorCell && !rowId.IsEmpty() ? rowId :
                    (ids.empty() ? CStringW() : OptionalIdAttributeFromRaw(ids.front(), ctx));
                row += L"          <" + cellName + cellId + TableCellAttributes(cell) + L">" + text + L"</" + cellName + L">\r\n";
                ++cellCount;
            }
            row += L"        </tr>\r\n";
            if (cellCount > 0)
            {
                table += row;
                ++rowCount;
            }
        }
        table += L"      </table>\r\n";

        if (rowCount > 0)
            out += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(node), 0, ctx) + table;
    }


    CStringW PreTextNormalize(const CStringW& text)
    {
        CStringW s = g_options.repairEncoding ? TryRepairUtf8DecodedAsCp1251(text) : text;

        CStringW out;
        out.Preallocate(s.GetLength());
        for (int i = 0; i < s.GetLength(); ++i)
        {
            wchar_t ch = s[i];
            if (g_options.cleanTypography && (ch == 0x00AD || ch == 0x200B || ch == 0xFEFF))
                continue;
            if (g_options.cleanTypography && (ch == 0x00A0 || ch == 0x2007 || ch == 0x202F))
                ch = L' ';
            out += ch;
        }
        out.Replace(L"\r\n", L"\n");
        out.Replace(L"\r", L"\n");
        return out;
    }

    struct PreAnchorOffset
    {
        CStringW id;
        int offset = 0;
    };

    struct PreImageOffset
    {
        MSXML2::IXMLDOMNodePtr node;
        int offset = 0;
    };

    struct PreLinkOffset
    {
        MSXML2::IXMLDOMNodePtr node;
        int start = 0, end = 0;
        CStringW opening;
    };

    void CollectPreContent(const MSXML2::IXMLDOMNodePtr& node, bool root, CStringW& raw,
                           std::vector<PreAnchorOffset>& anchors, std::vector<PreImageOffset>& images,
                           std::vector<PreLinkOffset>& links, bool hiddenAncestor = false, bool insideLink = false)
    {
        if (!node) return;
        const auto type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION)
        {
            raw += MsxmlGetNodeText(node);
            return;
        }
        if (type != MSXML2::NODE_ELEMENT) return;
        const bool hidden = hiddenAncestor || IsHiddenElement(node);
        const CStringW id = RawAnchorId(node);
        if (!root && !id.IsEmpty()) anchors.push_back({ id, raw.GetLength() });
        const CStringW name = LocalName(node);
        const bool link = name == L"a" && !Attr(node, L"href").IsEmpty();
        const int linkStart = raw.GetLength();
        if (name == L"img" || name == L"image" || name == L"picture" || name == L"object")
        {
            if (!hidden) images.push_back({ node, raw.GetLength() });
            return;
        }
        if (name == L"br")
        {
            raw += L'\n';
            return;
        }
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            CollectPreContent(MsxmlGetNodeListItem(children, i), false, raw, anchors, images, links, hidden, insideLink || link);
        if (link && !insideLink && !hidden && raw.GetLength() > linkStart)
            links.push_back({ node, linkStart, raw.GetLength(), CStringW() });
    }

    void AppendPreNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!node)
            return;

        CStringW raw;
        std::vector<PreAnchorOffset> anchors;
        std::vector<PreImageOffset> images;
        std::vector<PreLinkOffset> links;
        CollectPreContent(node, true, raw, anchors, images, links);
        CStringW text = PreTextNormalize(raw);
        if (text.IsEmpty() && images.empty())
        {
            out += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(node), 0, ctx);
            return;
        }

        // First map raw offsets through encoding repair. CP1251 is single-byte;
        // successful repair decodes that byte stream as UTF-8, which can change
        // the number of UTF-16 code units before an anchor or image.
        const CStringW repaired = g_options.repairEncoding ? TryRepairUtf8DecodedAsCp1251(raw) : raw;
        std::vector<int> rawToRepaired(static_cast<size_t>(raw.GetLength()) + 1, 0);
        if (repaired == raw)
        {
            for (int i = 0; i <= raw.GetLength(); ++i) rawToRepaired[static_cast<size_t>(i)] = i;
        }
        else
        {
            const int byteCount = ::WideCharToMultiByte(1251, 0, raw.GetString(), raw.GetLength(), nullptr, 0, nullptr, nullptr);
            std::vector<char> bytes(static_cast<size_t>(max(0, byteCount)));
            if (byteCount > 0)
                ::WideCharToMultiByte(1251, 0, raw.GetString(), raw.GetLength(), bytes.data(), byteCount, nullptr, nullptr);
            int sourceOffset = 0;
            int repairedOffset = 0;
            while (sourceOffset < byteCount)
            {
                const unsigned char lead = static_cast<unsigned char>(bytes[static_cast<size_t>(sourceOffset)]);
                const int count = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
                for (int i = 0; i < count && sourceOffset + i < byteCount; ++i)
                    rawToRepaired[static_cast<size_t>(sourceOffset + i)] = repairedOffset;
                sourceOffset += count;
                repairedOffset += count == 4 ? 2 : 1;
                if (sourceOffset <= raw.GetLength()) rawToRepaired[static_cast<size_t>(sourceOffset)] = repairedOffset;
            }
        }
        // Then map repaired offsets through typography cleanup and CRLF folding.
        std::vector<int> repairedToNormalized(static_cast<size_t>(repaired.GetLength()) + 1, 0);
        int normalizedOffset = 0;
        for (int i = 0; i < repaired.GetLength(); ++i)
        {
            repairedToNormalized[static_cast<size_t>(i)] = normalizedOffset;
            const wchar_t ch = repaired[i];
            if (g_options.cleanTypography && (ch == 0x00AD || ch == 0x200B || ch == 0xFEFF)) continue;
            ++normalizedOffset;
            if (ch == L'\r' && i + 1 < repaired.GetLength() && repaired[i + 1] == L'\n')
            {
                repairedToNormalized[static_cast<size_t>(i + 1)] = normalizedOffset;
                ++i;
            }
        }
        repairedToNormalized[static_cast<size_t>(repaired.GetLength())] = normalizedOffset;
        std::vector<int> mapped(static_cast<size_t>(raw.GetLength()) + 1, 0);
        for (int i = 0; i <= raw.GetLength(); ++i)
            mapped[static_cast<size_t>(i)] = repairedToNormalized[static_cast<size_t>(rawToRepaired[static_cast<size_t>(i)])];
        for (auto& link : links)
        {
            const CStringW rendered = InlineToFb2(link.node, ctx);
            const int openingEnd = rendered.Find(L'>');
            if (rendered.Left(3) == L"<a " && openingEnd >= 0 &&
                rendered.Find(L"</a>") == rendered.GetLength() - 4)
                link.opening = rendered.Left(openingEnd + 1);
        }
        int start = 0;
        int lineCount = 0;
        CStringW firstLineId = OptionalIdAttribute(node, ctx);
        size_t nextAnchor = 0;
        size_t nextImage = 0;
        while (start <= text.GetLength())
        {
            const int newline = text.Find(L'\n', start);
            const int end = newline < 0 ? text.GetLength() : newline;
            CStringW line = text.Mid(start, end - start);
            while (nextAnchor < anchors.size())
            {
                const int rawOffset = max(0, min(anchors[nextAnchor].offset, raw.GetLength()));
                if (mapped[static_cast<size_t>(rawOffset)] > end) break;
                const CStringW markerId = OptionalIdAttributeFromRaw(anchors[nextAnchor++].id, ctx);
                if (!markerId.IsEmpty()) out += L"      <p" + markerId + L"></p>\r\n";
            }
            CStringW escapedLine = EscapePreLinePreservingIndent(line);
            struct PreInsertion { int offset, kind; CStringW text; };
            std::vector<PreInsertion> insertions;
            while (nextImage < images.size())
            {
                const int rawOffset = max(0, min(images[nextImage].offset, raw.GetLength()));
                const int imageOffset = mapped[static_cast<size_t>(rawOffset)];
                if (imageOffset > end) break;
                const int localOffset = max(0, min(imageOffset - start, line.GetLength()));
                const int escapedOffset = EscapePreLinePreservingIndent(line.Left(localOffset)).GetLength();
                const CStringW image = InlineToFb2(images[nextImage].node, ctx);
                if (!image.IsEmpty()) insertions.push_back({ escapedOffset, 1, image });
                ++nextImage;
            }
            for (const auto& link : links)
            {
                if (link.opening.IsEmpty()) continue;
                const int linkStart = mapped[static_cast<size_t>(link.start)];
                const int linkEnd = mapped[static_cast<size_t>(link.end)];
                const int partStart = max(start, linkStart), partEnd = min(end, linkEnd);
                if (partStart >= partEnd) continue;
                const int left = EscapePreLinePreservingIndent(line.Left(partStart - start)).GetLength();
                const int right = EscapePreLinePreservingIndent(line.Left(partEnd - start)).GetLength();
                insertions.push_back({ left, 0, link.opening });
                insertions.push_back({ right, 2, L"</a>" });
            }
            // Inserting repeatedly at one offset reverses insertion order.
            // Reverse equal-kind events first to retain the source image order.
            std::reverse(insertions.begin(), insertions.end());
            std::stable_sort(insertions.begin(), insertions.end(), [](const PreInsertion& a, const PreInsertion& b) {
                return a.offset != b.offset ? a.offset > b.offset : a.kind < b.kind;
            });
            for (const auto& insertion : insertions) escapedLine.Insert(insertion.offset, insertion.text);
            if (!line.IsEmpty() || !insertions.empty())
            {
                CStringW idAttr = lineCount == 0 ? firstLineId : CStringW();
                out += L"      <p" + idAttr + L"><code>" + escapedLine + L"</code></p>\r\n";
                ++lineCount;
            }
            else
                out += L"      <empty-line/>\r\n";
            if (newline < 0) break;
            start = newline + 1;
        }

        while (nextAnchor < anchors.size())
        {
            const CStringW markerId = OptionalIdAttributeFromRaw(anchors[nextAnchor++].id, ctx);
            if (!markerId.IsEmpty()) out += L"      <p" + markerId + L"></p>\r\n";
        }

        if (lineCount == 0 && !firstLineId.IsEmpty())
            out += L"      <p" + firstLineId + L"></p>\r\n";
    }

    void AppendDefinitionListNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!g_options.importLists || !node)
            return;

        const CStringW listId = OptionalIdAttribute(node, ctx);
        if (!listId.IsEmpty()) out += L"      <p" + listId + L"></p>\r\n";
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (!child || MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT)
                continue;

            CStringW name = LocalName(child);
            if (name.CompareNoCase(L"dt") != 0 && name.CompareNoCase(L"dd") != 0)
                continue;
            const auto parts = MsxmlGetChildNodes(child);
            const long partCount = parts ? MsxmlGetNodeListLength(parts) : 0;
            bool hasBlocks = false;
            for (long j = 0; j < partCount; ++j)
            {
                const CStringW partName = LocalName(MsxmlGetNodeListItem(parts, j));
                if (partName == L"p" || partName == L"div" || partName == L"pre" ||
                    partName == L"ul" || partName == L"ol" || partName == L"dl" ||
                    partName == L"table" || IsHeadingName(partName)) { hasBlocks = true; break; }
            }
            if (name == L"dd" && hasBlocks)
            {
                CStringW content, pending, inlineMarkers;
                const InlineFontState font = InheritedInlineFont(child);
                const auto flush = [&]() {
                    pending.Trim();
                    if (!pending.IsEmpty()) content += L"      <p>" + pending + L"</p>\r\n";
                    pending.Empty();
                };
                for (long j = 0; j < partCount; ++j)
                {
                    const auto part = MsxmlGetNodeListItem(parts, j);
                    const CStringW partName = LocalName(part);
                    if (MsxmlGetNodeType(part) == MSXML2::NODE_ELEMENT &&
                        (partName == L"p" || partName == L"div" || partName == L"pre" ||
                         partName == L"ul" || partName == L"ol" || partName == L"dl" ||
                         partName == L"table" || IsHeadingName(partName)))
                    {
                        flush();
                        AppendBlockNode(part, content, ctx);
                    }
                    else
                    {
                        inlineMarkers += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(part), 0, ctx);
                        pending += InlineToFb2(part, ctx, font);
                    }
                }
                flush();
                const CStringW id = OptionalIdAttribute(child, ctx);
                if (!id.IsEmpty()) out += L"      <p" + id + L"></p>\r\n";
                out += inlineMarkers;
                int scan = 0;
                while ((scan = content.Find(L"<p", scan)) >= 0)
                {
                    const int end = content.Find(L'>', scan);
                    if (end < 0) break;
                    if (content.Mid(end + 1, 4) != L"</p>") { content.Insert(end + 1, L"— "); break; }
                    scan = end + 1;
                }
                out += content;
                continue;
            }
            CStringW text = ChildrenInlineToFb2(child, ctx);
            text.Trim();
            const auto ids = CollectRawAnchorIds(child);
            const CStringW idAttr = ids.empty() ? CStringW() : OptionalIdAttributeFromRaw(ids.front(), ctx);
            out += AnchorMarkerParagraphsFromRawIds(ids, 1, ctx);
            if (text.IsEmpty())
            {
                if (!idAttr.IsEmpty()) out += L"      <p" + idAttr + L"></p>\r\n";
                continue;
            }
            if (name.CompareNoCase(L"dt") == 0)
                out += L"      <p" + idAttr + L"><strong>" + text + L"</strong></p>\r\n";
            else if (name.CompareNoCase(L"dd") == 0)
                out += L"      <p" + idAttr + L">— " + text + L"</p>\r\n";
        }
    }

    void AppendBlockNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx);
    void AppendFb2Section(CStringW& out, const CStringW& title, const CStringW& content, const CStringW& idAttr);
    bool TryConvertBodyToSplitSections(const MSXML2::IXMLDOMNodePtr& body, const CStringW& fallbackTitle, const ConvertContext& ctx, CStringW& outSection);

    void AppendFigureNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!node)
            return;

        const CStringW figureId = OptionalIdAttribute(node, ctx);
        if (!figureId.IsEmpty()) out += L"      <p" + figureId + L"></p>\r\n";
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            const auto child = MsxmlGetNodeListItem(children, i);
            if (LocalName(child) == L"figcaption")
            {
                const auto captionChildren = MsxmlGetChildNodes(child);
                const long captionCount = captionChildren ? MsxmlGetNodeListLength(captionChildren) : 0;
                bool hasParagraphs = false;
                for (long j = 0; j < captionCount; ++j)
                    if (LocalName(MsxmlGetNodeListItem(captionChildren, j)) == L"p") { hasParagraphs = true; break; }
                if (hasParagraphs)
                {
                    const CStringW captionId = OptionalIdAttribute(child, ctx);
                    if (!captionId.IsEmpty()) out += L"      <p" + captionId + L"></p>\r\n";
                    for (long j = 0; j < captionCount; ++j)
                    {
                        const auto part = MsxmlGetNodeListItem(captionChildren, j);
                        const auto partChildren = MsxmlGetChildNodes(part);
                        if (LocalName(part) == L"p" && (!partChildren || MsxmlGetNodeListLength(partChildren) == 0))
                        {
                            out += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(part), 0, ctx);
                            out += L"      <empty-line/>\r\n";
                        }
                        else AppendBlockNode(part, out, ctx);
                    }
                    continue;
                }
                const CStringW caption = ChildrenInlineToFb2(child, ctx);
                const auto ids = CollectRawAnchorIds(child);
                const CStringW captionId = ids.empty() ? CStringW() : OptionalIdAttributeFromRaw(ids.front(), ctx);
                out += AnchorMarkerParagraphsFromRawIds(ids, 1, ctx);
                if (!TrimString(caption).IsEmpty() || !captionId.IsEmpty())
                    out += L"      <subtitle" + captionId + L">" + caption + L"</subtitle>\r\n";
            }
            else AppendBlockNode(child, out, ctx);
        }
    }

    void AppendBlockNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx);

    bool LinkContainsBlockContent(const MSXML2::IXMLDOMNodePtr& node)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> pending;
        pending.push_back(node);
        while (!pending.empty())
        {
            const auto current = pending.back();
            pending.pop_back();
            const auto children = MsxmlGetChildNodes(current);
            for (long i = 0, count = children ? MsxmlGetNodeListLength(children) : 0; i < count; ++i)
            {
                const auto child = MsxmlGetNodeListItem(children, i);
                if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT) continue;
                const CStringW name = LocalName(child);
                if (name == L"p" || name == L"div" || name == L"section" || name == L"article" ||
                    name == L"aside" || name == L"figure" || name == L"blockquote" || name == L"table" ||
                    name == L"ul" || name == L"ol" || name == L"li" || name == L"hr" || IsHeadingName(name))
                    return true;
                pending.push_back(child);
            }
        }
        return false;
    }

    bool LinkContainsSingleWrappedImage(const MSXML2::IXMLDOMNodePtr& node)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> pending;
        pending.push_back(node);
        size_t images = 0;
        while (!pending.empty())
        {
            const auto current = pending.back();
            pending.pop_back();
            const auto children = MsxmlGetChildNodes(current);
            for (long i = 0, count = children ? MsxmlGetNodeListLength(children) : 0; i < count; ++i)
            {
                const auto child = MsxmlGetNodeListItem(children, i);
                const MSXML2::DOMNodeType type = MsxmlGetNodeType(child);
                if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION)
                {
                    if (!TrimString(MsxmlGetNodeText(child)).IsEmpty()) return false;
                    continue;
                }
                if (type != MSXML2::NODE_ELEMENT) continue;
                const CStringW name = LocalName(child);
                if (name == L"img") ++images;
                else if (name != L"div" && name != L"span") return false;
                pending.push_back(child);
            }
        }
        return images == 1;
    }

    void AppendDedicationInlineLines(const CStringW& source, CStringW& out)
    {
        const CStringW marker = L"<!--fbe-dedication-line-break-->";
        struct OpenTag { CStringW opening; CStringW closing; };
        std::vector<OpenTag> tags;
        CStringW line;
        bool hasContent = false;
        auto finishLine = [&](bool explicitBreak) {
            CStringW closed = line;
            for (auto it = tags.rbegin(); it != tags.rend(); ++it) closed += it->closing;
            closed.Trim();
            if (hasContent) out += L"      <p>" + closed + L"</p>\r\n";
            else if (explicitBreak) out += L"      <empty-line/>\r\n";
            line.Empty();
            for (const auto& tag : tags) line += tag.opening;
            hasContent = false;
        };
        for (int pos = 0; pos < source.GetLength(); )
        {
            if (source.Mid(pos, marker.GetLength()) == marker)
            {
                finishLine(true);
                pos += marker.GetLength();
                continue;
            }
            if (source[pos] != L'<')
            {
                int end = source.Find(L'<', pos);
                if (end < 0) end = source.GetLength();
                const CStringW part = source.Mid(pos, end - pos);
                line += part;
                if (!TrimString(part).IsEmpty()) hasContent = true;
                pos = end;
                continue;
            }
            int end = pos + 1;
            wchar_t quote = 0;
            for (; end < source.GetLength(); ++end)
            {
                const wchar_t ch = source[end];
                if (quote) { if (ch == quote) quote = 0; }
                else if (ch == L'\'' || ch == L'"') quote = ch;
                else if (ch == L'>') break;
            }
            if (end == source.GetLength()) { line += source.Mid(pos); hasContent = true; break; }
            const CStringW token = source.Mid(pos, end - pos + 1);
            line += token;
            if (token.GetLength() > 2 && token[1] == L'/')
            {
                if (!tags.empty()) tags.pop_back();
            }
            else if (token.GetLength() > 2 && token[token.GetLength() - 2] == L'/')
                hasContent = true;
            else if (token.GetLength() > 2 && token[1] != L'!' && token[1] != L'?')
            {
                int nameEnd = 1;
                while (nameEnd < token.GetLength() && !iswspace(token[nameEnd]) && token[nameEnd] != L'>') ++nameEnd;
                tags.push_back({token, L"</" + token.Mid(1, nameEnd - 1) + L">"});
            }
            pos = end + 1;
        }
        finishLine(false);
    }

    void AppendSpecialSectionNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx, LPCWSTR title)
    {
        CStringW content;
        if (LocalName(node) == L"p" && !LinkContainsBlockContent(node))
        {
            // A dedication can itself be a paragraph. Its children are inline
            // content, but standalone page markers still need block handling.
            CStringW text;
            const InlineFontState font = InheritedInlineFont(node);
            ConvertContext inlineContext = ctx;
            inlineContext.inlinePageBreaks = true;
            inlineContext.inlineLineBreaks = true;
            auto flush = [&]() {
                text.Trim();
                if (!text.IsEmpty()) AppendDedicationInlineLines(text, content);
                text.Empty();
            };
            const auto children = MsxmlGetChildNodes(node);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i)
            {
                const auto child = MsxmlGetNodeListItem(children, i);
                if (MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT && LooksLikePageBreak(child) && !PageBreakHasContent(child))
                {
                    flush();
                    AppendBlockNode(child, content, ctx);
                }
                else text += InlineToFb2(child, inlineContext, font);
            }
            flush();
        }
        else
        {
            MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
            long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i)
                AppendBlockNode(MsxmlGetNodeListItem(children, i), content, ctx);
        }

        content.Trim();
        if (content.IsEmpty())
        {
            CStringW text = ChildrenInlineToFb2(node, ctx);
            text.Trim();
            if (!text.IsEmpty())
                content = L"      <p>" + text + L"</p>\r\n";
        }

        if (!content.IsEmpty())
        {
            CStringW extraMarkers;
            CStringW idAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(node, ctx, extraMarkers);
            out += L"      <section" + idAttr + L">\r\n";
            out += L"        <title><p>" + XmlEscape(CStringW(title)) + L"</p></title>\r\n";
            out += extraMarkers;
            out += content;
            out += L"      </section>\r\n";
        }
    }

    void AppendBlockNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!node)
            return;

        if (MsxmlGetNodeType(node) == MSXML2::NODE_TEXT || MsxmlGetNodeType(node) == MSXML2::NODE_CDATA_SECTION)
        {
            CStringW text = NodeText(node);
            if (!text.IsEmpty())
                out += L"      <p>" + XmlEscape(NormalizeImportedText(text)) + L"</p>\r\n";
            return;
        }

        if (MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return;

        if (IsHiddenElement(node))
        {
            // Hidden content is omitted, but links can still target IDs in it.
            out += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(node), 0, ctx);
            return;
        }

        if (IsBacklinkElement(node))
        {
            RecordBacklink(node, ctx);
            // Keep anchors that can still be targets of other links.
            out += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(node), 0, ctx);
            return;
        }

        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"script") == 0 || name.CompareNoCase(L"style") == 0 || name.CompareNoCase(L"head") == 0)
            return;
        RecordUnsupportedInlineStyle(node, ctx);

        const CStringW backgroundImage = EmptyBackgroundImage(node, ctx, false);
        if (!backgroundImage.IsEmpty())
        {
            out += L"      <p" + OptionalIdAttribute(node, ctx) + L">" + backgroundImage + L"</p>\r\n";
            return;
        }

        if (ctx.collectingNotes && name.CompareNoCase(L"a") == 0)
        {
            const std::vector<CStringW> ids = CollectRawAnchorIds(node);
            const CStringW id = ids.empty() ? CStringW() : OptionalIdAttributeFromRaw(ids.front(), ctx);
            out += AnchorMarkerParagraphsFromRawIds(ids, 1, ctx);
            const CStringW link = InlineToFb2(node, ctx);
            if (!id.IsEmpty() || !link.IsEmpty()) out += L"      <p" + id + L">" + link + L"</p>\r\n";
            return;
        }

        if (name == L"a" && (!LinkContainsBlockContent(node) || LinkContainsSingleWrappedImage(node)))
        {
            CStringW link = InlineToFb2(node, ctx);
            if (link.Left(3) == L"<a " && link.Right(5) == L"></a>" && link.Find(L'>') == link.GetLength() - 5)
                link.Empty();
            const std::vector<CStringW> ids = CollectRawAnchorIds(node);
            const CStringW id = ids.empty() ? CStringW() : OptionalIdAttributeFromRaw(ids.front(), ctx);
            out += AnchorMarkerParagraphsFromRawIds(ids, 1, ctx);
            if (!link.IsEmpty() || !id.IsEmpty()) out += L"      <p" + id + L">" + link + L"</p>\r\n";
            return;
        }
        if (name == L"a" && g_options.preserveLinks && !Attr(node, L"href").IsEmpty()) {
            RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
            g_content.warnings += L"Block hyperlink could not be preserved as one FB2 link; its contents were retained.\r\n";
        }

        if (name.CompareNoCase(L"figure") == 0)
        {
            AppendFigureNode(node, out, ctx);
            return;
        }

        if (name == L"video")
        {
            RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
            g_content.warnings += L"EPUB video is not supported; its poster and fallback content were retained when available.\r\n";
            const CStringW image = Fb2ImageElementForNode(ctx, node);
            if (!image.IsEmpty()) out += L"      <p>" + image + L"</p>\r\n";
            const auto children = MsxmlGetChildNodes(node);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i) AppendBlockNode(MsxmlGetNodeListItem(children, i), out, ctx);
            return;
        }
        if (name == L"picture" || name == L"img" || name == L"image" || name == L"object")
        {
            const CStringW image = Fb2ImageElementForNode(ctx, node);
            if (!image.IsEmpty()) {
                const auto ids = CollectRawAnchorIds(node);
                const CStringW imageId = ids.empty() ? CStringW() : OptionalIdAttributeFromRaw(ids.front(), ctx);
                out += AnchorMarkerParagraphsFromRawIds(ids, 1, ctx);
                out += L"      <p" + imageId + L">" + image + L"</p>\r\n";
            }
            else if (name == L"object")
            {
                const auto children = MsxmlGetChildNodes(node);
                const long count = children ? MsxmlGetNodeListLength(children) : 0;
                for (long i = 0; i < count; ++i) AppendBlockNode(MsxmlGetNodeListItem(children, i), out, ctx);
            }
            return;
        }

        if (name.CompareNoCase(L"pre") == 0)
        {
            AppendPreNode(node, out, ctx);
            return;
        }

        if (name.CompareNoCase(L"dl") == 0)
        {
            AppendDefinitionListNode(node, out, ctx);
            return;
        }

        // EPUB notes are moved to a separate FB2 <body name="notes">.
        // Backlinks inside notes are purely navigational and must not pollute text.
        if (g_options.importNotes && !ctx.collectingNotes && IsCollectedNoteNode(node, ctx))
            return;

        if (name == L"section" || name == L"article")
        {
            CStringW content;
            const CStringW id = OptionalIdAttribute(node, ctx);
            if (!TryConvertBodyToSplitSections(node, CStringW(), ctx, content))
            {
                const auto children = MsxmlGetChildNodes(node);
                const long count = children ? MsxmlGetNodeListLength(children) : 0;
                for (long i = 0; i < count; ++i) AppendBlockNode(MsxmlGetNodeListItem(children, i), content, ctx);
            }
            if (!content.IsEmpty()) AppendFb2Section(out, CStringW(), content, id);
            return;
        }

        if (LooksLikeDedication(node))
        {
            AppendSpecialSectionNode(node, out, ctx, LoadImportRuntimeString(IDS_IMPORT_RUNTIME_DEDICATION_TITLE, L"Dedication"));
            return;
        }

        if (g_options.importNotes && !ctx.collectingNotes && IsEndnotesContainerElement(node))
        {
            // Walk note containers normally: quote/sidebar conversion would
            // otherwise render already-collected notes again in the main body.
            const auto children = MsxmlGetChildNodes(node);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i) AppendBlockNode(MsxmlGetNodeListItem(children, i), out, ctx);
            return;
        }

        if (LooksLikeSidebar(node) && !IsEndnoteElement(node))
        {
            AppendEpigraphOrCiteNode(node, out, ctx, false);
            return;
        }

        if (g_options.importPoemsEpigraphs && IsEpubType(node, L"z3998:poem")) {
            if (AppendSimpleZ3998Poem(node, out, ctx)) return;
            RecordDiagnosticElement(g_content.fidelityLossElements, node, ctx);
            if (g_content.warnedUnmappedPoems.emplace(ctx.currentXhtmlPath.GetString()).second)
                g_content.warnings += L"Poem structure could not be mapped unambiguously; review the existing conversion: " + ctx.currentXhtmlPath + L"\r\n";
        }

        if (g_options.importPoemsEpigraphs && LooksLikePoem(node))
        {
            AppendPoemNode(node, out, ctx);
            return;
        }

        if (g_options.importPoemsEpigraphs && LooksLikeEpigraph(node))
        {
            AppendEpigraphOrCiteNode(node, out, ctx, true);
            return;
        }

        if (g_options.importPoemsEpigraphs && LooksLikeTextAuthor(node))
        {
            CStringW author = ChildrenInlineToFb2(node, ctx);
            author.Trim();
            const auto ids = CollectRawAnchorIds(node);
            const CStringW id = ids.empty() ? CStringW() : OptionalIdAttributeFromRaw(ids.front(), ctx);
            out += AnchorMarkerParagraphsFromRawIds(ids, 1, ctx);
            if (!author.IsEmpty() || !id.IsEmpty())
                // A standalone attribution is a section block, not a child of
                // an epigraph, cite or poem. Keep it in place as a paragraph.
                out += L"      <p" + id + L">" + author + L"</p>\r\n";
            return;
        }

        if (g_options.importSubtitles && LooksLikeSubtitle(node))
        {
            if (AppendDirectBreakLines(node, out, ctx, true)) return;
            CStringW subtitle = ChildrenInlineToFb2(node, ctx);
            subtitle.Trim();
            const auto ids = CollectRawAnchorIds(node);
            const CStringW id = ids.empty() ? CStringW() : OptionalIdAttributeFromRaw(ids.front(), ctx);
            out += AnchorMarkerParagraphsFromRawIds(ids, 1, ctx);
            if (!subtitle.IsEmpty() || !id.IsEmpty())
                out += L"      <subtitle" + id + L">" + subtitle + L"</subtitle>\r\n";
            return;
        }

        if ((LooksLikePageBreak(node) && !PageBreakHasContent(node)) || name.CompareNoCase(L"hr") == 0)
        {
            out += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(node), 0, ctx);
            if (!g_options.importPageBreaks)
                return;

            CStringW pageLabel = NormalizeImportedText(Attr(node, L"title"));
            if (pageLabel.IsEmpty())
                pageLabel = NormalizeImportedText(Attr(node, L"aria-label"));
            if (pageLabel.IsEmpty())
                pageLabel = NodeText(node);
            pageLabel.Trim();
            out += L"      <empty-line/>\r\n";
            if (!pageLabel.IsEmpty() && name.CompareNoCase(L"hr") != 0)
                out += L"      <p><sub>" + XmlEscape(pageLabel) + L"</sub></p>\r\n";
            return;
        }

        if ((name.CompareNoCase(L"ul") == 0 || name.CompareNoCase(L"ol") == 0) && g_options.importLists)
        {
            AppendListNode(node, out, ctx);
            return;
        }

        if (name.CompareNoCase(L"table") == 0 && g_options.importTables)
        {
            AppendTableNode(node, out, ctx);
            return;
        }

        if (name.CompareNoCase(L"div") == 0 && HasCssClass(node, L"FixedLine"))
        {
            CStringW line = NodeText(node);
            bool blank = true;
            for (int i = 0; i < line.GetLength(); ++i)
                if (line[i] != L' ' && line[i] != L'\t' && line[i] != L'\r' && line[i] != L'\n' && line[i] != 0x00a0)
                { blank = false; break; }
            const auto lineChildren = MsxmlGetChildNodes(node);
            const long lineChildCount = lineChildren ? MsxmlGetNodeListLength(lineChildren) : 0;
            for (long i = 0; blank && i < lineChildCount; ++i)
            {
                const auto child = MsxmlGetNodeListItem(lineChildren, i);
                if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT) continue;
                if (LocalName(child) != L"span" || !Attr(child, L"class").IsEmpty() || !Attr(child, L"style").IsEmpty())
                { blank = false; break; }
                const auto nested = MsxmlGetChildNodes(child);
                const long nestedCount = nested ? MsxmlGetNodeListLength(nested) : 0;
                for (long j = 0; blank && j < nestedCount; ++j)
                    if (MsxmlGetNodeType(MsxmlGetNodeListItem(nested, j)) == MSXML2::NODE_ELEMENT) blank = false;
            }
            if (blank)
            {
                out += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(node), 0, ctx);
                out += L"      <empty-line/>\r\n";
                return;
            }
        }

        if (name.CompareNoCase(L"p") == 0 ||
            (name.CompareNoCase(L"div") == 0 && IsQuoteTextDiv(node)))
        {
            if (AppendDirectBreakLines(node, out, ctx, false)) return;
            CStringW paragraph = ChildrenInlineToFb2(node, ctx);
            paragraph.Trim();
            if (g_options.separatorSubtitles && !ctx.collectingNotes && IsStandaloneAsteriskSeparator(node) && !paragraph.IsEmpty())
            {
                CStringW extraMarkers;
                const CStringW idAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(node, ctx, extraMarkers);
                out += extraMarkers + L"      <subtitle" + idAttr + L">" + paragraph + L"</subtitle>\r\n";
                return;
            }
            AppendParagraphNode(out, node, paragraph, ctx);
            return;
        }

        if (IsHeadingName(name))
        {
            CStringW title = ChildrenInlineToFb2(node, ctx);
            title.Trim();
            CStringW markers;
            const CStringW id = OptionalIdAttributeAndExtraMarkersIncludingDescendant(node, ctx, markers);
            out += markers;
            if (!title.IsEmpty() || !id.IsEmpty())
                out += L"      <subtitle" + id + L">" + title + L"</subtitle>\r\n";
            return;
        }

        if (name.CompareNoCase(L"blockquote") == 0 || LooksLikeCite(node))
        {
            AppendEpigraphOrCiteNode(node, out, ctx, false);
            return;
        }

        if (name.CompareNoCase(L"li") == 0)
        {
            CStringW text = ChildrenInlineToFb2(node, ctx);
            text.Trim();
            if (!text.IsEmpty())
            {
                CStringW extraMarkers;
                CStringW idAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(node, ctx, extraMarkers);
                out += extraMarkers;
                out += L"      <p" + idAttr + L">• " + text + L"</p>\r\n";
            }
            return;
        }

        if (name.CompareNoCase(L"tr") == 0)
        {
            CStringW text = ChildrenInlineToFb2(node, ctx);
            text.Trim();
            if (!text.IsEmpty())
                out += L"      <p>" + text + L"</p>\r\n";
            return;
        }

        CStringW containerExtraMarkers;
        CStringW containerIdAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(node, ctx, containerExtraMarkers);
        out += containerExtraMarkers;
        if (!containerIdAttr.IsEmpty())
            out += L"      <p" + containerIdAttr + L"></p>\r\n";

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return;

        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
            AppendBlockNode(MsxmlGetNodeListItem(children, i), out, ctx);
    }

    MSXML2::IXMLDOMNodePtr FindFirstElementByLocalName(const MSXML2::IXMLDOMNodePtr& node, LPCWSTR localName)
    {
        if (!node)
            return nullptr;

        if (LocalName(node).CompareNoCase(localName) == 0)
            return node;

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return nullptr;

        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr found = FindFirstElementByLocalName(MsxmlGetNodeListItem(children, i), localName);
            if (found)
                return found;
        }

        return nullptr;
    }

    MSXML2::IXMLDOMNodePtr FindFirstHeadingChild(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node)
            return nullptr;

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return nullptr;

        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (child && MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT && IsHeadingName(LocalName(child)))
                return child;
        }
        return nullptr;
    }

    size_t CountIndependentNoteTargets(const MSXML2::IXMLDOMNodePtr& node, const CStringW& path)
    {
        if (!node) return 0;
        size_t result = g_content.requestedNotes.count(std::wstring(PathAnchorMapKey(path, RawAnchorId(node))));
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            result += CountIndependentNoteTargets(MsxmlGetNodeListItem(children, i), path);
        return result;
    }

    MSXML2::IXMLDOMNodePtr LogicalNoteContainer(const MSXML2::IXMLDOMNodePtr& target, const CStringW& path)
    {
        auto result = target;
        for (auto ancestor = ParentNode(target); ancestor && LocalName(ancestor) != L"body"; ancestor = ParentNode(ancestor))
        {
            const CStringW name = LocalName(ancestor);
            if (IsEndnotesContainerElement(ancestor)) break;
            if (name == L"li" || IsEndnoteElement(ancestor))
            {
                if (CountIndependentNoteTargets(ancestor, path) <= 1) return ancestor;
                break; // A shared container must never merge independent notes.
            }
            if ((LocalName(result) == L"a" || LocalName(result) == L"span") && name == L"p") result = ancestor;
        }
        return result;
    }

    bool IsWithinContents(const MSXML2::IXMLDOMNodePtr& node);

    // Some publishers encode one note as consecutive paragraphs instead of
    // wrapping it in a container. Only extend the specific foot/foot-para
    // convention, and stop before a new anchored note or any other block.
    std::vector<MSXML2::IXMLDOMNodePtr> FootNoteContinuations(const MSXML2::IXMLDOMNodePtr& root)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> result;
        if (LocalName(root) != L"p" || !HasSemanticCssClass(root, L"foot") || CollectRawAnchorIds(root).empty())
            return result;
        auto current = root;
        for (;;)
        {
            IXMLDOMNode* rawNext = nullptr;
            current->get_nextSibling(&rawNext);
            MSXML2::IXMLDOMNodePtr next(rawNext, false);
            if (!next) break;
            current = next;
            if (MsxmlGetNodeType(next) != MSXML2::NODE_ELEMENT) continue;
            if (LocalName(next) != L"p" ||
                (!HasSemanticCssClass(next, L"foot") && !HasSemanticCssClass(next, L"foot-para")) ||
                !CollectRawAnchorIds(next).empty()) break;
            result.push_back(next);
        }
        return result;
    }

    void CollectEndnoteElements(const MSXML2::IXMLDOMNodePtr& node, std::vector<MSXML2::IXMLDOMNodePtr>& out, const CStringW& path, bool insideNotesContainer = false)
    {
        if (!node)
            return;

        // Navigation may contain entries named "footnote" or "note".
        // They are table-of-contents labels, not the note bodies themselves.
        if (IsEpubType(node, L"toc") || IsRoleToken(node, L"doc-toc") ||
            HasSemanticCssClass(node, L"toc") || HasSemanticCssClass(node, L"contents"))
            return;

        const CStringW name = LocalName(node);
        const bool nowInsideNotesContainer = insideNotesContainer || IsEndnotesContainerElement(node);
        const bool isListNoteInNotesContainer = nowInsideNotesContainer &&
            name.CompareNoCase(L"li") == 0 && !Attr(node, L"id").IsEmpty();
        const bool requested = g_content.requestedNotes.count(std::wstring(PathAnchorMapKey(path, RawAnchorId(node)))) != 0;
        // A standalone CSS class "note" is also widely used for visible
        // callouts. Move it only when the source marks or references it as a
        // footnote, or places it in a notes container.
        const bool unreferencedCallout = HasSemanticCssClass(node, L"note") &&
            !HasSemanticCssClass(node, L"footnote") && !HasSemanticCssClass(node, L"endnote") &&
            !IsEpubType(node, L"footnote") && !IsEpubType(node, L"endnote") &&
            !IsRoleToken(node, L"doc-footnote") && !IsRoleToken(node, L"doc-endnote") &&
            !nowInsideNotesContainer && !requested;

        if ((IsEndnoteElement(node) && !unreferencedCallout) ||
            ((isListNoteInNotesContainer || requested) && !IsWithinContents(node)))
        {
            CStringW id = RawAnchorId(node);
            if (!id.IsEmpty())
            {
                const auto root = LogicalNoteContainer(node, path);
                const CStringW rootKey = NoteRootKey(root, path);
                CStringW documentKey = path;documentKey.MakeLower();
                g_content.noteRootDocuments.emplace(std::wstring(documentKey));
                const CStringW rootId = RawAnchorId(root).IsEmpty() ? id : RawAnchorId(root);
                const bool inserted = g_content.noteRoots.emplace(std::wstring(rootKey), rootId).second;
                const auto continuations = FootNoteContinuations(root);
                for (const auto& continuation : continuations)
                    g_content.noteRoots.emplace(std::wstring(NoteRootKey(continuation, path)), rootId);
                if (inserted && continuations.empty() && LocalName(root) == L"p" && !IsEndnoteElement(root))
                {
                    const auto parent = ParentNode(root);
                    IXMLDOMNode* rawNext = nullptr;root->get_nextSibling(&rawNext);
                    MSXML2::IXMLDOMNodePtr next(rawNext, false);
                    while (next && MsxmlGetNodeType(next) != MSXML2::NODE_ELEMENT)
                    {rawNext = nullptr;next->get_nextSibling(&rawNext);next.Attach(rawNext);}
                    if (parent && next && LocalName(next) == L"p" && RawAnchorId(next).IsEmpty() &&
                        (LocalName(parent) == L"div" || LocalName(parent) == L"section" || LocalName(parent) == L"body"))
                        g_content.warnings += L"Note boundary recognition limited to one paragraph: " + path + L"#" + id + L"\r\n";
                }
                bool duplicate = false;
                for (const auto& selected : out) duplicate = duplicate || selected == root;
                if (!duplicate) out.push_back(root);
                return; // Do not descend into an already selected note.
            }
        }

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return;

        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
            CollectEndnoteElements(MsxmlGetNodeListItem(children, i), out, path, nowInsideNotesContainer);
    }

    bool HasMainContent(const MSXML2::IXMLDOMNodePtr& node, const CStringW& path)
    {
        if (!node || IsHiddenElement(node) || IsBacklinkElement(node)) return false;
        const CStringW localName = LocalName(node);
        if (localName == L"script" || localName == L"style" || localName == L"head") return false;
        ConvertContext context = {};context.currentXhtmlPath = path;
        if (IsCollectedNoteNode(node, context)) return false;
        const auto type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION) return !TrimString(NodeText(node)).IsEmpty();
        const CStringW name = LocalName(node);
        if (name == L"img" || name == L"image" || name == L"picture" || name == L"object" || name == L"video") return g_options.importImages;
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i) if (HasMainContent(MsxmlGetNodeListItem(children, i), path)) return true;
        return false;
    }

    void IndexSourceNodes(const MSXML2::IXMLDOMNodePtr& node, const CStringW& path,
        std::map<std::wstring, MSXML2::IXMLDOMNodePtr>& nodes)
    {
        if (!node) return;
        const CStringW raw = RawAnchorId(node);
        if (!raw.IsEmpty() && !nodes.emplace(PathAnchorMapKey(path, raw).GetString(), node).second)
            g_content.warnings += L"Duplicate XHTML anchor: " + path + L"#" + raw + L"\r\n";
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i) IndexSourceNodes(MsxmlGetNodeListItem(children, i), path, nodes);
    }

    bool HasTextBeforeInBlock(const MSXML2::IXMLDOMNodePtr& node, bool ignoreMarkerPunctuation = false)
    {
        auto current = node;
        for (int depth = 0; current && depth < 16; ++depth)
        {
            const auto parent = ParentNode(current);
            if (!parent) break;
            const auto children = MsxmlGetChildNodes(parent);
            const long count = children ? MsxmlGetNodeListLength(children) : 0;
            for (long i = 0; i < count; ++i)
            {
                const auto child = MsxmlGetNodeListItem(children, i);
                if (child == current) break;
                if (!child || IsHiddenElement(child)) continue;
                const CStringW text = TrimString(NodeText(child));
                if (!ignoreMarkerPunctuation && !text.IsEmpty()) return true;
                if (ignoreMarkerPunctuation)
                    for (int c = 0; c < text.GetLength(); ++c)
                        if (!iswspace(text[c]) && wcschr(L"[](){}.,:;", text[c]) == nullptr) return true;
            }
            const CStringW name = LocalName(parent);
            if (name == L"p" || name == L"li" || name == L"td" || name == L"section" || name == L"body") break;
            current = parent;
        }
        return false;
    }

    bool IsInsideSuperscript(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (LocalName(node) == L"a")
        {
            std::vector<MSXML2::IXMLDOMNodePtr> superscripts;
            CollectElementsByLocalName(node, L"sup", superscripts);
            if (!superscripts.empty()) return true;
        }
        for (auto current = node; current; current = ParentNode(current))
        {
            const CStringW name = LocalName(current);
            if (name == L"sup") return true;
            if (name == L"p" || name == L"li" || IsHeadingName(name) || name == L"body") break;
        }
        return false;
    }

    bool IsWithinContents(const MSXML2::IXMLDOMNodePtr& node)
    {
        for (auto current = node; current; current = ParentNode(current))
        {
            if (IsEpubType(current, L"toc") || IsRoleToken(current, L"doc-toc") ||
                HasSemanticCssClass(current, L"toc") || HasSemanticCssClass(current, L"contents"))
                return true;
            if (LocalName(current) == L"body") break;
        }
        return false;
    }

    bool IsAnswerListEntry(const MSXML2::IXMLDOMNodePtr& node)
    {
        for (auto current = node; current; current = ParentNode(current))
        {
            if (LocalName(current) == L"li" && HasSemanticCssClass(current, L"answer")) return true;
            if (LocalName(current) == L"body") break;
        }
        return false;
    }

    bool IsBibliographyEntry(const MSXML2::IXMLDOMNodePtr& node, bool searchNearbyHeading)
    {
        bool referenceEntry = false;
        MSXML2::IXMLDOMNodePtr body;
        MSXML2::IXMLDOMNodePtr paragraph;
        for (auto current = node; current; current = ParentNode(current))
        {
            if (IsEndnoteElement(current)) return false;
            if (!paragraph && LocalName(current) == L"p") paragraph = current;
            if (HasSemanticCssClass(current, L"ref") || HasSemanticCssClass(current, L"ref1") ||
                HasSemanticCssClass(current, L"bibliography"))
                referenceEntry = true;
            if (LocalName(current) == L"body") { body = current; break; }
        }
        // Some books put an unmarked bibliography after a later heading in
        // the chapter, using ordinary paragraphs and reciprocal numeric links.
        // Inspect nearby siblings only; the nearest heading determines the
        // paragraph's context and an explicitly marked endnote stays a note.
        auto previous = searchNearbyHeading ? paragraph : MSXML2::IXMLDOMNodePtr();
        for (int count = 0; previous && count < 128; ++count)
        {
            IXMLDOMNode* raw = nullptr;
            previous->get_previousSibling(&raw);
            previous = MSXML2::IXMLDOMNodePtr(raw, false);
            if (!previous || !IsHeadingName(LocalName(previous))) continue;
            const CStringW heading = LowerMapKey(TrimString(NodeText(previous)));
            return heading == L"references" || heading == L"bibliography" ||
                heading == L"библиография" || heading == L"список литературы";
        }
        if (!referenceEntry || !body) return false;
        CStringW heading = LowerMapKey(TrimString(NodeText(FindFirstHeadingChild(body))));
        return heading == L"references" || heading == L"bibliography" ||
            heading == L"библиография" || heading == L"список литературы";
    }

    void PrepareContentInventory(const EpubPackage& package, std::map<std::wstring, std::wstring>& anchors)
    {
        std::set<std::wstring> spinePaths;
        std::map<std::wstring, MSXML2::IXMLDOMDocument2Ptr> documents;
        std::map<std::wstring, MSXML2::IXMLDOMNodePtr> nodes;
        std::map<std::wstring, std::wstring> nonSpineByPath;
        for (const auto& id : package.spine)
        {
            const auto item = package.manifest.find(std::wstring(id));
            if (item != package.manifest.end()) spinePaths.emplace(LowerMapKey(item->second.absPath).GetString());
        }
        g_content.mainDocuments = spinePaths;
        for (const auto& entry : package.manifest)
        {
            const auto& item = entry.second;
            if (!g_options.unifyImageNames && (IsImageMediaType(item.mediaType) || IsImageHrefByExtension(item.href)))
                g_content.binaryIds.emplace(entry.first, UniqueContentId(BinaryIdForImageItem(item)));
            const std::wstring path(LowerMapKey(item.absPath).GetString());
            if (!spinePaths.count(path) && (IsProbablyXhtmlHref(item.href) ||
                item.mediaType.CompareNoCase(L"application/xhtml+xml") == 0 || item.mediaType.CompareNoCase(L"text/html") == 0))
                nonSpineByPath.emplace(path, entry.first);
        }
        for (const auto& entry : package.manifest)
        {
            const auto& item = entry.second;
            if (!IsProbablyXhtmlHref(item.href) && item.mediaType.CompareNoCase(L"application/xhtml+xml") != 0 &&
                item.mediaType.CompareNoCase(L"text/html") != 0) continue;
            MSXML2::IXMLDOMDocument2Ptr doc; CStringW error;
            if (!LoadXmlFile(item.absPath, doc, error)) continue;
            documents.emplace(entry.first, doc);
            ConvertContext ctx = {};ctx.currentXhtmlPath = item.absPath;ctx.anchorIdMap = &anchors;
            const auto root = MsxmlGetDocumentElement(doc);
            if (spinePaths.count(LowerMapKey(item.absPath).GetString()))
            {
                ++g_content.spineXhtmlDocuments;
                const auto title = FindFirstElementByLocalName(root, L"title");
                const CStringW titleKey = title ? LowerMapKey(NodeText(title)) : CStringW();
                if (!titleKey.IsEmpty()) ++g_content.spineDocumentTitleCounts[std::wstring(titleKey.GetString())];
            }
            IndexSourceNodes(root, item.absPath, nodes);
            // Reserve all document anchors before rendering any forward links.
            for (const auto& raw : CollectRawAnchorIds(root))
            {
                const CStringW key = PathAnchorMapKey(item.absPath, raw);
                if (anchors.count(std::wstring(key))) continue;
                AddAnchorIdMap(ctx, raw, UniqueContentId(NormalizeFb2Id(raw)));
            }
            const auto body = FindFirstElementByLocalName(root, L"body");
            CStringW documentRaw = RawAnchorId(body);
            if (documentRaw.IsEmpty()) documentRaw = RawAnchorId(FindFirstHeadingChild(body));
            if (documentRaw.IsEmpty()) documentRaw = FirstDescendantAnchorId(FindFirstHeadingChild(body));
            if (documentRaw.IsEmpty()) documentRaw = FileNameOnly(item.absPath);
            const CStringW documentKey = PathAnchorMapKey(item.absPath, documentRaw);
            if (!anchors.count(std::wstring(documentKey))) AddAnchorIdMap(ctx, documentRaw, UniqueContentId(NormalizeFb2Id(documentRaw)));
            AddDocumentHrefMap(ctx, CStringW(anchors[std::wstring(documentKey)].c_str()));
        }
        // Only explicit whole-document links from spine content qualify. Do
        // not infer targets from filenames or follow links out of the archive.
        if (g_options.preserveLinks) for (const auto& entry : package.manifest)
        {
            const auto& item = entry.second;
            if (!spinePaths.count(std::wstring(LowerMapKey(item.absPath).GetString()))) continue;
            const auto doc = documents.find(entry.first);
            if (doc == documents.end()) continue;
            ConvertContext scanContext = {}; scanContext.package = &package; scanContext.currentXhtmlPath = item.absPath;
            CStringW scanWarnings;
            const auto previousCssWarnings = g_content.warnedCssAtRules;
            ScopedCssFontRules scanCss(doc->second, item.absPath, scanContext, scanWarnings);
            g_content.warnedCssAtRules = previousCssWarnings;
            std::vector<MSXML2::IXMLDOMNodePtr> links;
            CollectElementsByLocalName(MsxmlGetDocumentElement(doc->second), L"a", links);
            for (const auto& link : links)
            {
                if (IsBacklinkElement(link)) continue;
                bool hidden = false;
                for (auto current = link; current; current = ParentNode(current))
                    if (IsHiddenElement(current)) { hidden = true; break; }
                if (hidden) continue;
                CStringW href = TrimString(Attr(link, L"href"));
                if (href.IsEmpty() || IsExternalHref(href)) continue;
                const int hash = href.Find(L'#');
                if (hash >= 0 && !TrimString(href.Mid(hash + 1)).IsEmpty()) continue;
                CStringW path = hash < 0 ? href : href.Left(hash);
                const int query = path.Find(L'?');
                if (query >= 0) path = path.Left(query);
                if (path.IsEmpty()) continue;
                const CStringW absolute = CombineAndCanonicalizePath(DirectoryOf(item.absPath), path);
                if (absolute.IsEmpty()) continue;
                const std::wstring targetKey(LowerMapKey(absolute).GetString());
                if (spinePaths.count(targetKey) || g_content.linkedNonSpineDocuments.count(targetKey)) continue;
                const auto found = nonSpineByPath.find(targetKey);
                ManifestItem targetItem;
                MSXML2::IXMLDOMDocument2Ptr targetDoc;
                bool unmanifested = false;
                if (found != nonSpineByPath.end())
                {
                    const auto manifest = package.manifest.find(found->second);
                    const auto targetDocument = documents.find(found->second);
                    if (manifest == package.manifest.end() || targetDocument == documents.end()) continue;
                    targetItem = manifest->second;
                    targetDoc = targetDocument->second;
                }
                else
                {
                    if (!IsProbablyXhtmlHref(path) || !FileExists(absolute)) continue;
                    CStringW error;
                    if (!LoadXmlFile(absolute, targetDoc, error)) continue;
                    targetItem.href = path; targetItem.absPath = absolute;
                    targetItem.mediaType = L"application/xhtml+xml";
                    unmanifested = true;
                }
                if (IsTokenInList(targetItem.properties, L"nav")) continue;
                if (g_options.skipServicePages && g_options.removeServiceSections &&
                    package.guideServiceDocuments.count(std::wstring(targetItem.absPath.GetString()))) continue;
                const auto root = MsxmlGetDocumentElement(targetDoc);
                const auto body = FindFirstElementByLocalName(root, L"body");
                if (!body || IsEndnotesBody(body) || TrimString(NodeText(body)).IsEmpty()) continue;
                if (unmanifested)
                {
                    ConvertContext targetCtx = {}; targetCtx.currentXhtmlPath = absolute; targetCtx.anchorIdMap = &anchors;
                    IndexSourceNodes(root, absolute, nodes);
                    for (const auto& raw : CollectRawAnchorIds(root))
                    {
                        const CStringW key = PathAnchorMapKey(absolute, raw);
                        if (!anchors.count(std::wstring(key))) AddAnchorIdMap(targetCtx, raw, UniqueContentId(NormalizeFb2Id(raw)));
                    }
                    CStringW documentRaw = RawAnchorId(body);
                    if (documentRaw.IsEmpty()) documentRaw = RawAnchorId(FindFirstHeadingChild(body));
                    if (documentRaw.IsEmpty()) documentRaw = FirstDescendantAnchorId(FindFirstHeadingChild(body));
                    if (documentRaw.IsEmpty()) documentRaw = FileNameOnly(absolute);
                    const CStringW documentKey = PathAnchorMapKey(absolute, documentRaw);
                    if (!anchors.count(std::wstring(documentKey))) AddAnchorIdMap(targetCtx, documentRaw, UniqueContentId(NormalizeFb2Id(documentRaw)));
                    AddDocumentHrefMap(targetCtx, CStringW(anchors[std::wstring(documentKey)].c_str()));
                }
                g_content.linkedNonSpineDocuments.emplace(targetKey, targetItem);
            }
        }
        for (const auto& entry : package.manifest)
        {
            const auto doc = documents.find(entry.first);
            if (doc == documents.end()) continue;
            const auto& item = entry.second;
            const auto root = MsxmlGetDocumentElement(doc->second);
            std::vector<MSXML2::IXMLDOMNodePtr> links;CollectElementsByLocalName(root, L"a", links);
            for (const auto& link : links)
            {
                const CStringW href = Attr(link, L"href");
                const CStringW fragment = HrefFragment(href);
                if (fragment.IsEmpty() || IsExternalHref(href)) continue;
                const int hash = href.Find(L'#');
                const CStringW path = hash == 0 ? item.absPath : CombineAndCanonicalizePath(DirectoryOf(item.absPath), href.Left(hash));
                if (path.IsEmpty()) continue;
                CStringW marker = TrimString(NodeText(link));
                marker.Trim(L"[]() .");
                bool numeric = !marker.IsEmpty() && marker.GetLength() <= 8;
                for (int i = 0; i < marker.GetLength(); ++i) numeric = numeric && iswdigit(marker[i]) != 0;
                bool symbolic = !marker.IsEmpty() && marker.GetLength() <= 3;
                for (int i = 0; i < marker.GetLength(); ++i)
                    symbolic = symbolic && (marker[i] == L'*' || marker[i] == 0x2020 || marker[i] == 0x2021 || marker[i] == 0x00A7);
                const bool letter = marker.GetLength() == 1 && ((marker[0] >= L'a' && marker[0] <= L'z') ||
                    (marker[0] >= L'A' && marker[0] <= L'Z') || (marker[0] >= 0x0410 && marker[0] <= 0x044F) || marker[0] == 0x0401 || marker[0] == 0x0451);
                const CStringW filename = LowerMapKey(FileNameOnly(path));
                const bool noteFile = filename == L"notes" || filename == L"links" || filename == L"footnotes" || filename == L"endnotes";
                const bool externalDocument = path.CompareNoCase(item.absPath) != 0;
                bool backlink = false;
                const auto target = nodes.find(std::wstring(PathAnchorMapKey(path, fragment)));
                const CStringW sourceId = RawAnchorId(link);
                bool noteShape = false;
                if (!IsNoteReferenceElement(link) && IsWithinContents(link)) continue;
                if (!IsNoteReferenceElement(link) && spinePaths.count(LowerMapKey(item.absPath).GetString()) &&
                    IsAnswerListEntry(link)) continue;
                if (target != nodes.end())
                {
                    // A numeric index entry may point into a note to a page
                    // marker, rather than reference that note itself. Some
                    // EPUBs use plain empty anchors such as pg_255 for this.
                    CStringW pageId(fragment); pageId.MakeLower();
                    CStringW pageNumber;
                    if (pageId.Left(3) == L"pg_") pageNumber = pageId.Mid(3);
                    else if (pageId.Left(5) == L"page_") pageNumber = pageId.Mid(5);
                    else if (pageId.Left(4) == L"page") pageNumber = pageId.Mid(4);
                    bool conventionalPage = !pageNumber.IsEmpty() && pageNumber == marker &&
                        TrimString(NodeText(target->second)).IsEmpty() &&
                        (LocalName(target->second) == L"a" || LocalName(target->second) == L"span");
                    for (int i = 0; conventionalPage && i < pageNumber.GetLength(); ++i)
                        conventionalPage = iswdigit(pageNumber[i]) != 0;
                    if (!IsNoteReferenceElement(link) &&
                        (LooksLikePageBreak(target->second) || IsRoleToken(target->second, L"doc-pagebreak") || conventionalPage))
                    {
                        g_content.navigationTargets.emplace(PathAnchorMapKey(path, fragment).GetString());
                        continue;
                    }
                    if (!IsNoteReferenceElement(link) && spinePaths.count(LowerMapKey(path).GetString()) &&
                        IsAnswerListEntry(target->second)) continue;
                    // Quiz and answer sections often repeat a numbered
                    // question and cross-link their list entries in both
                    // directions. They are ordinary spine content, not notes.
                    const auto sourceParagraph = ParentNode(link);
                    const auto targetParagraph = ParentNode(target->second);
                    const auto sourceItem = ParentNode(sourceParagraph);
                    const auto targetItem = ParentNode(targetParagraph);
                    const auto hasAnswerParagraph = [](const MSXML2::IXMLDOMNodePtr& listItem) {
                        const auto children = MsxmlGetChildNodes(listItem);
                        for (long i = 0, n = children ? MsxmlGetNodeListLength(children) : 0; i < n; ++i)
                        {
                            const auto part = MsxmlGetNodeListItem(children, i);
                            if (LocalName(part) != L"p") continue;
                            CStringW text = TrimString(NodeText(part)); text.MakeLower();
                            if (text.Left(7) == L"answer:" || text.Left(7) == L"answer ") return true;
                        }
                        return false;
                    };
                    const auto hasPre = [](const MSXML2::IXMLDOMNodePtr& listItem) {
                        const auto children = MsxmlGetChildNodes(listItem);
                        for (long i = 0, n = children ? MsxmlGetNodeListLength(children) : 0; i < n; ++i)
                            if (LocalName(MsxmlGetNodeListItem(children, i)) == L"pre") return true;
                        return false;
                    };
                    const CStringW reverseHref = Attr(target->second, L"href");
                    const int reverseHash = reverseHref.Find(L'#');
                    const CStringW reversePath = reverseHash == 0 ? path : reverseHash > 0 ?
                        CombineAndCanonicalizePath(DirectoryOf(path), reverseHref.Left(reverseHash)) : CStringW();
                    const bool reciprocal = !sourceId.IsEmpty() && !IsExternalHref(reverseHref) &&
                        HrefFragment(reverseHref) == sourceId && reversePath.CompareNoCase(item.absPath) == 0;
                    if (!IsNoteReferenceElement(link) && externalDocument &&
                        spinePaths.count(LowerMapKey(item.absPath).GetString()) &&
                        spinePaths.count(LowerMapKey(path).GetString()) &&
                        LocalName(target->second) == L"a" &&
                        LocalName(sourceParagraph) == L"p" && LocalName(targetParagraph) == L"p" &&
                        LocalName(sourceItem) == L"li" && LocalName(targetItem) == L"li" &&
                        !IsEndnoteElement(sourceItem) && !IsEndnoteElement(targetItem) && reciprocal &&
                        !TrimString(NodeText(sourceParagraph)).IsEmpty() &&
                        (TrimString(NodeText(sourceParagraph)) == TrimString(NodeText(targetParagraph)) ||
                         hasAnswerParagraph(sourceItem) || hasAnswerParagraph(targetItem) ||
                         (hasPre(sourceItem) && hasPre(targetItem))))
                        continue;
                    // Numbered citations in a spine bibliography are links
                    // to entries, not a request to relocate them into notes.
                    if (!IsNoteReferenceElement(link) &&
                        spinePaths.count(LowerMapKey(path).GetString()) && IsBibliographyEntry(target->second, numeric))
                        continue;
                    // A marker may contain a link to its own enclosing anchor.
                    // It cannot be a reference to a separate note; treating the
                    // self-link as a backlink moves its paragraph into notes.
                    bool selfTarget = false;
                    for (auto current = link; current; current = ParentNode(current))
                        if (current == target->second) { selfTarget = true; break; }
                    if (selfTarget) continue;
                    // Chapter headings can put an empty target anchor next to
                    // a numbered link back to that same anchor. The link is
                    // navigation within its own paragraph, not a footnote.
                    if (!IsNoteReferenceElement(link) && !externalDocument &&
                        LocalName(target->second) == L"a" &&
                        Attr(target->second, L"href").IsEmpty() &&
                        TrimString(NodeText(target->second)).IsEmpty() &&
                        ParentNode(link) == ParentNode(target->second) &&
                        LocalName(ParentNode(link)) == L"p")
                        continue;
                    if (!IsNoteReferenceElement(link) &&
                        (IsEpubType(target->second, L"chapter") || IsRoleToken(target->second, L"doc-chapter") ||
                         HasSemanticCssClass(target->second, L"chapter")))
                        continue;
                    // A reference in another document may point to the inline
                    // superscript marker, not to the footnote itself. Moving
                    // its containing paragraph to notes would lose main text.
                    if (!IsNoteReferenceElement(link) && IsInsideSuperscript(target->second) &&
                        HasTextBeforeInBlock(target->second))
                        continue;
                    // A chapter heading may link back to its entry in a
                    // visible table of contents. That entry is navigation,
                    // even when the heading consists of a numeric link.
                    bool targetIsContents = false;
                    MSXML2::IXMLDOMNodePtr ancestor = target->second;
                    while (ancestor)
                    {
                        const CStringW css = LowerMapKey(Attr(ancestor, L"class"));
                        if (IsEpubType(ancestor, L"toc") || IsRoleToken(ancestor, L"doc-toc") ||
                            HasSemanticCssClass(ancestor, L"toc") || css.Left(4) == L"toc_")
                        {
                            targetIsContents = true;
                            break;
                        }
                        IXMLDOMNode* rawAncestor = nullptr;
                        ancestor->get_parentNode(&rawAncestor);
                        ancestor = MSXML2::IXMLDOMNodePtr(rawAncestor, false);
                    }
                    // Some publishers use a plain toc.xhtml with private CSS
                    // names such as tocCt and reciprocal chapter links. A
                    // numbered heading linking back to that entry is still
                    // navigation; reciprocal notes in other files remain notes.
                    if (!IsNoteReferenceElement(link) && reciprocal &&
                        !IsEndnoteElement(LogicalNoteContainer(target->second, path)) &&
                        (filename == L"toc" || filename == L"contents"))
                        targetIsContents = true;
                    if (targetIsContents)
                    {
                        g_content.navigationTargets.emplace(PathAnchorMapKey(path, fragment).GetString());
                        continue;
                    }
                    const CStringW targetName = LocalName(target->second);
                    noteShape = targetName == L"p" || targetName == L"li" || targetName == L"div" ||
                        targetName == L"aside" || targetName == L"section" || targetName == L"article" ||
                        targetName == L"a" || targetName == L"span";
                    if (FindFirstHeadingChild(target->second) && !noteFile) noteShape = false;
                }
                std::vector<MSXML2::IXMLDOMNodePtr> superscripts;CollectElementsByLocalName(link, L"sup", superscripts);
                IXMLDOMNode* rawParent = nullptr;link->get_parentNode(&rawParent);
                MSXML2::IXMLDOMNodePtr parent(rawParent, false);
                const bool superscript = !superscripts.empty() || LocalName(parent) == L"sup";
                std::vector<CStringW> sourceIds;
                if (!sourceId.IsEmpty()) sourceIds.push_back(sourceId);
                if (numeric || symbolic || letter)
                {
                    // Some notes put the target ID on a sibling empty anchor,
                    // rather than on the backlink itself.
                    bool foundBlock = false;
                    int ownerDepth = 0;
                    for (auto owner = ParentNode(link); owner && ownerDepth++ < 12; owner = ParentNode(owner))
                    {
                        const CStringW ownerName = LocalName(owner);
                        if (ownerName == L"body" || IsHeadingName(ownerName)) break;
                        if (!foundBlock && (ownerName == L"p" || ownerName == L"li"))
                        {
                            const auto ownerIds = CollectRawAnchorIds(owner);
                            sourceIds.insert(sourceIds.end(), ownerIds.begin(), ownerIds.end());
                            foundBlock = true;
                        }
                        else if (foundBlock)
                        {
                            const CStringW ownerId = RawAnchorId(owner);
                            if (!ownerId.IsEmpty()) sourceIds.push_back(ownerId);
                        }
                    }
                }
                // Word-generated notes commonly use paired _ednN / _ednrefN
                // (or _ftnN / _ftnrefN) identifiers. The latter is a return
                // link even when the source uses a cross-document href or a
                // duplicate reference ID; it must not become another note.
                CStringW lowerFragment(fragment); lowerFragment.MakeLower();
                bool conventionalBacklink = false;
                for (const auto& sourceAnchor : sourceIds)
                {
                    CStringW lowerSource(sourceAnchor); lowerSource.MakeLower();
                    if ((lowerFragment.Left(7) == L"_ednref" && lowerFragment.GetLength() > 7 &&
                         lowerSource == CStringW(L"_edn") + lowerFragment.Mid(7)) ||
                        (lowerFragment.Left(7) == L"_ftnref" && lowerFragment.GetLength() > 7 &&
                         lowerSource == CStringW(L"_ftn") + lowerFragment.Mid(7)))
                    { conventionalBacklink = true; break; }
                }
                if (conventionalBacklink && !IsNoteReferenceElement(link)) continue;
                if (target != nodes.end() && !sourceIds.empty())
                {
                    std::vector<MSXML2::IXMLDOMNodePtr> backLinks;
                    CollectElementsByLocalName(LogicalNoteContainer(target->second, path), L"a", backLinks);
                    for (const auto& back : backLinks)
                    {
                        const CStringW backHref = Attr(back, L"href");
                        const CStringW backFragment = HrefFragment(backHref);
                        bool matchesSource = false;
                        for (const auto& sourceAnchor : sourceIds)
                            if (backFragment == sourceAnchor) { matchesSource = true; break; }
                        if (IsExternalHref(backHref) || !matchesSource) continue;
                        const int backHash = backHref.Find(L'#');
                        const CStringW backPath = backHash == 0 ? path : CombineAndCanonicalizePath(DirectoryOf(path), backHref.Left(backHash));
                        backlink = backlink || backPath.CompareNoCase(item.absPath) == 0;
                    }
                }
                // A numeric link from an unmarked notes document back to a
                // spine reference can have the same bidirectional shape as a
                // noteref. Treating both directions as notes pulls the entire
                // surrounding chapter paragraph into body name="notes".
                const CStringW sourceName = LowerMapKey(FileNameOnly(item.absPath));
                const bool sourceNotesFile = sourceName == L"notes" || sourceName == L"footnotes" || sourceName == L"endnotes" ||
                    sourceName.Right(6) == L"_notes" || sourceName.Right(6) == L"-notes" ||
                    sourceName.Right(10) == L"_footnotes" || sourceName.Right(10) == L"-footnotes" ||
                    sourceName.Right(9) == L"_endnotes" || sourceName.Right(9) == L"-endnotes";
                const bool explicitBacklink = IsEpubType(link, L"backlink") || IsRoleToken(link, L"doc-backlink") ||
                    HasSemanticCssClass(link, L"backlink") || HasSemanticCssClass(link, L"backlinks") || HasSemanticCssClass(link, L"fn-backref");
                const CStringW targetText = !backlink || target == nodes.end() ? CStringW() :
                    TrimString(NodeText(LogicalNoteContainer(target->second, path)));
                int proseStart = 0;
                while (proseStart < targetText.GetLength() &&
                    (iswspace(targetText[proseStart]) || targetText[proseStart] == L'\"' ||
                     targetText[proseStart] == L'\'' || targetText[proseStart] == L'(' ||
                     targetText[proseStart] == L'.' || targetText[proseStart] == L'-' ||
                     targetText[proseStart] == 0x2018 || targetText[proseStart] == 0x2019 ||
                     targetText[proseStart] == 0x201C || targetText[proseStart] == 0x201D ||
                     targetText[proseStart] == 0x00AB || targetText[proseStart] == 0x2013 ||
                     targetText[proseStart] == 0x2014 || targetText[proseStart] == 0x2026))
                    ++proseStart;
                const bool targetStartsWithProse = proseStart < targetText.GetLength() && iswalpha(targetText[proseStart]) != 0;
                const bool startsNoteButTargetFollowsText = backlink && target != nodes.end() &&
                    !HasTextBeforeInBlock(link, true) && (HasTextBeforeInBlock(target->second) || targetStartsWithProse);
                if (explicitBacklink || (backlink && (sourceNotesFile || startsNoteButTargetFollowsText) && !noteFile &&
                    spinePaths.count(LowerMapKey(path).GetString()) && !IsNoteReferenceElement(link)))
                    continue;
                const bool marked = numeric || ((symbolic || letter) && (noteFile || backlink ||
                    (target != nodes.end() && IsEndnoteElement(LogicalNoteContainer(target->second, path)))));
                if (IsNoteReferenceElement(link) || (marked &&
                    ((noteShape && ((externalDocument && !spinePaths.count(LowerMapKey(path).GetString())) || noteFile || superscript || backlink)) ||
                     (target == nodes.end() && noteFile))))
                    g_content.requestedNotes.emplace(PathAnchorMapKey(path, fragment).GetString());
            }
        }
        // Publish every selected note target before rendering any note content.
        if (g_options.importNotes) for (const auto& entry : package.manifest)
        {
            const auto doc = documents.find(entry.first);
            if (doc == documents.end()) continue;
            std::vector<MSXML2::IXMLDOMNodePtr> notes;
            SourceNoteIndex index(doc->second);
            CollectEndnoteElements(MsxmlGetDocumentElement(doc->second), notes, entry.second.absPath);
            for (const auto& note : notes)
            {
                const CStringW key = PathAnchorMapKey(entry.second.absPath, NoteAnchorId(note, entry.second.absPath));
                const auto id = anchors.find(std::wstring(key));
                if (id == anchors.end()) continue;
                g_content.noteLinks.emplace(key.GetString(), CStringW(id->second.c_str()));
                g_content.noteTargets.emplace(key.GetString());
                g_content.emittedIds.emplace(id->second);
                for (const auto& raw : CollectRawAnchorIds(note))
                {
                    g_content.noteContentAnchors.emplace(PathAnchorMapKey(entry.second.absPath, raw).GetString());
                    const CStringW alias = PathAnchorMapKey(entry.second.absPath, raw);
                    if (g_content.requestedNotes.count(std::wstring(alias)))
                        g_content.noteLinks[std::wstring(alias)] = CStringW(id->second.c_str());
                }
            }
            const CStringW path = LowerMapKey(entry.second.absPath);
            if (!notes.empty() && !spinePaths.count(std::wstring(path)) &&
                HasMainContent(FindFirstElementByLocalName(MsxmlGetDocumentElement(doc->second), L"body"), entry.second.absPath))
            {
                g_content.residualDocuments.emplace(std::wstring(path));
                // Publish before rendering notes or main chapters, including
                // forward links to residual anchors outside the original spine.
                g_content.mainDocuments.emplace(std::wstring(path));
            }
        }
    }

    CStringW BuildNoteContent(const MSXML2::IXMLDOMNodePtr& noteNode, const ConvertContext& ctx)
    {
        CStringW content;
        ConvertContext noteContext = ctx;noteContext.collectingNotes = true;
        const CStringW name = LocalName(noteNode);
        // A table used as one note is flattened through its rows below. Emit
        // descendant anchors first so links back to nested references survive.
        if (name == L"table")
            content += AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(noteNode), 0, noteContext);
        if (name == L"p")
        {
            CStringW extra;
            const CStringW id = OptionalIdAttributeAndExtraMarkersIncludingDescendant(noteNode, noteContext, extra);
            return extra + L"      <p" + id + L">" + ChildrenInlineToFb2(noteNode, noteContext) + L"</p>\r\n";
        }
        if (name == L"li")
        {
            bool blocks = false;
            const auto items = MsxmlGetChildNodes(noteNode);
            const long count = items ? MsxmlGetNodeListLength(items) : 0;
            for (long i = 0; i < count; ++i)
            {
                const CStringW childName = LocalName(MsxmlGetNodeListItem(items, i));
                blocks = blocks || childName == L"p" || childName == L"div" || childName == L"section" || childName == L"ol" || childName == L"ul";
            }
            if (!blocks)
            {
                const CStringW markers = AnchorMarkerParagraphsFromRawIds(CollectRawAnchorIds(noteNode), 0, noteContext);
                return markers + L"      <p>" + ChildrenInlineToFb2(noteNode, noteContext) + L"</p>\r\n";
            }
        }
        MSXML2::IXMLDOMNodePtr firstHeading = FindFirstHeadingChild(noteNode);

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(noteNode);
        if (!children)
            return content;

        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (child.GetInterfacePtr() == firstHeading.GetInterfacePtr())
                continue;
            AppendBlockNode(child, content, noteContext);
        }
        return content;
    }

    bool CollectNotesFromXhtmlFile(
        const CStringW& xhtmlPath,
        const ConvertContext& ctx,
        std::vector<EpubNote>& notes,
        std::set<std::wstring>& noteIds,
        bool& isNotesDocument,
        CStringW& warning)
    {
        isNotesDocument = false;

        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (!LoadXmlFile(xhtmlPath, doc, error))
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XHTML_NOTES_CHECK_FAILED, L"Could not check XHTML for notes: ") + xhtmlPath + L"\r\n";
            if (!error.IsEmpty())
                warning += error + L"\r\n";
            return false;
        }

        ScopedCssFontRules cssRules(doc, xhtmlPath, ctx, warning);
        MSXML2::IXMLDOMNodePtr body = FindFirstElementByLocalName(MsxmlGetDocumentElement(doc), L"body");
        if (!body)
            return false;

        SourceNoteIndex index(doc);
        std::vector<MSXML2::IXMLDOMNodePtr> noteNodes;
        CollectEndnoteElements(body, noteNodes, xhtmlPath);

        for (const auto& node : noteNodes)
        {
            const CStringW key = PathAnchorMapKey(xhtmlPath, NoteAnchorId(node, xhtmlPath));
            const auto id = ctx.anchorIdMap->find(std::wstring(key));
            if (id == ctx.anchorIdMap->end()) continue;
            g_content.noteLinks[std::wstring(key)] = CStringW(id->second.c_str());
            g_content.noteTargets.emplace(key.GetString());
            g_content.emittedIds.emplace(id->second);
        }

        for (size_t i = 0; i < noteNodes.size(); ++i)
        {
            CStringW rawId = NoteAnchorId(noteNodes[i], xhtmlPath);
            const auto found = g_content.noteLinks.find(std::wstring(PathAnchorMapKey(xhtmlPath, rawId)));
            CStringW id = found == g_content.noteLinks.end() ? CStringW() : found->second;
            if (id.IsEmpty())
                continue;
            std::wstring noteKey(id.GetString());
            if (!noteIds.emplace(noteKey).second)
                continue;

            EpubNote note;
            note.id = id;

            MSXML2::IXMLDOMNodePtr titleNode = FindFirstHeadingChild(noteNodes[i]);
            if (titleNode)
                note.title = ChildrenInlineToFb2(titleNode, ctx);
            note.title.Trim();
            if (note.title.IsEmpty())
                note.title = XmlEscape(rawId);

            auto contentNode = noteNodes[i];
            const CStringW nodeName = LocalName(contentNode);
            if (nodeName == L"a" || nodeName == L"span")
            {
                IXMLDOMNode* parent = nullptr;
                if (SUCCEEDED(contentNode->get_parentNode(&parent)) && parent)
                {
                    MSXML2::IXMLDOMNodePtr parentNode(parent, false);
                    if (LocalName(parentNode) == L"p" || LocalName(parentNode) == L"li") contentNode = parentNode;
                }
            }
            note.content = BuildNoteContent(contentNode, ctx);
            for (const auto& continuation : FootNoteContinuations(contentNode))
                note.content += BuildNoteContent(continuation, ctx);
            note.content.Trim();
            if (note.content.IsEmpty())
            {
                warning += L"Empty or unsupported note: " + xhtmlPath + L"#" + rawId + L"\r\n";
                // The inventory reserved this identifier as a note before
                // rendering proved it empty. Release every alias so the
                // original heading or other target can still emit its anchor.
                g_content.emittedIds.erase(noteKey);
                noteIds.erase(noteKey);
                g_content.noteRoots.erase(std::wstring(NoteRootKey(noteNodes[i], xhtmlPath)));
                for (auto link = g_content.noteLinks.begin(); link != g_content.noteLinks.end();)
                {
                    if (link->second == id)
                    {
                        g_content.noteTargets.erase(link->first);
                        g_content.noteContentAnchors.erase(link->first);
                        link = g_content.noteLinks.erase(link);
                    }
                    else ++link;
                }
                continue;
            }

            notes.push_back(note);
        }

        bool hasRetainedNotes = false;
        for (const auto& node : noteNodes)
        {
            const CStringW key = PathAnchorMapKey(xhtmlPath, NoteAnchorId(node, xhtmlPath));
            const auto note = g_content.noteLinks.find(std::wstring(key));
            if (note != g_content.noteLinks.end() && noteIds.count(std::wstring(note->second.GetString())))
            {
                hasRetainedNotes = true;
                break;
            }
        }
        isNotesDocument = hasRetainedNotes && !HasMainContent(body, xhtmlPath);
        if (isNotesDocument)
        {
            for (const auto& node : noteNodes)
            {
                const auto target = g_content.noteLinks.find(std::wstring(PathAnchorMapKey(xhtmlPath, NoteAnchorId(node, xhtmlPath))));
                if (target == g_content.noteLinks.end()) continue;
                (*ctx.anchorIdMap)[std::wstring(LowerMapKey(xhtmlPath))] = std::wstring(target->second);
                break;
            }
        }

        return !noteNodes.empty();
    }

    bool IsImageOnlyCoverPage(const ManifestItem& item)
    {
        if (IsImageMediaType(item.mediaType) || IsImageHrefByExtension(item.href)) return true;
        const std::wstring key(LowerMapKey(item.absPath).GetString());
        const auto cached = g_content.imageOnlyCoverPages.find(key);
        if (cached != g_content.imageOnlyCoverPages.end()) return cached->second;

        bool imageOnly = false;
        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (LoadXmlFile(item.absPath, doc, error))
        {
            const auto body = FindFirstElementByLocalName(MsxmlGetDocumentElement(doc), L"body");
            if (body)
            {
                const CStringW text = NodeText(body);
                imageOnly = true;
                for (int i = 0; i < text.GetLength(); ++i)
                    if (!iswspace(text[i]) && text[i] != 0x00a0 && text[i] != 0xfeff && text[i] != 0x200b)
                    {
                        imageOnly = false;
                        break;
                    }
                if (imageOnly)
                {
                    std::vector<MSXML2::IXMLDOMNodePtr> images;
                    CollectElementsByLocalName(body, L"img", images);
                    CollectElementsByLocalName(body, L"image", images);
                    CollectElementsByLocalName(body, L"video", images);
                    CollectElementsByLocalName(body, L"object", images);
                    imageOnly = images.size() == 1;
                    if (!imageOnly)
                        g_content.warnings += L"Cover-labelled XHTML has multiple or unclassified visual resources and was retained.\r\n";
                }
                else
                    g_content.warnings += L"Cover-labelled XHTML contains text and was retained.\r\n";
            }
        }
        g_content.imageOnlyCoverPages.emplace(key, imageOnly);
        return imageOnly;
    }

    bool IsLikelyCoverSpineItem(const ManifestItem& item, const EpubPackage& package)
    {
        if (!g_options.skipServicePages)
            return false;

        CStringW href = item.href;
        href.MakeLower();
        CStringW props = item.properties;
        props.MakeLower();
        CStringW id = item.id;
        id.MakeLower();
        CStringW stem = FileNameOnly(href);
        const int extension = stem.ReverseFind(L'.');
        if (extension >= 0) stem = stem.Left(extension);

        if (g_options.removeServiceSections &&
            package.guideServiceDocuments.find(std::wstring(item.absPath.GetString())) != package.guideServiceDocuments.end())
            return true;

        if (item.mediaType.CompareNoCase(L"application/x-dtbncx+xml") == 0)
            return true;

        if (g_options.removeServiceSections)
        {
            if (id == L"nav" || id == L"toc" || id == L"ncx" || IsTokenInList(props, L"nav") ||
                stem == L"nav" || stem == L"toc" || stem == L"contents")
                return true;
            if (stem == L"titlepage" || stem == L"title-page" || stem == L"landmarks" ||
                stem == L"page-list" || stem == L"copyright" || stem == L"calibre")
                return true;
            if (id == L"titlepage" || id == L"landmarks" || id == L"pagelist")
                return true;
        }

        bool cover = item.id.CompareNoCase(L"cover") == 0 || IsTokenInList(props, L"cover");
        if (!package.coverId.IsEmpty())
        {
            const ManifestItem* coverItem = FindManifestItemById(package, package.coverId);
            cover = cover || (coverItem && item.href.CompareNoCase(coverItem->href) == 0);
        }
        cover = cover || (stem == L"cover" && (href.Right(6) == L".xhtml" || href.Right(5) == L".html"));
        if (!cover || !IsImageOnlyCoverPage(item)) return false;
        const ManifestItem* selected = ResolveCoverImageItem(package);
        const ManifestItem* pageImage = (IsImageMediaType(item.mediaType) || IsImageHrefByExtension(item.href))
            ? &item : FirstImageInXhtml(item, package);
        return selected && pageImage && selected->absPath.CompareNoCase(pageImage->absPath) == 0;
    }

    CStringW BuildNotesBodyXml(const std::vector<EpubNote>& notes)
    {
        if (notes.empty())
            return CStringW();

        CStringW xml;
        xml += L"  <body name=\"notes\">\r\n";
        for (size_t i = 0; i < notes.size(); ++i)
        {
            xml += L"    <section id=\"" + XmlEscape(notes[i].id) + L"\">\r\n";
            if (!notes[i].title.IsEmpty())
                xml += L"      <title><p>" + notes[i].title + L"</p></title>\r\n";
            xml += notes[i].content;
            xml += L"    </section>\r\n";
        }
        xml += L"  </body>\r\n";
        return xml;
    }


    void AppendFb2Section(CStringW& out, const CStringW& title, const CStringW& content, const CStringW& idAttr = CStringW())
    {
        CStringW cleanTitle(title);
        cleanTitle.Trim();

        out += L"    <section" + idAttr + L">\r\n";
        if (!cleanTitle.IsEmpty())
            out += L"      <title><p>" + cleanTitle + L"</p></title>\r\n";
        out += content;
        out += L"    </section>\r\n";
    }


    bool ConvertImageSpineItemToFb2Section(const ManifestItem& item, const CStringW& fallbackTitle, const ConvertContext& ctx, CStringW& outSection, CStringW& warning)
    {
        if (!g_options.importImages)
            return false;

        if (!IsImageMediaType(item.mediaType) && !IsImageHrefByExtension(item.href))
            return false;
        if (IsSvgImage(item) && g_options.svgConversionMode == SVG_IMPORT_SKIP)
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SVG_SPINE_SKIPPED, L"SVG spine item skipped by import option: ") + item.href + L"\r\n";
            return false;
        }

        if (!FileExists(item.absPath))
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SPINE_IMAGE_MISSING, L"Image file from spine was not found: ") + item.href + L"\r\n";
            return false;
        }

        CStringW title(fallbackTitle);
        title.Trim();
        if (title.IsEmpty())
            title = FileNameOnly(item.href);
        if (title.IsEmpty())
            title = item.id;
        title.Trim();

        CStringW id = BinaryIdForImageItem(item);
        if (ctx.usedImageIds)
            ctx.usedImageIds->insert(std::wstring(item.id.GetString()));

        CStringW content;
        content += L"      <p><image l:href=\"#" + XmlEscape(id) + L"\"/></p>\r\n";

        AppendFb2Section(outSection, XmlEscape(title), content);
        return true;
    }

    bool TryConvertBodyToSplitSections(const MSXML2::IXMLDOMNodePtr& body, const CStringW& fallbackTitle, const ConvertContext& ctx, CStringW& outSection)
    {
        if (!g_options.splitSectionsByHeadings || !body)
            return false;

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(body);
        long count = children ? MsxmlGetNodeListLength(children) : 0;

        bool sawHeading = false;
        for (long i = 0; i < count; ++i)
            sawHeading = sawHeading || IsHeadingName(LocalName(MsxmlGetNodeListItem(children, i)));
        if (!sawHeading) return false;
        struct Section { int level; CStringW title, content, id; };
        std::vector<Section> stack;
        const int initialLength = outSection.GetLength();
        CStringW preface;
        auto close = [&]() {
            const Section section = stack.back();stack.pop_back();
            CStringW xml;AppendFb2Section(xml, section.title, section.content, section.id);
            if (stack.empty()) outSection += xml;else stack.back().content += xml;
        };
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            CStringW name = LocalName(child);

            if (child && MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT && IsHeadingName(name))
            {
                CStringW heading = ChildrenInlineToFb2(child, ctx);
                heading.Trim();
                if (heading.IsEmpty())
                    heading = XmlEscape(fallbackTitle);

                const int level = name[1] - L'0';
                while (!stack.empty() && stack.back().level >= level) close();
                if (!preface.IsEmpty())
                {
                    CStringW prefaceId;
                    if (outSection.GetLength() == initialLength && stack.empty() && ctx.anchorIdMap)
                    {
                        const CStringW filename = FileNameOnly(ctx.currentXhtmlPath);
                        const auto document = ctx.anchorIdMap->find(std::wstring(LowerMapKey(ctx.currentXhtmlPath)));
                        const auto filenameAnchor = ctx.anchorIdMap->find(std::wstring(PathAnchorMapKey(ctx.currentXhtmlPath, filename)));
                        if (document != ctx.anchorIdMap->end() && filenameAnchor != ctx.anchorIdMap->end() &&
                            document->second == filenameAnchor->second)
                            prefaceId = OptionalIdAttributeFromRaw(filename, ctx);
                    }
                    AppendFb2Section(outSection, XmlEscape(fallbackTitle), preface, prefaceId);
                    preface.Empty();
                }
                Section section = {};section.level = level;section.title = heading;
                section.id = OptionalIdAttributeAndExtraMarkersIncludingDescendant(child, ctx, section.content);
                if (outSection.GetLength() == initialLength && stack.empty())
                {
                    if (section.id.IsEmpty()) section.id = OptionalIdAttributeFromRaw(FileNameOnly(ctx.currentXhtmlPath), ctx);
                    AddDocumentHrefMap(ctx, IdFromAttributeText(section.id));
                }
                stack.push_back(section);
                continue;
            }

            AppendBlockNode(child, stack.empty() ? preface : stack.back().content, ctx);
        }

        while (!stack.empty()) close();
        return true;
    }

    bool ConvertXhtmlFileToFb2Section(const CStringW& xhtmlPath, const CStringW& fallbackTitle, const ConvertContext& ctx, CStringW& outSection, CStringW& warning, bool supplementary = false)
    {
        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (!LoadXmlFile(xhtmlPath, doc, error))
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XHTML_READ_FAILED, L"Could not read XHTML: ") + xhtmlPath + L"\r\n";
            if (!error.IsEmpty())
                warning += error + L"\r\n";
            return false;
        }

        ScopedCssFontRules cssRules(doc, xhtmlPath, ctx, warning);
        SourceNoteIndex index(doc);
        MSXML2::IXMLDOMNodePtr body = FindFirstElementByLocalName(MsxmlGetDocumentElement(doc), L"body");
        if (!body)
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XHTML_BODY_MISSING, L"No body found in XHTML: ") + xhtmlPath + L"\r\n";
            return false;
        }

        if (g_options.importNotes && IsEndnotesBody(body) && !HasMainContent(body, xhtmlPath))
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_NOTES_BODY_MOVED, L"Footnote file moved to body name=notes: ") + xhtmlPath + L"\r\n";
            return false;
        }

        CStringW splitSections;
        if (TryConvertBodyToSplitSections(body, fallbackTitle, ctx, splitSections))
        {
            const CStringW bodyId = OptionalIdAttribute(body, ctx);
            if (!bodyId.IsEmpty())
            {
                CStringW wrapped;AppendFb2Section(wrapped, CStringW(), splitSections, bodyId);splitSections = wrapped;
            }
            outSection += splitSections;
            return true;
        }

        CStringW content;
        CStringW sectionTitle;
        CStringW sectionIdAttr = OptionalIdAttribute(body, ctx);
        if (sectionIdAttr.IsEmpty())
            sectionIdAttr = OptionalIdAttributeFromRaw(FileNameOnly(xhtmlPath), ctx);
        AddDocumentHrefMap(ctx, IdFromAttributeText(sectionIdAttr));

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(body);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        bool titleTaken = false;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            CStringW name = LocalName(child);
            if (!titleTaken && MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT && IsHeadingName(name))
            {
                sectionTitle = ChildrenInlineToFb2(child, ctx);
                sectionTitle.Trim();
                CStringW headingExtraMarkers;
                if (sectionIdAttr.IsEmpty())
                    sectionIdAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(child, ctx, headingExtraMarkers);
                else
                {
                    const CStringW headingId = OptionalIdAttributeAndExtraMarkersIncludingDescendant(child, ctx, headingExtraMarkers);
                    if (!headingId.IsEmpty()) headingExtraMarkers = L"      <p" + headingId + L"></p>\r\n" + headingExtraMarkers;
                }
                content += headingExtraMarkers;
                titleTaken = !sectionTitle.IsEmpty();
                continue;
            }
            AppendBlockNode(child, content, ctx);
        }

        bool suppressGenericTitle = false;
        if (sectionTitle.IsEmpty())
        {
            std::vector<MSXML2::IXMLDOMNodePtr> titles;
            CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"title", titles);
            if (!titles.empty())
            {
                const CStringW documentTitle = TrimString(NodeText(titles[0]));
                const auto repeated = g_content.spineDocumentTitleCounts.find(std::wstring(LowerMapKey(documentTitle)));
                suppressGenericTitle = ctx.package && !documentTitle.IsEmpty() &&
                    repeated != g_content.spineDocumentTitleCounts.end() && repeated->second > 1 &&
                    repeated->second * 2 >= g_content.spineXhtmlDocuments &&
                    (documentTitle.GetLength() >= 4 || documentTitle.CompareNoCase(ctx.package->title) == 0) &&
                    ContainsNoCase(ctx.package->title, documentTitle);
                if (!suppressGenericTitle) sectionTitle = XmlEscape(documentTitle);
            }
        }

        if (sectionTitle.IsEmpty() && !suppressGenericTitle)
            sectionTitle = XmlEscape(fallbackTitle);

        const int sectionOffset = outSection.GetLength();
        const bool firstGeneratedBookTitle = !supplementary && sectionOffset == 0 && !titleTaken && ctx.package &&
            !sectionTitle.IsEmpty() && sectionTitle.CompareNoCase(XmlEscape(ctx.package->title)) == 0;
        AppendFb2Section(outSection, sectionTitle, content, sectionIdAttr);
        if (firstGeneratedBookTitle)
        {
            const CStringW generatedTitle = L"      <title><p>" + sectionTitle + L"</p></title>\r\n";
            const int offset = outSection.Find(generatedTitle, sectionOffset);
            if (offset >= 0) { g_content.generatedBookTitleOffset = offset; g_content.generatedBookTitleLength = generatedTitle.GetLength(); }
        }
        return true;
    }

    CStringW CompactContentText(const CStringW& text)
    {
        CStringW result;
        const CStringW normalized = NormalizeImportedText(text);
        for (int i = 0; i < normalized.GetLength(); ++i)
            if (!iswspace(normalized[i])) result += normalized[i];
        return result;
    }

    void CollectResidualContent(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx,
        std::vector<CStringW>& text, unsigned& fragments, unsigned& images)
    {
        if (!node || IsHiddenElement(node) || IsBacklinkElement(node) || IsCollectedNoteNode(node, ctx)) return;
        const CStringW name = LocalName(node);
        if (name == L"script" || name == L"style" || name == L"head" ||
            (LooksLikePageBreak(node) && !PageBreakHasContent(node) && !g_options.importPageBreaks)) return;
        if (name == L"img" || name == L"image" || name == L"picture" || name == L"object")
        {
            if (g_options.importImages)
            {
                bool filteredSvg = false, available = false;
                std::vector<CStringW> candidates;ImageCandidates(node, candidates);
                for (size_t i = 0; i < candidates.size(); ++i)
                {
                    const auto item = FindManifestImageByHref(ctx, candidates[i]);
                    const bool skippedSvg = item && IsSvgImage(*item) && g_options.svgConversionMode == SVG_IMPORT_SKIP;
                    if (i == 0) filteredSvg = skippedSvg;
                    available = available || (item && FileExists(item->absPath) && !skippedSvg);
                }
                if (!filteredSvg || available) {++fragments;++images;}
            }
            return; // Alternate image content follows the existing image options.
        }
        const auto type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION)
        {
            const CStringW value = CompactContentText(MsxmlGetNodeText(node));
            if (!value.IsEmpty()) {text.push_back(value);++fragments;}
            return;
        }
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i) CollectResidualContent(MsxmlGetNodeListItem(children, i), ctx, text, fragments, images);
    }

    bool RecoverNoteDocumentRemainder(const ManifestItem& item, const ConvertContext& ctx, CStringW& body, CStringW& warning)
    {
        MSXML2::IXMLDOMDocument2Ptr source;
        CStringW error;
        if (!LoadXmlFile(item.absPath, source, error))
        {warning += L"Cannot read residual note content: " + item.href + L"\r\n" + error;return false;}
        SourceNoteIndex index(source);
        std::vector<CStringW> expectedText;
        unsigned fragments = 0, expectedImages = 0;
        CollectResidualContent(FindFirstElementByLocalName(MsxmlGetDocumentElement(source), L"body"), ctx, expectedText, fragments, expectedImages);
        g_content.residualFragments += fragments;
        CStringW recovered;
        if (!ConvertXhtmlFileToFb2Section(item.absPath, CStringW(), ctx, recovered, warning) || recovered.IsEmpty())
        {warning += L"Cannot preserve residual note content: " + item.href + L"\r\n";return false;}
        MSXML2::IXMLDOMDocument2Ptr check;
        if (FAILED(CreateMsxmlDom(check))) return false;
        VARIANT_BOOL loaded = VARIANT_FALSE;
        const CStringW wrapped = L"<body xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" xmlns:l=\"http://www.w3.org/1999/xlink\">" + recovered + L"</body>";
        if (FAILED(check->loadXML(CComBSTR(wrapped), &loaded)) || loaded != VARIANT_TRUE) return false;
        std::vector<MSXML2::IXMLDOMNodePtr> images;
        CollectElementsByLocalName(MsxmlGetDocumentElement(check), L"image", images);
        if (images.size() != expectedImages)
        {warning += L"Residual note image integrity check failed: " + item.href + L"\r\n";return false;}
        const CStringW actualText = CompactContentText(NodeText(MsxmlGetDocumentElement(check)));
        int position = 0;
        for (const auto& expected : expectedText)
        {
            const int found = actualText.Find(expected, position);
            if (found < 0)
            {warning += L"Residual note text integrity check failed: " + item.href + L"\r\n";return false;}
            position = found + expected.GetLength();
        }
        AppendFb2Section(body, XmlEscape(item.href), recovered);
        g_content.recoveredFragments += fragments;
        warning += L"Preserved ambiguous residual note content in main body from: " + item.href + L"\r\n";
        return true;
    }

    unsigned long PngBigEndian(const unsigned char* p)
    {
        return (static_cast<unsigned long>(p[0]) << 24) | (static_cast<unsigned long>(p[1]) << 16) |
            (static_cast<unsigned long>(p[2]) << 8) | static_cast<unsigned long>(p[3]);
    }

    struct PngInflateGuard
    {
        z_stream& stream;
        explicit PngInflateGuard(z_stream& value) : stream(value) {}
        ~PngInflateGuard() { ::inflateEnd(&stream); }
    };

    bool OptimizePngLosslessly(const std::vector<unsigned char>& input, std::vector<unsigned char>& output)
    {
        const unsigned char signature[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        if (input.size() < 20 || input.size() > 64U * 1024 * 1024 || memcmp(input.data(), signature, 8) != 0) return false;
        std::vector<unsigned char> compressed;
        bool seenHeader = false, seenData = false, afterData = false, seenEnd = false;
        size_t pos = 8;
        while (pos < input.size())
        {
            if (input.size() - pos < 12) return false;
            const unsigned long length = PngBigEndian(input.data() + pos);
            if (length > input.size() - pos - 12) return false;
            const unsigned char* type = input.data() + pos + 4;
            const unsigned char* payload = type + 4;
            const size_t end = pos + 12 + length;
            uLong crc = ::crc32(0L, Z_NULL, 0);
            crc = ::crc32(crc, type, static_cast<uInt>(length + 4));
            if (crc != PngBigEndian(input.data() + pos + 8 + length)) return false;
            const bool header = memcmp(type, "IHDR", 4) == 0;
            const bool idat = memcmp(type, "IDAT", 4) == 0;
            const bool endChunk = memcmp(type, "IEND", 4) == 0;
            if (memcmp(type, "acTL", 4) == 0 || memcmp(type, "fcTL", 4) == 0 || memcmp(type, "fdAT", 4) == 0 ||
                memcmp(type, "dSIG", 4) == 0) return false;
            if (!seenHeader && (!header || length != 13)) return false;
            if (header) { if (seenHeader) return false; seenHeader = true; }
            const bool critical = (type[0] & 0x20) == 0;
            if (critical && !header && !idat && !endChunk && memcmp(type, "PLTE", 4) != 0) return false;
            if (!critical && (type[3] & 0x20) == 0 && memcmp(type, "iCCP", 4) != 0 &&
                memcmp(type, "cHRM", 4) != 0 && memcmp(type, "gAMA", 4) != 0 &&
                memcmp(type, "sRGB", 4) != 0 && memcmp(type, "sBIT", 4) != 0 &&
                memcmp(type, "tRNS", 4) != 0 && memcmp(type, "bKGD", 4) != 0 &&
                memcmp(type, "hIST", 4) != 0 && memcmp(type, "sPLT", 4) != 0 &&
                memcmp(type, "eXIf", 4) != 0) return false;
            if (idat)
            {
                if (afterData) return false;
                seenData = true;
                compressed.insert(compressed.end(), payload, payload + length);
            }
            else if (seenData) afterData = true;
            pos = end;
            if (endChunk) { if (length || pos != input.size()) return false; seenEnd = true; break; }
        }
        if (!seenData || !seenEnd || compressed.empty()) return false;
        z_stream stream = {};
        if (::inflateInit(&stream) != Z_OK) return false;
        PngInflateGuard streamGuard(stream);
        stream.next_in = compressed.data(); stream.avail_in = static_cast<uInt>(compressed.size());
        const size_t limit = 128U * 1024 * 1024;
        std::vector<unsigned char> raw;
        int status = Z_OK;
        while (status == Z_OK)
        {
            const size_t old = raw.size();
            if (old >= limit) return false;
            const size_t capacity = min(static_cast<size_t>(64 * 1024), limit - old);
            raw.resize(old + capacity);
            stream.next_out = raw.data() + old; stream.avail_out = static_cast<uInt>(capacity);
            status = ::inflate(&stream, Z_NO_FLUSH);
            const size_t produced = capacity - stream.avail_out;
            raw.resize(old + produced);
            if (status == Z_OK && produced == 0 && stream.avail_in == 0) return false;
        }
        const bool complete = status == Z_STREAM_END && stream.avail_in == 0;
        if (!complete || raw.empty()) return false;
        uLongf newSize = ::compressBound(static_cast<uLong>(raw.size()));
        std::vector<unsigned char> recompressed(newSize);
        if (::compress2(recompressed.data(), &newSize, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK ||
            newSize + 12 >= compressed.size()) return false;
        recompressed.resize(newSize);
        // Independently inflate the proposed replacement before committing it.
        z_stream verify = {};
        if (::inflateInit(&verify) != Z_OK) return false;
        PngInflateGuard verifyGuard(verify);
        verify.next_in = recompressed.data(); verify.avail_in = static_cast<uInt>(recompressed.size());
        unsigned char buffer[64 * 1024] = {}; size_t compared = 0; status = Z_OK;
        while (status == Z_OK)
        {
            verify.next_out = buffer; verify.avail_out = sizeof(buffer);
            status = ::inflate(&verify, Z_NO_FLUSH);
            const size_t produced = sizeof(buffer) - verify.avail_out;
            if (produced > raw.size() - compared || memcmp(buffer, raw.data() + compared, produced) != 0)
                return false;
            compared += produced;
            if (status == Z_OK && produced == 0 && verify.avail_in == 0) return false;
        }
        const bool identical = status == Z_STREAM_END && verify.avail_in == 0 && compared == raw.size();
        if (!identical) return false;
        output.clear(); output.reserve(input.size()); output.insert(output.end(), signature, signature + 8);
        bool inserted = false; pos = 8;
        while (pos < input.size())
        {
            const unsigned long length = PngBigEndian(input.data() + pos);
            const unsigned char* type = input.data() + pos + 4;
            const size_t end = pos + 12 + length;
            if (memcmp(type, "IDAT", 4) == 0)
            {
                if (!inserted)
                {
                    const unsigned long n = static_cast<unsigned long>(recompressed.size());
                    output.push_back(static_cast<unsigned char>(n >> 24)); output.push_back(static_cast<unsigned char>(n >> 16));
                    output.push_back(static_cast<unsigned char>(n >> 8)); output.push_back(static_cast<unsigned char>(n));
                    output.insert(output.end(), type, type + 4); output.insert(output.end(), recompressed.begin(), recompressed.end());
                    uLong crc = ::crc32(0L, Z_NULL, 0); crc = ::crc32(crc, type, 4);
                    crc = ::crc32(crc, recompressed.data(), static_cast<uInt>(recompressed.size()));
                    output.push_back(static_cast<unsigned char>(crc >> 24)); output.push_back(static_cast<unsigned char>(crc >> 16));
                    output.push_back(static_cast<unsigned char>(crc >> 8)); output.push_back(static_cast<unsigned char>(crc));
                    inserted = true;
                }
            }
            else output.insert(output.end(), input.begin() + pos, input.begin() + end);
            pos = end;
        }
        return inserted && output.size() < input.size();
    }

    CStringW BuildBinariesXml(const EpubPackage& package, const std::set<std::wstring>& usedImageIds, CStringW& warning,
        unsigned long long prefixReserve)
    {
        CStringW xml;
        std::set<std::wstring> ids = usedImageIds;
        if (g_options.importCover && !package.coverId.IsEmpty())
        {
            const ManifestItem* coverImage = ResolveCoverImageItem(package);
            if (coverImage)
                ids.emplace(coverImage->id.GetString());
        }

        // ATL's geometric string growth can briefly request far more memory
        // than the final base64 data. Reserve the known raster payload once;
        // SVG output size is not predictable and remains dynamically grown.
        unsigned long long rasterCharacters = 0;
        bool rasterEstimateTooLarge = false;
        for (const auto& imageId : ids)
        {
            const auto manifest = package.manifest.find(imageId);
            const auto extra = g_content.extraImages.find(imageId);
            const ManifestItem* item = manifest != package.manifest.end() ? &manifest->second :
                (extra != g_content.extraImages.end() ? &extra->second : nullptr);
            if (!item || IsSvgImage(*item) || (!IsImageMediaType(item->mediaType) && !IsImageHrefByExtension(item->href))) continue;
            WIN32_FILE_ATTRIBUTE_DATA info = {};
            if (!::GetFileAttributesExW(item->absPath, GetFileExInfoStandard, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            const unsigned long long bytes = (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
            const unsigned long long maxCharacters = 1000000000ULL;
            if (bytes > maxCharacters / 4 * 3) { rasterEstimateTooLarge = true; break; }
            const unsigned long long entryCharacters = 4 * ((bytes + 2) / 3) + 4096;
            if (entryCharacters > maxCharacters || rasterCharacters > maxCharacters - entryCharacters)
            { rasterEstimateTooLarge = true; break; }
            rasterCharacters += entryCharacters;
        }
        wchar_t testMode[4] = {}, testOptimize[4] = {};
        const bool forceTestOptimization = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) == 1 &&
            testMode[0] == L'1' && ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_EPUB_PNG_OPTIMIZE", testOptimize, _countof(testOptimize)) == 1 &&
            testOptimize[0] == L'1';
        const bool optimizeLargePng = rasterEstimateTooLarge || rasterCharacters >= 256ULL * 1024 * 1024 * 4 / 3 || forceTestOptimization;
        if (!rasterEstimateTooLarge && rasterCharacters >= 4ULL * 1024 * 1024 && prefixReserve <= 1000000000ULL - rasterCharacters)
        {
            try { xml.Preallocate(static_cast<int>(rasterCharacters + prefixReserve)); }
            catch (const ATL::CAtlException& error) { if (error.m_hr != E_OUTOFMEMORY) throw; }
        }

        unsigned optimizedPng = 0;
        unsigned long long optimizedBytesSaved = 0;
        for (std::set<std::wstring>::const_iterator itId = ids.begin(); itId != ids.end(); ++itId)
        {
            ::swprintf_s(g_imageStageDetail, _countof(g_imageStageDetail),
                L"image conversion, processed %u of %u, XML characters %d",
                g_content.importedImages, static_cast<unsigned>(ids.size()), xml.GetLength());
            g_importStage = g_imageStageDetail;
            std::map<std::wstring, ManifestItem>::const_iterator it = package.manifest.find(*itId);
            const ManifestItem* resource = it == package.manifest.end() ? nullptr : &it->second;
            if (!resource)
            {
                const auto extra = g_content.extraImages.find(*itId);
                if (extra != g_content.extraImages.end()) resource = &extra->second;
            }
            if (!resource) continue;
            const ManifestItem& item = *resource;
            if (!IsImageMediaType(item.mediaType) && !IsImageHrefByExtension(item.href))
                continue;

            std::vector<unsigned char> data;
            CStringW id = BinaryIdForImageItem(item);
            CStringW contentType;
            if (!ReadSvgImageData(item, data, contentType, warning) || data.empty())
            {
                ++g_content.skippedImages;
                warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_IMAGE_PREPARE_FAILED, L"Could not prepare image: ") + item.href + L"\r\n";
                continue;
            }
            if (optimizeLargePng && contentType.CompareNoCase(L"image/png") == 0)
            {
                try
                {
                    std::vector<unsigned char> optimized;
                    if (OptimizePngLosslessly(data, optimized))
                    {
                        optimizedBytesSaved += data.size() - optimized.size();
                        data.swap(optimized);
                        ++optimizedPng;
                    }
                }
                catch (const std::bad_alloc&) { /* Preserve the original if optional optimization cannot run. */ }
            }
            xml += L"  <binary id=\"" + XmlEscape(id) + L"\" content-type=\"" + XmlEscape(contentType) + L"\">\r\n";
            xml += Base64Encode(data);
            xml += L"\r\n  </binary>\r\n";
            ++g_content.importedImages;
        }
        if (optimizedPng)
        {
            CStringW detail;
            detail.Format(L"Losslessly recompressed %u PNG images, saving %llu bytes; filtered pixels and non-IDAT chunks were retained, binary hashes changed.\r\n",
                optimizedPng, optimizedBytesSaved);
            warning += detail;
            try { xml.FreeExtra(); }
            catch (const ATL::CAtlException& error) { if (error.m_hr != E_OUTOFMEMORY) throw; }
        }
        return xml;
    }


    CStringW IsoCalendarDate(const CStringW& source)
    {
        CStringW value(source); value.Trim();
        const int n = value.GetLength();
        if (n < 10 || value[4] != L'-' || value[7] != L'-') return CStringW();
        for (int i : {0, 1, 2, 3, 5, 6, 8, 9}) if (value[i] < L'0' || value[i] > L'9') return CStringW();
        const int year = _wtoi(value.Left(4)), month = _wtoi(value.Mid(5, 2)), day = _wtoi(value.Mid(8, 2));
        const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        if (year < 1 || month < 1 || month > 12) return CStringW();
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        if (day < 1 || day > days[month] + (month == 2 && leap ? 1 : 0)) return CStringW();
        if (n > 10) {
            if (n < 19 || (value[10] != L'T' && value[10] != L't') || value[13] != L':' || value[16] != L':') return CStringW();
            for (int i : {11, 12, 14, 15, 17, 18}) if (value[i] < L'0' || value[i] > L'9') return CStringW();
            if (_wtoi(value.Mid(11, 2)) > 23 || _wtoi(value.Mid(14, 2)) > 59 || _wtoi(value.Mid(17, 2)) > 59) return CStringW();
            int pos = 19;
            if (pos < n && value[pos] == L'.') { ++pos; const int start = pos; while (pos < n && value[pos] >= L'0' && value[pos] <= L'9') ++pos; if (pos == start) return CStringW(); }
            if (pos < n && (value[pos] == L'Z' || value[pos] == L'z')) ++pos;
            else if (pos < n && (value[pos] == L'+' || value[pos] == L'-')) {
                if (pos + 6 != n || value[pos+3] != L':' || value[pos+1] < L'0' || value[pos+1] > L'9' || value[pos+2] < L'0' || value[pos+2] > L'9' || value[pos+4] < L'0' || value[pos+4] > L'9' || value[pos+5] < L'0' || value[pos+5] > L'9') return CStringW();
                const int hours = _wtoi(value.Mid(pos+1, 2)), minutes = _wtoi(value.Mid(pos+4, 2));
                if (hours > 14 || minutes > 59 || (hours == 14 && minutes != 0)) return CStringW();
                pos += 6;
            }
            if (pos != n) return CStringW();
        }
        return value.Left(10);
    }

    CStringW CurrentDateIso()
    {
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        CStringW date;
        date.Format(L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
        return date;
    }

    CStringW PublicationYear(const CStringW& source)
    {
        CStringW value(source); value.Trim();
        const int length = value.GetLength();
        if (length < 4) return CStringW();
        for (int i = 0; i < 4; ++i)
            if (value[i] < L'0' || value[i] > L'9') return CStringW();
        if (_wtoi(value.Left(4)) == 0) return CStringW();
        if (length == 4) return value;
        if (length == 7 && value[4] == L'-' && value[5] >= L'0' && value[5] <= L'9' && value[6] >= L'0' && value[6] <= L'9') {
            const int month = _wtoi(value.Mid(5, 2));
            return month >= 1 && month <= 12 ? value.Left(4) : CStringW();
        }
        return IsoCalendarDate(value).IsEmpty() ? CStringW() : value.Left(4);
    }

    void SplitPersonName(const CStringW& fullName, std::vector<CStringW>& parts)
    {
        parts.clear();
        CStringW s = NormalizeImportedText(fullName);
        s.Trim();
        int start = 0;
        CStringW token = s.Tokenize(L" \t\r\n", start);
        while (start != -1 || !token.IsEmpty())
        {
            token.Trim();
            if (!token.IsEmpty())
                parts.push_back(token);
            if (start == -1)
                break;
            token = s.Tokenize(L" \t\r\n", start);
        }
    }

    void AppendPersonXml(CStringW& xml, LPCWSTR tagName, const CStringW& fullName)
    {
        std::vector<CStringW> parts;
        SplitPersonName(fullName, parts);

        xml += L"      <";
        xml += tagName;
        xml += L">";

        if (parts.size() == 2)
        {
            xml += L"<first-name>" + XmlEscape(parts[0]) + L"</first-name>";
            xml += L"<last-name>" + XmlEscape(parts[1]) + L"</last-name>";
        }
        else if (parts.size() >= 3)
        {
            xml += L"<first-name>" + XmlEscape(parts[0]) + L"</first-name>";
            xml += L"<middle-name>" + XmlEscape(parts[1]) + L"</middle-name>";
            CStringW lastName;
            for (size_t i = 2; i < parts.size(); ++i)
            {
                if (!lastName.IsEmpty())
                    lastName += L" ";
                lastName += parts[i];
            }
            xml += L"<last-name>" + XmlEscape(lastName) + L"</last-name>";
        }
        else if (parts.size() == 1)
        {
            xml += L"<nickname>" + XmlEscape(parts[0]) + L"</nickname>";
        }
        else
        {
            xml += L"<nickname>" + XmlEscape(fullName) + L"</nickname>";
        }

        xml += L"</";
        xml += tagName;
        xml += L">\r\n";
    }


    bool IsKnownFb2Genre(const CStringW& genre)
    {
        if (!g_content.fb2GenresLoaded)
        {
            g_content.fb2GenresLoaded = true;
            MSXML2::IXMLDOMDocument2Ptr schema;
            CStringW error;
            const CStringW path = DirectoryOfCurrentModule() + L"\\..\\FictionBookGenres.xsd";
            if (LoadXmlFile(path, schema, error))
            {
                std::vector<MSXML2::IXMLDOMNodePtr> values;
                CollectElementsByLocalName(MsxmlGetDocumentElement(schema), L"enumeration", values);
                for (const auto& value : values)
                {
                    const CStringW name = Attr(value, L"value");
                    if (!name.IsEmpty()) g_content.fb2GenreIds.emplace(name.GetString());
                }
            }
            if (g_content.fb2GenreIds.empty())
                g_content.warnings += L"FB2 genre list could not be loaded; original EPUB subjects remain in custom-info.\r\n";
        }
        return g_content.fb2GenreIds.count(std::wstring(genre.GetString())) != 0;
    }

    CStringW MapSubjectToFb2Genre(const CStringW& subject)
    {
        CStringW s(subject);
        s.Trim();
        if (s.IsEmpty())
            return CStringW();

        CStringW lower(s);
        lower.MakeLower();

        // Preserve a direct identifier only if it occurs in the FB2 schema.
        if (s.Find(L" ") < 0 && s.Find(L",") < 0 && s.Find(L"_") >= 0)
        {
            if (IsKnownFb2Genre(s)) return s;
            g_content.warnings += L"Unrecognized FB2-like genre retained in EPUB subjects.\r\n";
        }

        if (ContainsNoCase(lower, L"\x043D\x0430\x0443\x0447") || ContainsNoCase(lower, L"science") || ContainsNoCase(lower, L"\x0430\x0441\x0442\x0440\x043E\x043D") || ContainsNoCase(lower, L"\x043A\x043E\x0441\x043C\x043E\x0441"))
            return L"sci_popular";
        if (ContainsNoCase(lower, L"\x0444\x0430\x043D\x0442\x0430\x0441\x0442") || ContainsNoCase(lower, L"fiction"))
            return L"sf";
        if (ContainsNoCase(lower, L"\x0438\x0441\x0442\x043E\x0440") || ContainsNoCase(lower, L"history"))
            return L"prose_history";
        if (ContainsNoCase(lower, L"\x0434\x0435\x0442\x0435\x043A\x0442\x0438\x0432") || ContainsNoCase(lower, L"detective"))
            return L"det_classic";
        if (ContainsNoCase(lower, L"\x0431\x0438\x043E\x0433\x0440\x0430\x0444") || ContainsNoCase(lower, L"biograph"))
            return L"nonf_biography";

        return CStringW();
    }

    CStringW NormalizeFb2Language(const CStringW& language)
    {
        CStringW s = NormalizeImportedText(language);
        s.Trim();
        if (s.IsEmpty())
            return L"und";

        int dash = s.Find(L'-');
        int underscore = s.Find(L'_');
        int cut = -1;
        if (dash >= 0 && underscore >= 0)
            cut = min(dash, underscore);
        else if (dash >= 0)
            cut = dash;
        else if (underscore >= 0)
            cut = underscore;
        if (cut > 0)
            s = s.Left(cut);
        s.MakeLower();
        if (s.GetLength() > 3)
            s = s.Left(3);
        return s.IsEmpty() ? CStringW(L"und") : s;
    }

    CStringW DetectBodyLanguage(const CStringW& body, const std::vector<std::pair<int, int>>& excludedRanges)
    {
        // Inspect only generated body paragraphs, never metadata, titles or
        // epigraphs. A small bounded sample avoids another DOM or book copy.
        const std::set<std::wstring> russian = { L"и", L"в", L"на", L"он", L"она", L"что", L"это", L"как", L"был", L"его", L"не", L"с", L"по", L"за", L"его", L"из", L"для" };
        const std::set<std::wstring> english = { L"the", L"and", L"of", L"to", L"in", L"was", L"is", L"it", L"that", L"he", L"she", L"a", L"with", L"for", L"on", L"his", L"her" };
        unsigned latin = 0, cyrillic = 0, ruWords = 0, enWords = 0;
        std::set<std::wstring> ruDistinct, enDistinct;
        bool paragraph = false, excluded = false, entity = false;
        int excludedDepth = 0;
        std::wstring word;
        size_t rangeIndex = 0;
        auto finishWord = [&]() {
            if (russian.count(word)) { ++ruWords; ruDistinct.insert(word); }
            if (english.count(word)) { ++enWords; enDistinct.insert(word); }
            word.clear();
        };
        for (int i = 0; i < body.GetLength() && latin + cyrillic < 20000; ++i)
        {
            if (rangeIndex < excludedRanges.size() && i == excludedRanges[rangeIndex].first)
            {
                finishWord();
                i = excludedRanges[rangeIndex++].second - 1;
                paragraph = excluded = entity = false;
                excludedDepth = 0;
                continue;
            }
            wchar_t ch = body[i];
            if (ch == L'<')
            {
                finishWord();
                const int end = body.Find(L'>', i + 1); if (end < 0) break;
                CStringW tag = body.Mid(i + 1, end - i - 1); tag.Trim();
                const bool closing = !tag.IsEmpty() && tag[0] == L'/';
                if (closing) tag = tag.Mid(1);
                int space = tag.FindOneOf(L" \t\r\n/"); if (space >= 0) tag = tag.Left(space);
                tag.MakeLower();
                if (tag == L"title" || tag == L"subtitle" || tag == L"epigraph" || tag == L"text-author")
                {
                    if (closing) { if (excludedDepth > 0) --excludedDepth; }
                    else ++excludedDepth;
                    excluded = excludedDepth != 0;
                }
                if (tag == L"p") paragraph = !closing;
                i = end; entity = false; continue;
            }
            if (!paragraph || excluded) continue;
            if (ch == L'&') { finishWord(); entity = true; continue; }
            if (entity) { if (ch == L';') entity = false; continue; }
            if (ch >= L'A' && ch <= L'Z') ch += L'a' - L'A';
            if (ch >= L'А' && ch <= L'Я') ch += L'а' - L'А';
            if (ch == L'Ё') ch = L'ё';
            if (ch >= L'a' && ch <= L'z') { ++latin; word += ch; }
            else if ((ch >= L'а' && ch <= L'я') || ch == L'ё') { ++cyrillic; word += ch; }
            else finishWord();
        }
        finishWord();
        const unsigned letters = latin + cyrillic;
        if (letters < 400) return CStringW();
        if (cyrillic * 100 >= letters * 85 && ruWords >= 8 && ruDistinct.size() >= 4) return L"ru";
        if (latin * 100 >= letters * 85 && enWords >= 8 && enDistinct.size() >= 4) return L"en";
        return CStringW();
    }

    bool HasEquivalentOpeningTitle(const CStringW& body, const EpubPackage& package, bool& readable)
    {
        readable = false;
        MSXML2::IXMLDOMDocument2Ptr document;
        if (FAILED(CreateMsxmlDom(document))) return false;
        // Only inspect a bounded prefix of the first generated section. A
        // second DOM for the entire book would multiply peak memory usage.
        CStringW wrapped = L"<root xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" xmlns:l=\"http://www.w3.org/1999/xlink\">";
        const int sectionEnd = body.Find(L"</section>");
        const int limit = sectionEnd < 0 ? body.GetLength() : sectionEnd;
        const int titleOpen = body.Find(L"<title>");
        const int titleClose = titleOpen >= 0 ? body.Find(L"</title>", titleOpen) : -1;
        int position = 0, captured = 0, bytes = 0;
        while (captured < 10 && position < limit)
        {
            int start = body.Find(L"<p", position);
            if (start < 0 || start >= limit) break;
            const int next = start + 2;
            if (next >= body.GetLength() || (body[next] != L'>' && body[next] != L' ' && body[next] != L'\t' && body[next] != L'\r' && body[next] != L'\n'))
            { position = next; continue; }
            const int end = body.Find(L"</p>", next);
            if (end < 0 || end >= limit) return false;
            const int length = end + 4 - start;
            if (length > 262144 - bytes) return false;
            const bool firstTitle = captured == 0 && titleOpen >= 0 && titleOpen < start && titleClose > end;
            if (firstTitle) wrapped += L"<title>";
            wrapped += body.Mid(start, length);
            if (firstTitle) wrapped += L"</title>";
            bytes += length; ++captured; position = end + 4;
        }
        wrapped += L"</root>";
        VARIANT_BOOL loaded = VARIANT_FALSE;
        if (FAILED(document->loadXML(CComBSTR(wrapped), &loaded)) || loaded != VARIANT_TRUE) return false;
        readable = true;
        std::vector<MSXML2::IXMLDOMNodePtr> paragraphs;
        CollectElementsByLocalName(MsxmlGetDocumentElement(document), L"p", paragraphs);
        bool titleFound = false;
        std::vector<bool> authorsFound(package.creators.size(), false);
        const size_t count = (std::min)(paragraphs.size(), static_cast<size_t>(10));
        for (size_t i = 0; i < count; ++i)
        {
            const CStringW text = NodeText(paragraphs[i]);
            if (text.IsEmpty()) continue;
            bool matches = false;
            if (text.CompareNoCase(package.title) == 0) { titleFound = true; matches = true; }
            for (size_t j = 0; j < package.creators.size(); ++j)
                if (text.CompareNoCase(package.creators[j]) == 0) { authorsFound[j] = true; matches = true; }
            if (!matches)
            {
                MSXML2::IXMLDOMNode* rawParent = nullptr;
                paragraphs[i]->get_parentNode(&rawParent);
                MSXML2::IXMLDOMNodePtr parent(rawParent, false);
                if (i == 0 && LocalName(parent) == L"title") continue;
                break;
            }
        }
        if (!titleFound) return false;
        return std::find(authorsFound.begin(), authorsFound.end(), false) == authorsFound.end();
    }

    CStringW BuildMetadataTitleBlock(const EpubPackage& package)
    {
        CStringW authors;
        for (const auto& author : package.creators)
        {
            CStringW name = TrimString(author);
            if (name.IsEmpty()) continue;
            if (!authors.IsEmpty()) authors += L", ";
            authors += name;
        }
        CStringW block = L"    <title>\r\n";
        if (!authors.IsEmpty()) block += L"      <p>" + XmlEscape(authors) + L"</p>\r\n";
        block += L"      <p>" + XmlEscape(package.title) + L"</p>\r\n    </title>\r\n";
        return block;
    }

    bool HasStringValue(const std::vector<CStringW>& values, const CStringW& value)
    {
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (values[i].CompareNoCase(value) == 0)
                return true;
        }
        return false;
    }

    CStringW BuildSubjectsText(const EpubPackage& package)
    {
        CStringW text;
        for (size_t i = 0; i < package.subjects.size(); ++i)
        {
            CStringW subject = NormalizeImportedText(package.subjects[i]);
            subject.Trim();
            if (subject.IsEmpty())
                continue;
            if (!text.IsEmpty())
                text += L"; ";
            text += subject;
        }
        return text;
    }

    CStringW NormalizeFb2SeriesNumber(const CStringW& raw)
    {
        CStringW value(raw);
        value.Trim();
        if (value.IsEmpty()) return CStringW();
        int i = (value[0] == L'+' || value[0] == L'-') ? 1 : 0;
        const int digits = i;
        while (i < value.GetLength() && value[i] >= L'0' && value[i] <= L'9') ++i;
        if (i == digits) return CStringW();
        if (i == value.GetLength()) return value;
        if (value[i] != L'.' || i + 1 == value.GetLength()) return CStringW();
        for (int j = i + 1; j < value.GetLength(); ++j)
            if (value[j] != L'0') return CStringW();
        return value.Left(i);
    }

    CStringW ReplaceAnnotationHtmlEntities(CStringW text)
    {
        struct Entity { LPCWSTR name; LPCWSTR value; };
        static const Entity entities[] = {
#include "Html4Entities.inl"
        };
        for (const auto& entity : entities) text.Replace(entity.name, entity.value);
        return text;
    }

    bool AnnotationInline(const MSXML2::IXMLDOMNodePtr& node, std::vector<CStringW>& out, bool allowParagraph = true)
    {
        const MSXML2::DOMNodeType type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION)
        {
            out.back() += XmlEscape(MsxmlGetNodeText(node));
            return true;
        }
        if (type != MSXML2::NODE_ELEMENT) return false;
        const auto attributes = MsxmlGetAttributes(node);
        if (attributes && MsxmlGetNamedNodeMapLength(attributes) != 0) return false;
        CStringW name = LocalName(node);
        name.MakeLower();
        if (name == L"br")
        {
            out.emplace_back();
            return true;
        }
        if (name == L"p" && !allowParagraph) return false;
        if (name != L"p" && name != L"strong" && name != L"b" && name != L"em" && name != L"i") return false;
        std::vector<CStringW> content(1);
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            if (!AnnotationInline(MsxmlGetNodeListItem(children, i), content, false)) return false;
        for (size_t i = 0; i < content.size(); ++i)
        {
            if (name == L"strong" || name == L"b") content[i] = WrapInlinePreservingBoundarySpace(content[i], L"strong");
            if (name == L"em" || name == L"i") content[i] = WrapInlinePreservingBoundarySpace(content[i], L"emphasis");
            if (i == 0) out.back() += content[i];
            else out.push_back(content[i]);
        }
        return true;
    }

    bool AnnotationHtmlParagraphs(const CStringW& description, CStringW& paragraphs)
    {
        CStringW source = TrimString(description);
        CStringW lower(source); lower.MakeLower();
        // Only recognize a complete, narrowly supported paragraph fragment.
        // Plain descriptions and ambiguous HTML retain their original text.
        if (lower.Left(3) != L"<p>" || source.GetLength() > 262144) return false;
        MSXML2::IXMLDOMDocument2Ptr dom;
        if (FAILED(CreateMsxmlDom(dom))) return false;
        const CStringW sanitized = SanitizeMarkupForXml(ReplaceAnnotationHtmlEntities(source), true);
        CStringW wrapped = L"<root>" + sanitized + L"</root>";
        VARIANT_BOOL loaded = VARIANT_FALSE;
        HRESULT hr = dom->loadXML(CComBSTR(wrapped), &loaded);
        if (FAILED(hr) || loaded != VARIANT_TRUE)
        {
            // Publishers sometimes truncate a description in its final <p>.
            // XML parsing still validates every other tag after this repair.
            wrapped = L"<root>" + sanitized + L"</p></root>";
            loaded = VARIANT_FALSE;
            hr = dom->loadXML(CComBSTR(wrapped), &loaded);
            if (FAILED(hr) || loaded != VARIANT_TRUE) return false;
        }
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(MsxmlGetDocumentElement(dom));
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        CStringW result;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (MsxmlGetNodeType(child) == MSXML2::NODE_TEXT && TrimString(MsxmlGetNodeText(child)).IsEmpty()) continue;
            if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT || LocalName(child).CompareNoCase(L"p") != 0) return false;
            std::vector<CStringW> content(1);
            if (!AnnotationInline(child, content)) return false;
            for (const auto& line : content)
                result += line.IsEmpty() ? CStringW(L"        <empty-line/>\r\n") : L"        <p>" + line + L"</p>\r\n";
        }
        if (result.IsEmpty()) return false;
        paragraphs = result;
        return true;
    }

    CStringW Fb2DocumentIdentifier(const CStringW& source)
    {
        if (source.GetLength() != 45 || source.Left(9).CompareNoCase(L"urn:uuid:") != 0)
            return source;
        const CStringW uuid = source.Mid(9);
        for (int i = 0; i < uuid.GetLength(); ++i)
        {
            const wchar_t c = uuid[i];
            if (i == 8 || i == 13 || i == 18 || i == 23)
            {
                if (c != L'-') return source;
            }
            else if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F')))
                return source;
        }
        return uuid;
    }

    CStringW BuildDescriptionXml(const EpubPackage& package)
    {
        CStringW xml;
        xml += L"  <description>\r\n";
        xml += L"    <title-info>\r\n";
        std::vector<CStringW> genres;
        for (size_t i = 0; i < package.subjects.size(); ++i)
        {
            CStringW mappedGenre = MapSubjectToFb2Genre(package.subjects[i]);
            if (!mappedGenre.IsEmpty() && !HasStringValue(genres, mappedGenre))
                genres.push_back(mappedGenre);
        }
        if (genres.empty())
            genres.push_back(L"prose_contemporary");
        for (size_t i = 0; i < genres.size(); ++i)
            xml += L"      <genre>" + XmlEscape(genres[i]) + L"</genre>\r\n";

        if (package.creators.empty())
        {
            xml += L"      <author><first-name></first-name><last-name></last-name></author>\r\n";
        }
        else
        {
            for (size_t i = 0; i < package.creators.size(); ++i)
                AppendPersonXml(xml, L"author", package.creators[i]);
        }

        xml += L"      <book-title>" + XmlEscape(package.title) + L"</book-title>\r\n";
        if (!package.description.IsEmpty())
        {
            xml += L"      <annotation>\r\n";
            CStringW paragraphs;
            if (AnnotationHtmlParagraphs(package.description, paragraphs)) xml += paragraphs;
            else xml += L"        <p>" + XmlEscape(package.description) + L"</p>\r\n";
            xml += L"      </annotation>\r\n";
        }
        if (!package.date.IsEmpty()) {
            const CStringW normalizedDate = IsoCalendarDate(package.date);
            const CStringW valueAttr = normalizedDate.IsEmpty() ? CStringW() : L" value=\"" + normalizedDate + L"\"";
            const CStringW displayedDate = normalizedDate.IsEmpty() ? package.date : normalizedDate;
            xml += L"      <date" + valueAttr + L">" + XmlEscape(displayedDate) + L"</date>\r\n";
        }
        if (g_options.importCover && !package.coverId.IsEmpty())
        {
            const ManifestItem* coverImage = ResolveCoverImageItem(package);
            if (coverImage)
            {
                CStringW coverBinaryId = BinaryIdForImageItem(*coverImage);
                xml += L"      <coverpage><image l:href=\"#" + XmlEscape(coverBinaryId) + L"\"/></coverpage>\r\n";
            }
        }
        CStringW fb2Lang = NormalizeFb2Language(package.language);
        if (fb2Lang.IsEmpty())
            fb2Lang = L"und";
        xml += L"      <lang>" + XmlEscape(fb2Lang) + L"</lang>\r\n";
        for (size_t i = 0; i < package.contributors.size(); ++i)
            AppendPersonXml(xml, L"translator", package.contributors[i]);
        const CStringW seriesNumber = NormalizeFb2SeriesNumber(package.seriesNumber);
        if (!package.seriesName.IsEmpty())
        {
            CStringW numberAttr;
            if (!seriesNumber.IsEmpty())
                numberAttr = L" number=\"" + XmlEscape(seriesNumber) + L"\"";
            xml += L"      <sequence name=\"" + XmlEscape(package.seriesName) + L"\"" + numberAttr + L"/>\r\n";
        }
        xml += L"    </title-info>\r\n";
        xml += L"    <document-info>\r\n";
        xml += L"      <author><nickname>ImportEPUB</nickname></author>\r\n";
        xml += L"      <program-used>FBE Next ImportEPUB plugin, FictionBook Editor Next " FBE_VERSION_WSTRING L"</program-used>\r\n";
        xml += L"      <date>" + CurrentDateIso() + L"</date>\r\n";
        if (!package.identifier.IsEmpty())
            xml += L"      <id>" + XmlEscape(Fb2DocumentIdentifier(package.identifier)) + L"</id>\r\n";
        else
            xml += L"      <id>" + GuidString() + L"</id>\r\n";
        xml += L"      <version>1.0</version>\r\n";
        xml += L"    </document-info>\r\n";
        const CStringW publicationYear = package.dateEvent.IsEmpty() || package.dateEvent == L"publication" ? PublicationYear(package.date) : CStringW();
        if (!package.publisher.IsEmpty() || !publicationYear.IsEmpty() || !package.isbn.IsEmpty())
        {
            xml += L"    <publish-info>\r\n";
            if (!package.publisher.IsEmpty())
                xml += L"      <publisher>" + XmlEscape(package.publisher) + L"</publisher>\r\n";
            if (!publicationYear.IsEmpty())
                xml += L"      <year>" + publicationYear + L"</year>\r\n";
            if (!package.isbn.IsEmpty())
                xml += L"      <isbn>" + XmlEscape(package.isbn) + L"</isbn>\r\n";
            xml += L"    </publish-info>\r\n";
        }
        if (!package.seriesNumber.IsEmpty() && (seriesNumber != package.seriesNumber || package.seriesName.IsEmpty()))
        {
            xml += L"    <custom-info info-type=\"EPUB series position\">" + XmlEscape(package.seriesNumber) + L"</custom-info>\r\n";
            if (seriesNumber.IsEmpty())
                g_content.warnings += L"EPUB series position is not an integer; original value preserved in custom-info.\r\n";
        }
        if (!package.source.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB source\">" + XmlEscape(package.source) + L"</custom-info>\r\n";
        if (!package.originalLanguage.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB original language\">" + XmlEscape(package.originalLanguage) + L"</custom-info>\r\n";
        if (!package.identifier.IsEmpty() && Fb2DocumentIdentifier(package.identifier) != package.identifier)
            xml += L"    <custom-info info-type=\"EPUB identifier\">" + XmlEscape(package.identifier) + L"</custom-info>\r\n";
        if (!package.date.IsEmpty() && !IsoCalendarDate(package.date).IsEmpty() && IsoCalendarDate(package.date) != package.date)
            xml += L"    <custom-info info-type=\"EPUB original date\">" + XmlEscape(package.date) + L"</custom-info>\r\n";
        if (!package.rights.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB rights\">" + XmlEscape(package.rights) + L"</custom-info>\r\n";
        if (!package.modifiedDate.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB modified\">" + XmlEscape(package.modifiedDate) + L"</custom-info>\r\n";
        for (const auto& datedEvent : package.datedEvents)
            if (!datedEvent.first.IsEmpty() || package.datedEvents.size() > 1)
                xml += L"    <custom-info info-type=\"EPUB date" + (datedEvent.first.IsEmpty() ? CStringW() : L" " + XmlEscape(datedEvent.first)) + L"\">" + XmlEscape(datedEvent.second) + L"</custom-info>\r\n";
        if (!package.titleSort.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB title sort\">" + XmlEscape(package.titleSort) + L"</custom-info>\r\n";
        CStringW subjectsText = BuildSubjectsText(package);
        if (!subjectsText.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB subjects\">" + XmlEscape(subjectsText) + L"</custom-info>\r\n";
        if (!package.accessibilitySummary.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB accessibility summary\">" + XmlEscape(package.accessibilitySummary) + L"</custom-info>\r\n";
        xml += L"  </description>\r\n";
        return xml;
    }

    CStringW ReadSmallAsciiFile(const CStringW& path)
    {
        std::vector<unsigned char> data;
        if (!ReadBinaryFile(path, data) || data.empty())
            return CStringW();
        if (data.size() > 4096)
            data.resize(4096);
        CStringW text;
        for (size_t i = 0; i < data.size(); ++i)
        {
            unsigned char ch = data[i];
            if (ch == 0)
                break;
            text += static_cast<wchar_t>(ch);
        }
        text.Trim();
        return text;
    }

    void InspectEpubContainerFiles(const CStringW& extractDir, ImportLog& log, CStringW& warning)
    {
        CStringW mimetypePath = extractDir + L"\\mimetype";
        CStringW mimetype = ReadSmallAsciiFile(mimetypePath);
        if (mimetype.IsEmpty())
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_MIMETYPE_MISSING, L"EPUB does not contain a mimetype file. Some validators treat this as a container error.\r\n");
            log.Add(L"Warning: mimetype file is missing.");
        }
        else
        {
            log.AddFormat(L"Mimetype: %s", mimetype);
            if (mimetype.CompareNoCase(L"application/epub+zip") != 0)
            {
                warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_MIMETYPE_UNEXPECTED, L"The mimetype file contains an unexpected value: ") + mimetype + L"\r\n";
                log.Add(L"Warning: unexpected EPUB mimetype value.");
            }
        }

        CStringW encryptionPath = extractDir + L"\\META-INF\\encryption.xml";
        if (FileExists(encryptionPath))
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_ENCRYPTION_DETECTED, L"META-INF/encryption.xml was found in EPUB. Some resources may be encrypted or obfuscated; some images/fonts may not be imported.\r\n");
            log.Add(L"Warning: META-INF/encryption.xml detected.");
        }

        CStringW rightsPath = extractDir + L"\\META-INF\\rights.xml";
        if (FileExists(rightsPath))
            log.Add(L"META-INF/rights.xml detected.");

        CStringW signaturesPath = extractDir + L"\\META-INF\\signatures.xml";
        if (FileExists(signaturesPath))
            log.Add(L"META-INF/signatures.xml detected.");
    }

    void CollectAttributeValuesInXml(const CStringW& xml, const CStringW& token, std::set<std::wstring>& values)
    {
        int pos = 0;
        while ((pos = xml.Find(token, pos)) >= 0)
        {
            pos += token.GetLength();
            int end = xml.Find(L"\"", pos);
            if (end < 0)
                break;
            CStringW value = xml.Mid(pos, end - pos);
            if (!value.IsEmpty())
                values.emplace(value.GetString());
            pos = end + 1;
        }
    }

    int FindPreviousChar(const CStringW& text, wchar_t ch, int beforePos)
    {
        int pos = min(beforePos, text.GetLength() - 1);
        for (int i = pos; i >= 0; --i)
        {
            if (text[i] == ch)
                return i;
        }
        return -1;
    }

    bool IsInsideXmlTag(const CStringW& xml, int pos)
    {
        const int open = FindPreviousChar(xml, L'<', pos);
        return open >= 0 && open > FindPreviousChar(xml, L'>', pos);
    }

    void ValidateInternalReferences(const CStringW& xml, CStringW& warning)
    {
        std::set<std::wstring> binaryIds;
        CollectAttributeValuesInXml(xml, L"<binary id=\"", binaryIds);

        int imagePos = 0;
        while ((imagePos = xml.Find(L"<image ", imagePos)) >= 0)
        {
            int hrefPos = xml.Find(L"l:href=\"#", imagePos);
            int tagEnd = xml.Find(L">", imagePos);
            if (hrefPos >= 0 && tagEnd >= 0 && hrefPos < tagEnd)
            {
                hrefPos += 9; // length of l:href="#
                int end = xml.Find(L"\"", hrefPos);
                if (end > hrefPos)
                {
                    CStringW id = xml.Mid(hrefPos, end - hrefPos);
                    if (binaryIds.find(std::wstring(id.GetString())) == binaryIds.end())
                        warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_IMAGE_BINARY_MISSING, L"Image refers to a missing binary: ") + id + L"\r\n";
                }
            }
            imagePos += 7;
        }

        std::set<std::wstring> noteSectionIds;
        int notesBody = xml.Find(L"<body name=\"notes\">");
        if (notesBody >= 0)
        {
            int notesEnd = xml.Find(L"</body>", notesBody);
            CStringW notesXml = notesEnd > notesBody ? xml.Mid(notesBody, notesEnd - notesBody) : xml.Mid(notesBody);
            CollectAttributeValuesInXml(notesXml, L"<section id=\"", noteSectionIds);
        }


        std::set<std::wstring> allIds;
        std::set<std::wstring> duplicateIds;
        int idSearchPos = 0;
        while ((idSearchPos = xml.Find(L" id=\"", idSearchPos)) >= 0)
        {
            if (!IsInsideXmlTag(xml, idSearchPos))
            {
                idSearchPos += 5;
                continue;
            }
            idSearchPos += 5;
            int end = xml.Find(L"\"", idSearchPos);
            if (end < 0)
                break;
            CStringW id = xml.Mid(idSearchPos, end - idSearchPos);
            if (!allIds.emplace(id.GetString()).second)
                duplicateIds.emplace(id.GetString());
            idSearchPos = end + 1;
        }
        for (std::set<std::wstring>::const_iterator dup = duplicateIds.begin(); dup != duplicateIds.end(); ++dup)
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_DUPLICATE_FB2_ID, L"Duplicate id in generated FB2: ") + CStringW(dup->c_str()) + L"\r\n";

        // Check ordinary FB2 internal links. Images are validated against <binary>
        // above, and note links are validated separately below, so this block focuses
        // on anchors such as <a l:href="#section-id"> that should point to an
        // existing element id in the generated FB2.
        int internalHrefPos = 0;
        while ((internalHrefPos = xml.Find(L"l:href=\"#", internalHrefPos)) >= 0)
        {
            if (!IsInsideXmlTag(xml, internalHrefPos))
            {
                internalHrefPos += 9;
                continue;
            }
            int tagStart = FindPreviousChar(xml, L'<', internalHrefPos);
            int tagEnd = xml.Find(L">", internalHrefPos);
            int valueStart = internalHrefPos + 9;
            int valueEnd = xml.Find(L"\"", valueStart);
            if (tagStart >= 0 && tagEnd > tagStart && valueEnd > valueStart)
            {
                CStringW tag = xml.Mid(tagStart, tagEnd - tagStart + 1);
                const bool isImage = tag.Find(L"<image") >= 0;
                const bool isNote = tag.Find(L"type=\"note\"") >= 0;
                if (!isImage && !isNote)
                {
                    CStringW id = xml.Mid(valueStart, valueEnd - valueStart);
                    if (allIds.find(std::wstring(id.GetString())) == allIds.end())
                    {
                        ++g_content.missingGeneratedTargets;
                        warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_INTERNAL_LINK_MISSING_ID, L"Internal link points to a missing id: ") + id + L"\r\n";
                    }
                }
            }
            internalHrefPos += 9;
        }

        // External http/https links are allowed and reported only in the CSV counter.

        int notePos = 0;
        while ((notePos = xml.Find(L"type=\"note\"", notePos)) >= 0)
        {
            if (!IsInsideXmlTag(xml, notePos))
            {
                notePos += 11;
                continue;
            }
            int tagStart = FindPreviousChar(xml, L'<', notePos);
            int hrefPos = tagStart >= 0 ? xml.Find(L"l:href=\"#", tagStart) : -1;
            if (hrefPos >= 0 && hrefPos < notePos)
            {
                hrefPos += 9;
                int end = xml.Find(L"\"", hrefPos);
                if (end > hrefPos)
                {
                    CStringW id = xml.Mid(hrefPos, end - hrefPos);
                    if (noteSectionIds.find(std::wstring(id.GetString())) == noteSectionIds.end())
                        warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_NOTE_SECTION_MISSING, L"Footnote link has no matching notes section: ") + id + L"\r\n";
                }
            }
            notePos += 11;
        }
    }

    void AppendDomChild(const MSXML2::IXMLDOMNodePtr& parent, const MSXML2::IXMLDOMNodePtr& child)
    {
        IXMLDOMNode* moved = nullptr;CheckError(parent->appendChild(child, &moved));if (moved) moved->Release();
    }

    void ApplyNavigationHierarchy(const MSXML2::IXMLDOMDocument2Ptr& dom, const EpubPackage& package,
        const std::map<std::wstring, std::wstring>& anchors, CStringW& warning)
    {
        if (!g_options.splitSectionsByHeadings) return;
        std::map<std::wstring, MSXML2::IXMLDOMNodePtr> targets;
        std::vector<MSXML2::IXMLDOMNodePtr> sections;
        CollectElementsByLocalName(FindFirstElementByLocalName(MsxmlGetDocumentElement(dom), L"body"), L"section", sections);
        for (const auto& section : sections)
        {
            std::vector<CStringW> ids = CollectRawAnchorIds(section);
            for (const auto& id : ids)
                // Traverse outer sections first, so the innermost owner wins.
                targets[std::wstring(id)] = section;
        }
        std::vector<MSXML2::IXMLDOMNodePtr> mapped(package.navigation.size());
        for (size_t i = 0; i < package.navigation.size(); ++i)
        {
            const auto& entry = package.navigation[i];
            if (entry.spineIndex < 0) continue;
            const CStringW key = entry.fragment.IsEmpty() ? LowerMapKey(entry.path) : PathAnchorMapKey(entry.path, entry.fragment);
            const auto anchor = anchors.find(std::wstring(key));
            if (anchor == anchors.end()) continue;
            const auto target = targets.find(anchor->second);
            if (target != targets.end()) mapped[i] = target->second;
        }
        // A split continuation is part of the same source document. Move it
        // only across an immediate reading-order boundary, and never if it
        // contains an independently mapped navigation target.
        for (size_t i = 0; i < mapped.size(); ++i)
        {
            const auto& entry = package.navigation[i];
            const auto parent = mapped[i];
            if (!parent || entry.spineIndex < 0 ||
                static_cast<size_t>(entry.spineIndex + 1) >= package.spine.size()) continue;
            CStringW continuationPath(entry.path);
            if (continuationPath.Replace(L"_split_000.", L"_split_001.") != 1) continue;
            const auto nextItem = package.manifest.find(std::wstring(package.spine[static_cast<size_t>(entry.spineIndex + 1)]));
            if (nextItem == package.manifest.end() || LowerMapKey(nextItem->second.absPath) != LowerMapKey(continuationPath)) continue;
            const auto anchor = anchors.find(std::wstring(LowerMapKey(continuationPath)));
            if (anchor == anchors.end()) continue;
            const auto found = targets.find(anchor->second);
            if (found == targets.end()) continue;
            auto continuation = found->second;
            const auto container = ParentNode(parent);
            while (continuation && ParentNode(continuation) != container)
                continuation = ParentNode(continuation);
            if (!continuation || continuation == parent || LocalName(continuation) != L"section") continue;
            IXMLDOMNode* raw = nullptr; continuation->get_previousSibling(&raw);
            MSXML2::IXMLDOMNodePtr previous(raw, false);
            while (previous && MsxmlGetNodeType(previous) != MSXML2::NODE_ELEMENT)
            {raw = nullptr;previous->get_previousSibling(&raw);previous.Attach(raw);}
            if (previous != parent) continue;
            bool mappedInside = false;
            for (const auto& target : mapped)
                for (auto ancestor = target; ancestor; ancestor = ParentNode(ancestor))
                    if (ancestor == continuation) {mappedInside = true;break;}
            if (!mappedInside) AppendDomChild(parent, continuation);
        }
        for (size_t i = 0; i < mapped.size(); ++i)
        {
            const auto child = mapped[i];
            int parentIndex = package.navigation[i].parent;
            while (parentIndex >= 0 && (!mapped[static_cast<size_t>(parentIndex)] || mapped[static_cast<size_t>(parentIndex)] == child))
                parentIndex = package.navigation[static_cast<size_t>(parentIndex)].parent;
            if (!child || parentIndex < 0) continue;
            const auto parent = mapped[static_cast<size_t>(parentIndex)];
            bool alreadyNested = false;
            for (auto ancestor = ParentNode(child); ancestor; ancestor = ParentNode(ancestor))
                alreadyNested = alreadyNested || ancestor == parent;
            if (alreadyNested) continue;
            // Append only at the current reading-order boundary. Never sort by TOC.
            auto edge = parent;
            const auto container = ParentNode(child);
            bool safe = true;
            while (ParentNode(edge) != container)
            {
                const auto upper = ParentNode(edge);
                if (!upper || LocalName(upper) != L"section") {safe = false;break;}
                IXMLDOMNode* raw = nullptr;upper->get_lastChild(&raw);
                MSXML2::IXMLDOMNodePtr last(raw, false);
                while (last && MsxmlGetNodeType(last) != MSXML2::NODE_ELEMENT)
                {raw = nullptr;last->get_previousSibling(&raw);last.Attach(raw);}
                if (last != edge) {safe = false;break;}
                edge = upper;
            }
            IXMLDOMNode* raw = nullptr;child->get_previousSibling(&raw);
            MSXML2::IXMLDOMNodePtr previous(raw, false);
            while (previous && MsxmlGetNodeType(previous) != MSXML2::NODE_ELEMENT)
            {raw = nullptr;previous->get_previousSibling(&raw);previous.Attach(raw);}
            if (safe && previous == edge) AppendDomChild(parent, child);
            else warning += L"Navigation hierarchy not applied because it conflicts with spine order or section boundaries: " +
                package.navigation[i].path + L"#" + package.navigation[i].fragment + L"\r\n";
        }
    }

    void NormalizeSectionChildren(const MSXML2::IXMLDOMDocument2Ptr& dom, const MSXML2::IXMLDOMNodePtr& node, CStringW& warning)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> children;
        const auto list = MsxmlGetChildNodes(node);
        const long count = list ? MsxmlGetNodeListLength(list) : 0;
        bool sections = false;
        for (long i = 0; i < count; ++i)
        {
            const auto child = MsxmlGetNodeListItem(list, i);
            if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT) continue;
            NormalizeSectionChildren(dom, child, warning);
            children.push_back(child);sections = sections || LocalName(child) == L"section";
        }
        if (LocalName(node) != L"section") return;
        if (!sections)
        {
            // Move a direct edge image out of a mixed paragraph without
            // splitting its text or moving its ID. Leave styled paragraphs
            // untouched, since their styling can also apply to the image.
            bool edgeHasBody = false, hasHeaderImage = false;
            std::vector<MSXML2::IXMLDOMNodePtr> expanded;
            for (const auto& child : children)
            {
                const CStringW name = LocalName(child);
                if (name == L"p" && !TrimString(NodeText(child)).IsEmpty() &&
                    Attr(child, L"style").IsEmpty() && Attr(child, L"xml:lang").IsEmpty())
                {
                    const auto nodes = MsxmlGetChildNodes(child);
                    const long length = nodes ? MsxmlGetNodeListLength(nodes) : 0;
                    MSXML2::IXMLDOMNodePtr first, last;
                    for (long j = 0; j < length; ++j)
                    {
                        const auto part = MsxmlGetNodeListItem(nodes, j);
                        const auto type = MsxmlGetNodeType(part);
                        if (type == MSXML2::NODE_COMMENT ||
                            (type == MSXML2::NODE_TEXT && TrimString(NodeText(part)).IsEmpty())) continue;
                        if (!first) first = part;
                        last = part;
                    }
                    if (first && LocalName(first) == L"image" && (edgeHasBody || !hasHeaderImage))
                    {
                        IXMLDOMNode* inserted = nullptr;
                        CheckError(node->insertBefore(first, CComVariant(static_cast<IDispatch*>(child.GetInterfacePtr())), &inserted));
                        if (inserted) inserted->Release();
                        expanded.push_back(first);
                        if (!edgeHasBody) hasHeaderImage = true;
                    }
                    expanded.push_back(child);
                    edgeHasBody = true;
                    if (last && last != first && LocalName(last) == L"image")
                    {
                        IXMLDOMNode* rawNext = nullptr;
                        CheckError(child->get_nextSibling(&rawNext));
                        MSXML2::IXMLDOMNodePtr next(rawNext, false);
                        IXMLDOMNode* inserted = nullptr;
                        if (next)
                            CheckError(node->insertBefore(last, CComVariant(static_cast<IDispatch*>(next.GetInterfacePtr())), &inserted));
                        else
                            CheckError(node->appendChild(last, &inserted));
                        if (inserted) inserted->Release();
                        expanded.push_back(last);
                    }
                    continue;
                }
                expanded.push_back(child);
                if (name == L"image" && !edgeHasBody) hasHeaderImage = true;
                else if (name != L"title" && name != L"epigraph" && name != L"annotation") edgeHasBody = true;
            }
            children.swap(expanded);
            // An epigraph is section metadata only before the image,
            // annotation and body. Later quotations must stay in reading
            // order, so retain them as body citations instead.
            bool pastEpigraphs = false;
            for (auto& child : children)
            {
                const CStringW name = LocalName(child);
                if (name == L"epigraph" && pastEpigraphs)
                {
                    IXMLDOMNode* created = nullptr;
                    CheckError(dom->createNode(CComVariant(static_cast<long>(MSXML2::NODE_ELEMENT)), CComBSTR(L"cite"),
                        CComBSTR(L"http://www.gribuser.ru/xml/fictionbook/2.0"), &created));
                    MSXML2::IXMLDOMNodePtr cite(created, false);
                    const CStringW id = Attr(child, L"id");
                    if (!id.IsEmpty())
                    {
                        CComQIPtr<IXMLDOMElement> element(cite);
                        if (!element) CheckError(E_NOINTERFACE);
                        CheckError(element->setAttribute(CComBSTR(L"id"), CComVariant(id)));
                    }
                    for (;;)
                    {
                        IXMLDOMNode* raw = nullptr; child->get_firstChild(&raw);
                        MSXML2::IXMLDOMNodePtr first(raw, false); if (!first) break;
                        AppendDomChild(cite, first);
                    }
                    IXMLDOMNode* replaced = nullptr;
                    CheckError(node->replaceChild(cite, child, &replaced));
                    if (replaced) replaced->Release();
                    child = cite;
                    warning += L"Epigraph after section content preserved as cite.\r\n";
                }
                else if (name != L"title" && name != L"epigraph") pastEpigraphs = true;
            }
            // A section's first body block cannot be an image unless it is
            // followed by a non-image body block. Promote image-only paragraphs
            // only where the resulting order satisfies that FB2 rule.
            auto soleImage = [](const MSXML2::IXMLDOMNodePtr& paragraph) -> MSXML2::IXMLDOMNodePtr {
                if (LocalName(paragraph) != L"p" || !TrimString(NodeText(paragraph)).IsEmpty() ||
                    !Attr(paragraph, L"style").IsEmpty() || !Attr(paragraph, L"xml:lang").IsEmpty()) return nullptr;
                const auto nodes = MsxmlGetChildNodes(paragraph);
                const long length = nodes ? MsxmlGetNodeListLength(nodes) : 0;
                MSXML2::IXMLDOMNodePtr image;
                for (long i = 0; i < length; ++i) {
                    const auto child = MsxmlGetNodeListItem(nodes, i);
                    const auto type = MsxmlGetNodeType(child);
                    if (type == MSXML2::NODE_ELEMENT) {
                        if (image || LocalName(child) != L"image" || !Attr(child, L"id").IsEmpty()) return nullptr;
                        image = child;
                    } else if (type != MSXML2::NODE_TEXT && type != MSXML2::NODE_COMMENT) return nullptr;
                }
                return image;
            };
            bool bodyStarted = false, headerImageUsed = false;
            for (size_t i = 0; i < children.size(); ++i) {
                const CStringW name = LocalName(children[i]);
                if (name == L"title" || name == L"epigraph" || name == L"annotation") continue;
                auto image = soleImage(children[i]);
                bool followingBody = false;
                for (size_t j = i + 1; image && !bodyStarted && j < children.size(); ++j) {
                    const CStringW next = LocalName(children[j]);
                    if (next == L"title" || next == L"epigraph" || next == L"annotation") continue;
                    followingBody = !soleImage(children[j]) && next != L"image";
                    break;
                }
                if (image && (bodyStarted || (!headerImageUsed && followingBody))) {
                    // FB2 permits an id on a block image. Transfer the
                    // paragraph's anchor instead of leaving an otherwise
                    // standalone picture inline merely because it is linked.
                    const CStringW paragraphId = Attr(children[i], L"id");
                    if (!paragraphId.IsEmpty()) {
                        CComQIPtr<IXMLDOMElement> element(image);
                        if (!element) CheckError(E_NOINTERFACE);
                        CheckError(element->setAttribute(CComBSTR(L"id"), CComVariant(paragraphId)));
                    }
                    IXMLDOMNode* removed = nullptr;
                    CheckError(children[i]->removeChild(image, &removed)); if (removed) removed->Release();
                    IXMLDOMNode* replaced = nullptr;
                    CheckError(node->replaceChild(image, children[i], &replaced)); if (replaced) replaced->Release();
                    children[i] = image;
                    if (bodyStarted) continue;
                    // The first image occupies the optional section image slot.
                    headerImageUsed = true;
                    continue;
                }
                if (image)
                    warning += L"Image-only paragraph retained: FB2 section ordering requires a text block at this position.\r\n";
                if (name == L"image" && !bodyStarted) headerImageUsed = true;
                else if (name != L"image") bodyStarted = true;
            }
            MSXML2::IXMLDOMNodePtr title;
            bool content = false;
            for (const auto& child : children)
            {
                if (LocalName(child) == L"title") title = child;
                else content = content || LocalName(child) != L"p" || !TrimString(NodeText(child)).IsEmpty() ||
                    FindFirstElementByLocalName(child, L"image") != nullptr;
            }
            // FB2 requires a content block after section metadata.  If the
            // source consists only of epigraphs, represent the last one as a
            // cite, retaining its children and their reading order.
            bool hasBody = false;
            MSXML2::IXMLDOMNodePtr lastEpigraph;
            for (const auto& child : children)
            {
                const CStringW name = LocalName(child);
                if (name == L"epigraph") lastEpigraph = child;
                else if (name != L"title" && name != L"image" && name != L"annotation") hasBody = true;
            }
            if (!hasBody && lastEpigraph)
            {
                IXMLDOMNode* created = nullptr;
                CheckError(dom->createNode(CComVariant(static_cast<long>(MSXML2::NODE_ELEMENT)), CComBSTR(L"cite"),
                    CComBSTR(L"http://www.gribuser.ru/xml/fictionbook/2.0"), &created));
                MSXML2::IXMLDOMNodePtr cite(created, false);
                const CStringW id = Attr(lastEpigraph, L"id");
                if (!id.IsEmpty())
                {
                    CComQIPtr<IXMLDOMElement> element(cite);
                    if (!element) CheckError(E_NOINTERFACE);
                    CheckError(element->setAttribute(CComBSTR(L"id"), CComVariant(id)));
                }
                for (;;)
                {
                    IXMLDOMNode* raw = nullptr; lastEpigraph->get_firstChild(&raw);
                    MSXML2::IXMLDOMNodePtr first(raw, false); if (!first) break;
                    AppendDomChild(cite, first);
                }
                IXMLDOMNode* replaced = nullptr;
                CheckError(node->replaceChild(cite, lastEpigraph, &replaced));
                if (replaced) replaced->Release();
                warning += L"Epigraph-only section: final epigraph preserved as cite.\r\n";
                content = true;
            }
            if (title && !content)
            {
                const auto paragraphs = MsxmlGetChildNodes(title);
                std::vector<MSXML2::IXMLDOMNodePtr> saved;
                const long length = paragraphs ? MsxmlGetNodeListLength(paragraphs) : 0;
                for (long i = 0; i < length; ++i)
                {
                    const auto paragraph = MsxmlGetNodeListItem(paragraphs, i);
                    if (LocalName(paragraph) == L"p") saved.push_back(paragraph);
                }
                for (const auto& paragraph : saved)
                {
                    IXMLDOMNode* created = nullptr;
                    CheckError(dom->createNode(CComVariant(static_cast<long>(MSXML2::NODE_ELEMENT)), CComBSTR(L"strong"),
                        CComBSTR(L"http://www.gribuser.ru/xml/fictionbook/2.0"), &created));
                    MSXML2::IXMLDOMNodePtr strong(created, false);
                    for (;;)
                    {
                        IXMLDOMNode* raw = nullptr;paragraph->get_firstChild(&raw);
                        MSXML2::IXMLDOMNodePtr first(raw, false);if (!first) break;
                        AppendDomChild(strong, first);
                    }
                    AppendDomChild(paragraph, strong);
                    IXMLDOMNode* inserted = nullptr;
                    CheckError(node->insertBefore(paragraph, CComVariant(static_cast<IDispatch*>(title.GetInterfacePtr())), &inserted));
                    if (inserted) inserted->Release();
                }
                IXMLDOMNode* removed = nullptr;CheckError(node->removeChild(title, &removed));if (removed) removed->Release();
                warning += L"Title-only section preserved as emphasized text: " + Attr(node, L"id") + L"\r\n";
            }
            return;
        }
        MSXML2::IXMLDOMNodePtr run;
        for (const auto& child : children)
        {
            const CStringW name = LocalName(child);
            if (name == L"section" || name == L"title")
            {
                if (run) NormalizeSectionChildren(dom, run, warning);
                run = nullptr;continue;
            }
            if (!run)
            {
                IXMLDOMNode* created = nullptr;
                CheckError(dom->createNode(CComVariant(static_cast<long>(MSXML2::NODE_ELEMENT)), CComBSTR(L"section"),
                    CComBSTR(L"http://www.gribuser.ru/xml/fictionbook/2.0"), &created));
                run.Attach(created);
                IXMLDOMNode* inserted = nullptr;
                CheckError(node->insertBefore(run, CComVariant(static_cast<IDispatch*>(child.GetInterfacePtr())), &inserted));
                if (inserted) inserted->Release();
            }
            IXMLDOMNode* moved = nullptr;CheckError(run->appendChild(child, &moved));if (moved) moved->Release();
        }
        if (run) NormalizeSectionChildren(dom, run, warning);
    }

    void ApplyImageSpacing(const MSXML2::IXMLDOMDocument2Ptr& dom, CStringW& warning)
    {
        std::vector<MSXML2::IXMLDOMNodePtr> sections;
        CollectElementsByLocalName(MsxmlGetDocumentElement(dom), L"section", sections);
        for (const auto& section : sections)
        {
            std::vector<MSXML2::IXMLDOMNodePtr> children;
            const auto list = MsxmlGetChildNodes(section);
            const long count = list ? MsxmlGetNodeListLength(list) : 0;
            bool hasSections = false, hasAnnotation = false;
            for (long i = 0; i < count; ++i)
            {
                const auto child = MsxmlGetNodeListItem(list, i);
                if (MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT) continue;
                children.push_back(child);
                hasSections = hasSections || LocalName(child) == L"section";
                hasAnnotation = hasAnnotation || LocalName(child) == L"annotation";
            }
            bool bodyStarted = false;
            for (const auto& child : children)
            {
                const CStringW name = LocalName(child);
                if (name == L"title" || name == L"epigraph" || name == L"annotation") continue;
                bool imageBlock = name == L"image";
                if (name == L"p" && TrimString(NodeText(child)).IsEmpty())
                {
                    const auto nested = MsxmlGetChildNodes(child);
                    const long length = nested ? MsxmlGetNodeListLength(nested) : 0;
                    int images = 0; bool other = false;
                    for (long j = 0; j < length; ++j)
                    {
                        const auto item = MsxmlGetNodeListItem(nested, j);
                        if (MsxmlGetNodeType(item) == MSXML2::NODE_ELEMENT)
                        { if (LocalName(item) == L"image") ++images; else other = true; }
                    }
                    imageBlock = images == 1 && !other;
                }
                const bool headerImage = name == L"image" && !bodyStarted;
                if (imageBlock)
                {
                    if (hasSections || (headerImage && hasAnnotation))
                        warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_IMAGE_SPACING_SKIPPED, L"Image spacing skipped: FB2 section ordering would be invalid.") + L"\r\n";
                    else
                    {
                        auto makeLine = [&]() -> MSXML2::IXMLDOMNodePtr {
                            IXMLDOMNode* raw = nullptr;
                            CheckError(dom->createNode(CComVariant(static_cast<long>(MSXML2::NODE_ELEMENT)), CComBSTR(L"empty-line"),
                                CComBSTR(L"http://www.gribuser.ru/xml/fictionbook/2.0"), &raw));
                            return MSXML2::IXMLDOMNodePtr(raw, false);
                        };
                        if (g_options.blankBeforeImages)
                        {
                            const auto line = makeLine(); IXMLDOMNode* inserted = nullptr;
                            CheckError(section->insertBefore(line, CComVariant(static_cast<IDispatch*>(child.GetInterfacePtr())), &inserted));
                            if (inserted) inserted->Release();
                        }
                        if (g_options.blankAfterImages)
                        {
                            const auto line = makeLine(); IXMLDOMNode* rawNext = nullptr;
                            CheckError(child->get_nextSibling(&rawNext)); MSXML2::IXMLDOMNodePtr next(rawNext, false);
                            if (next)
                            {
                                IXMLDOMNode* inserted = nullptr;
                                CheckError(section->insertBefore(line, CComVariant(static_cast<IDispatch*>(next.GetInterfacePtr())), &inserted));
                                if (inserted) inserted->Release();
                            }
                            else AppendDomChild(section, line);
                        }
                    }
                }
                if (name != L"image") bodyStarted = true;
            }
        }
    }

    void CollectNoteMarkerTextNodes(const MSXML2::IXMLDOMNodePtr& node, std::vector<MSXML2::IXMLDOMNodePtr>& textNodes)
    {
        if (!node) return;
        const auto type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION) { textNodes.push_back(node); return; }
        const auto children = MsxmlGetChildNodes(node);
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i) CollectNoteMarkerTextNodes(MsxmlGetNodeListItem(children, i), textNodes);
    }

    void ApplyUnifiedNoteNumbers(const MSXML2::IXMLDOMDocument2Ptr& dom)
    {
        const auto root = MsxmlGetDocumentElement(dom);
        const auto children = MsxmlGetChildNodes(root);
        MSXML2::IXMLDOMNodePtr mainBody, notesBody;
        const long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            const auto child = MsxmlGetNodeListItem(children, i);
            if (LocalName(child) != L"body") continue;
            if (Attr(child, L"name") == L"notes") notesBody = child;
            else if (!mainBody) mainBody = child;
        }
        if (!mainBody || !notesBody) return;
        std::map<std::wstring, MSXML2::IXMLDOMNodePtr> sections;
        std::vector<MSXML2::IXMLDOMNodePtr> originalSections;
        const auto noteChildren = MsxmlGetChildNodes(notesBody);
        const long noteCount = noteChildren ? MsxmlGetNodeListLength(noteChildren) : 0;
        for (long i = 0; i < noteCount; ++i)
        {
            const auto section = MsxmlGetNodeListItem(noteChildren, i);
            if (LocalName(section) != L"section") continue;
            const CStringW id = Attr(section, L"id");
            if (!id.IsEmpty()) sections.emplace(std::wstring(id), section);
            originalSections.push_back(section);
        }
        struct Renumbered { CStringW id, marker; MSXML2::IXMLDOMNodePtr section; };
        std::map<std::wstring, Renumbered> renamed;
        std::vector<MSXML2::IXMLDOMNodePtr> ordered;
        std::set<IXMLDOMNode*> selectedSections;
        std::vector<MSXML2::IXMLDOMNodePtr> mainLinks;
        CollectElementsByLocalName(mainBody, L"a", mainLinks);
        for (const auto& link : mainLinks)
        {
            if (Attr(link, L"type") != L"note") continue;
            const CStringW href = AttrByLocalName(link, L"href");
            if (href.GetLength() < 2 || href[0] != L'#') continue;
            const std::wstring oldId(href.Mid(1));
            const auto found = sections.find(oldId);
            if (found == sections.end()) continue;
            auto mapped = renamed.find(oldId);
            if (mapped == renamed.end())
            {
                const unsigned number = static_cast<unsigned>(ordered.size() + 1);
                CStringW desired, marker; desired.Format(L"n_%u", number); marker.Format(L"[%u]", number);
                const CStringW newId = oldId == std::wstring(desired) ? desired : UniqueContentId(desired);
                mapped = renamed.emplace(oldId, Renumbered{newId, marker, found->second}).first;
                ordered.push_back(found->second);
                selectedSections.insert(found->second.GetInterfacePtr());
            }
            std::vector<MSXML2::IXMLDOMNodePtr> textNodes;
            CollectNoteMarkerTextNodes(link, textNodes);
            CStringW text = TrimString(NodeText(link));
            bool markerOnly = !text.IsEmpty();
            for (int i = 0; markerOnly && i < text.GetLength(); ++i)
            {
                const wchar_t c = text[i];
                markerOnly = (c >= L'0' && c <= L'9') || c == L'[' || c == L']' || c == L'(' || c == L')' || c == L'*' || c == L' ';
            }
            if (markerOnly && textNodes.size() == 1)
                CheckError(textNodes[0]->put_nodeValue(CComVariant(mapped->second.marker)));
            else
            {
                IXMLDOMText* raw = nullptr;
                CheckError(dom->createTextNode(CComBSTR(L" " + mapped->second.marker), &raw));
                MSXML2::IXMLDOMNodePtr textNode(raw, false); AppendDomChild(link, textNode);
            }
        }
        if (ordered.empty()) return;
        for (const auto& entry : renamed)
        {
            const auto attributes = MsxmlGetAttributes(entry.second.section);
            IXMLDOMNode* raw = nullptr; CheckError(attributes->getNamedItem(CComBSTR(L"id"), &raw));
            MSXML2::IXMLDOMNodePtr idNode(raw, false);
            if (!idNode) CheckError(E_FAIL);
            CheckError(idNode->put_nodeValue(CComVariant(entry.second.id)));
            const auto title = FindFirstElementByLocalName(entry.second.section, L"title");
            const auto paragraph = FindFirstElementByLocalName(title, L"p");
            if (paragraph)
            {
                const CStringW oldTitle = TrimString(NodeText(paragraph));
                bool numericTitle = !oldTitle.IsEmpty();
                for (int i = 0; numericTitle && i < oldTitle.GetLength(); ++i)
                    numericTitle = oldTitle[i] >= L'0' && oldTitle[i] <= L'9';
                if (numericTitle || oldTitle == CStringW(entry.first.c_str()))
                {
                    const CStringW number = entry.second.marker.Mid(1, entry.second.marker.GetLength() - 2);
                    CheckError(paragraph->put_text(CComBSTR(number)));
                    continue;
                }
                IXMLDOMText* rawText = nullptr;
                CheckError(dom->createTextNode(CComBSTR(entry.second.marker + L" "), &rawText));
                MSXML2::IXMLDOMNodePtr marker(rawText, false);
                IXMLDOMNode* rawFirst = nullptr; CheckError(paragraph->get_firstChild(&rawFirst));
                MSXML2::IXMLDOMNodePtr first(rawFirst, false);
                if (first)
                {
                    IXMLDOMNode* inserted = nullptr;
                    CheckError(paragraph->insertBefore(marker, CComVariant(static_cast<IDispatch*>(first.GetInterfacePtr())), &inserted));
                    if (inserted) inserted->Release();
                }
                else AppendDomChild(paragraph, marker);
            }
        }
        std::vector<MSXML2::IXMLDOMNodePtr> links;
        CollectElementsByLocalName(root, L"a", links);
        for (const auto& link : links)
        {
            CStringW href = AttrByLocalName(link, L"href");
            if (href.GetLength() < 2 || href[0] != L'#') continue;
            const auto entry = renamed.find(std::wstring(href.Mid(1)));
            if (entry == renamed.end()) continue;
            const auto attributes = MsxmlGetAttributes(link);
            IXMLDOMNode* raw = nullptr; CheckError(attributes->getNamedItem(CComBSTR(L"l:href"), &raw));
            MSXML2::IXMLDOMNodePtr attr(raw, false);
            if (!attr) CheckError(E_FAIL);
            CheckError(attr->put_nodeValue(CComVariant(L"#" + entry->second.id)));
        }
        for (const auto& section : originalSections)
        {
            if (!selectedSections.count(section.GetInterfacePtr())) ordered.push_back(section);
        }
        for (const auto& section : ordered) AppendDomChild(notesBody, section);
    }

    struct CompactBinaryValidation
    {
        CStringW xml;
        int originalFirst = -1;
        int originalEnd = -1;
        int count = 0;
    };

    bool CompactGeneratedBinaries(const CStringW& xml, CompactBinaryValidation& compact)
    {
        int cursor = 0;
        int open = xml.Find(L"<binary ", cursor);
        if (open < 0) return false;
        compact.originalFirst = open;
        const wchar_t* text = xml.GetString();
        while (open >= 0)
        {
            const int headerEnd = xml.Find(L'>', open);
            const int close = headerEnd < 0 ? -1 : xml.Find(L"</binary>", headerEnd + 1);
            if (headerEnd < 0 || close < 0) return false;
            unsigned long long characters = 0;
            int padding = 0, lastValue = -1;
            for (int i = headerEnd + 1; i < close; ++i)
            {
                const wchar_t ch = text[i];
                if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') continue;
                ++characters;
                if (ch == L'=') { if (++padding > 2) return false; continue; }
                if (padding) return false;
                if (ch >= L'A' && ch <= L'Z') lastValue = ch - L'A';
                else if (ch >= L'a' && ch <= L'z') lastValue = ch - L'a' + 26;
                else if (ch >= L'0' && ch <= L'9') lastValue = ch - L'0' + 52;
                else if (ch == L'+') lastValue = 62;
                else if (ch == L'/') lastValue = 63;
                else return false;
            }
            if (characters % 4 != 0 || (padding == 1 && (lastValue < 0 || (lastValue & 3) != 0)) ||
                (padding == 2 && (lastValue < 0 || (lastValue & 15) != 0))) return false;
            compact.xml.Append(text + cursor, headerEnd + 1 - cursor);
            compact.xml += L"AA==";
            cursor = close;
            compact.originalEnd = close + 9;
            ++compact.count;
            open = xml.Find(L"<binary ", compact.originalEnd);
            if (open >= 0)
                for (int i = compact.originalEnd; i < open; ++i)
                    if (text[i] != L' ' && text[i] != L'\t' && text[i] != L'\r' && text[i] != L'\n') return false;
        }
        compact.xml.Append(text + cursor, xml.GetLength() - cursor);
        return compact.count > 0;
    }

    bool RestoreGeneratedBinaries(CStringW& xml, const CompactBinaryValidation& compact)
    {
        const int first = compact.xml.Find(L"<binary ");
        int next = first, count = 0, end = -1;
        while (next >= 0)
        {
            const int close = compact.xml.Find(L"</binary>", next);
            if (close < 0) return false;
            end = close + 9; ++count;
            next = compact.xml.Find(L"<binary ", end);
        }
        if (first < 0 || end < 0 || count != compact.count || compact.originalFirst < 0 ||
            compact.originalEnd < compact.originalFirst) return false;
        const int binaryLength = compact.originalEnd - compact.originalFirst;
        const int suffixLength = compact.xml.GetLength() - end;
        if (first > INT_MAX - binaryLength || first + binaryLength > INT_MAX - suffixLength) return false;
        const int newLength = first + binaryLength + suffixLength;
        wchar_t* buffer = xml.GetBuffer(max(xml.GetLength(), newLength));
        ::memmove(buffer + first, buffer + compact.originalFirst, static_cast<size_t>(binaryLength) * sizeof(wchar_t));
        ::memcpy(buffer, compact.xml.GetString(), static_cast<size_t>(first) * sizeof(wchar_t));
        ::memcpy(buffer + first + binaryLength, compact.xml.GetString() + end, static_cast<size_t>(suffixLength) * sizeof(wchar_t));
        xml.ReleaseBufferSetLength(newLength);
        return true;
    }

    bool ValidateGeneratedFb2Xml(CStringW& xml, CStringW& warning, const EpubPackage& package,
        const std::map<std::wstring, std::wstring>& anchors)
    {
        MSXML2::IXMLDOMDocument2Ptr dom;
        HRESULT hr = CreateMsxmlDom(dom);
        if (FAILED(hr))
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_FINAL_MSXML_CREATE_FAILED, L"Could not create MSXML to validate generated FB2.\r\n");
            return false;
        }

        CompactBinaryValidation compact;
        const bool useCompact = xml.GetLength() >= 4 * 1024 * 1024 && xml.Find(L"<binary ") >= 0;
        if (useCompact && !CompactGeneratedBinaries(xml, compact))
        {
            warning += L"Could not validate generated binary payloads safely.\r\n";
            return false;
        }
        CStringW& validationXml = useCompact ? compact.xml : xml;
        VARIANT_BOOL loaded = VARIANT_FALSE;
        CComBSTR xmlText(static_cast<LPCWSTR>(validationXml));
        HRESULT loadHr = dom->loadXML(xmlText, &loaded);
        xmlText.Empty();
        if (FAILED(loadHr) || loaded != VARIANT_TRUE)
        {
            CStringW reason = MsxmlGetParseErrorReason(dom);
            if (reason.IsEmpty())
                reason = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_XML_UNKNOWN, L"unknown XML error");
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_FINAL_XML_FAILED, L"Generated FB2 did not pass XML validation: ") + reason + L"\r\n";
            return false;
        }

        if (!FindFirstElementByLocalName(MsxmlGetDocumentElement(dom), L"body"))
        {
            warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_FINAL_BODY_MISSING, L"No body found in generated FB2.\r\n");
            return false;
        }

        ApplyNavigationHierarchy(dom, package, anchors, warning);
        NormalizeSectionChildren(dom, MsxmlGetDocumentElement(dom), warning);
        if (g_options.blankBeforeImages || g_options.blankAfterImages) ApplyImageSpacing(dom, warning);
        if (g_options.importNotes && g_options.unifyNoteNumbers) ApplyUnifiedNoteNumbers(dom);
        // Keep the original only when the user requested an intermediate FB2
        // on error. Otherwise release it before serializing the DOM, which can
        // require another very large allocation for image-heavy books.
        if (!g_options.saveIntermediateFb2OnError) validationXml.Empty();
        BSTR normalized = nullptr;
        const HRESULT serializeHr = dom->get_xml(&normalized);
        if (FAILED(serializeHr) || !normalized)
        {
            CStringW reason;
            reason.Format(L"Could not serialize normalized FB2 (HRESULT 0x%08lX).\r\n", static_cast<unsigned long>(serializeHr));
            warning += reason;
            return false;
        }
        // The DOM and its serialized result now own the content. Release the
        // pre-normalized copy before allocating the replacement, otherwise
        // image-heavy FB2 files can exhaust the Win32 address space.
        validationXml.Empty();
        validationXml = MsxmlBstrToString(normalized);
        CStringW referenceWarnings;ValidateInternalReferences(validationXml, referenceWarnings);warning += referenceWarnings;
        if (!referenceWarnings.IsEmpty()) return false;
        if (!g_options.validateResult)
        {
            if (useCompact && !RestoreGeneratedBinaries(xml, compact)) { warning += L"Could not restore validated binary payloads.\r\n"; return false; }
            return true;
        }
        HMODULE module = nullptr;
        ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ValidateGeneratedFb2Xml), &module);
        wchar_t modulePath[MAX_PATH] = {};::GetModuleFileNameW(module, modulePath, _countof(modulePath));
        const CStringW schemaPath = DirectoryOf(CStringW(modulePath)) + L"\\..\\FictionBook.xsd";
        if (!FileExists(schemaPath)) {warning += L"FB2 schema unavailable: " + schemaPath + L"\r\n";return false;}
        CComPtr<IXMLDOMSchemaCollection2> schemas;
        HRESULT schemaHr = schemas.CoCreateInstance(__uuidof(XMLSchemaCache60));
        if (SUCCEEDED(schemaHr)) schemaHr = schemas->add(CComBSTR(L"http://www.gribuser.ru/xml/fictionbook/2.0"), CComVariant(schemaPath));
        if (SUCCEEDED(schemaHr)) schemaHr = dom->putref_schemas(CComVariant(static_cast<IDispatch*>(schemas)));
        IXMLDOMParseError* rawError = nullptr;
        if (SUCCEEDED(schemaHr)) schemaHr = dom->validate(&rawError);
        CComPtr<IXMLDOMParseError> schemaError;schemaError.Attach(rawError);
        long code = 0;if (schemaError) schemaError->get_errorCode(&code);
        if (FAILED(schemaHr) || code)
        {
            BSTR reason = nullptr;if (schemaError) schemaError->get_reason(&reason);
            warning += L"FB2 schema validation failed: " + MsxmlBstrToString(reason) + L"\r\n";return false;
        }
        if (useCompact && !RestoreGeneratedBinaries(xml, compact)) { warning += L"Could not restore validated binary payloads.\r\n"; return false; }
        return true;
    }
}

EpubImportRuntimeStats GetLastEpubImportRuntimeStats()
{
    return g_runtimeStats;
}

bool BuildFb2XmlFromEpub(const CStringW& epubPath, const EpubImportOptions& options, CStringW& outFb2Xml, CStringW& errorText)
{
    g_importStage = L"initialization";
    g_options = options;
    g_runtimeStats = EpubImportRuntimeStats();
    g_content = ContentInventory();
    XhtmlCacheScope xhtmlCacheScope;
    outFb2Xml.Empty();
    errorText.Empty();

    ImportLog log;
    log.Add(L"ImportEPUB log");
    log.Add(L"=============");
    log.AddFormat(L"Source: %s", epubPath);
    log.AddFormatInt(L"Import page breaks: %d", options.importPageBreaks ? 1 : 0);
    log.AddFormatInt(L"Skip hidden elements: %d", options.skipHiddenElements ? 1 : 0);
    log.AddFormatInt(L"Save FB2 copy: %d", options.saveFb2Copy ? 1 : 0);
    log.AddFormatInt(L"Use CSS semantic classes: %d", options.useCssSemanticClasses ? 1 : 0);
    log.AddFormatInt(L"Remove footnote backlinks: %d", options.removeFootnoteBacklinks ? 1 : 0);
    log.AddFormatInt(L"Remove service sections: %d", options.removeServiceSections ? 1 : 0);
    log.AddFormatInt(L"Auto log on warnings: %d", options.writeLogOnWarnings ? 1 : 0);

    if (!FileExists(epubPath))
    {
        errorText = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SELECTED_EPUB_MISSING, L"The selected EPUB file was not found.");
        log.Add(errorText);
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }

    TempDirectory temp;
    if (temp.Path().IsEmpty())
    {
        errorText = L"Cannot create a temporary directory for EPUB import.";
        log.Add(errorText);
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }
    auto failWithImportLog = [&]() -> bool {
        log.AddFormatInt(L"Recovered XHTML documents: %d", static_cast<int>(g_content.repairedXhtmlDocuments.size()));
        log.AddFormatInt(L"Unrecovered XHTML documents: %d", static_cast<int>(g_content.unrecoveredXhtmlDocuments.size()));
        log.AddFormatInt(L"Rewritten internal links: %d", static_cast<int>(g_content.mappedLinkElements.size()));
        log.AddFormatInt(L"Marked backlinks removed: %d", static_cast<int>(g_content.removedBacklinkElements.size()));
        log.AddFormatInt(L"Marked backlinks retained: %d", static_cast<int>(g_content.retainedBacklinkElements.size()));
        log.AddFormatInt(L"Marked backlinks retained as text: %d", static_cast<int>(g_content.textOnlyBacklinkElements.size()));
        log.AddFormatInt(L"Elements with detected fidelity limitations: %d", static_cast<int>(g_content.fidelityLossElements.size()));
        log.Add(L"ERROR: " + errorText);
        if (options.keepTempOnError) temp.Keep();
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    };
    BookRootScope bookRoot(temp.Path()+L"\\epub");
    CStringW extractDir;
    g_importStage = L"archive extraction";
    if (!ExpandEpubToDirectory(epubPath, temp.Path(), extractDir, errorText))
    {
        log.Add(errorText);
        if (options.keepTempOnError)
            temp.Keep();
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }
    log.AddFormat(L"Temp: %s", extractDir);

    CStringW containerWarning;
    InspectEpubContainerFiles(extractDir, log, containerWarning);

    CStringW opfPath;
    if (!LocateOpfFile(extractDir, opfPath, errorText))
    {
        log.Add(errorText);
        if (options.keepTempOnError)
            temp.Keep();
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }
    log.AddFormat(L"OPF: %s", opfPath);

    EpubPackage package;
    g_importStage = L"package metadata";
    if (!ParseOpf(opfPath, package, errorText))
    {
        log.Add(errorText);
        if (options.keepTempOnError)
            temp.Keep();
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }
    log.AddFormat(L"Title: %s", package.title);
    for(const auto& id:package.spine) {
        const auto item=package.manifest.find(std::wstring(id));
        if(item==package.manifest.end() || !FileExists(item->second.absPath)) {
            if(package.mandatorySpine.count(std::wstring(id.GetString()))) {
                errorText=L"Missing mandatory EPUB spine resource: "+id;
                if(item!=package.manifest.end() && !item->second.href.IsEmpty()) errorText+=L" ("+item->second.href+L")";
                return failWithImportLog();
            }
            log.AddFormat(L"Skipped missing optional EPUB spine resource: %s", id);
        }
    }
    if (!package.seriesName.IsEmpty())
        log.AddFormat(L"Series: %s", package.seriesName);
    log.AddFormatInt(L"Manifest items: %d", static_cast<int>(package.manifest.size()));
    log.AddFormatInt(L"Spine items: %d", static_cast<int>(package.spine.size()));
    log.AddFormatInt(L"OPF guide service documents: %d", static_cast<int>(package.guideServiceDocuments.size()));

    ParsePackageNavigation(package, log);

    CStringW body;
    std::vector<std::pair<int, int>> languageExcludedRanges;
    CStringW warning = containerWarning;
    std::set<std::wstring> usedImageIds;
    std::set<std::wstring> usedFb2Ids;
    std::map<std::wstring, std::wstring> anchorIdMap;
    std::vector<EpubNote> notes;
    std::set<std::wstring> noteIds;
    std::set<std::wstring> noteDocumentIds;
    int importedSections = 0;

    g_importStage = L"content inventory";
    PrepareContentInventory(package, anchorIdMap);
    if (options.unifyImageNames && options.separateCoverName && options.importCover)
    {
        const ManifestItem* cover = ResolveCoverImageItem(package);
        if (cover) g_content.coverImageId = cover->id;
    }
    // Inventory retains all spine documents for note discovery and forward links.
    // Record explicit service exclusions separately before rendering any chapter.
    for (const auto& id : package.spine)
    {
        const auto item = package.manifest.find(std::wstring(id));
        if (item != package.manifest.end() && IsLikelyCoverSpineItem(item->second, package))
            g_content.excludedServiceDocuments.emplace(LowerMapKey(item->second.absPath).GetString());
    }

    if (options.importNotes)
    {
        g_importStage = L"notes collection";
        // First pass: collect EPUB endnotes/footnotes into a separate FB2 notes body.
        // This makes FBE note links work instead of leaving raw XHTML links like
        // chapter_003.xhtml#n_1 in the main text.
        for (std::map<std::wstring, ManifestItem>::const_iterator it = package.manifest.begin(); it != package.manifest.end(); ++it)
        {
            const ManifestItem& item = it->second;
            if (item.mediaType.CompareNoCase(L"application/xhtml+xml") != 0 &&
                item.mediaType.CompareNoCase(L"text/html") != 0 &&
                item.href.Right(5).CompareNoCase(L".html") != 0 &&
                item.href.Right(6).CompareNoCase(L".xhtml") != 0)
            {
                continue;
            }

            CStringW xhtmlPath = CombineAndCanonicalizePath(package.opfDir, item.href);
            if (!FileExists(xhtmlPath))
                continue;

            ConvertContext noteCtx = {};
            noteCtx.package = &package;
            noteCtx.currentXhtmlDir = DirectoryOf(xhtmlPath);
            noteCtx.currentXhtmlPath = xhtmlPath;
            noteCtx.usedImageIds = &usedImageIds;
            noteCtx.noteIds = &noteIds;
            noteCtx.usedFb2Ids = &usedFb2Ids;
            noteCtx.anchorIdMap = &anchorIdMap;

            bool isNotesDocument = false;
            if (CollectNotesFromXhtmlFile(xhtmlPath, noteCtx, notes, noteIds, isNotesDocument, warning) && isNotesDocument)
                noteDocumentIds.emplace(item.id.GetString());
        }
        log.AddFormatInt(L"Notes found: %d", static_cast<int>(notes.size()));
    }

    // Prepare explicitly linked, textual non-spine documents before resolving
    // links in the main body, but append them only after the spine chapters.
    CStringW linkedNonSpineBody;
    for (const auto& linked : g_content.linkedNonSpineDocuments)
    {
        const ManifestItem& item = linked.second;
        if (!item.id.IsEmpty() && noteDocumentIds.count(std::wstring(item.id.GetString()))) continue;
        ConvertContext ctx = {};
        ctx.package = &package; ctx.currentXhtmlDir = DirectoryOf(item.absPath); ctx.currentXhtmlPath = item.absPath;
        ctx.usedImageIds = &usedImageIds; ctx.noteIds = &noteIds; ctx.usedFb2Ids = &usedFb2Ids; ctx.anchorIdMap = &anchorIdMap;
        CStringW converted;
        if (!ConvertXhtmlFileToFb2Section(item.absPath, FileNameOnly(item.href), ctx, converted, warning, true) || converted.IsEmpty())
        {
            warning += L"Could not preserve linked non-spine document: " + item.href + L"\r\n";
            continue;
        }
        const auto anchor = anchorIdMap.find(std::wstring(LowerMapKey(item.absPath).GetString()));
        if (anchor == anchorIdMap.end())
        {
            errorText = L"Cannot identify preserved linked non-spine document: " + item.href;
            return false;
        }
        if (!g_content.emittedIds.count(anchor->second))
        {
            g_content.emittedIds.emplace(anchor->second);
            CStringW wrapped;
            AppendFb2Section(wrapped, CStringW(), converted, L" id=\"" + XmlEscape(CStringW(anchor->second.c_str())) + L"\"");
            converted = wrapped;
        }
        g_content.mainDocuments.emplace(LowerMapKey(item.absPath).GetString());
        linkedNonSpineBody += converted;
        ++importedSections;
        log.AddFormat(L"Preserved linked non-spine document: %s", item.href);
    }

    g_importStage = L"chapter conversion";
    for (size_t i = 0; i < package.spine.size(); ++i)
    {
        std::map<std::wstring, ManifestItem>::const_iterator it = package.manifest.find(std::wstring(package.spine[i].GetString()));
        if (it == package.manifest.end())
            continue;

        const ManifestItem& item = it->second;
        const bool mandatory = package.mandatorySpine.count(std::wstring(item.id.GetString())) != 0;
        if (!mandatory && !FileExists(item.absPath)) continue;

        if (options.importNotes && noteDocumentIds.find(std::wstring(item.id.GetString())) != noteDocumentIds.end())
        {
            log.AddFormat(L"Skipped notes document: %s", item.href);
            continue;
        }

        if (IsLikelyCoverSpineItem(item, package))
        {
            log.AddFormat(L"Skipped service document: %s", item.href);
            continue;
        }

        CStringW title = NavigationTitleForItem(package, item);
        if (title.IsEmpty())
        {
            title = item.href;
            int slash = max(title.ReverseFind(L'/'), title.ReverseFind(L'\\'));
            if (slash >= 0)
                title = title.Mid(slash + 1);
        }

        ConvertContext ctx = {};
        ctx.package = &package;
        ctx.currentXhtmlDir = package.opfDir;
        ctx.currentXhtmlPath = item.absPath;
        ctx.usedImageIds = &usedImageIds;
        ctx.noteIds = &noteIds;
        ctx.usedFb2Ids = &usedFb2Ids;
        ctx.anchorIdMap = &anchorIdMap;

        if (IsImageMediaType(item.mediaType) || IsImageHrefByExtension(item.href))
        {
            // These are explicit user filters, not a failed chapter conversion.
            const bool excludedByOption = !options.importImages ||
                (IsSvgImage(item) && options.svgConversionMode == SVG_IMPORT_SKIP);
            if (ConvertImageSpineItemToFb2Section(item, title, ctx, body, warning))
            {
                ++importedSections;
                log.AddFormat(L"Imported image spine item: %s", item.href);
            }
            else if (mandatory && !excludedByOption) { errorText=L"Cannot import mandatory EPUB image chapter: "+item.href+L"\r\n"+warning; return failWithImportLog(); }
            else log.AddFormat(excludedByOption ? L"Skipped image spine item by import option: %s" : L"Skipped optional EPUB image chapter: %s", item.href);
            continue;
        }

        if (item.mediaType.CompareNoCase(L"application/xhtml+xml") != 0 &&
            item.mediaType.CompareNoCase(L"text/html") != 0 &&
            item.href.Right(5).CompareNoCase(L".html") != 0 &&
            item.href.Right(6).CompareNoCase(L".xhtml") != 0)
        {
            if (mandatory) { errorText=L"Unsupported mandatory EPUB spine resource: "+item.href; return failWithImportLog(); }
            log.AddFormat(L"Skipped unsupported optional spine item: %s", item.href);
            continue;
        }

        CStringW xhtmlPath = CombineAndCanonicalizePath(package.opfDir, item.href);
        if (!FileExists(xhtmlPath))
        {
            errorText = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SPINE_FILE_MISSING, L"Spine file was not found: ") + item.href;
            log.AddFormat(L"Missing spine file: %s", item.href);
            if (mandatory) return failWithImportLog();
            continue;
        }

        ctx.currentXhtmlDir = DirectoryOf(xhtmlPath);
        ctx.currentXhtmlPath = xhtmlPath;

        const int bodyStart = body.GetLength();
        if (ConvertXhtmlFileToFb2Section(xhtmlPath, title, ctx, body, warning))
        {
            if (IsTokenInList(item.properties, L"nav") && body.GetLength() > bodyStart)
                languageExcludedRanges.emplace_back(bodyStart, body.GetLength());
            ++importedSections;
            log.AddFormat(L"Imported spine item: %s", item.href);
        }
        else if (mandatory) { errorText=L"Cannot import mandatory EPUB chapter: "+item.href+L"\r\n"+warning; return failWithImportLog(); }
        else log.AddFormat(L"Skipped invalid optional EPUB chapter: %s", item.href);
    }

    body += linkedNonSpineBody;

    // Recover only unaccounted content from documents actually used for notes.
    // Append after spine text so navigation can never reorder mandatory chapters.
    if (options.importNotes) for (const auto& entry : package.manifest)
    {
        const auto& item = entry.second;
        if (!g_content.residualDocuments.count(std::wstring(LowerMapKey(item.absPath)))) continue;
        ConvertContext ctx = {};
        ctx.package = &package;ctx.currentXhtmlDir = DirectoryOf(item.absPath);ctx.currentXhtmlPath = item.absPath;
        ctx.usedImageIds = &usedImageIds;ctx.noteIds = &noteIds;ctx.usedFb2Ids = &usedFb2Ids;ctx.anchorIdMap = &anchorIdMap;
        if (!RecoverNoteDocumentRemainder(item, ctx, body, warning))
        {
            CStringW counts;counts.Format(L"Residual note fragments: found=%u recovered=%u unaccounted=%u\r\n",
                g_content.residualFragments, g_content.recoveredFragments, g_content.residualFragments - g_content.recoveredFragments);
            errorText = L"Cannot safely preserve unaccounted EPUB note content: " + item.href + L"\r\n" + counts + warning;
            log.Add(errorText);log.SaveIfRequested(epubPath, options.writeLogOnWarnings);return false;
        }
        ++importedSections;
    }
    CStringW accounting;
    accounting.Format(L"Residual note fragments: found=%u recovered=%u unaccounted=%u\r\nNote structure index: elements=%u lookups=%u\r\n",
        g_content.residualFragments, g_content.recoveredFragments, g_content.residualFragments - g_content.recoveredFragments,
        g_content.indexedElements, g_content.structuralLookups);
    log.Add(accounting);
    unsigned xhtmlLoadTotal = 0, xhtmlLoadMax = 0;
    for (const auto& entry : g_content.xhtmlLoads) { xhtmlLoadTotal += entry.second; xhtmlLoadMax = max(xhtmlLoadMax, entry.second); }
    CStringW xhtmlAccounting;
    xhtmlAccounting.Format(L"XHTML load calls: total=%u unique=%u max-per-file=%u\r\n", xhtmlLoadTotal,
        static_cast<unsigned>(g_content.xhtmlLoads.size()), xhtmlLoadMax);
    log.Add(xhtmlAccounting);
    log.AddFormatInt(L"XHTML cache hits: %d", static_cast<int>(g_content.xhtmlCacheHits));

    if (options.importCover)
    {
        const auto cover = ResolveCoverImageItem(package);
        if (cover) usedImageIds.emplace(cover->id.GetString());
    }
    log.AddFormatInt(L"Links to excluded service documents preserved as text: %d", g_content.skippedServiceLinks);
    log.AddFormatInt(L"Links to missing EPUB resources: %d", g_content.missingSourceResources);
    log.AddFormatInt(L"Links to missing EPUB anchors: %d", g_content.missingSourceAnchors);
    log.AddFormatInt(L"Imported sections: %d", importedSections);
    log.AddFormatInt(L"Images used: %d", static_cast<int>(usedImageIds.size()));
    log.AddFormatInt(L"Recovered XHTML documents: %d", static_cast<int>(g_content.repairedXhtmlDocuments.size()));
    log.AddFormatInt(L"Unrecovered XHTML documents: %d", static_cast<int>(g_content.unrecoveredXhtmlDocuments.size()));

    if (importedSections == 0)
    {
        errorText = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_SPINE_NO_SUPPORTED, L"Failed to import any supported file from spine.");
        if (!warning.IsEmpty())
            errorText += L"\r\n" + warning;
        log.Add(errorText);
        if (options.keepTempOnError)
            temp.Keep();
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }

    const CStringW detectedLanguage = DetectBodyLanguage(body, languageExcludedRanges);
    const CStringW declaredLanguage = NormalizeFb2Language(package.language);
    if (package.language.IsEmpty() || declaredLanguage == L"und")
    {
        package.language = detectedLanguage.IsEmpty() ? CStringW(L"und") : detectedLanguage;
        if (detectedLanguage.IsEmpty()) warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_LANGUAGE_UNDETERMINED, L"Book language could not be determined from a sufficient body sample.") + L"\r\n";
    }
    else if (!detectedLanguage.IsEmpty() && (declaredLanguage == L"ru" || declaredLanguage == L"en") && declaredLanguage != detectedLanguage)
    {
        package.originalLanguage = package.language;
        package.language = detectedLanguage;
        warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_LANGUAGE_CONFLICT, L"Language metadata conflicts with the main text sample; metadata was retained.") +
            L" (" + declaredLanguage + L" → " + detectedLanguage + L")\r\n";
    }

    CStringW titleBlock;
    if (options.createTitleBlock)
    {
        bool readable = false;
        const bool alreadyPresent = HasEquivalentOpeningTitle(body, package, readable);
        if (readable && !alreadyPresent)
        {
            titleBlock = BuildMetadataTitleBlock(package);
            if (!titleBlock.IsEmpty() && g_content.generatedBookTitleOffset >= 0 && g_content.generatedBookTitleLength > 0)
                body.Delete(g_content.generatedBookTitleOffset, g_content.generatedBookTitleLength);
        }
        else if (!readable) warning += LoadImportRuntimeString(IDS_IMPORT_RUNTIME_TITLE_BLOCK_NOT_INSPECTED, L"Title block was not added because the existing body could not be inspected.") + L"\r\n";
    }

    g_importStage = L"image conversion";
    CStringW binariesXml = BuildBinariesXml(package, usedImageIds, warning,
        static_cast<unsigned long long>(body.GetLength()) + 1024ULL * 1024);
    g_importStage = L"FB2 assembly";
    const CStringW descriptionXml = BuildDescriptionXml(package);
    warning += g_content.warnings;
    CStringW inventory;
    inventory.Format(L"Notes references: found=%d imported=%d lost=%d; images: references=%d used=%u imported=%d skipped=%d\r\n",
        g_content.noteReferences, g_content.importedNoteReferences, g_content.lostNoteReferences,
        g_content.imageReferences, static_cast<unsigned>(usedImageIds.size()), g_content.importedImages, g_content.skippedImages);
    log.Add(inventory);
    log.AddFormatInt(L"Rewritten internal links: %d", static_cast<int>(g_content.mappedLinkElements.size()));
    log.AddFormatInt(L"Marked backlinks removed: %d", static_cast<int>(g_content.removedBacklinkElements.size()));
    log.AddFormatInt(L"Marked backlinks retained: %d", static_cast<int>(g_content.retainedBacklinkElements.size()));
    log.AddFormatInt(L"Marked backlinks retained as text: %d", static_cast<int>(g_content.textOnlyBacklinkElements.size()));
    log.AddFormatInt(L"Elements with detected fidelity limitations: %d", static_cast<int>(g_content.fidelityLossElements.size()));

    if (!warning.IsEmpty())
        log.Add(warning);

    outFb2Xml += L"<?xml version=\"1.0\"?>\r\n";
    outFb2Xml += L"<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" ";
    outFb2Xml += L"xmlns:l=\"http://www.w3.org/1999/xlink\">\r\n";
    outFb2Xml += descriptionXml;
    outFb2Xml += L"  <body>\r\n";
    outFb2Xml += titleBlock;
    if (options.addDiagnosticSection && !warning.IsEmpty())
    {
        outFb2Xml += L"    <section>\r\n";
        CStringW warningTitle = LoadImportRuntimeString(IDS_IMPORT_RUNTIME_IMPORT_WARNINGS_TITLE, L"      <title><p>EPUB import warnings</p></title>\r\n");
        warningTitle.Replace(L"\\r\\n", L"\r\n");
        outFb2Xml += warningTitle;
        CStringW summary;
        summary.Format(L"Service links preserved as text: %d; missing EPUB resources: %d; missing EPUB anchors: %d.",
            g_content.skippedServiceLinks, g_content.missingSourceResources, g_content.missingSourceAnchors);
        outFb2Xml += L"      <p>" + XmlEscape(summary) + L"</p>\r\n";
        outFb2Xml += L"    </section>\r\n";
    }
    outFb2Xml += body;
    outFb2Xml += L"  </body>\r\n";
    if (options.importNotes)
        outFb2Xml += BuildNotesBodyXml(notes);
    if (!binariesXml.IsEmpty())
    {
        // Reuse the image buffer instead of allocating another full-size copy
        // while joining the small document prefix to the encoded payload.
        const int prefixLength = outFb2Xml.GetLength();
        const int binaryLength = binariesXml.GetLength();
        if (prefixLength > INT_MAX - binaryLength) AtlThrow(E_OUTOFMEMORY);
        wchar_t* combined = binariesXml.GetBufferSetLength(prefixLength + binaryLength);
        ::memmove(combined + prefixLength, combined, static_cast<size_t>(binaryLength) * sizeof(wchar_t));
        ::memcpy(combined, outFb2Xml.GetString(), static_cast<size_t>(prefixLength) * sizeof(wchar_t));
        binariesXml.ReleaseBufferSetLength(prefixLength + binaryLength);
        outFb2Xml = binariesXml;
    }
    binariesXml.Empty();
    body.Empty();
    titleBlock.Empty();
    outFb2Xml += L"</FictionBook>\r\n";

    // XML, section shape and reference integrity are always checked. The schema
    // check additionally follows the user's existing validation option.
    {
        g_importStage = L"FB2 validation";
        CStringW validationWarning;
        const bool valid = ValidateGeneratedFb2Xml(outFb2Xml, validationWarning, package, anchorIdMap);
        log.AddFormatInt(L"Generated FB2 links with missing targets: %d", g_content.missingGeneratedTargets);
        if (!validationWarning.IsEmpty())
        {
            warning += validationWarning;
            log.Add(validationWarning);
        }
        if (!valid)
        {
            if (options.saveIntermediateFb2OnError && !outFb2Xml.IsEmpty())
            {
                const CStringW recovery = ChangeExtension(epubPath, L".failed-import.fb2");
                if (WriteUtf8TextFile(recovery, outFb2Xml)) log.AddFormat(L"Saved intermediate FB2 for diagnostics: %s", recovery);
            }
            CStringW summary;
            summary.Format(L"FB2 validation failed; missing generated link targets: %d. Detailed diagnostics are available in the import log if it could be written.\r\n", g_content.missingGeneratedTargets);
            errorText = inventory + summary;
            log.SaveIfRequested(epubPath, true);return false;
        }
    }

    if (options.saveIntermediateFb2OnError && !warning.IsEmpty() && !outFb2Xml.IsEmpty())
    {
        CStringW failedCopyPath = ChangeExtension(epubPath, L".failed-import.fb2");
        if (WriteUtf8TextFile(failedCopyPath, outFb2Xml))
            log.AddFormat(L"Saved intermediate FB2 for diagnostics: %s", failedCopyPath);
        else
            log.AddFormat(L"Failed to save intermediate FB2: %s", failedCopyPath);
    }

    if (options.saveFb2Copy)
    {
        CStringW fb2CopyPath = ChangeExtension(epubPath, L".imported.fb2");
        if (WriteUtf8TextFile(fb2CopyPath, outFb2Xml))
            log.AddFormat(L"Saved FB2 copy: %s", fb2CopyPath);
        else
            log.AddFormat(L"Failed to save FB2 copy: %s", fb2CopyPath);
    }

    if (!warning.IsEmpty())
    {
        CStringW diagnosticCounts;
        diagnosticCounts.Format(static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_DIAGNOSTIC_COUNTS,
            L"Sections: %d; missing source resources: %d; missing source anchors: %d; service links retained as text: %d; missing FB2 targets: %d.")),
            importedSections, g_content.missingSourceResources, g_content.missingSourceAnchors,
            g_content.skippedServiceLinks, g_content.missingGeneratedTargets);
        CStringW markupCounts;
        markupCounts.Format(static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_MARKUP_COUNTS,
            L"XHTML documents recovered: %d; not recovered: %d.")),
            static_cast<int>(g_content.repairedXhtmlDocuments.size()), static_cast<int>(g_content.unrecoveredXhtmlDocuments.size()));
        CStringW fidelityCounts;
        fidelityCounts.Format(static_cast<LPCWSTR>(LoadImportRuntimeString(IDS_IMPORT_RUNTIME_FIDELITY_COUNTS,
            L"Rewritten internal links: %d; marked backlinks removed: %d, retained: %d, retained as text: %d; elements with detected fidelity limitations: %d.")),
            static_cast<int>(g_content.mappedLinkElements.size()), static_cast<int>(g_content.removedBacklinkElements.size()),
            static_cast<int>(g_content.retainedBacklinkElements.size()), static_cast<int>(g_content.textOnlyBacklinkElements.size()),
            static_cast<int>(g_content.fidelityLossElements.size()));
        errorText = inventory + diagnosticCounts + L"\r\n" + markupCounts + L"\r\n" + fidelityCounts + L"\r\nImport completed with warnings. Detailed diagnostics are available in the import log when enabled.\r\n";
        log.Add(L"[WARNING] " + diagnosticCounts);
    }
    log.Add(warning.IsEmpty() ? L"Import completed." : L"Import completed with warnings.");
    log.SaveIfRequested(epubPath, options.writeLogOnWarnings && !warning.IsEmpty());
    return true;
}

namespace
{
    BSTR AllocateImportEpubApiText(const CStringW& source)
    {
        return SysAllocStringLen(source.GetString(), static_cast<UINT>(source.GetLength()));
    }

    BSTR AllocateLargeImportTextViaTemporaryFile(CStringW& source, DWORD& failure)
    {
        failure = ERROR_SUCCESS;
        TempDirectory temp;
        if (temp.Path().IsEmpty()) { failure = ERROR_CANNOT_MAKE; return nullptr; }
        const CStringW path = temp.Path() + L"\\fb2-utf16.tmp";
        HANDLE file = ::CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE) { failure = ::GetLastError(); return nullptr; }
        const UINT length = static_cast<UINT>(source.GetLength());
        const unsigned long long total = static_cast<unsigned long long>(length) * sizeof(wchar_t);
        const unsigned char* input = reinterpret_cast<const unsigned char*>(source.GetString());
        unsigned long long position = 0;
        while (position < total)
        {
            const DWORD chunk = static_cast<DWORD>(min(8ULL * 1024 * 1024, total - position));
            DWORD written = 0;
            if (!::WriteFile(file, input + static_cast<size_t>(position), chunk, &written, nullptr) || written != chunk)
            { failure = ::GetLastError(); if (!failure) failure = ERROR_WRITE_FAULT; ::CloseHandle(file); return nullptr; }
            position += written;
        }
        LARGE_INTEGER zero = {};
        if (!::SetFilePointerEx(file, zero, nullptr, FILE_BEGIN))
        { failure = ::GetLastError(); ::CloseHandle(file); return nullptr; }
        // Release the large ATL buffer before requesting the BSTR. The file is
        // private to this invocation and is deleted when its handle closes.
        source.Empty();
        BSTR output = ::SysAllocStringLen(nullptr, length);
        if (!output) { failure = ERROR_NOT_ENOUGH_MEMORY; ::CloseHandle(file); return nullptr; }
        unsigned char* destination = reinterpret_cast<unsigned char*>(output);
        position = 0;
        while (position < total)
        {
            const DWORD chunk = static_cast<DWORD>(min(8ULL * 1024 * 1024, total - position));
            DWORD read = 0;
            if (!::ReadFile(file, destination + static_cast<size_t>(position), chunk, &read, nullptr) || read != chunk)
            { failure = ::GetLastError(); if (!failure) failure = ERROR_READ_FAULT; ::SysFreeString(output); ::CloseHandle(file); return nullptr; }
            position += read;
        }
        ::CloseHandle(file);
        return output;
    }

    EpubImportOptions ImportEpubOptionsFromApi(const ImportEpubOptionsV1& source)
    {
        EpubImportOptions result;
        result.importCover = source.importCover != 0;
        result.importImages = source.importImages != 0;
        result.importNotes = source.importNotes != 0;
        result.useNavigationTitles = source.useNavigationTitles != 0;
        result.repairEncoding = source.repairEncoding != 0;
        result.skipServicePages = source.skipServicePages != 0;
        result.importTables = source.importTables != 0;
        result.importLists = source.importLists != 0;
        result.importPoemsEpigraphs = source.importPoemsEpigraphs != 0;
        result.importSubtitles = source.importSubtitles != 0;
        if ((source.version >= IMPORT_EPUB_API_VERSION_2 && source.version <= IMPORT_EPUB_API_VERSION_6) && source.cbSize >= sizeof(ImportEpubOptionsV2))
            result.separatorSubtitles = reinterpret_cast<const ImportEpubOptionsV2&>(source).separatorSubtitles != 0;
        if ((source.version >= IMPORT_EPUB_API_VERSION_3 && source.version <= IMPORT_EPUB_API_VERSION_6) && source.cbSize >= sizeof(ImportEpubOptionsV3))
            result.createTitleBlock = reinterpret_cast<const ImportEpubOptionsV3&>(source).createTitleBlock != 0;
        if ((source.version >= IMPORT_EPUB_API_VERSION_4 && source.version <= IMPORT_EPUB_API_VERSION_6) && source.cbSize >= sizeof(ImportEpubOptionsV4))
        {
            const auto& extended = reinterpret_cast<const ImportEpubOptionsV4&>(source);
            result.unifyImageNames = extended.unifyImageNames != 0;
            result.separateCoverName = extended.separateCoverName != 0;
        }
        if ((source.version == IMPORT_EPUB_API_VERSION_5 || source.version == IMPORT_EPUB_API_VERSION_6) && source.cbSize >= sizeof(ImportEpubOptionsV5))
            result.unifyNoteNumbers = reinterpret_cast<const ImportEpubOptionsV5&>(source).unifyNoteNumbers != 0;
        if (source.version == IMPORT_EPUB_API_VERSION_6 && source.cbSize >= sizeof(ImportEpubOptionsV6))
        {
            const auto& extended = reinterpret_cast<const ImportEpubOptionsV6&>(source);
            result.blankBeforeImages = extended.blankBeforeImages != 0;
            result.blankAfterImages = extended.blankAfterImages != 0;
        }
        result.splitSectionsByHeadings = source.splitSectionsByHeadings != 0;
        result.preserveLinks = source.preserveLinks != 0;
        result.cleanTypography = source.cleanTypography != 0;
        result.importPageBreaks = source.importPageBreaks != 0;
        result.skipHiddenElements = source.skipHiddenElements != 0;
        result.validateResult = source.validateResult != 0;
        result.addDiagnosticSection = source.addDiagnosticSection != 0;
        result.writeImportLog = source.writeImportLog != 0;
        result.saveFb2Copy = source.saveFb2Copy != 0;
        result.useCssSemanticClasses = source.useCssSemanticClasses != 0;
        result.removeFootnoteBacklinks = source.removeFootnoteBacklinks != 0;
        result.removeServiceSections = source.removeServiceSections != 0;
        result.writeLogOnWarnings = source.writeLogOnWarnings != 0;
        result.saveIntermediateFb2OnError = source.saveIntermediateFb2OnError != 0;
        result.keepTempOnError = source.keepTempOnError != 0;
        result.svgConversionMode = source.svgConversionMode;
        return result;
    }
}

extern "C" HRESULT WINAPI ImportEPUB_BuildFb2XmlW(
    LPCWSTR epubPath,
    const ImportEpubOptionsV1* apiOptions,
    BSTR* apiFb2Xml,
    ImportEpubRuntimeStatsV1* apiRuntimeStats,
    BSTR* apiErrorText)
{
    if (!apiFb2Xml || !apiErrorText)
        return E_POINTER;
    *apiFb2Xml = nullptr;
    *apiErrorText = nullptr;
    if (!epubPath || !*epubPath || (apiOptions && (apiOptions->cbSize < sizeof(ImportEpubOptionsV1) ||
        (apiOptions->version < IMPORT_EPUB_API_VERSION || apiOptions->version > IMPORT_EPUB_API_VERSION_6) ||
        (apiOptions->version == IMPORT_EPUB_API_VERSION_2 && apiOptions->cbSize < sizeof(ImportEpubOptionsV2)) ||
        (apiOptions->version == IMPORT_EPUB_API_VERSION_3 && apiOptions->cbSize < sizeof(ImportEpubOptionsV3)) ||
        (apiOptions->version == IMPORT_EPUB_API_VERSION_4 && apiOptions->cbSize < sizeof(ImportEpubOptionsV4)) ||
        (apiOptions->version == IMPORT_EPUB_API_VERSION_5 && apiOptions->cbSize < sizeof(ImportEpubOptionsV5)) ||
        (apiOptions->version == IMPORT_EPUB_API_VERSION_6 && apiOptions->cbSize < sizeof(ImportEpubOptionsV6)))) ||
        (apiRuntimeStats && apiRuntimeStats->cbSize < sizeof(ImportEpubRuntimeStatsV1)))
        return E_INVALIDARG;

    CStringW fb2Xml;
    CStringW errorText;
    bool converted = false;
    const auto reportOutOfMemory = [apiErrorText](LPCWSTR stage) {
        wchar_t message[256] = {};
        ::swprintf_s(message, _countof(message), L"ImportEPUB ran out of memory during conversion (stage: %s).", stage);
        *apiErrorText = ::SysAllocString(message);
        return E_OUTOFMEMORY;
    };
    try
    {
        const EpubImportOptions options = apiOptions ? ImportEpubOptionsFromApi(*apiOptions) : EpubImportOptions();
        converted = BuildFb2XmlFromEpub(CStringW(epubPath), options, fb2Xml, errorText);
    }
    catch (const _com_error& error)
    {
        const HRESULT hr = error.Error();
        if (hr == E_OUTOFMEMORY)
        {
            return reportOutOfMemory(g_importStage);
        }
        wchar_t message[256] = {};
        ::swprintf_s(message, _countof(message), L"ImportEPUB failed at %s with COM error 0x%08lX.", g_importStage, static_cast<unsigned long>(hr));
        *apiErrorText = ::SysAllocString(message);
        return *apiErrorText ? (FAILED(hr) ? hr : E_FAIL) : E_OUTOFMEMORY;
    }
    catch (const ATL::CAtlException& error)
    {
        const HRESULT hr = error.m_hr;
        if (hr == E_OUTOFMEMORY)
        {
            return reportOutOfMemory(g_importStage);
        }
        wchar_t message[256] = {};
        ::swprintf_s(message, _countof(message), L"ImportEPUB failed at %s with ATL error 0x%08lX.", g_importStage, static_cast<unsigned long>(hr));
        *apiErrorText = ::SysAllocString(message);
        return *apiErrorText ? (FAILED(hr) ? hr : E_FAIL) : E_OUTOFMEMORY;
    }
    catch (const std::bad_alloc&)
    {
        return reportOutOfMemory(g_importStage);
    }
    catch (...)
    {
        wchar_t message[256] = {};
        ::swprintf_s(message, _countof(message), L"ImportEPUB encountered an unexpected internal error at %s.", g_importStage);
        *apiErrorText = ::SysAllocString(message);
        return *apiErrorText ? E_FAIL : E_OUTOFMEMORY;
    }

    if (!converted)
    {
        *apiErrorText = AllocateImportEpubApiText(errorText);
        return *apiErrorText ? E_FAIL : E_OUTOFMEMORY;
    }

    *apiFb2Xml = AllocateImportEpubApiText(fb2Xml);
    if (!*apiFb2Xml)
    {
        DWORD failure = ERROR_SUCCESS;
        try { *apiFb2Xml = AllocateLargeImportTextViaTemporaryFile(fb2Xml, failure); }
        catch (...) { return reportOutOfMemory(L"API output temporary storage"); }
        if (!*apiFb2Xml)
        {
            if (failure == ERROR_NOT_ENOUGH_MEMORY) return reportOutOfMemory(L"API output allocation");
            wchar_t message[256] = {};
            ::swprintf_s(message, _countof(message), L"ImportEPUB could not use temporary output storage (Win32 error %lu).", failure);
            *apiErrorText = ::SysAllocString(message);
            return E_FAIL;
        }
    }
    if (!errorText.IsEmpty())
    {
        *apiErrorText = AllocateImportEpubApiText(errorText);
        if (!*apiErrorText) {::SysFreeString(*apiFb2Xml);*apiFb2Xml = nullptr;return E_OUTOFMEMORY;}
    }

    if (apiRuntimeStats)
    {
        const EpubImportRuntimeStats stats = GetLastEpubImportRuntimeStats();
        apiRuntimeStats->version = IMPORT_EPUB_API_VERSION;
        apiRuntimeStats->svgImages = stats.svgImages;
        apiRuntimeStats->svgConverted = stats.svgConverted;
        apiRuntimeStats->svgPlaceholders = stats.svgPlaceholders;
        apiRuntimeStats->svgSkipped = stats.svgSkipped;
        wcsncpy_s(apiRuntimeStats->svgBackend, stats.svgBackend, _TRUNCATE);
    }
    return S_OK;
}
