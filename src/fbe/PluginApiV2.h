#pragma once

#include "FBE.h"

namespace FbePluginApiV2
{
HRESULT CreateHost(HWND owner, LPCWSTR locale, IFBEPluginHost** host);
HRESULT CreateSnapshot(MSXML2::IXMLDOMDocument2* document, LPCWSTR sourcePath,
    LPCWSTR encoding, IFBEDocumentSnapshot** snapshot);
}
