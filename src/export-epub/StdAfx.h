#pragma once

#include "targetver.h"

#ifndef UNICODE
#error This plugin requires Unicode build.
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#define _ATL_APARTMENT_THREADED
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _ATL_ALL_WARNINGS

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <atltypes.h>

#include <comdef.h>
#include <comutil.h>
#include <comdefsp.h>

#include <msxml6.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

_COM_SMARTPTR_TYPEDEF(IXMLDOMDocument2, __uuidof(IXMLDOMDocument2));
_COM_SMARTPTR_TYPEDEF(IXMLDOMNode, __uuidof(IXMLDOMNode));
_COM_SMARTPTR_TYPEDEF(IXMLDOMNodeList, __uuidof(IXMLDOMNodeList));
_COM_SMARTPTR_TYPEDEF(IXMLDOMElement, __uuidof(IXMLDOMElement));
_COM_SMARTPTR_TYPEDEF(IXMLDOMParseError, __uuidof(IXMLDOMParseError));

using namespace _com_util;

inline void CheckHR(HRESULT hr)
{
    if (FAILED(hr)) {
        _com_issue_error(hr);
    }
}

inline std::wstring BstrToWString(BSTR value)
{
    return value ? std::wstring(value, SysStringLen(value)) : std::wstring();
}

#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "comctl32.lib")
