#pragma once

#include "FBE.h"

namespace FbePluginApiV2
{
HRESULT CreateHost(HWND owner, LPCWSTR locale, IFBEPluginHost** host);
// OpenXmlStream is UTF-8 and GetEncoding consequently always returns "utf-8".
HRESULT CreateSnapshot(MSXML2::IXMLDOMDocument2* document, LPCWSTR sourcePath,
    LPCWSTR documentEncoding, IFBEDocumentSnapshot** snapshot);
}
