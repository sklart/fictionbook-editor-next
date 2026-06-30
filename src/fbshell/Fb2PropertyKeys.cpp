#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <propsys.h>

#pragma comment(lib, "propsys.lib")

#include "Fb2PropertyKeys.h"
#include "FbeCustomProperties.h"

namespace FBShellModern {

bool TryGetConfirmedPropertyKey(FB2Shell::PropertyId propertyId, PROPERTYKEY& propertyKey)
{
    switch (propertyId) {
    case FB2Shell::PropertyId::Author:
        propertyKey = PKEY_Author;
        return true;
    case FB2Shell::PropertyId::Title:
        propertyKey = PKEY_Title;
        return true;
    case FB2Shell::PropertyId::Keywords:
    case FB2Shell::PropertyId::Genre:
    case FB2Shell::PropertyId::Sequence:
    case FB2Shell::PropertyId::DocumentId:
    case FB2Shell::PropertyId::DocumentVersion:
    case FB2Shell::PropertyId::DocumentDate:
        break;
    case FB2Shell::PropertyId::Language:
        propertyKey = PKEY_Language;
        return true;
    }

    propertyKey = PKEY_Null;
    return false;
}

bool TryGetPublishedPropertyKey(FB2Shell::PropertyId propertyId, PROPERTYKEY& propertyKey)
{
    switch (propertyId) {
    case FB2Shell::PropertyId::Author:
        propertyKey = PKEY_Author;
        return true;
    case FB2Shell::PropertyId::Title:
        propertyKey = PKEY_Title;
        return true;
    case FB2Shell::PropertyId::Genre:
        propertyKey = PKEY_FBE_Genre;
        return true;
    case FB2Shell::PropertyId::Keywords:
        propertyKey = PKEY_FBE_Keywords;
        return true;
    case FB2Shell::PropertyId::Language:
        propertyKey = PKEY_Language;
        return true;
    case FB2Shell::PropertyId::DocumentId:
        propertyKey = PKEY_FBE_DocumentId;
        return true;
    case FB2Shell::PropertyId::DocumentVersion:
        propertyKey = PKEY_FBE_DocumentVersion;
        return true;
    case FB2Shell::PropertyId::DocumentDate:
        propertyKey = PKEY_FBE_DocumentDate;
        return true;
    case FB2Shell::PropertyId::Sequence:
        propertyKey = PKEY_FBE_Sequence;
        return true;
    }

    propertyKey = PKEY_Null;
    return false;
}

bool TryGetMvpPropertyKeyByIndex(int index, FB2Shell::PropertyId& propertyId, PROPERTYKEY& propertyKey)
{
    const FB2Shell::PropertyDescriptor* descriptor = FB2Shell::GetMvpPropertyDescriptorByIndex(index);
    if (descriptor == nullptr) {
        propertyId = FB2Shell::PropertyId::Author;
        propertyKey = PKEY_Null;
        return false;
    }

    propertyId = descriptor->id;
    return TryGetConfirmedPropertyKey(propertyId, propertyKey);
}

bool TryGetPublishedPropertyKeyByIndex(int index, FB2Shell::PropertyId& propertyId, PROPERTYKEY& propertyKey)
{
    if (index < 0) {
        propertyId = FB2Shell::PropertyId::Author;
        propertyKey = PKEY_Null;
        return false;
    }

    int publishedIndex = 0;
    for (int descriptorIndex = 0; descriptorIndex < FB2Shell::GetPropertyDescriptorCount(); ++descriptorIndex) {
        const FB2Shell::PropertyDescriptor* descriptor = FB2Shell::GetPropertyDescriptorByIndex(descriptorIndex);
        if (descriptor == nullptr)
            continue;

        PROPERTYKEY currentKey = PKEY_Null;
        if (!TryGetPublishedPropertyKey(descriptor->id, currentKey))
            continue;

        if (publishedIndex == index) {
            propertyId = descriptor->id;
            propertyKey = currentKey;
            return true;
        }

        ++publishedIndex;
    }

    propertyId = FB2Shell::PropertyId::Author;
    propertyKey = PKEY_Null;
    return false;
}

HRESULT GetConfirmedPropertyCanonicalName(FB2Shell::PropertyId propertyId, ATL::CString& canonicalName)
{
    canonicalName.Empty();

    PROPERTYKEY propertyKey = PKEY_Null;
    if (!TryGetConfirmedPropertyKey(propertyId, propertyKey))
        return E_INVALIDARG;

    PWSTR propertyName = nullptr;
    const HRESULT hr = ::PSGetNameFromPropertyKey(propertyKey, &propertyName);
    if (FAILED(hr))
        return hr;

    canonicalName = propertyName;
    ::CoTaskMemFree(propertyName);
    return S_OK;
}

HRESULT GetConfirmedPropertyDisplayName(FB2Shell::PropertyId propertyId, ATL::CString& displayName)
{
    displayName.Empty();

    PROPERTYKEY propertyKey = PKEY_Null;
    if (!TryGetConfirmedPropertyKey(propertyId, propertyKey))
        return E_INVALIDARG;

    CComPtr<IPropertyDescription> propertyDescription;
    HRESULT hr = ::PSGetPropertyDescription(propertyKey, IID_PPV_ARGS(&propertyDescription));
    if (FAILED(hr))
        return hr;

    PWSTR propertyDisplayName = nullptr;
    hr = propertyDescription->GetDisplayName(&propertyDisplayName);
    if (FAILED(hr))
        return hr;

    displayName = propertyDisplayName;
    ::CoTaskMemFree(propertyDisplayName);
    return S_OK;
}

} // namespace FBShellModern
