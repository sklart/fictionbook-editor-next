#pragma once

#include <propidl.h>
#include <vector>

#include "Fb2PropertyKeys.h"
#include "..\fbe\Fb2Metadata.h"

namespace FBShellModern {

class Fb2PropertyStoreCache {
public:
    struct Entry {
        FB2Shell::PropertyId propertyId;
        PROPERTYKEY propertyKey;
        ATL::CString value;
    };

    void Clear();
    bool LoadFromMetadata(const FB2Metadata::Metadata& metadata);
    int GetCount() const;
    const Entry* GetAt(int index) const;
    const Entry* FindByPropertyKey(const PROPERTYKEY& propertyKey) const;
    HRESULT GetValue(const PROPERTYKEY& propertyKey, PROPVARIANT& value) const;

private:
    std::vector<Entry> m_entries;
};

} // namespace FBShellModern
