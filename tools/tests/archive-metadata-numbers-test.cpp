#include "../../src/fbe/stdafx.h"
#include "../../src/fbe/document/ArchiveRecentDocuments.h"
#include "../../src/fbe/recovery/RecoveryStore.h"
#include "../../src/common/DeploymentContext.h"
#include <limits>

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) { fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}

void WriteSidecar(const CString& path, const std::vector<CString>& fields) {
    CString text;
    for (const auto& field : fields) text += field + L"\r\n";
    const DWORD size = static_cast<DWORD>(text.GetLength() * sizeof(wchar_t));
    HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { Check(false, "create isolated sidecar"); return; }
    DWORD written = 0;
    Check(::WriteFile(file, text.GetString(), size, &written, nullptr) && written == size, "write isolated sidecar");
    ::CloseHandle(file);
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3 || wcscmp(argv[1], L"--installed") != 0) return 2;
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", L"1");
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SETTINGS_DIRECTORY", argv[2]);
    const std::wstring directory = DeploymentContext::RecoveryDirectory();
    Check(directory == std::wstring(argv[2]) + L"\\Recovery\\", "isolated recovery directory");
    if (failures) return 2;
    Check(::CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "create fixture directory");
    if (failures) return 2;

    unsigned int value = 17;
    Check(FbeArchiveRecentDocuments::ParseUnsigned(L"0", value) && value == 0, "zero occurrence");
    Check(FbeArchiveRecentDocuments::ParseUnsigned(L"4294967295", value) && value == UINT_MAX, "maximum occurrence");
    const wchar_t* invalid[] = { L"", L"9999999999999999999999999999", L"-1", L"+1", L" 1", L"1 ", L"1x", L"1.5" };
    for (const wchar_t* text : invalid) {
        value = 17;
        Check(!FbeArchiveRecentDocuments::ParseUnsigned(text, value) && value == 17, "reject invalid occurrence without changing output");
    }
    value = 17;
    Check(!FbeArchiveRecentDocuments::ParseUnsigned(L"4294967296", value) && value == 17, "reject 32-bit overflow");
    const CString embedded(L"1\0x", 3);
    value = 17;
    Check(!FbeArchiveRecentDocuments::ParseUnsigned(embedded, value) && value == 17, "reject embedded null");

    DocumentLocation original;
    original.containerKind = DocumentContainerKind::Zip;
    original.storagePath = L"C:\\fixture\\book.zip";
    original.entryPath = L"book.fb2";
    original.documentType = FictionBookFileType::Fb2;
    original.entryOccurrence = UINT_MAX;
    original.containerLastWriteTime = (std::numeric_limits<unsigned __int64>::max)();
    original.containerFileSize = original.containerLastWriteTime;
    DocumentLocation parsed;
    Check(FbeArchiveRecentDocuments::ParseKey(FbeArchiveRecentDocuments::Key(original), parsed) &&
        FbeArchiveRecentDocuments::SameIdentity(original, parsed), "MRU maximum occurrence round trip");
    Check(!FbeArchiveRecentDocuments::ParseKey(L"\x1e" L"archive\t1\t4294967296\tC:\\fixture\\book.zip\tbook.fb2", parsed), "MRU key overflow");

    FbeRecovery::RecoveryStore store;
    Check(store.WriteArchiveLocation(original), "write valid recovery metadata");
    Check(store.ReadArchiveLocation(parsed) && FbeArchiveRecentDocuments::SameIdentity(original, parsed) &&
        parsed.containerLastWriteTime == original.containerLastWriteTime && parsed.containerFileSize == original.containerFileSize,
        "recovery maximum values round trip");
    const std::vector<CString> valid = { L"2", L"1", L"0", L"0", L"0", L"18446744073709551615", L"C:\\fixture\\book.zip", L"book.fb2" };
    for (size_t field = 1; field <= 5; ++field) {
        for (const wchar_t* text : invalid) {
            auto fields = valid;
            fields[field] = text;
            WriteSidecar(store.ArchiveSidecarPath(), fields);
            Check(!store.ReadArchiveLocation(parsed) && parsed.storagePath.IsEmpty() && !parsed.IsArchive(), "reject malformed recovery metadata atomically");
        }
    }
    for (size_t field : { size_t(4), size_t(5) }) {
        auto fields = valid;
        fields[field] = L"18446744073709551616";
        WriteSidecar(store.ArchiveSidecarPath(), fields);
        Check(!store.ReadArchiveLocation(parsed), "reject 64-bit overflow");
    }
    auto fields = valid;
    fields[2] = L"4294967296";
    WriteSidecar(store.ArchiveSidecarPath(), fields);
    Check(!store.ReadArchiveLocation(parsed), "reject recovery occurrence overflow");
    WriteSidecar(store.ArchiveSidecarPath(), valid);
    Check(store.ReadArchiveLocation(parsed) && parsed.entryOccurrence == 0 && parsed.containerLastWriteTime == 0 &&
        parsed.containerFileSize == (std::numeric_limits<unsigned __int64>::max)(), "read valid zero and maximum metadata after invalid records");
    ::DeleteFileW(store.ArchiveSidecarPath());
    ::RemoveDirectoryW(directory.c_str());
    if (failures) { fprintf(stderr, "%d checks failed\n", failures); return 1; }
    puts("Archive metadata numeric regression passed.");
    return 0;
}
