#include "Fb2CoverImage.h"

#include <windows.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <atlstr.h>
#include <comdef.h>
#include <vector>

#import <msxml6.dll> named_guids rename_namespace("MSXML2") exclude("ISequentialStream", "_FILETIME")

namespace {

const wchar_t kSelectionNamespaces[] =
    L"xmlns:fb='http://www.gribuser.ru/xml/fictionbook/2.0' "
    L"xmlns:l='http://www.w3.org/1999/xlink'";

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

ATL::CString GetAttributeText(MSXML2::IXMLDOMNodePtr node, const wchar_t* attributeName)
{
    if (node == nullptr || node->attributes == nullptr)
        return ATL::CString();

    return GetNodeText(node->attributes->getNamedItem(_bstr_t(attributeName)));
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

bool ExtractBinaryBytes(MSXML2::IXMLDOMNodePtr binaryNode, std::vector<unsigned char>& bytes, ATL::CString& errorMessage)
{
    bytes.clear();

    if (binaryNode == nullptr) {
        errorMessage = L"Cover binary node was not found.";
        return false;
    }

    CComQIPtr<MSXML2::IXMLDOMElement> binaryElement(binaryNode);
    if (!binaryElement) {
        errorMessage = L"Cover binary node could not be converted to IXMLDOMElement.";
        return false;
    }

    HRESULT hr = binaryElement->put_dataType(_bstr_t(L"bin.base64"));
    if (FAILED(hr)) {
        errorMessage.Format(L"Failed to enable bin.base64 mode for the cover node: 0x%08X", static_cast<unsigned int>(hr));
        return false;
    }

    _variant_t typedValue;
    hr = binaryElement->get_nodeTypedValue(&typedValue);
    if (FAILED(hr)) {
        errorMessage.Format(L"Failed to read decoded cover bytes: 0x%08X", static_cast<unsigned int>(hr));
        return false;
    }

    if ((typedValue.vt & VT_ARRAY) == 0 || typedValue.parray == nullptr) {
        errorMessage = L"MSXML could not decode cover base64 data.";
        return false;
    }

    SAFEARRAY* safeArray = typedValue.parray;
    if (SafeArrayGetDim(safeArray) != 1) {
        errorMessage = L"Decoded cover data has an unexpected format.";
        return false;
    }

    LONG lowerBound = 0;
    LONG upperBound = -1;
    if (FAILED(SafeArrayGetLBound(safeArray, 1, &lowerBound)) ||
        FAILED(SafeArrayGetUBound(safeArray, 1, &upperBound)) ||
        upperBound < lowerBound) {
        errorMessage = L"Failed to read decoded cover size.";
        return false;
    }

    const ULONG byteCount = static_cast<ULONG>(upperBound - lowerBound + 1);
    bytes.resize(byteCount);
    if (byteCount == 0)
        return true;

    void* dataPointer = nullptr;
    const HRESULT accessHr = SafeArrayAccessData(safeArray, &dataPointer);
    if (FAILED(accessHr)) {
        errorMessage.Format(L"Failed to access cover bytes: 0x%08X", static_cast<unsigned int>(accessHr));
        bytes.clear();
        return false;
    }

    std::memcpy(bytes.data(), dataPointer, byteCount);
    SafeArrayUnaccessData(safeArray);
    return true;
}

} // namespace

namespace FB2CoverImage {

void CoverImage::Clear()
{
    href.Empty();
    binaryId.Empty();
    contentType.Empty();
    bytes.clear();
}

bool CoverImage::IsEmpty() const
{
    return bytes.empty();
}

bool TryRead(const wchar_t* filePath, CoverImage& coverImage, ATL::CString* errorMessage)
{
    coverImage.Clear();

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
                    errorMessage->Format(
                        L"FB2 parse error (line %ld, position %ld): %s",
                        parseError->line,
                        parseError->linepos,
                        static_cast<const wchar_t*>(parseError->reason));
                } else {
                    *errorMessage = L"MSXML could not load the FB2 file.";
                }
            }
            return false;
        }

        MSXML2::IXMLDOMNodePtr hrefNode = document->selectSingleNode(
            _bstr_t(L"/fb:FictionBook/fb:description/fb:title-info/fb:coverpage/fb:image/@l:href"));
        coverImage.href = GetNodeText(hrefNode);
        if (coverImage.href.IsEmpty()) {
            if (errorMessage != nullptr)
                *errorMessage = L"FB2 cover reference was not found.";
            return false;
        }

        coverImage.binaryId = coverImage.href;
        if (!coverImage.binaryId.IsEmpty() && coverImage.binaryId[0] == L'#')
            coverImage.binaryId.Delete(0);
        NormalizeWhitespace(coverImage.binaryId);
        if (coverImage.binaryId.IsEmpty()) {
            if (errorMessage != nullptr)
                *errorMessage = L"FB2 cover reference is present, but the binary identifier is empty.";
            return false;
        }

        MSXML2::IXMLDOMNodeListPtr binaryNodes = document->selectNodes(
            _bstr_t(L"/fb:FictionBook/fb:binary"));
        if (binaryNodes == nullptr) {
            if (errorMessage != nullptr)
                *errorMessage = L"FB2 cover binary section was not found.";
            return false;
        }

        MSXML2::IXMLDOMNodePtr matchedBinaryNode;
        const long count = binaryNodes->length;
        for (long index = 0; index < count; ++index) {
            MSXML2::IXMLDOMNodePtr binaryNode = binaryNodes->item[index];
            if (GetAttributeText(binaryNode, L"id") == coverImage.binaryId) {
                matchedBinaryNode = binaryNode;
                coverImage.contentType = GetAttributeText(binaryNode, L"content-type");
                break;
            }
        }

        if (matchedBinaryNode == nullptr) {
            if (errorMessage != nullptr)
                errorMessage->Format(L"Cover binary with id '%s' was not found.", static_cast<const wchar_t*>(coverImage.binaryId));
            return false;
        }

        ATL::CString localError;
        if (!ExtractBinaryBytes(matchedBinaryNode, coverImage.bytes, localError)) {
            coverImage.Clear();
            if (errorMessage != nullptr)
                *errorMessage = localError;
            return false;
        }

        if (coverImage.bytes.empty()) {
            coverImage.Clear();
            if (errorMessage != nullptr)
                *errorMessage = L"Cover data was decoded, but the result is empty.";
            return false;
        }

        return true;
    }
    catch (const _com_error& error) {
        coverImage.Clear();
        if (errorMessage != nullptr)
            *errorMessage = FormatComError(error);
        return false;
    }
}

} // namespace FB2CoverImage


