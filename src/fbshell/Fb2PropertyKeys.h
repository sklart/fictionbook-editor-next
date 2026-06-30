#pragma once

#include <atlstr.h>
#include <propkey.h>

#include "..\fbe\Fb2ShellProperties.h"

namespace FBShellModern {

bool TryGetConfirmedPropertyKey(FB2Shell::PropertyId propertyId, PROPERTYKEY& propertyKey);
bool TryGetPublishedPropertyKey(FB2Shell::PropertyId propertyId, PROPERTYKEY& propertyKey);
bool TryGetMvpPropertyKeyByIndex(int index, FB2Shell::PropertyId& propertyId, PROPERTYKEY& propertyKey);
bool TryGetPublishedPropertyKeyByIndex(int index, FB2Shell::PropertyId& propertyId, PROPERTYKEY& propertyKey);
HRESULT GetConfirmedPropertyCanonicalName(FB2Shell::PropertyId propertyId, ATL::CString& canonicalName);
HRESULT GetConfirmedPropertyDisplayName(FB2Shell::PropertyId propertyId, ATL::CString& displayName);

} // namespace FBShellModern
