#include <windows.h>
#include <atlstr.h>
#include <propvarutil.h>

#include "Fb2PropertyStore.h"

namespace FBShellModern {

void Fb2PropertyStoreCache::Clear()
{
    m_entries.clear();
}

bool Fb2PropertyStoreCache::LoadFromMetadata(const FB2Metadata::Metadata& metadata)
{
    Clear();

    FB2Shell::ExplorerProperties properties;
    if (!FB2Shell::BuildExplorerProperties(metadata, properties))
        return false;

    for (int index = 0;; ++index) {
        FB2Shell::PropertyId propertyId = FB2Shell::PropertyId::Author;
        PROPERTYKEY propertyKey = PKEY_Null;
        if (!TryGetPublishedPropertyKeyByIndex(index, propertyId, propertyKey))
            break;

        const ATL::CString value = properties.GetValue(propertyId);
        if (value.IsEmpty())
            continue;

        Entry entry = {};
        entry.propertyId = propertyId;
        entry.propertyKey = propertyKey;
        entry.value = value;
        m_entries.push_back(entry);
    }

    return !m_entries.empty();
}

int Fb2PropertyStoreCache::GetCount() const
{
    return static_cast<int>(m_entries.size());
}

const Fb2PropertyStoreCache::Entry* Fb2PropertyStoreCache::GetAt(int index) const
{
    if (index < 0 || index >= GetCount())
        return nullptr;

    return &m_entries[static_cast<size_t>(index)];
}

const Fb2PropertyStoreCache::Entry* Fb2PropertyStoreCache::FindByPropertyKey(const PROPERTYKEY& propertyKey) const
{
    for (size_t index = 0; index < m_entries.size(); ++index) {
        if (IsEqualPropertyKey(m_entries[index].propertyKey, propertyKey))
            return &m_entries[index];
    }

    return nullptr;
}

HRESULT Fb2PropertyStoreCache::GetValue(const PROPERTYKEY& propertyKey, PROPVARIANT& value) const
{
    ::PropVariantInit(&value);

    const Entry* entry = FindByPropertyKey(propertyKey);
    if (entry == nullptr)
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    return ::InitPropVariantFromString(entry->value, &value);
}

} // namespace FBShellModern
