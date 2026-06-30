#pragma once

// Do not use `#import <msxml6.dll>` here.
//
// MSVC #import creates msxml6.tlh/msxml6.tli in the intermediate directory.
// PVS-Studio may start analysis before these generated files and directories
// exist, which leads to V008 / C1083 errors.  Using the stable Windows SDK
// header keeps the project analyzable from a clean checkout with no separate
// preparation step.
#include <msxml6.h>
#include <comip.h>

_COM_SMARTPTR_TYPEDEF(IXMLDOMDocument2, __uuidof(IXMLDOMDocument2));
_COM_SMARTPTR_TYPEDEF(IXMLDOMNode, __uuidof(IXMLDOMNode));
_COM_SMARTPTR_TYPEDEF(IXMLDOMNodeList, __uuidof(IXMLDOMNodeList));
_COM_SMARTPTR_TYPEDEF(IXMLDOMNamedNodeMap, __uuidof(IXMLDOMNamedNodeMap));
_COM_SMARTPTR_TYPEDEF(IXMLDOMParseError, __uuidof(IXMLDOMParseError));

namespace MSXML2
{
    using ::DOMNodeType;
    using ::NODE_INVALID;
    using ::NODE_ATTRIBUTE;
    using ::NODE_CDATA_SECTION;
    using ::NODE_COMMENT;
    using ::NODE_DOCUMENT;
    using ::NODE_ELEMENT;
    using ::NODE_TEXT;

    using ::IXMLDOMDocument2;
    using ::IXMLDOMNamedNodeMap;
    using ::IXMLDOMNode;
    using ::IXMLDOMNodeList;
    using ::IXMLDOMParseError;

    using ::IXMLDOMDocument2Ptr;
    using ::IXMLDOMNodePtr;
    using ::IXMLDOMNodeListPtr;
    using ::IXMLDOMNamedNodeMapPtr;
    using ::IXMLDOMParseErrorPtr;
}

inline CStringW MsxmlBstrToString(BSTR value)
{
    CStringW result(value ? value : L"");
    ::SysFreeString(value);
    return result;
}

inline CStringW MsxmlGetNodeBaseName(const MSXML2::IXMLDOMNodePtr& node)
{
    if (!node)
        return CStringW();
    BSTR value = nullptr;
    if (FAILED(node->get_baseName(&value)))
        return CStringW();
    return MsxmlBstrToString(value);
}

inline CStringW MsxmlGetNodeName(const MSXML2::IXMLDOMNodePtr& node)
{
    if (!node)
        return CStringW();
    BSTR value = nullptr;
    if (FAILED(node->get_nodeName(&value)))
        return CStringW();
    return MsxmlBstrToString(value);
}

inline CStringW MsxmlGetNodeText(const MSXML2::IXMLDOMNodePtr& node)
{
    if (!node)
        return CStringW();
    BSTR value = nullptr;
    if (FAILED(node->get_text(&value)))
        return CStringW();
    return MsxmlBstrToString(value);
}

inline MSXML2::DOMNodeType MsxmlGetNodeType(const MSXML2::IXMLDOMNodePtr& node)
{
    if (!node)
        return MSXML2::NODE_INVALID;
    MSXML2::DOMNodeType value = MSXML2::NODE_INVALID;
    node->get_nodeType(&value);
    return value;
}

inline MSXML2::IXMLDOMNamedNodeMapPtr MsxmlGetAttributes(const MSXML2::IXMLDOMNodePtr& node)
{
    if (!node)
        return nullptr;
    MSXML2::IXMLDOMNamedNodeMap* raw = nullptr;
    if (FAILED(node->get_attributes(&raw)))
        return nullptr;
    return MSXML2::IXMLDOMNamedNodeMapPtr(raw, false);
}

inline MSXML2::IXMLDOMNodePtr MsxmlGetNamedItem(const MSXML2::IXMLDOMNamedNodeMapPtr& attrs, LPCWSTR name)
{
    if (!attrs || !name)
        return nullptr;
    MSXML2::IXMLDOMNode* raw = nullptr;
    CComBSTR bstrName(name);
    if (FAILED(attrs->getNamedItem(bstrName, &raw)))
        return nullptr;
    return MSXML2::IXMLDOMNodePtr(raw, false);
}

inline long MsxmlGetNamedNodeMapLength(const MSXML2::IXMLDOMNamedNodeMapPtr& attrs)
{
    if (!attrs)
        return 0;
    long value = 0;
    attrs->get_length(&value);
    return value;
}

inline MSXML2::IXMLDOMNodePtr MsxmlGetNamedNodeMapItem(const MSXML2::IXMLDOMNamedNodeMapPtr& attrs, long index)
{
    if (!attrs)
        return nullptr;
    MSXML2::IXMLDOMNode* raw = nullptr;
    if (FAILED(attrs->get_item(index, &raw)))
        return nullptr;
    return MSXML2::IXMLDOMNodePtr(raw, false);
}

inline MSXML2::IXMLDOMNodeListPtr MsxmlGetChildNodes(const MSXML2::IXMLDOMNodePtr& node)
{
    if (!node)
        return nullptr;
    MSXML2::IXMLDOMNodeList* raw = nullptr;
    if (FAILED(node->get_childNodes(&raw)))
        return nullptr;
    return MSXML2::IXMLDOMNodeListPtr(raw, false);
}

inline long MsxmlGetNodeListLength(const MSXML2::IXMLDOMNodeListPtr& list)
{
    if (!list)
        return 0;
    long value = 0;
    list->get_length(&value);
    return value;
}

inline MSXML2::IXMLDOMNodePtr MsxmlGetNodeListItem(const MSXML2::IXMLDOMNodeListPtr& list, long index)
{
    if (!list)
        return nullptr;
    MSXML2::IXMLDOMNode* raw = nullptr;
    if (FAILED(list->get_item(index, &raw)))
        return nullptr;
    return MSXML2::IXMLDOMNodePtr(raw, false);
}

inline MSXML2::IXMLDOMNodePtr MsxmlGetDocumentElement(const MSXML2::IXMLDOMDocument2Ptr& doc)
{
    if (!doc)
        return nullptr;
    ::IXMLDOMElement* rawElement = nullptr;
    if (FAILED(doc->get_documentElement(&rawElement)))
        return nullptr;
    MSXML2::IXMLDOMNode* rawNode = nullptr;
    if (rawElement && SUCCEEDED(rawElement->QueryInterface(__uuidof(::IXMLDOMNode), reinterpret_cast<void**>(&rawNode))))
    {
        rawElement->Release();
        return MSXML2::IXMLDOMNodePtr(rawNode, false);
    }
    if (rawElement)
        rawElement->Release();
    return nullptr;
}

inline CStringW MsxmlGetParseErrorReason(const MSXML2::IXMLDOMDocument2Ptr& doc)
{
    if (!doc)
        return CStringW();
    MSXML2::IXMLDOMParseError* raw = nullptr;
    if (FAILED(doc->get_parseError(&raw)) || !raw)
        return CStringW();
    MSXML2::IXMLDOMParseErrorPtr err(raw, false);
    BSTR reason = nullptr;
    if (FAILED(err->get_reason(&reason)))
        return CStringW();
    return MsxmlBstrToString(reason);
}
