#include "stdafx.h"
#include "MsxmlImport.h"
#include "EpubImport.h"

#include <vector>
#include <map>
#include <string>
#include <set>
#include <algorithm>
#include <cwctype>
#include <shlwapi.h>
#include <gdiplus.h>

namespace
{
    EpubImportOptions g_options;
    EpubImportRuntimeStats g_runtimeStats;

    struct ManifestItem
    {
        CStringW id;
        CStringW href;
        CStringW mediaType;
        CStringW properties;
        CStringW absPath;
    };

    struct EpubPackage
    {
        CStringW opfPath;
        CStringW opfDir;
        CStringW title;
        CStringW titleSort;
        CStringW language;
        CStringW description;
        CStringW identifier;
        CStringW isbn;
        CStringW date;
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
        std::map<std::wstring, CStringW> navTitlesByAbsPath;
        std::set<std::wstring> guideServiceDocuments;
        int guideReferenceCount;

        EpubPackage()
            : guideReferenceCount(0)
        {
        }
    };

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
        // pairs beginning with "Р" and "С" (for example: "РђРЅРЅРѕС‚Р°С†РёСЏ").
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

            // Drop zero-width layout marks which sometimes leak from EPUB.
            if (ch == 0x200B || ch == 0x200C || ch == 0x200D || ch == 0xFEFF)
                continue;

            // Normalize the most common typographic separators to stable Unicode.
            // Keep Russian quotation marks and the ellipsis as they are useful in FB2.
            if (ch == 0x2010 || ch == 0x2011 || ch == 0x2012)
                ch = L'-';
            else if (ch == 0x2014 || ch == 0x2015)
                ch = 0x2014; // em dash

            out += ch;
        }

        while (out.Find(L"  ") >= 0)
            out.Replace(L"  ", L" ");
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

    bool DecodeTextFileBytes(const std::vector<unsigned char>& bytes, CStringW& text)
    {
        text.Empty();
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
        int wideCount = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, dataSize, nullptr, 0);
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
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

        for (size_t i = 0; i < _countof(entities); ++i)
            s.Replace(entities[i].name, entities[i].value);
        return s;
    }

    CStringW EscapeUnsupportedAmpersands(CStringW s)
    {
        CStringW out;
        out.Preallocate(s.GetLength());
        for (int i = 0; i < s.GetLength(); ++i)
        {
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

    bool LoadXmlFileAfterMarkupSanitizing(const CStringW& path, bool htmlMode, MSXML2::IXMLDOMDocument2Ptr& dom, CStringW& errorText)
    {
        std::vector<unsigned char> bytes;
        CStringW xml;
        if (!ReadTextFileBytes(path, bytes) || !DecodeTextFileBytes(bytes, xml))
            return false;

        xml = SanitizeMarkupForXml(xml, htmlMode);

        HRESULT hr = CreateMsxmlDom(dom);
        if (FAILED(hr))
            return false;

        VARIANT_BOOL loaded = VARIANT_FALSE;
        CComBSTR xmlText(static_cast<LPCWSTR>(xml));
        HRESULT loadHr = dom->loadXML(xmlText, &loaded);
        if (FAILED(loadHr) || loaded != VARIANT_TRUE)
        {
            CStringW reason = MsxmlGetParseErrorReason(dom);
            if (reason.IsEmpty())
                reason = htmlMode ? L"неизвестная ошибка XML после HTML fallback" : L"неизвестная ошибка XML после XML fallback";
            errorText.Format(htmlMode
                ? L"MSXML не смог прочитать HTML/XHTML-файл даже после нормализации HTML-разметки:\r\n%s\r\n%s"
                : L"MSXML не смог прочитать XML-файл даже после безопасной нормализации:\r\n%s\r\n%s",
                path.GetString(), reason.GetString());
            return false;
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

    CStringW CombineAndCanonicalizePath(const CStringW& baseDir, const CStringW& relative)
    {
        CStringW rel = UrlDecodeUtf8(relative);
        rel.Replace(L'/', L'\\');

        wchar_t combined[4096] = L"";
        wchar_t canonical[4096] = L"";

        if (!PathCombineW(combined, baseDir, rel))
            return CStringW();
        if (!PathCanonicalizeW(canonical, combined))
            return CStringW(combined);

        return CStringW(canonical);
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

    bool DeleteDirectoryTree(const CStringW& dir)
    {
        if (!DirectoryExists(dir))
            return true;

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
                    ::SetFileAttributesW(child, FILE_ATTRIBUTE_NORMAL);
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
            wchar_t temp[MAX_PATH] = L"";
            ::GetTempPathW(_countof(temp), temp);
            m_path.Format(L"%sImportEPUB_%s", temp, GuidString().GetString());
            CreateDirectoryRecursive(m_path);
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
        CStringW canonicalRoot;
        wchar_t rootBuffer[4096] = L"";
        if (!PathCanonicalizeW(rootBuffer, root))
            canonicalRoot = root;
        else
            canonicalRoot = rootBuffer;

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

                wchar_t canonicalChild[4096] = L"";
                CStringW checkedPath = PathCanonicalizeW(canonicalChild, child) ? CStringW(canonicalChild) : child;
                if (!HasPathPrefixNoCase(checkedPath, canonicalRoot))
                {
                    FindClose(h);
                    errorText = L"ZIP содержит небезопасный путь вне временной папки: " + child;
                    return false;
                }

                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                {
                    FindClose(h);
                    errorText = L"ZIP содержит reparse point/symlink, импорт остановлен: " + child;
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

    bool WaitForZipFolderExtraction(const CStringW& extractDir, CStringW& errorText)
    {
        CStringW container = extractDir + L"\\META-INF\\container.xml";
        for (int i = 0; i < 600; ++i)
        {
            if (FileExists(container))
                return true;
            Sleep(100);
        }

        errorText = L"Windows ZIP folder provider не завершил распаковку EPUB за отведённое время или не создал META-INF/container.xml.";
        return false;
    }

    bool ExpandEpubToDirectory(const CStringW& epubPath, const CStringW& tempRoot, CStringW& extractDir, CStringW& errorText)
    {
        // No PowerShell Expand-Archive and no tar.exe. EPUB is copied to a
        // temporary .zip name and extracted through the built-in Windows ZIP
        // folder provider. The rest of the importer still works with a temp
        // directory, which keeps the parser code simple and compatible with FBE.
        CStringW zipPath = tempRoot + L"\\source.zip";
        extractDir = tempRoot + L"\\epub";

        if (!::CopyFileW(epubPath, zipPath, FALSE))
        {
            errorText.Format(L"Не удалось скопировать EPUB во временный ZIP-файл. Код Win32: %lu", ::GetLastError());
            return false;
        }

        CreateDirectoryRecursive(extractDir);

        CComPtr<IShellDispatch> shell;
        HRESULT hr = shell.CoCreateInstance(CLSID_Shell);
        if (FAILED(hr) || !shell)
        {
            errorText.Format(L"Не удалось создать Shell.Application для распаковки EPUB. HRESULT=0x%08lX", hr);
            return false;
        }

        VARIANT vZip;
        VariantInit(&vZip);
        vZip.vt = VT_BSTR;
        vZip.bstrVal = SysAllocString(zipPath);
        CComPtr<Folder> zipFolder;
        hr = shell->NameSpace(vZip, &zipFolder);
        VariantClear(&vZip);
        if (FAILED(hr) || !zipFolder)
        {
            errorText = L"Windows не смог открыть EPUB как ZIP-контейнер. Возможно, файл повреждён или заблокирован.";
            return false;
        }

        VARIANT vDest;
        VariantInit(&vDest);
        vDest.vt = VT_BSTR;
        vDest.bstrVal = SysAllocString(extractDir);
        CComPtr<Folder> destFolder;
        hr = shell->NameSpace(vDest, &destFolder);
        VariantClear(&vDest);
        if (FAILED(hr) || !destFolder)
        {
            errorText = L"Windows не смог открыть временную папку назначения для распаковки EPUB.";
            return false;
        }

        CComPtr<FolderItems> items;
        hr = zipFolder->Items(&items);
        if (FAILED(hr) || !items)
        {
            errorText = L"Windows ZIP provider не вернул список файлов EPUB.";
            return false;
        }

        CComPtr<IDispatch> itemsDispatch;
        hr = items->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&itemsDispatch.p));
        if (FAILED(hr) || !itemsDispatch)
        {
            errorText = L"Не удалось получить IDispatch для списка файлов EPUB.";
            return false;
        }

        _variant_t vItems(static_cast<IDispatch*>(itemsDispatch), true);
        _variant_t vOptions(static_cast<long>(FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR));
        hr = destFolder->CopyHere(vItems, vOptions);
        if (FAILED(hr))
        {
            errorText.Format(L"Windows ZIP provider не смог распаковать EPUB. HRESULT=0x%08lX", hr);
            return false;
        }

        if (!WaitForZipFolderExtraction(extractDir, errorText))
            return false;

        if (!ValidateExtractedDirectoryTree(extractDir, errorText))
            return false;

        return true;
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
        HRESULT hr = CreateMsxmlDom(dom);
        if (FAILED(hr))
        {
            errorText.Format(L"Не удалось создать MSXML DOMDocument.6.0. HRESULT: 0x%08X", static_cast<unsigned int>(hr));
            return false;
        }

        VARIANT_BOOL loaded = VARIANT_FALSE;
        _variant_t fileName(static_cast<LPCWSTR>(path));
        HRESULT loadHr = dom->load(fileName, &loaded);
        if (FAILED(loadHr) || loaded != VARIANT_TRUE)
        {
            CStringW reason = MsxmlGetParseErrorReason(dom);
            if (reason.IsEmpty())
                reason = L"неизвестная ошибка XML";

            // Some real-world EPUB files use .htm/.html spine items that are HTML,
            // not strict XHTML: undeclared &nbsp;, <link>, <meta>, <br>, <img>
            // and other void elements are often not closed as XML. Retry by
            // loading the file as text and applying a conservative XML-safe
            // normalization. For OPF/container XML the same fallback also fixes
            // common XML 1.1 declarations that MSXML 6.0 rejects as "bad version".
            const bool htmlMode = IsLikelyHtmlXmlDocumentPath(path);
            if (LoadXmlFileAfterMarkupSanitizing(path, htmlMode, dom, errorText))
                return true;

            errorText.Format(L"MSXML не смог прочитать XML-файл:\r\n%s\r\n%s", path.GetString(), reason.GetString());
            return false;
        }

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

    bool IsHiddenElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!g_options.skipHiddenElements || !node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;

        if (!Attr(node, L"hidden").IsEmpty())
            return true;

        CStringW ariaHidden = Attr(node, L"aria-hidden");
        ariaHidden.Trim();
        if (ariaHidden.CompareNoCase(L"true") == 0 || ariaHidden == L"1")
            return true;

        CStringW style = Attr(node, L"style");
        style.MakeLower();
        style.Remove(L' ');
        style.Remove(L'\t');
        if (style.Find(L"display:none") >= 0 || style.Find(L"visibility:hidden") >= 0)
            return true;

        return HasCssClass(node, L"hidden") ||
            HasCssClass(node, L"hide") ||
            HasCssClass(node, L"visually-hidden") ||
            HasCssClass(node, L"sr-only") ||
            HasCssClass(node, L"screen-reader-only");
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

    bool IsBacklinkElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;
        if (!g_options.removeFootnoteBacklinks)
            return false;
        return IsEpubType(node, L"backlink") || IsRoleToken(node, L"doc-backlink") ||
            HasSemanticCssClass(node, L"backlink") || HasSemanticCssClass(node, L"backlinks");
    }

    bool IsNoteReferenceElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;
        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"a") != 0)
            return false;
        return IsEpubType(node, L"noteref") || IsRoleToken(node, L"doc-noteref") ||
            HasSemanticCssClass(node, L"noteref") || HasSemanticCssClass(node, L"footnote-ref") ||
            HasSemanticCssClass(node, L"fnref") || HasSemanticCssClass(node, L"footnote_reference");
    }

    bool IsEndnoteElement(const MSXML2::IXMLDOMNodePtr& node)
    {
        if (!node || MsxmlGetNodeType(node) != MSXML2::NODE_ELEMENT)
            return false;
        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"aside") != 0 && name.CompareNoCase(L"section") != 0 && name.CompareNoCase(L"div") != 0)
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
            errorText = L"В EPUB не найден файл META-INF/container.xml.";
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

        errorText = L"В container.xml не найден rootfile с OPF-пакетом.";
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
        if (!nodes.empty())
            package.date = NodeText(nodes[0]);

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
                package.spine.push_back(idref);
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
        if (package.language.IsEmpty())
            package.language = L"ru";

        if (package.spine.empty())
        {
            errorText = L"В OPF не найден список spine/itemref — невозможно определить порядок глав.";
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

    void ParseNavigationXhtml(EpubPackage& package, const ManifestItem& navItem, ImportLog& log)
    {
        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (!LoadXmlFile(navItem.absPath, doc, error))
        {
            log.Add(L"nav.xhtml: не удалось прочитать оглавление.");
            log.Add(error);
            return;
        }

        std::vector<MSXML2::IXMLDOMNodePtr> links;
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"a", links);
        for (size_t i = 0; i < links.size(); ++i)
        {
            CStringW href = Attr(links[i], L"href");
            CStringW title = NodeText(links[i]);
            AddNavigationTitle(package, DirectoryOf(navItem.absPath), href, title);
        }
        log.AddFormatInt(L"nav.xhtml: заголовков найдено: %d", static_cast<int>(package.navTitlesByAbsPath.size()));
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
            log.Add(L"toc.ncx: не удалось прочитать оглавление.");
            log.Add(error);
            return;
        }

        std::vector<MSXML2::IXMLDOMNodePtr> navPoints;
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"navPoint", navPoints);
        for (size_t i = 0; i < navPoints.size(); ++i)
        {
            MSXML2::IXMLDOMNodePtr content = FindFirstDescendantByLocalNameForToc(navPoints[i], L"content");
            MSXML2::IXMLDOMNodePtr textNode = FindFirstDescendantByLocalNameForToc(navPoints[i], L"text");
            CStringW src = content ? Attr(content, L"src") : CStringW();
            CStringW title = textNode ? NodeText(textNode) : CStringW();
            AddNavigationTitle(package, DirectoryOf(ncxItem.absPath), src, title);
        }
        log.AddFormatInt(L"toc.ncx: navPoint найдено: %d", static_cast<int>(navPoints.size()));
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
        if (!FileExists(adapterPath))
        {
            warning += L"SVG не преобразован: рядом с ImportEPUB.dll/ImportEPUBBatch.exe не найдена дополнительная библиотека ImportEPUBLunaSVG.dll. Будет создана растровая заглушка: " + svgPath + L"\r\n";
            return false;
        }

        HMODULE adapter = ::LoadLibraryW(adapterPath);
        if (!adapter)
        {
            warning += L"SVG не преобразован: не удалось загрузить ImportEPUBLunaSVG.dll. Проверьте разрядность Win32/x86 и зависимости LunaSVG. Будет создана растровая заглушка: " + adapterPath + L"\r\n";
            return false;
        }

        ImportEPUBLunaSvgRenderPngFn render = reinterpret_cast<ImportEPUBLunaSvgRenderPngFn>(
            ::GetProcAddress(adapter, "ImportEPUBLunaSvgRenderPng"));
        if (!render)
        {
            ::FreeLibrary(adapter);
            warning += L"SVG не преобразован: ImportEPUBLunaSVG.dll не экспортирует функцию ImportEPUBLunaSvgRenderPng. Будет создана растровая заглушка.\r\n";
            return false;
        }

        wchar_t errorText[1024] = L"";
        const int ok = render(svgPath, pngPath, 2200, 3200, errorText, _countof(errorText));
        ::FreeLibrary(adapter);

        if (ok != 0 && FileExists(pngPath))
            return true;

        CStringW details(errorText);
        details.Trim();
        warning += L"SVG не преобразован через ImportEPUBLunaSVG.dll";
        if (!details.IsEmpty())
            warning += L": " + details;
        warning += L". Будет создана растровая заглушка: " + svgPath + L"\r\n";
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
            warning += L"Не удалось запустить GDI+ для преобразования PNG в JPEG.\r\n";
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
            warning += L"Не удалось сохранить SVG как JPEG: " + jpegPath + L"\r\n";
        return ok && FileExists(jpegPath);
    }

    bool CreateSvgPlaceholderImageFile(const CStringW& outputPath, bool jpeg, const CStringW& svgName, CStringW& warning)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken = 0;
        if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
        {
            warning += L"Не удалось запустить GDI+ для создания SVG-заглушки.\r\n";
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
            warning += L"Не удалось создать изображение-заглушку для SVG: " + outputPath + L"\r\n";
        return ok && FileExists(outputPath);
    }

    bool ReadBinaryFile(const CStringW& path, std::vector<unsigned char>& data);

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
            warning += L"SVG пропущен по настройке импорта: " + item.href + L"\r\n";
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
        bool rendered = RenderSvgToPngFile(item.absPath, pngPath, warning);
        bool ok = false;

        if (!rendered)
        {
            // Do not leave a dangling <image l:href="#..."> without a matching <binary>.
            // If no SVG renderer is available, create a visible placeholder bitmap instead.
            // The original SVG is not embedded in this mode because many FB2/FBE consumers do not display SVG.
            warning += L"Для SVG будет создана растровая заглушка, потому что дополнительная библиотека ImportEPUBLunaSVG.dll отсутствует или не смогла выполнить рендеринг: " + item.href + L"\r\n";
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
            warning += L"Не удалось прочитать результат преобразования SVG: " + item.href + L"\r\n";
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
    };

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
        return nullptr;
    }

    CStringW Fb2ImageElementForHref(const ConvertContext& ctx, const CStringW& href)
    {
        if (!g_options.importImages)
            return CStringW();

        const ManifestItem* item = FindManifestImageByHref(ctx, href);
        if (!item)
            return CStringW();
        if (IsSvgImage(*item) && g_options.svgConversionMode == SVG_IMPORT_SKIP)
            return CStringW();

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

    const ManifestItem* ResolveCoverImageItem(const EpubPackage& package)
    {
        if (package.coverId.IsEmpty())
            return nullptr;

        const ManifestItem* cover = FindManifestItemById(package, package.coverId);
        if (!cover)
            return nullptr;

        if (IsImageMediaType(cover->mediaType) || IsImageHrefByExtension(cover->href))
            return cover;

        if (cover->mediaType.CompareNoCase(L"application/xhtml+xml") != 0 &&
            cover->mediaType.CompareNoCase(L"text/html") != 0 &&
            cover->href.Right(5).CompareNoCase(L".html") != 0 &&
            cover->href.Right(6).CompareNoCase(L".xhtml") != 0)
        {
            return nullptr;
        }

        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (!LoadXmlFile(cover->absPath, doc, error))
            return nullptr;

        ConvertContext ctx = {};
        ctx.package = &package;
        ctx.currentXhtmlDir = DirectoryOf(cover->absPath);
        ctx.usedImageIds = nullptr;
        ctx.noteIds = nullptr;

        std::vector<MSXML2::IXMLDOMNodePtr> images;
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"img", images);
        CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"image", images);
        for (size_t i = 0; i < images.size(); ++i)
        {
            CStringW href = ImageHrefFromNode(images[i]);
            const ManifestItem* imageItem = FindManifestImageByHref(ctx, href);
            if (imageItem)
                return imageItem;
        }

        return nullptr;
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

    CStringW ResolveFb2HrefFragment(const ConvertContext& ctx, const CStringW& rawFragment);
    CStringW ResolveFb2Href(const ConvertContext& ctx, const CStringW& rawHref);

    CStringW InlineToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx);

    CStringW ChildrenInlineToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        CStringW out;
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return out;

        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
            out += InlineToFb2(MsxmlGetNodeListItem(children, i), ctx);
        return out;
    }

    CStringW InlineToFb2(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx)
    {
        if (!node)
            return CStringW();

        MSXML2::DOMNodeType type = MsxmlGetNodeType(node);
        if (type == MSXML2::NODE_TEXT || type == MSXML2::NODE_CDATA_SECTION)
        {
            return XmlEscape(NormalizeImportedText(MsxmlGetNodeText(node)));
        }

        if (type != MSXML2::NODE_ELEMENT)
            return CStringW();

        if (IsHiddenElement(node))
            return CStringW();

        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"em") == 0 || name.CompareNoCase(L"i") == 0)
            return L"<emphasis>" + ChildrenInlineToFb2(node, ctx) + L"</emphasis>";
        if (name.CompareNoCase(L"strong") == 0 || name.CompareNoCase(L"b") == 0)
            return L"<strong>" + ChildrenInlineToFb2(node, ctx) + L"</strong>";
        if (name.CompareNoCase(L"sup") == 0)
            return L"<sup>" + ChildrenInlineToFb2(node, ctx) + L"</sup>";
        if (name.CompareNoCase(L"sub") == 0)
            return L"<sub>" + ChildrenInlineToFb2(node, ctx) + L"</sub>";
        if (name.CompareNoCase(L"code") == 0 || name.CompareNoCase(L"kbd") == 0 || name.CompareNoCase(L"samp") == 0 || name.CompareNoCase(L"var") == 0)
            return L"<code>" + ChildrenInlineToFb2(node, ctx) + L"</code>";
        if (name.CompareNoCase(L"del") == 0 || name.CompareNoCase(L"s") == 0 || name.CompareNoCase(L"strike") == 0)
            return L"<strikethrough>" + ChildrenInlineToFb2(node, ctx) + L"</strikethrough>";
        if (name.CompareNoCase(L"q") == 0)
            return L"«" + ChildrenInlineToFb2(node, ctx) + L"»";
        if (name.CompareNoCase(L"abbr") == 0 || name.CompareNoCase(L"acronym") == 0)
        {
            CStringW text = ChildrenInlineToFb2(node, ctx);
            CStringW title = NormalizeImportedText(Attr(node, L"title"));
            title.Trim();
            if (!title.IsEmpty())
                return text + L" (" + XmlEscape(title) + L")";
            return text;
        }
        if (name.CompareNoCase(L"rt") == 0)
        {
            CStringW text = ChildrenInlineToFb2(node, ctx);
            text.Trim();
            return text.IsEmpty() ? CStringW() : L" (" + text + L")";
        }
        if (name.CompareNoCase(L"rp") == 0)
            return CStringW();
        if (name.CompareNoCase(L"br") == 0)
            return L" ";
        if (name.CompareNoCase(L"a") == 0)
        {
            if (IsBacklinkElement(node))
                return CStringW();

            CStringW href = Attr(node, L"href");
            CStringW text = ChildrenInlineToFb2(node, ctx);
            text.Trim();
            if (text.IsEmpty())
                text = XmlEscape(NormalizeImportedText(Attr(node, L"title")));
            if (text.IsEmpty())
                text = XmlEscape(NormalizeImportedText(Attr(node, L"aria-label")));
            CStringW noteId = NormalizeFb2Id(HrefFragment(href));
            bool referencesKnownNote = false;
            if (ctx.noteIds && !noteId.IsEmpty())
                referencesKnownNote = ctx.noteIds->find(std::wstring(noteId.GetString())) != ctx.noteIds->end();

            // Make a real FB2 note link only when the EPUB link is explicitly marked
            // as a noteref and the corresponding note section was actually collected.
            // Ordinary anchors such as #ln20 or #sec-intro must remain normal links.
            if (g_options.importNotes && IsNoteReferenceElement(node) && referencesKnownNote)
            {
                if (!noteId.IsEmpty())
                {
                    if (text.IsEmpty())
                        text = L"*";
                    return L"<a l:href=\"#" + XmlEscape(noteId) + L"\" type=\"note\">" + text + L"</a>";
                }
            }

            if (!href.IsEmpty() && g_options.preserveLinks)
            {
                CStringW normalizedHref = ResolveFb2Href(ctx, href);
                return L"<a l:href=\"" + XmlEscape(normalizedHref) + L"\">" + text + L"</a>";
            }
            return text;
        }
        if (name.CompareNoCase(L"img") == 0 || name.CompareNoCase(L"image") == 0)
        {
            CStringW src = ImageHrefFromNode(node);
            CStringW image = Fb2ImageElementForHref(ctx, src);
            if (!image.IsEmpty())
                return image;

            CStringW alt = NormalizeImportedText(Attr(node, L"alt"));
            if (alt.IsEmpty())
                alt = src;
            if (!alt.IsEmpty())
                return L"[Изображение: " + XmlEscape(alt) + L"]";
            return L"[Изображение]";
        }

        return ChildrenInlineToFb2(node, ctx);
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
        CStringW idKey = NormalizedAnchorMapKey(rawId);
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

        CStringW id = NormalizeFb2Id(raw);
        id.Trim();
        if (id.IsEmpty())
            return CStringW();

        if (!ctx.usedFb2Ids)
        {
            AddAnchorIdMap(ctx, rawId, id);
            return id;
        }

        if (ctx.usedFb2Ids->insert(std::wstring(id.GetString())).second)
        {
            AddAnchorIdMap(ctx, rawId, id);
            return id;
        }

        for (int suffix = 2; suffix < 10000; ++suffix)
        {
            CStringW unique;
            unique.Format(L"%s_%d", id.GetString(), suffix);
            if (ctx.usedFb2Ids->insert(std::wstring(unique.GetString())).second)
            {
                CStringW pathKey = PathAnchorMapKey(ctx.currentXhtmlPath, rawId);
                if (ctx.anchorIdMap && !pathKey.IsEmpty() && ctx.anchorIdMap->find(std::wstring(pathKey.GetString())) == ctx.anchorIdMap->end())
                    (*ctx.anchorIdMap)[std::wstring(pathKey.GetString())] = std::wstring(unique.GetString());
                return unique;
            }
        }

        return CStringW();
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

            CStringW rawKey = NormalizedAnchorMapKey(rawFragment);
            std::map<std::wstring, std::wstring>::const_iterator it = ctx.anchorIdMap->find(std::wstring(rawKey.GetString()));
            if (it != ctx.anchorIdMap->end())
                return CStringW(it->second.c_str());
        }

        return id;
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
                        return L"#" + CStringW(it->second.c_str());
                }
                return L"#" + ResolveFb2HrefFragment(ctx, fragment);
            }

            if (!pathPart.IsEmpty() && ctx.anchorIdMap)
            {
                CStringW abs = CombineAndCanonicalizePath(ctx.currentXhtmlDir, pathPart);
                CStringW key = LowerMapKey(abs);
                std::map<std::wstring, std::wstring>::const_iterator it = ctx.anchorIdMap->find(std::wstring(key.GetString()));
                if (it != ctx.anchorIdMap->end())
                    return L"#" + CStringW(it->second.c_str());
            }
        }
        else if (IsProbablyXhtmlHref(pathPart) && ctx.anchorIdMap)
        {
            CStringW abs = CombineAndCanonicalizePath(ctx.currentXhtmlDir, pathPart);
            CStringW key = LowerMapKey(abs);
            std::map<std::wstring, std::wstring>::const_iterator it = ctx.anchorIdMap->find(std::wstring(key.GetString()));
            if (it != ctx.anchorIdMap->end())
                return L"#" + CStringW(it->second.c_str());
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
        CStringW key = NormalizedAnchorMapKey(raw);
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

    CStringW OptionalIdAttributeAndExtraMarkersIncludingDescendant(const MSXML2::IXMLDOMNodePtr& node, const ConvertContext& ctx, CStringW& extraMarkers)
    {
        extraMarkers.Empty();
        std::vector<CStringW> ids = CollectRawAnchorIds(node);
        if (ids.empty())
            return CStringW();

        // Use explicit empty marker paragraphs for all source anchors. This is
        // more robust than attaching only the first EPUB id to the converted FB2
        // element, because real EPUBs often encode several aliases as
        // <a id="..."/> before one heading or paragraph.
        extraMarkers = AnchorMarkerParagraphsFromRawIds(ids, 0, ctx);
        return CStringW();
    }

    void AppendParagraphNode(CStringW& out, const MSXML2::IXMLDOMNodePtr& sourceNode, const CStringW& content, const ConvertContext& ctx)
    {
        CStringW tmp(content);
        tmp.Trim();
        if (!tmp.IsEmpty())
        {
            CStringW extraMarkers;
            CStringW idAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(sourceNode, ctx, extraMarkers);
            out += extraMarkers;
            out += L"      <p" + idAttr + L">" + tmp + L"</p>\r\n";
        }
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
        return IsTokenInList(type, L"quote") || IsTokenInList(type, L"citation") ||
            SemanticClassContains(cls, L"quote") || SemanticClassContains(cls, L"citation") || SemanticClassContains(cls, L"cite");
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
            out += poem;
    }

    void AppendEpigraphOrCiteNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx, bool epigraph)
    {
        if (!g_options.importPoemsEpigraphs)
            return;

        std::vector<MSXML2::IXMLDOMNodePtr> paragraphs;
        CollectElementsByLocalName(node, L"p", paragraphs);

        CStringW xml;
        xml += epigraph ? L"      <epigraph>\r\n" : L"      <cite>\r\n";
        int count = 0;
        if (paragraphs.empty())
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
            for (size_t i = 0; i < paragraphs.size(); ++i)
            {
                CStringW text = ChildrenInlineToFb2(paragraphs[i], ctx);
                text.Trim();
                if (!text.IsEmpty())
                {
                    if (LooksLikeTextAuthor(paragraphs[i]))
                        xml += L"        <text-author>" + text + L"</text-author>\r\n";
                    else
                        xml += L"        <p>" + text + L"</p>\r\n";
                    ++count;
                }
            }
        }
        xml += epigraph ? L"      </epigraph>\r\n" : L"      </cite>\r\n";

        if (count > 0)
            out += xml;
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
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(cell);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (!child)
                continue;

            CStringW part;
            if (MsxmlGetNodeType(child) == MSXML2::NODE_ELEMENT)
            {
                CStringW name = LocalName(child);
                if (name.CompareNoCase(L"p") == 0 || name.CompareNoCase(L"div") == 0 || name.CompareNoCase(L"section") == 0)
                    part = ChildrenInlineToFb2(child, ctx);
                else if (name.CompareNoCase(L"br") == 0)
                    part = L"; ";
                else if (name.CompareNoCase(L"ul") == 0 || name.CompareNoCase(L"ol") == 0)
                {
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

            part.Trim();
            if (part.IsEmpty())
                continue;

            if (!out.IsEmpty() && out.Right(2) != L"; ")
                out += L"; ";
            out += part;
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

    void AppendListNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!g_options.importLists)
            return;

        CStringW name = LocalName(node);
        const bool ordered = name.CompareNoCase(L"ol") == 0;
        int number = ordered ? ParsePositiveInt(Attr(node, L"start"), 1) : 1;
        CStringW listType = Attr(node, L"type");
        CStringW listStyle = Attr(node, L"style");

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (!child || MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT || LocalName(child).CompareNoCase(L"li") != 0)
                continue;

            int itemNumber = number;
            if (ordered)
                itemNumber = ParsePositiveInt(Attr(child, L"value"), number);

            CStringW text = ChildrenInlineToFb2(child, ctx);
            text.Trim();
            if (text.IsEmpty())
                continue;

            CStringW prefix;
            if (ordered)
            {
                prefix = OrderedListPrefix(itemNumber, listType, listStyle);
                number = itemNumber + 1;
            }
            else
            {
                prefix = L"• ";
            }

            out += L"      <p" + OptionalIdAttribute(child, ctx) + L">" + prefix + text + L"</p>\r\n";
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
        table += L"      <table>\r\n";
        int rowCount = 0;
        for (size_t r = 0; r < rows.size(); ++r)
        {
            CStringW row;
            row += L"        <tr>\r\n";
            MSXML2::IXMLDOMNodeListPtr cells = MsxmlGetChildNodes(rows[r]);
            int cellCount = 0;
            long n = cells ? MsxmlGetNodeListLength(cells) : 0;
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
                row += L"          <" + cellName + TableCellAttributes(cell) + L">" + text + L"</" + cellName + L">\r\n";
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
            out += table;
    }


    CStringW PreTextNormalize(const CStringW& text)
    {
        CStringW s = g_options.repairEncoding ? TryRepairUtf8DecodedAsCp1251(text) : text;

        CStringW out;
        out.Preallocate(s.GetLength());
        for (int i = 0; i < s.GetLength(); ++i)
        {
            wchar_t ch = s[i];
            if (ch == 0x00AD || ch == 0x200B || ch == 0x200C || ch == 0x200D || ch == 0xFEFF)
                continue;
            if (ch == 0x00A0 || ch == 0x2007 || ch == 0x202F)
                ch = L' ';
            out += ch;
        }
        out.Replace(L"\r\n", L"\n");
        out.Replace(L"\r", L"\n");
        return out;
    }

    void AppendPreNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!node)
            return;

        CStringW text = PreTextNormalize(MsxmlGetNodeText(node));
        text.Trim(L"\n\r");
        if (text.IsEmpty())
            return;

        int start = 0;
        CStringW line = text.Tokenize(L"\n", start);
        int lineCount = 0;
        CStringW firstLineId = OptionalIdAttribute(node, ctx);
        while (start != -1 || !line.IsEmpty())
        {
            CStringW cleanLine(line);
            cleanLine.TrimRight();
            CStringW nonEmptyCheck(cleanLine);
            nonEmptyCheck.Trim();
            if (!nonEmptyCheck.IsEmpty())
            {
                CStringW idAttr = lineCount == 0 ? firstLineId : CStringW();
                out += L"      <p" + idAttr + L"><code>" + EscapePreLinePreservingIndent(cleanLine) + L"</code></p>\r\n";
                ++lineCount;
            }
            if (start == -1)
                break;
            line = text.Tokenize(L"\n", start);
        }

        if (lineCount == 0)
            out += L"      <p" + firstLineId + L"><code>" + EscapePreLinePreservingIndent(text) + L"</code></p>\r\n";
    }

    void AppendDefinitionListNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!g_options.importLists || !node)
            return;

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
        {
            MSXML2::IXMLDOMNodePtr child = MsxmlGetNodeListItem(children, i);
            if (!child || MsxmlGetNodeType(child) != MSXML2::NODE_ELEMENT)
                continue;

            CStringW name = LocalName(child);
            CStringW text = ChildrenInlineToFb2(child, ctx);
            text.Trim();
            if (text.IsEmpty())
                continue;

            CStringW extraMarkers;
            CStringW idAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(child, ctx, extraMarkers);
            out += extraMarkers;
            if (name.CompareNoCase(L"dt") == 0)
                out += L"      <p" + idAttr + L"><strong>" + text + L"</strong></p>\r\n";
            else if (name.CompareNoCase(L"dd") == 0)
                out += L"      <p" + idAttr + L">— " + text + L"</p>\r\n";
        }
    }

    void AppendFigureNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx)
    {
        if (!node)
            return;

        std::vector<MSXML2::IXMLDOMNodePtr> images;
        CollectElementsByLocalName(node, L"img", images);
        CollectElementsByLocalName(node, L"image", images);

        CStringW figureXml;
        for (size_t i = 0; i < images.size(); ++i)
        {
            CStringW href = ImageHrefFromNode(images[i]);
            CStringW image = Fb2ImageElementForHref(ctx, href);
            if (!image.IsEmpty())
                figureXml += L"      " + image + L"\r\n";
        }

        CStringW caption;
        bool captionIsXml = false;
        std::vector<MSXML2::IXMLDOMNodePtr> captions;
        CollectElementsByLocalName(node, L"figcaption", captions);
        if (!captions.empty())
        {
            caption = ChildrenInlineToFb2(captions[0], ctx);
            captionIsXml = true;
        }

        if (caption.IsEmpty() && !images.empty())
        {
            caption = NormalizeImportedText(Attr(images[0], L"alt"));
            if (caption.IsEmpty())
                caption = NormalizeImportedText(Attr(images[0], L"title"));
            captionIsXml = false;
        }

        caption.Trim();
        if (!caption.IsEmpty())
            figureXml += L"      <subtitle>" + (captionIsXml ? caption : XmlEscape(caption)) + L"</subtitle>\r\n";

        if (!figureXml.IsEmpty())
        {
            out += figureXml;
            return;
        }

        CStringW text = ChildrenInlineToFb2(node, ctx);
        text.Trim();
        if (!text.IsEmpty())
        {
            CStringW extraMarkers;
            CStringW idAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(node, ctx, extraMarkers);
            out += extraMarkers;
            out += L"      <p" + idAttr + L">" + text + L"</p>\r\n";
        }
    }

    void AppendBlockNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx);

    void AppendSpecialSectionNode(const MSXML2::IXMLDOMNodePtr& node, CStringW& out, const ConvertContext& ctx, LPCWSTR title)
    {
        CStringW content;
        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        long count = children ? MsxmlGetNodeListLength(children) : 0;
        for (long i = 0; i < count; ++i)
            AppendBlockNode(MsxmlGetNodeListItem(children, i), content, ctx);

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
            return;

        CStringW name = LocalName(node);
        if (name.CompareNoCase(L"script") == 0 || name.CompareNoCase(L"style") == 0 || name.CompareNoCase(L"head") == 0)
            return;

        if (name.CompareNoCase(L"figure") == 0 || name.CompareNoCase(L"picture") == 0)
        {
            AppendFigureNode(node, out, ctx);
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
        if (IsBacklinkElement(node) || (g_options.importNotes && IsEndnoteElement(node)))
            return;

        if (LooksLikeDedication(node))
        {
            AppendSpecialSectionNode(node, out, ctx, L"Посвящение");
            return;
        }

        if (LooksLikeSidebar(node) && !IsEndnoteElement(node))
        {
            AppendEpigraphOrCiteNode(node, out, ctx, false);
            return;
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
            if (!author.IsEmpty())
                out += L"      <text-author>" + author + L"</text-author>\r\n";
            return;
        }

        if (g_options.importSubtitles && LooksLikeSubtitle(node))
        {
            CStringW subtitle = ChildrenInlineToFb2(node, ctx);
            subtitle.Trim();
            if (!subtitle.IsEmpty())
                out += L"      <subtitle>" + subtitle + L"</subtitle>\r\n";
            return;
        }

        if (LooksLikePageBreak(node) || name.CompareNoCase(L"hr") == 0)
        {
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

        if (name.CompareNoCase(L"p") == 0)
        {
            CStringW paragraph = ChildrenInlineToFb2(node, ctx);
            paragraph.Trim();
            if (!paragraph.IsEmpty() && paragraph.Left(7).CompareNoCase(L"<image ") == 0 && paragraph.Right(2) == L"/>")
                out += L"      " + paragraph + L"\r\n";
            else
                AppendParagraphNode(out, node, paragraph, ctx);
            return;
        }

        if (IsHeadingName(name))
        {
            CStringW title = ChildrenInlineToFb2(node, ctx);
            title.Trim();
            if (!title.IsEmpty())
                out += L"      <subtitle>" + title + L"</subtitle>\r\n";
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

    void CollectEndnoteElements(const MSXML2::IXMLDOMNodePtr& node, std::vector<MSXML2::IXMLDOMNodePtr>& out, bool insideNotesContainer = false)
    {
        if (!node)
            return;

        const CStringW name = LocalName(node);
        const bool nowInsideNotesContainer = insideNotesContainer || IsEndnotesContainerElement(node);
        const bool isListNoteInNotesContainer = nowInsideNotesContainer &&
            name.CompareNoCase(L"li") == 0 && !Attr(node, L"id").IsEmpty();

        if (IsEndnoteElement(node) || isListNoteInNotesContainer)
        {
            CStringW id = Attr(node, L"id");
            if (!id.IsEmpty())
                out.push_back(node);
            return; // Do not descend into an already selected note.
        }

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(node);
        if (!children)
            return;

        long count = MsxmlGetNodeListLength(children);
        for (long i = 0; i < count; ++i)
            CollectEndnoteElements(MsxmlGetNodeListItem(children, i), out, nowInsideNotesContainer);
    }

    CStringW BuildNoteContent(const MSXML2::IXMLDOMNodePtr& noteNode, const ConvertContext& ctx)
    {
        CStringW content;
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
            if (IsBacklinkElement(child))
                continue;
            AppendBlockNode(child, content, ctx);
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
            warning += L"Не удалось проверить XHTML на сноски: " + xhtmlPath + L"\r\n";
            if (!error.IsEmpty())
                warning += error + L"\r\n";
            return false;
        }

        MSXML2::IXMLDOMNodePtr body = FindFirstElementByLocalName(MsxmlGetDocumentElement(doc), L"body");
        if (!body)
            return false;

        isNotesDocument = IsEndnotesBody(body);

        std::vector<MSXML2::IXMLDOMNodePtr> noteNodes;
        CollectEndnoteElements(body, noteNodes);
        if (!noteNodes.empty())
            isNotesDocument = true;

        for (size_t i = 0; i < noteNodes.size(); ++i)
        {
            CStringW rawId = Attr(noteNodes[i], L"id");
            CStringW id = NormalizeFb2Id(rawId);
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
                note.title = rawId;

            note.content = BuildNoteContent(noteNodes[i], ctx);
            note.content.Trim();
            if (note.content.IsEmpty())
                note.content = L"      <p>[Пустая сноска]</p>\r\n";

            notes.push_back(note);
        }

        return !noteNodes.empty();
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

        if (g_options.removeServiceSections &&
            package.guideServiceDocuments.find(std::wstring(item.absPath.GetString())) != package.guideServiceDocuments.end())
            return true;

        if (id == L"nav" || id == L"toc" || id == L"ncx")
            return true;
        if (IsTokenInList(props, L"nav") || IsTokenInList(props, L"cover"))
            return true;
        if (item.mediaType.CompareNoCase(L"application/x-dtbncx+xml") == 0)
            return true;
        if (href.Find(L"nav") >= 0 || href.Find(L"toc") >= 0 || href.Find(L"contents") >= 0)
            return true;

        if (g_options.removeServiceSections)
        {
            if (href.Find(L"titlepage") >= 0 || href.Find(L"title-page") >= 0 ||
                href.Find(L"landmarks") >= 0 || href.Find(L"page-list") >= 0 ||
                href.Find(L"copyright") >= 0 || href.Find(L"calibre") >= 0)
                return true;
            if (id.Find(L"titlepage") >= 0 || id.Find(L"landmarks") >= 0 || id.Find(L"pagelist") >= 0)
                return true;
        }

        if (item.id.CompareNoCase(L"cover") == 0)
            return true;
        if (item.properties.Find(L"cover") >= 0)
            return true;
        if (!package.coverId.IsEmpty())
        {
            const ManifestItem* cover = FindManifestItemById(package, package.coverId);
            if (cover && item.href.CompareNoCase(cover->href) == 0)
                return true;
        }
        return href.Find(L"cover") >= 0 && (href.Right(6) == L".xhtml" || href.Right(5) == L".html");
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
        if (content.IsEmpty())
            out += L"      <p>[Текст главы не извлечён]</p>\r\n";
        else
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
            warning += L"SVG-элемент spine пропущен по настройке импорта: " + item.href + L"\r\n";
            return false;
        }

        if (!FileExists(item.absPath))
        {
            warning += L"Файл изображения из spine не найден: " + item.href + L"\r\n";
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
        content += L"      <image l:href=\"#" + XmlEscape(id) + L"\"/>\r\n";

        AppendFb2Section(outSection, XmlEscape(title), content);
        return true;
    }

    bool TryConvertBodyToSplitSections(const MSXML2::IXMLDOMNodePtr& body, const CStringW& fallbackTitle, const ConvertContext& ctx, CStringW& outSection)
    {
        if (!g_options.splitSectionsByHeadings || !body)
            return false;

        MSXML2::IXMLDOMNodeListPtr children = MsxmlGetChildNodes(body);
        long count = children ? MsxmlGetNodeListLength(children) : 0;

        CStringW currentTitle;
        CStringW currentContent;
        CStringW currentIdAttr;
        bool sawHeading = false;
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

                if (!sawHeading)
                {
                    if (!currentContent.IsEmpty())
                    {
                        AppendFb2Section(outSection, XmlEscape(fallbackTitle), currentContent);
                        currentContent.Empty();
                    }
                    sawHeading = true;
                    currentTitle = heading;
                    CStringW headingExtraMarkers;
                    currentIdAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(child, ctx, headingExtraMarkers);
                    if (currentIdAttr.IsEmpty())
                        currentIdAttr = OptionalIdAttributeFromRaw(FileNameOnly(ctx.currentXhtmlPath), ctx);
                    AddDocumentHrefMap(ctx, IdFromAttributeText(currentIdAttr));
                    currentContent += headingExtraMarkers;
                    continue;
                }

                AppendFb2Section(outSection, currentTitle, currentContent, currentIdAttr);
                currentContent.Empty();
                currentTitle = heading;
                CStringW headingExtraMarkers;
                currentIdAttr = OptionalIdAttributeAndExtraMarkersIncludingDescendant(child, ctx, headingExtraMarkers);
                currentContent += headingExtraMarkers;
                continue;
            }

            AppendBlockNode(child, currentContent, ctx);
        }

        if (!sawHeading)
            return false;

        AppendFb2Section(outSection, currentTitle.IsEmpty() ? XmlEscape(fallbackTitle) : currentTitle, currentContent, currentIdAttr);
        return true;
    }

    bool ConvertXhtmlFileToFb2Section(const CStringW& xhtmlPath, const CStringW& fallbackTitle, const ConvertContext& ctx, CStringW& outSection, CStringW& warning)
    {
        MSXML2::IXMLDOMDocument2Ptr doc;
        CStringW error;
        if (!LoadXmlFile(xhtmlPath, doc, error))
        {
            warning += L"Не удалось прочитать XHTML: " + xhtmlPath + L"\r\n";
            if (!error.IsEmpty())
                warning += error + L"\r\n";
            return false;
        }

        MSXML2::IXMLDOMNodePtr body = FindFirstElementByLocalName(MsxmlGetDocumentElement(doc), L"body");
        if (!body)
        {
            warning += L"В XHTML не найден body: " + xhtmlPath + L"\r\n";
            return false;
        }

        if (IsEndnotesBody(body))
        {
            warning += L"Файл со сносками перенесён в body name=notes: " + xhtmlPath + L"\r\n";
            return false;
        }

        if (TryConvertBodyToSplitSections(body, fallbackTitle, ctx, outSection))
            return true;

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
                    OptionalIdAttributeAndExtraMarkersIncludingDescendant(child, ctx, headingExtraMarkers);
                content += headingExtraMarkers;
                titleTaken = !sectionTitle.IsEmpty();
                continue;
            }
            AppendBlockNode(child, content, ctx);
        }

        if (sectionTitle.IsEmpty())
        {
            std::vector<MSXML2::IXMLDOMNodePtr> titles;
            CollectElementsByLocalName(MsxmlGetDocumentElement(doc), L"title", titles);
            if (!titles.empty())
                sectionTitle = XmlEscape(NodeText(titles[0]));
        }

        if (sectionTitle.IsEmpty())
            sectionTitle = XmlEscape(fallbackTitle);

        AppendFb2Section(outSection, sectionTitle, content, sectionIdAttr);
        return true;
    }

    CStringW BuildBinariesXml(const EpubPackage& package, const std::set<std::wstring>& usedImageIds, CStringW& warning)
    {
        CStringW xml;
        std::set<std::wstring> ids = usedImageIds;
        if (g_options.importCover && !package.coverId.IsEmpty())
        {
            const ManifestItem* coverImage = ResolveCoverImageItem(package);
            if (coverImage)
                ids.emplace(coverImage->id.GetString());
        }

        for (std::set<std::wstring>::const_iterator itId = ids.begin(); itId != ids.end(); ++itId)
        {
            std::map<std::wstring, ManifestItem>::const_iterator it = package.manifest.find(*itId);
            if (it == package.manifest.end())
                continue;

            const ManifestItem& item = it->second;
            if (!IsImageMediaType(item.mediaType) && !IsImageHrefByExtension(item.href))
                continue;

            std::vector<unsigned char> data;
            CStringW id = BinaryIdForImageItem(item);
            CStringW contentType;
            if (!ReadSvgImageData(item, data, contentType, warning))
            {
                warning += L"Не удалось подготовить изображение: " + item.href + L"\r\n";
                continue;
            }
            xml += L"  <binary id=\"" + XmlEscape(id) + L"\" content-type=\"" + XmlEscape(contentType) + L"\">\r\n";
            xml += Base64Encode(data);
            xml += L"\r\n  </binary>\r\n";
        }
        return xml;
    }


    CStringW CurrentDateIso()
    {
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        CStringW date;
        date.Format(L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
        return date;
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


    CStringW MapSubjectToFb2Genre(const CStringW& subject)
    {
        CStringW s(subject);
        s.Trim();
        if (s.IsEmpty())
            return CStringW();

        CStringW lower(s);
        lower.MakeLower();

        // If the EPUB already contains an FB2-like genre id, keep it.
        if (s.Find(L" ") < 0 && s.Find(L",") < 0 && s.Find(L"_") >= 0)
            return s;

        if (ContainsNoCase(lower, L"науч") || ContainsNoCase(lower, L"science") || ContainsNoCase(lower, L"астрон") || ContainsNoCase(lower, L"космос"))
            return L"sci_popular";
        if (ContainsNoCase(lower, L"фантаст") || ContainsNoCase(lower, L"fiction"))
            return L"sf";
        if (ContainsNoCase(lower, L"истор") || ContainsNoCase(lower, L"history"))
            return L"prose_history";
        if (ContainsNoCase(lower, L"детектив") || ContainsNoCase(lower, L"detective"))
            return L"det_classic";
        if (ContainsNoCase(lower, L"биограф") || ContainsNoCase(lower, L"biograph"))
            return L"nonf_biography";

        return CStringW();
    }

    CStringW NormalizeFb2Language(const CStringW& language)
    {
        CStringW s = NormalizeImportedText(language);
        s.Trim();
        if (s.IsEmpty())
            return L"ru";

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
        return s.IsEmpty() ? CStringW(L"ru") : s;
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
            xml += L"        <p>" + XmlEscape(package.description) + L"</p>\r\n";
            xml += L"      </annotation>\r\n";
        }
        if (!package.date.IsEmpty())
            xml += L"      <date>" + XmlEscape(package.date) + L"</date>\r\n";
        if (!package.coverId.IsEmpty())
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
        if (!package.seriesName.IsEmpty())
        {
            CStringW numberAttr;
            if (!package.seriesNumber.IsEmpty())
                numberAttr = L" number=\"" + XmlEscape(package.seriesNumber) + L"\"";
            xml += L"      <sequence name=\"" + XmlEscape(package.seriesName) + L"\"" + numberAttr + L"/>\r\n";
        }
        xml += L"    </title-info>\r\n";
        xml += L"    <document-info>\r\n";
        xml += L"      <author><nickname>ImportEPUB</nickname></author>\r\n";
        xml += L"      <program-used>FBE ImportEPUB plugin</program-used>\r\n";
        xml += L"      <date>" + CurrentDateIso() + L"</date>\r\n";
        if (!package.identifier.IsEmpty())
            xml += L"      <id>" + XmlEscape(package.identifier) + L"</id>\r\n";
        else
            xml += L"      <id>" + GuidString() + L"</id>\r\n";
        xml += L"      <version>1.0</version>\r\n";
        xml += L"    </document-info>\r\n";
        if (!package.publisher.IsEmpty() || !package.date.IsEmpty() || !package.isbn.IsEmpty())
        {
            xml += L"    <publish-info>\r\n";
            if (!package.publisher.IsEmpty())
                xml += L"      <publisher>" + XmlEscape(package.publisher) + L"</publisher>\r\n";
            if (!package.date.IsEmpty() && package.date.GetLength() >= 4)
                xml += L"      <year>" + XmlEscape(package.date.Left(4)) + L"</year>\r\n";
            if (!package.isbn.IsEmpty())
                xml += L"      <isbn>" + XmlEscape(package.isbn) + L"</isbn>\r\n";
            xml += L"    </publish-info>\r\n";
        }
        if (!package.source.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB source\">" + XmlEscape(package.source) + L"</custom-info>\r\n";
        if (!package.rights.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB rights\">" + XmlEscape(package.rights) + L"</custom-info>\r\n";
        if (!package.modifiedDate.IsEmpty())
            xml += L"    <custom-info info-type=\"EPUB modified\">" + XmlEscape(package.modifiedDate) + L"</custom-info>\r\n";
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
            warning += L"В EPUB не найден файл mimetype. Некоторые валидаторы считают это ошибкой контейнера.\r\n";
            log.Add(L"Warning: mimetype file is missing.");
        }
        else
        {
            log.AddFormat(L"Mimetype: %s", mimetype);
            if (mimetype.CompareNoCase(L"application/epub+zip") != 0)
            {
                warning += L"Файл mimetype содержит неожиданное значение: " + mimetype + L"\r\n";
                log.Add(L"Warning: unexpected EPUB mimetype value.");
            }
        }

        CStringW encryptionPath = extractDir + L"\\META-INF\\encryption.xml";
        if (FileExists(encryptionPath))
        {
            warning += L"В EPUB найден META-INF/encryption.xml. Возможны зашифрованные или обфусцированные ресурсы; часть изображений/шрифтов может не импортироваться.\r\n";
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
                        warning += L"Изображение ссылается на отсутствующий binary: " + id + L"\r\n";
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
            warning += L"Дублирующийся id в итоговом FB2: " + CStringW(dup->c_str()) + L"\r\n";

        // Check ordinary FB2 internal links. Images are validated against <binary>
        // above, and note links are validated separately below, so this block focuses
        // on anchors such as <a l:href="#section-id"> that should point to an
        // existing element id in the generated FB2.
        int internalHrefPos = 0;
        while ((internalHrefPos = xml.Find(L"l:href=\"#", internalHrefPos)) >= 0)
        {
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
                        warning += L"Внутренняя ссылка указывает на отсутствующий id: " + id + L"\r\n";
                }
            }
            internalHrefPos += 9;
        }

        // External http/https links are allowed and reported only in the CSV counter.

        int notePos = 0;
        while ((notePos = xml.Find(L"type=\"note\"", notePos)) >= 0)
        {
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
                        warning += L"Ссылка на сноску без соответствующей секции notes: " + id + L"\r\n";
                }
            }
            notePos += 11;
        }
    }

    bool ValidateGeneratedFb2Xml(const CStringW& xml, CStringW& warning)
    {
        MSXML2::IXMLDOMDocument2Ptr dom;
        HRESULT hr = CreateMsxmlDom(dom);
        if (FAILED(hr))
        {
            warning += L"Не удалось создать MSXML для проверки итогового FB2.\r\n";
            return false;
        }

        VARIANT_BOOL loaded = VARIANT_FALSE;
        CComBSTR xmlText(static_cast<LPCWSTR>(xml));
        HRESULT loadHr = dom->loadXML(xmlText, &loaded);
        if (FAILED(loadHr) || loaded != VARIANT_TRUE)
        {
            CStringW reason = MsxmlGetParseErrorReason(dom);
            if (reason.IsEmpty())
                reason = L"неизвестная ошибка XML";
            warning += L"Итоговый FB2 не прошёл XML-проверку: " + reason + L"\r\n";
            return false;
        }

        if (!FindFirstElementByLocalName(MsxmlGetDocumentElement(dom), L"body"))
        {
            warning += L"В итоговом FB2 не найден body.\r\n";
            return false;
        }

        ValidateInternalReferences(xml, warning);
        return true;
    }
}

EpubImportRuntimeStats GetLastEpubImportRuntimeStats()
{
    return g_runtimeStats;
}

bool BuildFb2XmlFromEpub(const CStringW& epubPath, const EpubImportOptions& options, CStringW& outFb2Xml, CStringW& errorText)
{
    g_options = options;
    g_runtimeStats = EpubImportRuntimeStats();
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
        errorText = L"Выбранный EPUB-файл не найден.";
        log.Add(errorText);
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }

    TempDirectory temp;
    CStringW extractDir;
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
    if (!ParseOpf(opfPath, package, errorText))
    {
        log.Add(errorText);
        if (options.keepTempOnError)
            temp.Keep();
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }
    log.AddFormat(L"Title: %s", package.title);
    if (!package.seriesName.IsEmpty())
        log.AddFormat(L"Series: %s", package.seriesName);
    log.AddFormatInt(L"Manifest items: %d", static_cast<int>(package.manifest.size()));
    log.AddFormatInt(L"Spine items: %d", static_cast<int>(package.spine.size()));
    log.AddFormatInt(L"OPF guide service documents: %d", static_cast<int>(package.guideServiceDocuments.size()));

    ParsePackageNavigation(package, log);

    CStringW body;
    CStringW warning = containerWarning;
    std::set<std::wstring> usedImageIds;
    std::set<std::wstring> usedFb2Ids;
    std::map<std::wstring, std::wstring> anchorIdMap;
    std::vector<EpubNote> notes;
    std::set<std::wstring> noteIds;
    std::set<std::wstring> noteDocumentIds;
    int importedSections = 0;

    if (options.importNotes)
    {
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

    for (size_t i = 0; i < package.spine.size(); ++i)
    {
        std::map<std::wstring, ManifestItem>::const_iterator it = package.manifest.find(std::wstring(package.spine[i].GetString()));
        if (it == package.manifest.end())
            continue;

        const ManifestItem& item = it->second;

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
            if (ConvertImageSpineItemToFb2Section(item, title, ctx, body, warning))
            {
                ++importedSections;
                log.AddFormat(L"Imported image spine item: %s", item.href);
            }
            continue;
        }

        if (item.mediaType.CompareNoCase(L"application/xhtml+xml") != 0 &&
            item.mediaType.CompareNoCase(L"text/html") != 0 &&
            item.href.Right(5).CompareNoCase(L".html") != 0 &&
            item.href.Right(6).CompareNoCase(L".xhtml") != 0)
        {
            log.AddFormat(L"Skipped unsupported spine item: %s", item.href);
            continue;
        }

        CStringW xhtmlPath = CombineAndCanonicalizePath(package.opfDir, item.href);
        if (!FileExists(xhtmlPath))
        {
            warning += L"Файл из spine не найден: " + item.href + L"\r\n";
            log.AddFormat(L"Missing spine file: %s", item.href);
            continue;
        }

        ctx.currentXhtmlDir = DirectoryOf(xhtmlPath);
        ctx.currentXhtmlPath = xhtmlPath;

        if (ConvertXhtmlFileToFb2Section(xhtmlPath, title, ctx, body, warning))
        {
            ++importedSections;
            log.AddFormat(L"Imported spine item: %s", item.href);
        }
    }

    log.AddFormatInt(L"Imported sections: %d", importedSections);
    log.AddFormatInt(L"Images used: %d", static_cast<int>(usedImageIds.size()));

    if (importedSections == 0)
    {
        errorText = L"Не удалось импортировать ни одного поддерживаемого файла из spine.";
        if (!warning.IsEmpty())
            errorText += L"\r\n" + warning;
        log.Add(errorText);
        if (options.keepTempOnError)
            temp.Keep();
        log.SaveIfRequested(epubPath, options.writeLogOnWarnings);
        return false;
    }

    CStringW binariesXml = BuildBinariesXml(package, usedImageIds, warning);
    if (!warning.IsEmpty())
        log.Add(warning);

    outFb2Xml += L"<?xml version=\"1.0\"?>\r\n";
    outFb2Xml += L"<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" ";
    outFb2Xml += L"xmlns:l=\"http://www.w3.org/1999/xlink\">\r\n";
    outFb2Xml += BuildDescriptionXml(package);
    outFb2Xml += L"  <body>\r\n";
    if (options.addDiagnosticSection && !warning.IsEmpty())
    {
        outFb2Xml += L"    <section>\r\n";
        outFb2Xml += L"      <title><p>Предупреждения импорта EPUB</p></title>\r\n";
        outFb2Xml += L"      <p>" + XmlEscape(warning) + L"</p>\r\n";
        outFb2Xml += L"    </section>\r\n";
    }
    outFb2Xml += body;
    outFb2Xml += L"  </body>\r\n";
    if (options.importNotes)
        outFb2Xml += BuildNotesBodyXml(notes);
    outFb2Xml += binariesXml;
    outFb2Xml += L"</FictionBook>\r\n";

    if (options.validateResult)
    {
        CStringW validationWarning;
        ValidateGeneratedFb2Xml(outFb2Xml, validationWarning);
        if (!validationWarning.IsEmpty())
        {
            warning += validationWarning;
            log.Add(validationWarning);
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

    log.Add(L"Import completed.");
    log.SaveIfRequested(epubPath, options.writeLogOnWarnings && !warning.IsEmpty());
    return true;
}
