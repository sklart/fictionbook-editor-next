#include "Fb2Metadata.h"

#include <windows.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <atlstr.h>
#include <comdef.h>

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
    genres.Empty();
    keywords.Empty();
    language.Empty();
    sourceLanguage.Empty();
    sequence.Empty();
    documentAuthors.Empty();
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
            *errorMessage = L"Не указан путь к FB2-файлу.";
        return false;
    }

    try {
        MSXML2::IXMLDOMDocument2Ptr document;
        HRESULT hr = document.CreateInstance(__uuidof(MSXML2::DOMDocument60));
        if (FAILED(hr)) {
            if (errorMessage != nullptr)
                errorMessage->Format(L"Не удалось создать DOMDocument60: 0x%08X", static_cast<unsigned int>(hr));
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
                        L"Ошибка разбора FB2 (строка %ld, позиция %ld): %s",
                        line,
                        linePos,
                        static_cast<const wchar_t*>(reason));
                } else {
                    *errorMessage = L"MSXML не смог загрузить FB2-файл.";
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
                *errorMessage = L"В FB2 не найдены ни title-info, ни document-info.";
            return false;
        }

        metadata.title = SelectText(titleInfo, L"fb:book-title");
        metadata.authors = ReadAuthors(titleInfo);
        metadata.genres = ReadGenres(titleInfo);
        metadata.keywords = SelectText(titleInfo, L"fb:keywords");
        metadata.language = SelectText(titleInfo, L"fb:lang");
        metadata.sourceLanguage = SelectText(titleInfo, L"fb:src-lang");
        metadata.sequence = ReadSequence(titleInfo);

        metadata.documentAuthors = ReadAuthors(documentInfo);
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

} // namespace FB2Metadata
