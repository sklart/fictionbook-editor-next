#include "Fb2Metadata.h"

#include <windows.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <atlstr.h>
#include <comdef.h>
#include <xmllite.h>

#pragma comment(lib, "xmllite.lib")

#import <msxml6.dll> named_guids rename_namespace("MSXML2") exclude("ISequentialStream", "_FILETIME")

namespace {

const wchar_t kSelectionNamespaces[] =
    L"xmlns:fb='http://www.gribuser.ru/xml/fictionbook/2.0'";

void NormalizeWhitespace(ATL::CString& value)
{
    const int length = value.GetLength();
    const wchar_t* source = value;
    ATL::CString normalized;
    normalized.Preallocate(length);

    bool inWhitespace = true;
    for (int i = 0; i < length; ++i) {
        const wchar_t ch = source[i];
        if (ch <= L' ') {
            if (!inWhitespace) {
                normalized += L' ';
                inWhitespace = true;
            }
            continue;
        }

        normalized += ch;
        inWhitespace = false;
    }

    if (!normalized.IsEmpty() && normalized[normalized.GetLength() - 1] == L' ')
        normalized.Truncate(normalized.GetLength() - 1);

    value = normalized;
}

ATL::CString GetNodeText(MSXML2::IXMLDOMNodePtr node)
{
    if (node == nullptr)
        return ATL::CString();

    _bstr_t text = node->text;
    ATL::CString result(static_cast<const wchar_t*>(text));
    NormalizeWhitespace(result);
    return result;
}

ATL::CString SelectText(MSXML2::IXMLDOMNodePtr contextNode, const wchar_t* xpath)
{
    if (contextNode == nullptr)
        return ATL::CString();

    return GetNodeText(contextNode->selectSingleNode(_bstr_t(xpath)));
}

ATL::CString JoinNonEmpty(const ATL::CString& first, const ATL::CString& second, const ATL::CString& third)
{
    ATL::CString result;
    const ATL::CString parts[] = { first, second, third };
    for (const ATL::CString& part : parts) {
        if (part.IsEmpty())
            continue;
        if (!result.IsEmpty())
            result += L' ';
        result += part;
    }
    return result;
}

ATL::CString ReadAuthor(MSXML2::IXMLDOMNodePtr authorNode)
{
    ATL::CString fullName = JoinNonEmpty(
        SelectText(authorNode, L"fb:first-name"),
        SelectText(authorNode, L"fb:middle-name"),
        SelectText(authorNode, L"fb:last-name"));

    if (!fullName.IsEmpty())
        return fullName;

    return SelectText(authorNode, L"fb:nickname");
}

ATL::CString ReadAuthors(MSXML2::IXMLDOMNodePtr contextNode)
{
    ATL::CString authors;
    MSXML2::IXMLDOMNodeListPtr authorNodes = contextNode->selectNodes(_bstr_t(L"fb:author"));
    if (authorNodes == nullptr)
        return authors;

    const long count = authorNodes->length;
    for (long index = 0; index < count; ++index) {
        ATL::CString author = ReadAuthor(authorNodes->item[index]);
        if (author.IsEmpty())
            continue;
        if (!authors.IsEmpty())
            authors += L", ";
        authors += author;
    }
    return authors;
}

std::vector<ATL::CString> ReadAuthorValues(MSXML2::IXMLDOMNodePtr contextNode)
{
    std::vector<ATL::CString> result;
    if (contextNode == nullptr)
        return result;
    MSXML2::IXMLDOMNodeListPtr authorNodes = contextNode->selectNodes(_bstr_t(L"fb:author"));
    if (authorNodes == nullptr)
        return result;
    for (long index = 0; index < authorNodes->length; ++index) {
        ATL::CString author = ReadAuthor(authorNodes->item[index]);
        if (!author.IsEmpty())
            result.push_back(author);
    }
    return result;
}

ATL::CString ReadGenres(MSXML2::IXMLDOMNodePtr titleInfoNode)
{
    ATL::CString genres;
    MSXML2::IXMLDOMNodeListPtr genreNodes = titleInfoNode->selectNodes(_bstr_t(L"fb:genre"));
    if (genreNodes == nullptr)
        return genres;

    const long count = genreNodes->length;
    for (long index = 0; index < count; ++index) {
        ATL::CString genre = GetNodeText(genreNodes->item[index]);
        if (genre.IsEmpty())
            continue;
        if (!genres.IsEmpty())
            genres += L", ";
        genres += genre;
    }
    return genres;
}

ATL::CString ReadSequence(MSXML2::IXMLDOMNodePtr titleInfoNode)
{
    ATL::CString sequence;
    MSXML2::IXMLDOMNodeListPtr sequenceNodes = titleInfoNode->selectNodes(_bstr_t(L"fb:sequence"));
    if (sequenceNodes == nullptr)
        return sequence;

    const long count = sequenceNodes->length;
    for (long index = 0; index < count; ++index) {
        MSXML2::IXMLDOMNodePtr node = sequenceNodes->item[index];
        if (node == nullptr)
            continue;

        MSXML2::IXMLDOMNamedNodeMapPtr attributes = node->attributes;
        ATL::CString name;
        ATL::CString number;
        if (attributes != nullptr) {
            MSXML2::IXMLDOMNodePtr nameNode = attributes->getNamedItem(_bstr_t(L"name"));
            MSXML2::IXMLDOMNodePtr numberNode = attributes->getNamedItem(_bstr_t(L"number"));
            name = GetNodeText(nameNode);
            number = GetNodeText(numberNode);
        }

        if (name.IsEmpty())
            continue;

        if (!sequence.IsEmpty())
            sequence += L"; ";
        sequence += name;
        if (!number.IsEmpty()) {
            sequence += L" [";
            sequence += number;
            sequence += L']';
        }
    }

    return sequence;
}

ATL::CString FormatComError(const _com_error& error)
{
    ATL::CString message;
    if (error.ErrorMessage() != nullptr)
        message = error.ErrorMessage();
    if (message.IsEmpty())
        message.Format(L"COM error 0x%08X", static_cast<unsigned int>(error.Error()));
    return message;
}

} // namespace

namespace FB2Metadata {

void Metadata::Clear()
{
    title.Empty();
    authors.Empty();
    authorValues.clear();
    genres.Empty();
    keywords.Empty();
    language.Empty();
    sourceLanguage.Empty();
    sequence.Empty();
    documentAuthors.Empty();
    documentAuthorValues.clear();
    documentDate.Empty();
    documentDateValue.Empty();
    documentId.Empty();
    documentVersion.Empty();
}

bool TryRead(const wchar_t* filePath, Metadata& metadata, ATL::CString* errorMessage)
{
    metadata.Clear();

    if (errorMessage != nullptr)
        errorMessage->Empty();

    if (filePath == nullptr || *filePath == L'\0') {
        if (errorMessage != nullptr)
            *errorMessage = L"FB2 file path is not specified.";
        return false;
    }

    try {
        MSXML2::IXMLDOMDocument2Ptr document;
        HRESULT hr = document.CreateInstance(__uuidof(MSXML2::DOMDocument60));
        if (FAILED(hr)) {
            if (errorMessage != nullptr)
                errorMessage->Format(L"Failed to create DOMDocument60: 0x%08X", static_cast<unsigned int>(hr));
            return false;
        }

        document->async = VARIANT_FALSE;
        document->validateOnParse = VARIANT_FALSE;
        document->resolveExternals = VARIANT_FALSE;
        document->setProperty(_bstr_t(L"SelectionLanguage"), _variant_t(L"XPath"));
        document->setProperty(_bstr_t(L"SelectionNamespaces"), _variant_t(kSelectionNamespaces));

        const VARIANT_BOOL loaded = document->load(_variant_t(filePath));
        if (loaded != VARIANT_TRUE) {
            if (errorMessage != nullptr) {
                MSXML2::IXMLDOMParseErrorPtr parseError = document->parseError;
                if (parseError != nullptr) {
                    const long line = parseError->line;
                    const long linePos = parseError->linepos;
                    const _bstr_t reason = parseError->reason;
                    errorMessage->Format(
                        L"FB2 parse error (line %ld, position %ld): %s",
                        line,
                        linePos,
                        static_cast<const wchar_t*>(reason));
                } else {
                    *errorMessage = L"MSXML could not load the FB2 file.";
                }
            }
            return false;
        }

        MSXML2::IXMLDOMNodePtr titleInfo = document->selectSingleNode(
            _bstr_t(L"/fb:FictionBook/fb:description/fb:title-info"));
        MSXML2::IXMLDOMNodePtr documentInfo = document->selectSingleNode(
            _bstr_t(L"/fb:FictionBook/fb:description/fb:document-info"));

        if (titleInfo == nullptr && documentInfo == nullptr) {
            if (errorMessage != nullptr)
                *errorMessage = L"Neither title-info nor document-info was found in FB2.";
            return false;
        }

        metadata.title = SelectText(titleInfo, L"fb:book-title");
        metadata.authors = ReadAuthors(titleInfo);
        metadata.authorValues = ReadAuthorValues(titleInfo);
        metadata.genres = ReadGenres(titleInfo);
        metadata.keywords = SelectText(titleInfo, L"fb:keywords");
        metadata.language = SelectText(titleInfo, L"fb:lang");
        metadata.sourceLanguage = SelectText(titleInfo, L"fb:src-lang");
        metadata.sequence = ReadSequence(titleInfo);

        metadata.documentAuthors = ReadAuthors(documentInfo);
        metadata.documentAuthorValues = ReadAuthorValues(documentInfo);
        metadata.documentDate = SelectText(documentInfo, L"fb:date");
        metadata.documentId = SelectText(documentInfo, L"fb:id");
        metadata.documentVersion = SelectText(documentInfo, L"fb:version");

        if (documentInfo != nullptr) {
            MSXML2::IXMLDOMNodePtr dateNode = documentInfo->selectSingleNode(_bstr_t(L"fb:date"));
            if (dateNode != nullptr && dateNode->attributes != nullptr) {
                MSXML2::IXMLDOMNodePtr valueNode = dateNode->attributes->getNamedItem(_bstr_t(L"value"));
                metadata.documentDateValue = GetNodeText(valueNode);
            }
        }

        return true;
    }
    catch (const _com_error& error) {
        if (errorMessage != nullptr)
            *errorMessage = FormatComError(error);
        return false;
    }
}

bool TryReadStreamDomFallback(IStream* stream, Metadata& metadata, ATL::CString* errorMessage)
{
    metadata.Clear();
    if (errorMessage != nullptr)
        errorMessage->Empty();
    if (stream == nullptr) {
        if (errorMessage != nullptr)
            *errorMessage = L"FB2 stream is not specified.";
        return false;
    }

    try {
        LARGE_INTEGER origin = {};
        HRESULT hr = stream->Seek(origin, STREAM_SEEK_SET, nullptr);
        if (FAILED(hr)) {
            if (errorMessage != nullptr)
                errorMessage->Format(L"Failed to seek FB2 stream: 0x%08X", static_cast<unsigned int>(hr));
            return false;
        }

        MSXML2::IXMLDOMDocument2Ptr document;
        hr = document.CreateInstance(__uuidof(MSXML2::DOMDocument60));
        if (FAILED(hr))
            return false;
        document->async = VARIANT_FALSE;
        document->validateOnParse = VARIANT_FALSE;
        document->resolveExternals = VARIANT_FALSE;
        document->setProperty(_bstr_t(L"SelectionLanguage"), _variant_t(L"XPath"));
        document->setProperty(_bstr_t(L"SelectionNamespaces"), _variant_t(kSelectionNamespaces));

        CComQIPtr<IPersistStreamInit> persistentDocument(document);
        if (!persistentDocument || FAILED(persistentDocument->Load(stream))) {
            if (errorMessage != nullptr)
                *errorMessage = L"MSXML could not load the FB2 stream.";
            return false;
        }

        MSXML2::IXMLDOMNodePtr titleInfo = document->selectSingleNode(_bstr_t(L"/fb:FictionBook/fb:description/fb:title-info"));
        MSXML2::IXMLDOMNodePtr documentInfo = document->selectSingleNode(_bstr_t(L"/fb:FictionBook/fb:description/fb:document-info"));
        if (titleInfo == nullptr && documentInfo == nullptr) {
            if (errorMessage != nullptr)
                *errorMessage = L"Neither title-info nor document-info was found in FB2.";
            return false;
        }

        metadata.title = SelectText(titleInfo, L"fb:book-title");
        metadata.authors = ReadAuthors(titleInfo);
        metadata.authorValues = ReadAuthorValues(titleInfo);
        metadata.genres = ReadGenres(titleInfo);
        metadata.keywords = SelectText(titleInfo, L"fb:keywords");
        metadata.language = SelectText(titleInfo, L"fb:lang");
        metadata.sourceLanguage = SelectText(titleInfo, L"fb:src-lang");
        metadata.sequence = ReadSequence(titleInfo);
        metadata.documentAuthors = ReadAuthors(documentInfo);
        metadata.documentAuthorValues = ReadAuthorValues(documentInfo);
        metadata.documentDate = SelectText(documentInfo, L"fb:date");
        metadata.documentId = SelectText(documentInfo, L"fb:id");
        metadata.documentVersion = SelectText(documentInfo, L"fb:version");
        if (documentInfo != nullptr) {
            MSXML2::IXMLDOMNodePtr dateNode = documentInfo->selectSingleNode(_bstr_t(L"fb:date"));
            if (dateNode != nullptr && dateNode->attributes != nullptr)
                metadata.documentDateValue = GetNodeText(dateNode->attributes->getNamedItem(_bstr_t(L"value")));
        }
        return true;
    }
    catch (const _com_error& error) {
        if (errorMessage != nullptr)
            *errorMessage = FormatComError(error);
        return false;
    }
}

bool TryReadStream(IStream* stream, Metadata& metadata, ATL::CString* errorMessage)
{
    metadata.Clear();
    if (errorMessage != nullptr)
        errorMessage->Empty();
    if (stream == nullptr)
        return false;

    CComPtr<IXmlReader> reader;
    HRESULT hr = ::CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(&reader), nullptr);
    if (FAILED(hr) || FAILED(reader->SetInput(stream))) {
        if (errorMessage != nullptr) *errorMessage = L"XmlLite could not open the FB2 stream.";
        return false;
    }

    enum class Field { None, Title, Genre, Keywords, Language, SourceLanguage, FirstName, MiddleName, LastName, Nickname, DocumentDate, DocumentId, DocumentVersion };
    int descriptionDepth = -1, titleInfoDepth = -1, documentInfoDepth = -1, authorDepth = -1, depth = 0;
    bool documentAuthor = false, haveDescription = false;
    Field field = Field::None;
    ATL::CString text, firstName, middleName, lastName, nickname;
    std::vector<ATL::CString>* authorValues = nullptr;
    ATL::CString* authorText = nullptr;
    auto appendAuthor = [&]() {
        ATL::CString author = JoinNonEmpty(firstName, middleName, lastName);
        if (author.IsEmpty()) author = nickname;
        NormalizeWhitespace(author);
        if (!author.IsEmpty() && authorValues != nullptr) {
            authorValues->push_back(author);
            if (!authorText->IsEmpty()) *authorText += L", ";
            *authorText += author;
        }
    };
    auto getAttribute = [&](const wchar_t* name) {
        ATL::CString value;
        if (SUCCEEDED(reader->MoveToFirstAttribute())) {
            do {
                const wchar_t* localName = nullptr; UINT length = 0;
                if (SUCCEEDED(reader->GetLocalName(&localName, &length)) && wcslen(name) == length && wcsncmp(localName, name, length) == 0) {
                    const wchar_t* attrValue = nullptr; UINT attrLength = 0;
                    if (SUCCEEDED(reader->GetValue(&attrValue, &attrLength))) value.SetString(attrValue, attrLength);
                    break;
                }
            } while (SUCCEEDED(reader->MoveToNextAttribute()));
            reader->MoveToElement();
        }
        NormalizeWhitespace(value);
        return value;
    };

    XmlNodeType type;
    while (S_OK == (hr = reader->Read(&type))) {
        if (type == XmlNodeType_Element) {
            const wchar_t* name = nullptr; UINT length = 0; reader->GetLocalName(&name, &length);
            ++depth;
            const auto named = [&](const wchar_t* expected) { return wcslen(expected) == length && wcsncmp(name, expected, length) == 0; };
            if (named(L"description")) { descriptionDepth = depth; haveDescription = true; }
            else if (depth == descriptionDepth + 1 && named(L"title-info")) titleInfoDepth = depth;
            else if (descriptionDepth >= 0 && named(L"document-info")) documentInfoDepth = depth;
            else if (((titleInfoDepth >= 0 && depth == titleInfoDepth + 1) || documentInfoDepth >= 0) && named(L"author")) {
                authorDepth = depth; documentAuthor = documentInfoDepth >= 0;
                firstName.Empty(); middleName.Empty(); lastName.Empty(); nickname.Empty();
                authorValues = documentAuthor ? &metadata.documentAuthorValues : &metadata.authorValues;
                authorText = documentAuthor ? &metadata.documentAuthors : &metadata.authors;
            } else if (depth == titleInfoDepth + 1 && named(L"sequence")) {
                ATL::CString sequence = getAttribute(L"name"), number = getAttribute(L"number");
                if (!sequence.IsEmpty()) { if (!metadata.sequence.IsEmpty()) metadata.sequence += L"; "; metadata.sequence += sequence; if (!number.IsEmpty()) { metadata.sequence += L" ["; metadata.sequence += number; metadata.sequence += L']'; } }
            } else {
                text.Empty(); field = Field::None;
                if (depth == titleInfoDepth + 1) {
                    if (named(L"book-title")) field = Field::Title; else if (named(L"genre")) field = Field::Genre; else if (named(L"keywords")) field = Field::Keywords; else if (named(L"lang")) field = Field::Language; else if (named(L"src-lang")) field = Field::SourceLanguage;
                } else if (documentInfoDepth >= 0) {
                    if (named(L"date")) { field = Field::DocumentDate; metadata.documentDateValue = getAttribute(L"value"); } else if (named(L"id")) field = Field::DocumentId; else if (named(L"version")) field = Field::DocumentVersion;
                } else if (authorDepth >= 0 && depth == authorDepth + 1) {
                    if (named(L"first-name")) field = Field::FirstName; else if (named(L"middle-name")) field = Field::MiddleName; else if (named(L"last-name")) field = Field::LastName; else if (named(L"nickname")) field = Field::Nickname;
                }
            }
            if (reader->IsEmptyElement()) { --depth; if (authorDepth == depth + 1) { appendAuthor(); authorDepth = -1; } }
        } else if ((type == XmlNodeType_Text || type == XmlNodeType_Whitespace) && field != Field::None) {
            const wchar_t* value = nullptr; UINT length = 0; if (SUCCEEDED(reader->GetValue(&value, &length))) text.Append(value, length);
        } else if (type == XmlNodeType_EndElement) {
            const wchar_t* name = nullptr; UINT length = 0; reader->GetLocalName(&name, &length);
            NormalizeWhitespace(text);
            switch (field) { case Field::Title: metadata.title = text; break; case Field::Genre: if (!text.IsEmpty()) { if (!metadata.genres.IsEmpty()) metadata.genres += L", "; metadata.genres += text; } break; case Field::Keywords: metadata.keywords = text; break; case Field::Language: metadata.language = text; break; case Field::SourceLanguage: metadata.sourceLanguage = text; break; case Field::FirstName: firstName = text; break; case Field::MiddleName: middleName = text; break; case Field::LastName: lastName = text; break; case Field::Nickname: nickname = text; break; case Field::DocumentDate: metadata.documentDate = text; break; case Field::DocumentId: metadata.documentId = text; break; case Field::DocumentVersion: metadata.documentVersion = text; break; default: break; }
            field = Field::None;
            if (length == 6 && wcsncmp(name, L"author", 6) == 0 && (authorDepth == depth || documentInfoDepth >= 0)) {
                if (documentInfoDepth >= 0) {
                    ATL::CString author = JoinNonEmpty(firstName, middleName, lastName);
                    if (author.IsEmpty()) author = nickname;
                    NormalizeWhitespace(author);
                    if (!author.IsEmpty()) { metadata.documentAuthorValues.push_back(author); if (!metadata.documentAuthors.IsEmpty()) metadata.documentAuthors += L", "; metadata.documentAuthors += author; }
                } else appendAuthor();
                authorDepth = -1;
            }
            if (titleInfoDepth == depth && length == 10 && wcsncmp(name, L"title-info", 10) == 0) titleInfoDepth = -1;
            if (documentInfoDepth == depth && length == 13 && wcsncmp(name, L"document-info", 13) == 0) documentInfoDepth = -1;
            if (descriptionDepth == depth && length == 11 && wcsncmp(name, L"description", 11) == 0) return haveDescription;
            --depth;
        }
    }
    if (errorMessage != nullptr) *errorMessage = L"FB2 description is incomplete or malformed.";
    return false;
}

} // namespace FB2Metadata
