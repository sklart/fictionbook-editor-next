#include "Fb2ShellProperties.h"

namespace FB2Shell {

namespace {

const PropertyDescriptor kPropertyDescriptors[] = {
    { PropertyId::Author,          L"author",          L"Автор",            L"System.Author",                 MappingStatus::Confirmed,   DisplayNameSource::WindowsLocalized, L"Подходит как стандартное свойство автора документа." },
    { PropertyId::Title,           L"title",           L"Название",         L"System.Title",                  MappingStatus::Confirmed,   DisplayNameSource::WindowsLocalized, L"Подходит как стандартное свойство названия документа." },
    { PropertyId::Genre,           L"genre",           L"FBE:Жанр",         nullptr,                          MappingStatus::FbeSpecific, DisplayNameSource::FbeFallback,      L"Книжный жанр переведён в отдельное FBE-specific свойство, чтобы не использовать музыкальный системный ключ Windows." },
    { PropertyId::Keywords,        L"keywords",        L"FBE:Ключевые слова", nullptr,                         MappingStatus::FbeSpecific, DisplayNameSource::FbeFallback,      L"Ключевые слова переведены в отдельное FBE-specific свойство, потому что UI Проводника Windows 11 непредсказуемо показывает System.Keywords." },
    { PropertyId::Language,        L"language",        L"Язык",             L"System.Language",               MappingStatus::Confirmed,   DisplayNameSource::WindowsLocalized, L"Подходит как стандартное свойство языка." },
    { PropertyId::Sequence,        L"sequence",        L"FBE:Серия",        nullptr,                          MappingStatus::FbeSpecific, DisplayNameSource::FbeFallback,      L"Книжная серия публикуется как отдельное FBE-specific свойство." },
    { PropertyId::DocumentId,      L"documentId",      L"FBE:Идентификатор документа", nullptr,                 MappingStatus::FbeSpecific, DisplayNameSource::FbeFallback,      L"Идентификатор документа переведён в отдельное FBE-specific свойство, потому что UI Проводника Windows 11 непредсказуемо показывает System.Document.DocumentID." },
    { PropertyId::DocumentVersion, L"documentVersion", L"FBE:Версия документа", nullptr,                       MappingStatus::FbeSpecific, DisplayNameSource::FbeFallback,      L"Версия FB2-документа публикуется как отдельное FBE-specific свойство, а не через общее системное поле версии документа." },
    { PropertyId::DocumentDate,    L"documentDate",    L"FBE:Дата документа",   nullptr,                       MappingStatus::FbeSpecific, DisplayNameSource::FbeFallback,      L"Дата FB2-документа публикуется как отдельное FBE-specific свойство, чтобы не смешивать её с системной датой создания файла или документа Windows." }
};

ATL::CString SelectNonEmpty(const ATL::CString& primary, const ATL::CString& fallback)
{
    return primary.IsEmpty() ? fallback : primary;
}

const PropertyDescriptor* FindDescriptor(PropertyId propertyId)
{
    for (const PropertyDescriptor& descriptor : kPropertyDescriptors) {
        if (descriptor.id == propertyId)
            return &descriptor;
    }

    return nullptr;
}

} // namespace

void ExplorerProperties::Clear()
{
    author.Empty();
    title.Empty();
    genre.Empty();
    keywords.Empty();
    language.Empty();
    sequence.Empty();
    documentId.Empty();
    documentVersion.Empty();
    documentDate.Empty();
}

bool ExplorerProperties::HasValue(PropertyId propertyId) const
{
    return !GetValue(propertyId).IsEmpty();
}

ATL::CString ExplorerProperties::GetValue(PropertyId propertyId) const
{
    switch (propertyId) {
    case PropertyId::Author:
        return author;
    case PropertyId::Title:
        return title;
    case PropertyId::Genre:
        return genre;
    case PropertyId::Keywords:
        return keywords;
    case PropertyId::Language:
        return language;
    case PropertyId::Sequence:
        return sequence;
    case PropertyId::DocumentId:
        return documentId;
    case PropertyId::DocumentVersion:
        return documentVersion;
    case PropertyId::DocumentDate:
        return documentDate;
    }

    return ATL::CString();
}

bool BuildExplorerProperties(const FB2Metadata::Metadata& metadata, ExplorerProperties& properties)
{
    properties.Clear();

    properties.author = SelectNonEmpty(metadata.authors, metadata.documentAuthors);
    properties.title = metadata.title;
    properties.genre = metadata.genres;
    properties.keywords = metadata.keywords;
    properties.language = SelectNonEmpty(metadata.language, metadata.sourceLanguage);
    properties.sequence = metadata.sequence;
    properties.documentId = metadata.documentId;
    properties.documentVersion = metadata.documentVersion;
    properties.documentDate = SelectNonEmpty(metadata.documentDate, metadata.documentDateValue);

    return properties.HasValue(PropertyId::Author)
        || properties.HasValue(PropertyId::Title)
        || properties.HasValue(PropertyId::Genre)
        || properties.HasValue(PropertyId::Keywords)
        || properties.HasValue(PropertyId::Language)
        || properties.HasValue(PropertyId::Sequence)
        || properties.HasValue(PropertyId::DocumentId)
        || properties.HasValue(PropertyId::DocumentVersion)
        || properties.HasValue(PropertyId::DocumentDate);
}

int GetPropertyDescriptorCount()
{
    return static_cast<int>(sizeof(kPropertyDescriptors) / sizeof(kPropertyDescriptors[0]));
}

const PropertyDescriptor* GetPropertyDescriptorByIndex(int index)
{
    if (index < 0 || index >= GetPropertyDescriptorCount())
        return nullptr;

    return &kPropertyDescriptors[index];
}

const PropertyDescriptor* GetPropertyDescriptor(PropertyId propertyId)
{
    return FindDescriptor(propertyId);
}

int GetMvpPropertyDescriptorCount()
{
    int count = 0;
    for (const PropertyDescriptor& descriptor : kPropertyDescriptors) {
        if (IsMvpWindowsProperty(descriptor.id))
            ++count;
    }

    return count;
}

const PropertyDescriptor* GetMvpPropertyDescriptorByIndex(int index)
{
    if (index < 0)
        return nullptr;

    int currentIndex = 0;
    for (const PropertyDescriptor& descriptor : kPropertyDescriptors) {
        if (!IsMvpWindowsProperty(descriptor.id))
            continue;

        if (currentIndex == index)
            return &descriptor;

        ++currentIndex;
    }

    return nullptr;
}

const wchar_t* GetPropertyDebugName(PropertyId propertyId)
{
    const PropertyDescriptor* descriptor = FindDescriptor(propertyId);
    return descriptor != nullptr ? descriptor->debugName : L"unknown";
}

const wchar_t* GetPropertyFallbackDisplayName(PropertyId propertyId)
{
    const PropertyDescriptor* descriptor = FindDescriptor(propertyId);
    return descriptor != nullptr ? descriptor->fallbackDisplayName : L"Неизвестное свойство";
}

const wchar_t* GetPropertyWindowsCanonicalName(PropertyId propertyId)
{
    const PropertyDescriptor* descriptor = FindDescriptor(propertyId);
    return descriptor != nullptr ? descriptor->windowsCanonicalName : nullptr;
}

MappingStatus GetPropertyMappingStatus(PropertyId propertyId)
{
    const PropertyDescriptor* descriptor = FindDescriptor(propertyId);
    return descriptor != nullptr ? descriptor->mappingStatus : MappingStatus::FbeSpecific;
}

const wchar_t* GetPropertyMappingStatusName(PropertyId propertyId)
{
    switch (GetPropertyMappingStatus(propertyId)) {
    case MappingStatus::Confirmed:
        return L"confirmed";
    case MappingStatus::Candidate:
        return L"candidate";
    case MappingStatus::FbeSpecific:
        return L"fbe-specific";
    }

    return L"unknown";
}

DisplayNameSource GetPropertyDisplayNameSource(PropertyId propertyId)
{
    const PropertyDescriptor* descriptor = FindDescriptor(propertyId);
    return descriptor != nullptr ? descriptor->displayNameSource : DisplayNameSource::FbeFallback;
}

const wchar_t* GetPropertyDisplayNameSourceName(PropertyId propertyId)
{
    switch (GetPropertyDisplayNameSource(propertyId)) {
    case DisplayNameSource::WindowsLocalized:
        return L"windows-localized";
    case DisplayNameSource::FbeFallback:
        return L"fbe-fallback";
    }

    return L"unknown";
}

const wchar_t* GetPropertyMappingNote(PropertyId propertyId)
{
    const PropertyDescriptor* descriptor = FindDescriptor(propertyId);
    return descriptor != nullptr ? descriptor->mappingNote : L"";
}

bool UsesWindowsProperty(PropertyId propertyId)
{
    return GetPropertyWindowsCanonicalName(propertyId) != nullptr;
}

bool IsConfirmedWindowsProperty(PropertyId propertyId)
{
    return UsesWindowsProperty(propertyId)
        && GetPropertyMappingStatus(propertyId) == MappingStatus::Confirmed
        && GetPropertyDisplayNameSource(propertyId) == DisplayNameSource::WindowsLocalized;
}

bool IsMvpWindowsProperty(PropertyId propertyId)
{
    switch (propertyId) {
    case PropertyId::Author:
    case PropertyId::Title:
    case PropertyId::Language:
        return IsConfirmedWindowsProperty(propertyId);
    case PropertyId::Keywords:
    case PropertyId::Genre:
    case PropertyId::Sequence:
    case PropertyId::DocumentId:
    case PropertyId::DocumentVersion:
    case PropertyId::DocumentDate:
        return false;
    }

    return false;
}

} // namespace FB2Shell
