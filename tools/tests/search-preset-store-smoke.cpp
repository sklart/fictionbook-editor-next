#include "../../src/fbe/stdafx.h"
#include "../../src/fbe/search/SearchPresetStore.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <sddl.h>
#pragma comment(lib, "advapi32.lib")

using namespace FbeSearchPresets;

namespace
{
CString TempDirectory()
{
    wchar_t root[MAX_PATH] = {};
    if (::GetTempPathW(_countof(root), root) == 0) return CString();
    wchar_t temporary[MAX_PATH] = {};
    if (::GetTempFileNameW(root, L"fsp", 0, temporary) == 0) return CString();
    ::DeleteFileW(temporary);
    if (!::CreateDirectoryW(temporary, NULL)) return CString();
    return CString(temporary);
}

void WriteUtf16(const CString& path, const CString& text)
{
    HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) throw 100;
    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(text.GetLength() * sizeof(wchar_t));
    const bool ok = ::WriteFile(file, static_cast<LPCWSTR>(text), bytes, &written, NULL) != FALSE && written == bytes;
    ::CloseHandle(file);
    if (!ok) throw 101;
}

std::vector<BYTE> ReadBytes(const CString& path)
{
    HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) throw 102;
    const DWORD size = ::GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE) { ::CloseHandle(file); throw 103; }
    std::vector<BYTE> bytes(size);
    DWORD read = 0;
    const bool ok = ::ReadFile(file, bytes.data(), size, &read, NULL) != FALSE && read == size;
    ::CloseHandle(file);
    if (!ok) throw 104;
    return bytes;
}

bool NoTemporaryFiles(const CString& directory)
{
    WIN32_FIND_DATAW found = {};
    HANDLE search = ::FindFirstFileW(directory + L"\\*.tmp", &found);
    if (search == INVALID_HANDLE_VALUE) return ::GetLastError() == ERROR_FILE_NOT_FOUND;
    ::FindClose(search);
    return false;
}

SearchPreset MakePreset(const wchar_t* id, SearchUiContext context, bool hasReplacement)
{
    SearchPreset preset;
    preset.id = id;
    preset.name = L"Имя & < > \" ' \U0001F642";
    preset.findText = L"строка & <tag> \" ' \r\nвторая";
    preset.hasReplacement = hasReplacement;
    preset.replacementText = L"";
    preset.regexp = true;
    preset.matchCase = true;
    preset.wholeWord = false;
    preset.unicodeProperties = context == SearchUiContext::Design;
    preset.context = context;
    return preset;
}

bool Same(const SearchPreset& left, const SearchPreset& right)
{
    return left.id == right.id && left.name == right.name && left.findText == right.findText &&
        left.hasReplacement == right.hasReplacement && left.replacementText == right.replacementText &&
        left.regexp == right.regexp && left.matchCase == right.matchCase &&
        left.wholeWord == right.wholeWord && left.unicodeProperties == right.unicodeProperties &&
        left.context == right.context && !left.builtIn && !right.builtIn;
}
}

int wmain()
{
    ::CoInitialize(NULL);
    int result = 0;
    CString directory = TempDirectory();
    if (directory.IsEmpty()) return 1;
    try
    {
        SearchPresetStore store(directory);
        std::vector<SearchPreset> loaded;
        if (!store.Load(loaded) || !loaded.empty()) result = 2; // empty store

        std::vector<SearchPreset> expected;
        expected.push_back(MakePreset(L"one", SearchUiContext::Design, true)); // empty replacement means delete
        expected.push_back(MakePreset(L"two", SearchUiContext::Source, false)); // find-only
        if (result == 0 && !store.Save(expected)) result = 3;
        HANDLE saved = ::CreateFileW(store.FilePath(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        wchar_t bom = 0; DWORD bomRead = 0;
        const bool hasUtf16Bom = saved != INVALID_HANDLE_VALUE && ::ReadFile(saved, &bom, sizeof(bom), &bomRead, NULL) != FALSE && bomRead == sizeof(bom) && bom == 0xFEFF;
        if (saved != INVALID_HANDLE_VALUE) ::CloseHandle(saved);
        if (result == 0 && !hasUtf16Bom) result = 13;
        loaded.clear();
        if (result == 0 && (!store.Load(loaded) || loaded.size() != expected.size() || !Same(loaded[0], expected[0]) || !Same(loaded[1], expected[1]))) result = 4;

        WriteUtf16(store.FilePath(), L"<SearchTemplates version=\"1\"><Template id=\"bad\" /></SearchTemplates>");
        loaded.clear();
        if (result == 0 && (!store.Load(loaded) || !loaded.empty())) result = 5; // malformed Template is skipped safely

        WriteUtf16(store.FilePath(), L"<SearchTemplates version=\"1\"><Template>");
        if (result == 0 && store.Load(loaded)) result = 6; // malformed XML

        WriteUtf16(store.FilePath(), L"<SearchTemplates version=\"2\" />");
        if (result == 0 && store.Load(loaded)) result = 7; // unsupported version
        WriteUtf16(store.FilePath(), L"<!DOCTYPE SearchTemplates [<!ENTITY e SYSTEM 'file:///not-allowed'>]><SearchTemplates version=\"1\"><Template id=\"a\" name=\"A\" context=\"design\" regexp=\"1\" matchCase=\"0\" wholeWord=\"0\" unicodeProperties=\"0\" hasReplacement=\"0\"><Find>&e;</Find></Template></SearchTemplates>");
        if (result == 0 && store.Load(loaded)) result = 8; // DTD and external entities are prohibited

        WriteUtf16(store.FilePath(), L"<SearchTemplates version=\"1\"><Template id=\"a\" name=\"A\" context=\"design\" regexp=\"1\" matchCase=\"0\" wholeWord=\"0\" unicodeProperties=\"0\" hasReplacement=\"0\"><Find>x</Find></Template><Template id=\"a\" name=\"B\" context=\"design\" regexp=\"1\" matchCase=\"0\" wholeWord=\"0\" unicodeProperties=\"0\" hasReplacement=\"0\"><Find>y</Find></Template></SearchTemplates>");
        loaded.clear();
        if (result == 0 && (!store.Load(loaded) || loaded.size() != 1 || loaded[0].findText != L"x")) result = 10; // duplicate id skipped

        std::vector<SearchPreset> tooMany;
        for (int index = 0; index != 257; ++index) { CString id; id.Format(L"id-%d", index); tooMany.push_back(MakePreset(id, SearchUiContext::Design, false)); }
        if (result == 0 && store.Save(tooMany)) result = 10;

        if (result == 0 && !store.Save(expected)) result = 10;
#if defined(FBE_SEARCH_PRESET_STORE_TESTING)
        expected[0].name = L"new value";
        store.FailNextWriteForTesting();
        if (result == 0 && store.Save(expected)) result = 10;
        loaded.clear();
        if (result == 0 && (!store.Load(loaded) || loaded.size() != 2 || loaded[0].name == L"new value")) result = 11; // old good file survives failed atomic write
#endif

        if (result == 0 && !store.Save(expected)) result = 14;
        std::vector<SearchPreset> oversized;
        for (int index = 0; index != 256; ++index)
        {
            CString id;
            id.Format(L"oversized-%d", index);
            SearchPreset preset = MakePreset(id, SearchUiContext::Design, false);
            preset.findText = CString(L'x', 8192);
            oversized.push_back(preset);
        }
        if (result == 0 && store.Save(oversized)) result = 15;
        loaded.clear();
        if (result == 0 && (!store.Load(loaded) || loaded.size() != expected.size())) result = 16; // oversized save preserves old file
        // Permanent denials still fail, preserve every byte, and remove the temporary file.
        if (result == 0)
        {
            const std::vector<BYTE> before = ReadBytes(store.FilePath());
            std::vector<SearchPreset> blocked = expected;
            blocked[0].name = L"must not replace good data";
            const DWORD attributes = ::GetFileAttributesW(store.FilePath());
            if (attributes == INVALID_FILE_ATTRIBUTES ||
                !::SetFileAttributesW(store.FilePath(), attributes | FILE_ATTRIBUTE_READONLY)) result = 17;
            if (result == 0)
            {
                const bool savedReadOnly = store.Save(blocked);
                const bool restored = ::SetFileAttributesW(store.FilePath(), attributes) != FALSE;
                if (savedReadOnly || !restored) result = 18;
                if (result == 0 && (ReadBytes(store.FilePath()) != before ||
                    !NoTemporaryFiles(directory))) result = 19;
            }
            HANDLE lock = ::CreateFileW(store.FilePath(), GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (result == 0 && lock == INVALID_HANDLE_VALUE) result = 20;
            const bool savedLocked = result == 0 && store.Save(blocked);
            if (lock != INVALID_HANDLE_VALUE) ::CloseHandle(lock);
            if (result == 0 && savedLocked) result = 21;
            if (result == 0 && (ReadBytes(store.FilePath()) != before ||
                !NoTemporaryFiles(directory))) result = 22;
            loaded.clear();
            if (result == 0 && (!store.Load(loaded) || loaded.size() != expected.size() ||
                !Same(loaded[0], expected[0]) || !Same(loaded[1], expected[1]))) result = 23;
            if (result == 0 && !store.Save(expected)) result = 24; // replacement works after unlocking

            // A real reader can outlive the former 150 ms retry budget. Saving
            // must wait for its release, while the permanent-lock case still fails.
            HANDLE transient = ::CreateFileW(store.FilePath(), GENERIC_READ, FILE_SHARE_READ, NULL,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            const CString probe = store.FilePath() + L".probe";
            WriteUtf16(probe, L"probe");
            const bool moved = ::MoveFileExW(probe, store.FilePath(), MOVEFILE_REPLACE_EXISTING);
            const DWORD denied = ::GetLastError();
            ::DeleteFileW(probe);
            if (transient == INVALID_HANDLE_VALUE || moved ||
                (denied != ERROR_ACCESS_DENIED && denied != ERROR_SHARING_VIOLATION && denied != ERROR_LOCK_VIOLATION)) result = 25;
            std::thread unlock([transient]() { ::Sleep(250); if (transient != INVALID_HANDLE_VALUE) ::CloseHandle(transient); });
            const bool savedTransient = result == 0 && store.Save(blocked);
            unlock.join();
            if (result == 0 && !savedTransient) result = 26;
            loaded.clear();
            if (result == 0 && (!store.Load(loaded) || loaded.size() != blocked.size() || !Same(loaded[0], blocked[0]))) result = 27;

            // A real directory ACL denial must not be converted into successful saving.
            const std::vector<BYTE> beforeAcl = ReadBytes(store.FilePath());
            DWORD needed = 0;
            ::GetFileSecurityW(directory, DACL_SECURITY_INFORMATION, NULL, 0, &needed);
            std::vector<BYTE> security(needed);
            PSECURITY_DESCRIPTOR deniedSecurity = NULL;
            if (needed == 0 || !::GetFileSecurityW(directory, DACL_SECURITY_INFORMATION,
                security.data(), needed, &needed) ||
                !::ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(D;;0x2;;;WD)(A;;FA;;;OW)",
                    SDDL_REVISION_1, &deniedSecurity, NULL)) result = 28;
            if (result == 0)
            {
                SECURITY_DESCRIPTOR_CONTROL control = 0;
                DWORD revision = 0;
                const bool gotControl = ::GetSecurityDescriptorControl(security.data(), &control, &revision) != FALSE;
                const bool applied = gotControl && ::SetFileSecurityW(directory,
                    DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, deniedSecurity) != FALSE;
                const bool savedDenied = applied && store.Save(expected);
                const DWORD accessError = ::GetLastError();
                const bool restored = !applied || ::SetFileSecurityW(directory, DACL_SECURITY_INFORMATION |
                    ((control & SE_DACL_PROTECTED) ? PROTECTED_DACL_SECURITY_INFORMATION : UNPROTECTED_DACL_SECURITY_INFORMATION),
                    security.data()) != FALSE;
                if (!applied || savedDenied || accessError != ERROR_ACCESS_DENIED || !restored) result = 29;
            }
            if (deniedSecurity != NULL) ::LocalFree(deniedSecurity);
            if (result == 0 && (ReadBytes(store.FilePath()) != beforeAcl || !NoTemporaryFiles(directory))) result = 30;

            // Independent store instances race in the same directory; whole XML always wins.
            std::atomic<bool> start(false);
            std::atomic<unsigned> failures(0);
            std::vector<std::thread> writers;
            if (result == 0)
            {
                for (unsigned writer = 0; writer != 4; ++writer)
                    writers.emplace_back([&, writer]() {
                        SearchPresetStore concurrent(directory);
                        std::vector<SearchPreset> values = expected;
                        values[0].name.Format(L"writer-%u", writer);
                        while (!start.load()) ::SwitchToThread();
                        for (unsigned attempt = 0; attempt != 4; ++attempt)
                            if (!concurrent.Save(values))
                            {
                                const DWORD error = ::GetLastError();
                                ++failures;
                                std::wcerr << L"Concurrent Save error=" << error << std::endl;
                            }
                    });
                start.store(true);
                for (auto& writer : writers) writer.join();
                if (failures.load() != 0) result = 31;
                loaded.clear();
                if (result == 0 && (!store.Load(loaded) || loaded.size() != expected.size() ||
                    loaded[0].name.Left(7) != L"writer-" || !Same(loaded[1], expected[1]) ||
                    !NoTemporaryFiles(directory))) result = 32;
            }
        }
    }
    catch (...) { result = 12; }
    ::DeleteFileW(directory + L"\\SearchTemplates.xml");
    ::DeleteFileW(directory + L"\\SearchTemplates.xml.tmp");
    ::RemoveDirectoryW(directory);
    ::CoUninitialize();
    if (result != 0) std::wcerr << L"SearchPresetStore smoke failed: " << result << std::endl;
    return result;
}
