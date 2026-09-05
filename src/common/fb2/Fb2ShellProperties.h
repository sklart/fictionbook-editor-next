#pragma once

#include <atlstr.h>

#include "Fb2Metadata.h"

namespace FB2Shell {

enum class PropertyId {
    Author,
    Title,
    Genre,
    Keywords,
    Language,
    Sequence,
    DocumentId,
    DocumentVersion,
    DocumentDate
};

enum class MappingStatus {
    Confirmed,
    Candidate,
    FbeSpecific
};

enum class DisplayNameSource {
    WindowsLocalized,
    FbeFallback
};

struct PropertyDescriptor {
    PropertyId id;
    const wchar_t* debugName;
    const wchar_t* fallbackDisplayName;
    const wchar_t* windowsCanonicalName;
    MappingStatus mappingStatus;
    DisplayNameSource displayNameSource;
    const wchar_t* mappingNote;
};

struct ExplorerProperties {
    ATL::CString author;
    ATL::CString title;
    ATL::CString genre;
    ATL::CString keywords;
    ATL::CString language;
    ATL::CString sequence;
    ATL::CString documentId;
    ATL::CString documentVersion;
    ATL::CString documentDate;

    void Clear();
    bool HasValue(PropertyId propertyId) const;
    ATL::CString GetValue(PropertyId propertyId) const;
};

bool BuildExplorerProperties(const FB2Metadata::Metadata& metadata, ExplorerProperties& properties);
int GetPropertyDescriptorCount();
const PropertyDescriptor* GetPropertyDescriptorByIndex(int index);
const PropertyDescriptor* GetPropertyDescriptor(PropertyId propertyId);
int GetMvpPropertyDescriptorCount();
const PropertyDescriptor* GetMvpPropertyDescriptorByIndex(int index);
const wchar_t* GetPropertyDebugName(PropertyId propertyId);
const wchar_t* GetPropertyFallbackDisplayName(PropertyId propertyId);
const wchar_t* GetPropertyWindowsCanonicalName(PropertyId propertyId);
MappingStatus GetPropertyMappingStatus(PropertyId propertyId);
const wchar_t* GetPropertyMappingStatusName(PropertyId propertyId);
DisplayNameSource GetPropertyDisplayNameSource(PropertyId propertyId);
const wchar_t* GetPropertyDisplayNameSourceName(PropertyId propertyId);
const wchar_t* GetPropertyMappingNote(PropertyId propertyId);
bool UsesWindowsProperty(PropertyId propertyId);
bool IsConfirmedWindowsProperty(PropertyId propertyId);
bool IsMvpWindowsProperty(PropertyId propertyId);

} // namespace FB2Shell
