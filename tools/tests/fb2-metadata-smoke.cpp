#include <windows.h>
#include <objidl.h>
#include <atlstr.h>
#include <iostream>
#include <vector>

#include "..\..\src\fbe\Fb2Metadata.h"
#include "..\..\src\fbe\Fb2ShellProperties.h"
#include "..\..\src\fbshell\FbeCustomProperties.h"
#include "..\..\src\fbshell\Fb2PropertyKeys.h"
#include "..\..\src\fbshell\Fb2PropertyStore.h"

namespace {

bool ExpectEqual(const wchar_t* name, const CString& actual, const wchar_t* expected)
{
    if (actual == expected)
        return true;

    std::wcerr
        << L"Проверка поля '" << name << L"' не прошла. Ожидалось: '"
        << expected << L"', получено: '" << static_cast<const wchar_t*>(actual)
        << L"'.\n";
    return false;
}

bool ExpectTextEqual(const wchar_t* name, const wchar_t* actual, const wchar_t* expected)
{
    const wchar_t* safeActual = actual != nullptr ? actual : L"(null)";
    const wchar_t* safeExpected = expected != nullptr ? expected : L"(null)";
    if (((actual == nullptr) && (expected == nullptr)) || (actual != nullptr && expected != nullptr && wcscmp(actual, expected) == 0))
        return true;

    std::wcerr
        << L"Проверка текста '" << name << L"' не прошла. Ожидалось: '"
        << safeExpected << L"', получено: '" << safeActual << L"'.\n";
    return false;
}

bool ExpectTrue(const wchar_t* name, bool value)
{
    if (value)
        return true;

    std::wcerr << L"Проверка условия '" << name << L"' не прошла.\n";
    return false;
}

bool ExpectHResultSuccess(const wchar_t* name, HRESULT hr)
{
    if (SUCCEEDED(hr))
        return true;

    std::wcerr << L"Проверка HRESULT '" << name << L"' не прошла. Код: 0x"
               << std::hex << hr << std::dec << L".\n";
    return false;
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2) {
        std::wcerr << L"Использование: fb2-metadata-smoke.exe <fb2-файл>\n";
        return 10;
    }

    const HRESULT initHr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initHr)) {
        std::wcerr << L"Ошибка CoInitializeEx: 0x" << std::hex << initHr << L"\n";
        return 11;
    }

    FB2Metadata::Metadata metadata;
    CString errorMessage;
    const bool ok = FB2Metadata::TryRead(argv[1], metadata, &errorMessage);
    if (!ok) {
        std::wcerr << L"Не удалось прочитать метаданные FB2: "
                   << static_cast<const wchar_t*>(errorMessage) << L"\n";
        ::CoUninitialize();
        return 1;
    }

    bool success = true;
    success = ExpectEqual(L"title", metadata.title, L"Metadata Smoke Test") && success;
    success = ExpectEqual(L"authors", metadata.authors, L"Codex Smoke") && success;
    success = ExpectEqual(L"genres", metadata.genres, L"computers, reference") && success;
    success = ExpectEqual(L"language", metadata.language, L"ru") && success;
    success = ExpectEqual(L"sourceLanguage", metadata.sourceLanguage, L"en") && success;
    success = ExpectEqual(L"sequence", metadata.sequence, L"Shell Series [2]") && success;
    success = ExpectEqual(L"documentAuthors", metadata.documentAuthors, L"Codex Editor") && success;
    success = ExpectEqual(L"documentDate", metadata.documentDate, L"12.06.2026") && success;
    success = ExpectEqual(L"documentDateValue", metadata.documentDateValue, L"2026-06-12") && success;
    success = ExpectEqual(L"documentId", metadata.documentId, L"fb2-metadata-smoke-test") && success;
    success = ExpectEqual(L"documentVersion", metadata.documentVersion, L"1.2") && success;
    success = ExpectEqual(L"keywords", metadata.keywords, L"alpha; beta; metadata") && success;

    FB2Shell::ExplorerProperties properties;
    if (!FB2Shell::BuildExplorerProperties(metadata, properties)) {
        std::wcerr << L"Не удалось построить модель shell-свойств для Проводника.\n";
        ::CoUninitialize();
        return 3;
    }

    success = ExpectEqual(L"explorer.author", properties.author, L"Codex Smoke") && success;
    success = ExpectEqual(L"explorer.title", properties.title, L"Metadata Smoke Test") && success;
    success = ExpectEqual(L"explorer.genre", properties.genre, L"computers, reference") && success;
    success = ExpectEqual(L"explorer.keywords", properties.keywords, L"alpha; beta; metadata") && success;
    success = ExpectEqual(L"explorer.language", properties.language, L"ru") && success;
    success = ExpectEqual(L"explorer.sequence", properties.sequence, L"Shell Series [2]") && success;
    success = ExpectEqual(L"explorer.documentId", properties.documentId, L"fb2-metadata-smoke-test") && success;
    success = ExpectEqual(L"explorer.documentVersion", properties.documentVersion, L"1.2") && success;
    success = ExpectEqual(L"explorer.documentDate", properties.documentDate, L"12.06.2026") && success;

    if (FB2Shell::GetPropertyDescriptorCount() != 9) {
        std::wcerr << L"Ожидалось 9 дескрипторов shell-свойств.\n";
        ::CoUninitialize();
        return 4;
    }

    if (FB2Shell::GetMvpPropertyDescriptorCount() != 3) {
        std::wcerr << L"Ожидалось 3 MVP-дескриптора shell-свойств.\n";
        ::CoUninitialize();
        return 5;
    }

    success = ExpectTextEqual(
        L"mvp[0].debugName",
        FB2Shell::GetMvpPropertyDescriptorByIndex(0)->debugName,
        L"author") && success;
    success = ExpectTextEqual(
        L"mvp[1].debugName",
        FB2Shell::GetMvpPropertyDescriptorByIndex(1)->debugName,
        L"title") && success;
    success = ExpectTextEqual(
        L"mvp[2].debugName",
        FB2Shell::GetMvpPropertyDescriptorByIndex(2)->debugName,
        L"language") && success;
    success = ExpectTextEqual(
        L"mvp[3].debugName",
        FB2Shell::GetMvpPropertyDescriptorByIndex(3) != nullptr ? FB2Shell::GetMvpPropertyDescriptorByIndex(3)->debugName : nullptr,
        nullptr) && success;

    PROPERTYKEY propertyKey = PKEY_Null;
    FB2Shell::PropertyId propertyId = FB2Shell::PropertyId::Author;
    success = ExpectTrue(
        L"propertyKey.author.exists",
        FBShellModern::TryGetConfirmedPropertyKey(FB2Shell::PropertyId::Author, propertyKey)) && success;
    success = ExpectTrue(
        L"propertyKey.title.exists",
        FBShellModern::TryGetConfirmedPropertyKey(FB2Shell::PropertyId::Title, propertyKey)) && success;
    success = ExpectTrue(
        L"propertyKey.language.exists",
        FBShellModern::TryGetConfirmedPropertyKey(FB2Shell::PropertyId::Language, propertyKey)) && success;
    success = ExpectTrue(
        L"propertyKey.genre.absent",
        !FBShellModern::TryGetConfirmedPropertyKey(FB2Shell::PropertyId::Genre, propertyKey)) && success;
    success = ExpectTrue(
        L"propertyKey.keywords.absent",
        !FBShellModern::TryGetConfirmedPropertyKey(FB2Shell::PropertyId::Keywords, propertyKey)) && success;
    success = ExpectTrue(
        L"propertyKey.documentId.absent",
        !FBShellModern::TryGetConfirmedPropertyKey(FB2Shell::PropertyId::DocumentId, propertyKey)) && success;
    success = ExpectTrue(
        L"propertyKey.mvp[0].exists",
        FBShellModern::TryGetMvpPropertyKeyByIndex(0, propertyId, propertyKey) && propertyId == FB2Shell::PropertyId::Author) && success;
    success = ExpectTrue(
        L"propertyKey.mvp[1].exists",
        FBShellModern::TryGetMvpPropertyKeyByIndex(1, propertyId, propertyKey) && propertyId == FB2Shell::PropertyId::Title) && success;
    success = ExpectTrue(
        L"propertyKey.mvp[2].exists",
        FBShellModern::TryGetMvpPropertyKeyByIndex(2, propertyId, propertyKey) && propertyId == FB2Shell::PropertyId::Language) && success;
    success = ExpectTrue(
        L"propertyKey.mvp[3].exists",
        !FBShellModern::TryGetMvpPropertyKeyByIndex(3, propertyId, propertyKey)) && success;

    CString canonicalName;
    CString displayName;
    HRESULT hr = FBShellModern::GetConfirmedPropertyCanonicalName(FB2Shell::PropertyId::Author, canonicalName);
    success = ExpectTrue(L"propertyKey.author.canonical.hr", SUCCEEDED(hr)) && success;
    success = ExpectTextEqual(L"propertyKey.author.canonical.name", canonicalName, L"System.Author") && success;
    hr = FBShellModern::GetConfirmedPropertyDisplayName(FB2Shell::PropertyId::Author, displayName);
    success = ExpectTrue(L"propertyKey.author.displayName.hr", SUCCEEDED(hr)) && success;
    success = ExpectTrue(L"propertyKey.author.displayName.notEmpty", !displayName.IsEmpty()) && success;

    FBShellModern::Fb2PropertyStoreCache propertyStore;
    success = ExpectTrue(L"propertyStore.load", propertyStore.LoadFromMetadata(metadata)) && success;
    success = ExpectTrue(L"propertyStore.count", propertyStore.GetCount() == 9) && success;
    success = ExpectTrue(
        L"propertyStore.entry[0]",
        propertyStore.GetAt(0) != nullptr && propertyStore.GetAt(0)->propertyId == FB2Shell::PropertyId::Author) && success;
    success = ExpectTrue(
        L"propertyStore.entry[1]",
        propertyStore.GetAt(1) != nullptr && propertyStore.GetAt(1)->propertyId == FB2Shell::PropertyId::Title) && success;
    success = ExpectTrue(
        L"propertyStore.entry[2]",
        propertyStore.GetAt(2) != nullptr && propertyStore.GetAt(2)->propertyId == FB2Shell::PropertyId::Genre) && success;
    success = ExpectTrue(
        L"propertyStore.entry[3]",
        propertyStore.GetAt(3) != nullptr && propertyStore.GetAt(3)->propertyId == FB2Shell::PropertyId::Keywords) && success;
    success = ExpectTrue(
        L"propertyStore.entry[4]",
        propertyStore.GetAt(4) != nullptr && propertyStore.GetAt(4)->propertyId == FB2Shell::PropertyId::Language) && success;
    success = ExpectTrue(
        L"propertyStore.entry[5]",
        propertyStore.GetAt(5) != nullptr && propertyStore.GetAt(5)->propertyId == FB2Shell::PropertyId::Sequence) && success;
    success = ExpectTrue(
        L"propertyStore.entry[6]",
        propertyStore.GetAt(6) != nullptr && propertyStore.GetAt(6)->propertyId == FB2Shell::PropertyId::DocumentId) && success;
    success = ExpectTrue(
        L"propertyStore.entry[7]",
        propertyStore.GetAt(7) != nullptr && propertyStore.GetAt(7)->propertyId == FB2Shell::PropertyId::DocumentVersion) && success;
    success = ExpectTrue(
        L"propertyStore.entry[8]",
        propertyStore.GetAt(8) != nullptr && propertyStore.GetAt(8)->propertyId == FB2Shell::PropertyId::DocumentDate) && success;

    PROPVARIANT propertyValue;
    hr = propertyStore.GetValue(PKEY_Author, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.author.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.author.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"Codex Smoke") && success;
    ::PropVariantClear(&propertyValue);

    hr = propertyStore.GetValue(PKEY_Title, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.title.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.title.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"Metadata Smoke Test") && success;
    ::PropVariantClear(&propertyValue);

    hr = propertyStore.GetValue(PKEY_Language, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.language.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.language.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"ru") && success;
    ::PropVariantClear(&propertyValue);

    hr = propertyStore.GetValue(FBShellModern::PKEY_FBE_Genre, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.genre.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.genre.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"computers, reference") && success;
    ::PropVariantClear(&propertyValue);

    hr = propertyStore.GetValue(FBShellModern::PKEY_FBE_Keywords, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.keywords.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.keywords.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"alpha; beta; metadata") && success;
    ::PropVariantClear(&propertyValue);

    hr = propertyStore.GetValue(FBShellModern::PKEY_FBE_Sequence, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.sequence.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.sequence.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"Shell Series [2]") && success;
    ::PropVariantClear(&propertyValue);

    hr = propertyStore.GetValue(FBShellModern::PKEY_FBE_DocumentId, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.documentId.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.documentId.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"fb2-metadata-smoke-test") && success;
    ::PropVariantClear(&propertyValue);

    hr = propertyStore.GetValue(FBShellModern::PKEY_FBE_DocumentVersion, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.documentVersion.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.documentVersion.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"1.2") && success;
    ::PropVariantClear(&propertyValue);

    hr = propertyStore.GetValue(FBShellModern::PKEY_FBE_DocumentDate, propertyValue);
    success = ExpectHResultSuccess(L"propertyStore.documentDate.hr", hr) && success;
    success = ExpectTextEqual(
        L"propertyStore.documentDate.value",
        SUCCEEDED(hr) ? propertyValue.pwszVal : nullptr,
        L"12.06.2026") && success;
    ::PropVariantClear(&propertyValue);

    success = ExpectTextEqual(
        L"descriptor.author.debugName",
        FB2Shell::GetPropertyDebugName(FB2Shell::PropertyId::Author),
        L"author") && success;
    success = ExpectTextEqual(
        L"descriptor.author.fallbackDisplayName",
        FB2Shell::GetPropertyFallbackDisplayName(FB2Shell::PropertyId::Author),
        L"Автор") && success;
    success = ExpectTextEqual(
        L"descriptor.author.displayNameSource",
        FB2Shell::GetPropertyDisplayNameSourceName(FB2Shell::PropertyId::Author),
        L"windows-localized") && success;
    success = ExpectTextEqual(
        L"descriptor.author.windowsName",
        FB2Shell::GetPropertyWindowsCanonicalName(FB2Shell::PropertyId::Author),
        L"System.Author") && success;
    success = ExpectTextEqual(
        L"descriptor.author.mappingStatus",
        FB2Shell::GetPropertyMappingStatusName(FB2Shell::PropertyId::Author),
        L"confirmed") && success;
    success = FB2Shell::UsesWindowsProperty(FB2Shell::PropertyId::Author) && success;
    success = FB2Shell::IsConfirmedWindowsProperty(FB2Shell::PropertyId::Author) && success;
    success = FB2Shell::IsMvpWindowsProperty(FB2Shell::PropertyId::Author) && success;
    success = ExpectTextEqual(
        L"descriptor.keywords.mappingStatus",
        FB2Shell::GetPropertyMappingStatusName(FB2Shell::PropertyId::Keywords),
        L"fbe-specific") && success;
    success = ExpectTextEqual(
        L"descriptor.keywords.windowsName",
        FB2Shell::GetPropertyWindowsCanonicalName(FB2Shell::PropertyId::Keywords),
        nullptr) && success;
    success = (!FB2Shell::UsesWindowsProperty(FB2Shell::PropertyId::Keywords)) && success;
    success = (!FB2Shell::IsConfirmedWindowsProperty(FB2Shell::PropertyId::Keywords)) && success;
    success = (!FB2Shell::IsMvpWindowsProperty(FB2Shell::PropertyId::Keywords)) && success;
    success = ExpectTextEqual(
        L"descriptor.genre.mappingStatus",
        FB2Shell::GetPropertyMappingStatusName(FB2Shell::PropertyId::Genre),
        L"fbe-specific") && success;
    success = ExpectTextEqual(
        L"descriptor.genre.windowsName",
        FB2Shell::GetPropertyWindowsCanonicalName(FB2Shell::PropertyId::Genre),
        nullptr) && success;
    success = (!FB2Shell::UsesWindowsProperty(FB2Shell::PropertyId::Genre)) && success;
    success = (!FB2Shell::IsConfirmedWindowsProperty(FB2Shell::PropertyId::Genre)) && success;
    success = (!FB2Shell::IsMvpWindowsProperty(FB2Shell::PropertyId::Genre)) && success;
    success = ExpectTextEqual(
        L"descriptor.sequence.windowsName",
        FB2Shell::GetPropertyWindowsCanonicalName(FB2Shell::PropertyId::Sequence),
        nullptr) && success;
    success = ExpectTextEqual(
        L"descriptor.sequence.displayNameSource",
        FB2Shell::GetPropertyDisplayNameSourceName(FB2Shell::PropertyId::Sequence),
        L"fbe-fallback") && success;
    success = ExpectTextEqual(
        L"descriptor.sequence.mappingStatus",
        FB2Shell::GetPropertyMappingStatusName(FB2Shell::PropertyId::Sequence),
        L"fbe-specific") && success;
    success = (!FB2Shell::UsesWindowsProperty(FB2Shell::PropertyId::Sequence)) && success;
    success = (!FB2Shell::IsConfirmedWindowsProperty(FB2Shell::PropertyId::Sequence)) && success;
    success = (!FB2Shell::IsMvpWindowsProperty(FB2Shell::PropertyId::Sequence)) && success;
    success = ExpectTextEqual(
        L"descriptor.documentId.mappingStatus",
        FB2Shell::GetPropertyMappingStatusName(FB2Shell::PropertyId::DocumentId),
        L"fbe-specific") && success;
    success = ExpectTextEqual(
        L"descriptor.documentId.windowsName",
        FB2Shell::GetPropertyWindowsCanonicalName(FB2Shell::PropertyId::DocumentId),
        nullptr) && success;
    success = (!FB2Shell::UsesWindowsProperty(FB2Shell::PropertyId::DocumentId)) && success;
    success = (!FB2Shell::IsConfirmedWindowsProperty(FB2Shell::PropertyId::DocumentId)) && success;
    success = (!FB2Shell::IsMvpWindowsProperty(FB2Shell::PropertyId::DocumentId)) && success;
    success = ExpectTextEqual(
        L"descriptor.documentVersion.fallbackDisplayName",
        FB2Shell::GetPropertyFallbackDisplayName(FB2Shell::PropertyId::DocumentVersion),
        L"FBE:Версия документа") && success;
    success = ExpectTextEqual(
        L"descriptor.documentVersion.mappingStatus",
        FB2Shell::GetPropertyMappingStatusName(FB2Shell::PropertyId::DocumentVersion),
        L"fbe-specific") && success;
    success = ExpectTextEqual(
        L"descriptor.documentVersion.windowsName",
        FB2Shell::GetPropertyWindowsCanonicalName(FB2Shell::PropertyId::DocumentVersion),
        nullptr) && success;
    success = (!FB2Shell::UsesWindowsProperty(FB2Shell::PropertyId::DocumentVersion)) && success;
    success = (!FB2Shell::IsConfirmedWindowsProperty(FB2Shell::PropertyId::DocumentVersion)) && success;
    success = (!FB2Shell::IsMvpWindowsProperty(FB2Shell::PropertyId::DocumentVersion)) && success;
    success = ExpectTextEqual(
        L"descriptor.documentDate.mappingStatus",
        FB2Shell::GetPropertyMappingStatusName(FB2Shell::PropertyId::DocumentDate),
        L"fbe-specific") && success;
    success = ExpectTextEqual(
        L"descriptor.documentDate.windowsName",
        FB2Shell::GetPropertyWindowsCanonicalName(FB2Shell::PropertyId::DocumentDate),
        nullptr) && success;
    success = (!FB2Shell::UsesWindowsProperty(FB2Shell::PropertyId::DocumentDate)) && success;
    success = (!FB2Shell::IsConfirmedWindowsProperty(FB2Shell::PropertyId::DocumentDate)) && success;
    success = (!FB2Shell::IsMvpWindowsProperty(FB2Shell::PropertyId::DocumentDate)) && success;

    ::CoUninitialize();

    if (!success)
        return 2;

    return 0;
}
