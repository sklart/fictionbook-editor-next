// Reuse the existing COM host/snapshot fixture; the plug-ins are still loaded
// by DllGetClassObject, without registering them or changing their ABI.
#define wmain ExistingExportRuntimeMain
#include "export-html-v2-runtime-harness.cpp"
#undef wmain
#include "../../src/common/PluginSettingsStore.h"
#include <atlbase.h>

static int dialogCount = 0;
static HWND posted = NULL;
static BOOL CALLBACK AcceptDialog(HWND window, LPARAM) {
    if(window != posted && ::GetDlgItem(window, IDOK)) {
        posted = window; ++dialogCount;
        ::ShowWindow(window, SW_SHOWNORMAL);
        ::PostMessageW(window, WM_COMMAND, IDOK, 0);
    }
    return TRUE;
}
static void CALLBACK AcceptTimer(HWND, UINT, UINT_PTR, DWORD) { ::EnumThreadWindows(::GetCurrentThreadId(), AcceptDialog, 0); }

static int Dialogs(const wchar_t* directory) {
    const wchar_t* names[] = {L"ExportHTML", L"ExportDOCX", L"ImportEPUB", L"ExportEPUB"};
    const wchar_t* ids[] = {L"{C3098839-EF69-4DE5-B27D-1E80051CA843}", L"{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}",
        L"{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}", L"{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}"};
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", L"1");
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_PLUGIN_SETTINGS", L"1");
    Host* host = new Host; Snapshot* snapshot = new Snapshot;
    for(int index = 0; index < 4; ++index) {
        const std::wstring dll = std::wstring(directory) + L"\\" + names[index] + L".dll";
        HMODULE module = ::LoadLibraryExW(dll.c_str(), NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module) return Die(L"load DLL", HRESULT_FROM_WIN32(::GetLastError()));
        CLSID clsid = {}; ::CLSIDFromString(ids[index], &clsid);
        GetClassObject get = reinterpret_cast<GetClassObject>(::GetProcAddress(module, "DllGetClassObject"));
        CComPtr<IClassFactory> factory; CComPtr<IUnknown> plugin;
        HRESULT hr = get ? get(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory)) : E_NOINTERFACE;
        if(SUCCEEDED(hr)) hr = factory->CreateInstance(NULL, IID_IUnknown, reinterpret_cast<void**>(&plugin));
        if(FAILED(hr)) return Die(L"local activation", hr);
        posted = NULL; const int before = dialogCount;
        std::wcout << names[index] << L" opening settings" << std::endl;
        const UINT_PTR timer = ::SetTimer(NULL, 0, 200, AcceptTimer);
        if(!timer) return 21;
        if(index == 2) {
            CComPtr<IFBEImportPlugin2> importer; hr = plugin->QueryInterface(IID_IFBEImportPlugin2, reinterpret_cast<void**>(&importer));
            CComBSTR name; CComPtr<IStream> stream;
            if(SUCCEEDED(hr)) hr = importer->Import(host, &name, &stream);
        } else {
            CComPtr<IFBEExportPlugin2> exporter; hr = plugin->QueryInterface(IID_IFBEExportPlugin2, reinterpret_cast<void**>(&exporter));
            if(SUCCEEDED(hr)) hr = Call(exporter, host, snapshot);
        }
        ::KillTimer(NULL, timer);
        if(FAILED(hr) || dialogCount != before + 1) return Die(names[index], FAILED(hr) ? hr : E_FAIL);
        std::wcout << names[index] << L" settings dialog/save: PASS\n";
        plugin.Release(); factory.Release(); ::FreeLibrary(module);
    }
    host->Release(); snapshot->Release(); return 0;
}

static int StoreChecks() {
    using namespace FbePluginSettings;
    const wchar_t* key = L"Software\\FBETeam\\PluginSettingsRuntimeFixture";
    Store first(L"store-fixture", key);
    if(FAILED(first.Error()) || FAILED(first.SetDWORDValue(L"Number", 42)) || FAILED(first.SetStringValue(L"Text", L"Unicode \x0416 \" \\ \n")) || FAILED(first.Commit())) return 31;
    Store loaded(L"store-fixture", key);
    if(ReadDword(loaded, L"nUmBeR", 0) != 42 || ReadString(loaded, L"Text") != L"Unicode \x0416 \" \\ \n") return 32;
    Store stale(L"store-fixture", key);
    loaded.SetDWORDValue(L"Number", 43); if(FAILED(loaded.Commit())) return 33;
    stale.SetDWORDValue(L"Number", 44);
    if(stale.Commit() != HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH)) return 34;
    Store locked(L"store-fixture", key);
    CHandle lock(::CreateFileW(locked.Path().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
    locked.SetDWORDValue(L"Number", 45); if(SUCCEEDED(locked.Commit())) return 35;
    lock.Close();
    Store preserved(L"store-fixture", key); if(ReadDword(preserved, L"Number", 0) != 43) return 36;
    if(!::SetFileAttributesW(preserved.Path().c_str(), FILE_ATTRIBUTE_READONLY)) return 41;
    Store denied(L"store-fixture", key); denied.SetDWORDValue(L"Number", 46);
    const HRESULT deniedResult = denied.Commit();
    ::SetFileAttributesW(preserved.Path().c_str(), FILE_ATTRIBUTE_NORMAL);
    if(SUCCEEDED(deniedResult)) return 42;
    Store afterDenied(L"store-fixture", key); if(ReadDword(afterDenied, L"Number", 0) != 43) return 43;
    wchar_t executable[MAX_PATH] = {};
    if(!::GetModuleFileNameW(NULL, executable, _countof(executable))) return 37;
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --portable interrupt";
    STARTUPINFOW startup = {}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process = {};
    if(!::CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) return 37;
    ::CloseHandle(process.hThread);
    const DWORD wait = ::WaitForSingleObject(process.hProcess, 10000); DWORD code = 1;
    if(wait != WAIT_OBJECT_0) ::TerminateProcess(process.hProcess, 1);
    else ::GetExitCodeProcess(process.hProcess, &code);
    ::CloseHandle(process.hProcess);
    if(code != 0) return 37;
    Store afterInterruption(L"store-fixture", key); if(ReadDword(afterInterruption, L"Number", 0) != 43) return 38;
    CHandle corrupt(::CreateFileW(preserved.Path().c_str(), GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, 0, NULL));
    if(corrupt == INVALID_HANDLE_VALUE) return 39;
    const char invalid[] = "{broken"; DWORD written = 0; ::WriteFile(corrupt, invalid, sizeof(invalid) - 1, &written, NULL); corrupt.Close();
    Store bad(L"store-fixture", key); if(SUCCEEDED(bad.Error()) || SUCCEEDED(bad.Commit())) return 40;
    std::wcout << L"Store Unicode/atomic/stale/locked/corrupt/interrupted: PASS\n";
    return 0;
}

static int Conversions(const wchar_t* directory) {
    const wchar_t* names[] = {L"ExportHTML", L"ExportDOCX", L"ExportEPUB", L"ImportEPUB"};
    const wchar_t* ids[] = {L"{C3098839-EF69-4DE5-B27D-1E80051CA843}", L"{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}",
        L"{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}", L"{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}"};
    const wchar_t* scenarios[] = {L"export-html", L"export-docx", L"export-epub", L"import-epub"};
    const wchar_t* paths[] = {L"FBE_NEXT_TEST_EXPORT_HTML_PATH", L"FBE_NEXT_TEST_EXPORT_DOCX_PATH", L"FBE_NEXT_TEST_EXPORT_EPUB_PATH"};
    const wchar_t* files[] = {L"settings-probe.html", L"settings-probe.docx", L"settings-probe.epub"};
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", L"1");
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_PLUGIN_SETTINGS", NULL);
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_EXPORT_HTML_MODE", L"1");
    for(int index = 0; index < 4; ++index) {
        ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_SCENARIO", scenarios[index]);
        const std::wstring dll = std::wstring(directory) + L"\\" + names[index] + L".dll";
        HMODULE module = ::LoadLibraryExW(dll.c_str(), NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module) return 50;
        CLSID clsid = {}; ::CLSIDFromString(ids[index], &clsid);
        GetClassObject get = reinterpret_cast<GetClassObject>(::GetProcAddress(module, "DllGetClassObject"));
        CComPtr<IClassFactory> factory; CComPtr<IUnknown> plugin;
        HRESULT hr = get ? get(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory)) : E_NOINTERFACE;
        if(SUCCEEDED(hr)) hr = factory->CreateInstance(NULL, IID_IUnknown, reinterpret_cast<void**>(&plugin));
        if(FAILED(hr)) return Die(L"conversion activation", hr);
        Host* host = new Host;
        if(index < 3) {
            const std::wstring file = DeploymentContext::ExecutableDirectory() + files[index];
            ::SetEnvironmentVariableW(paths[index], file.c_str());
            CComPtr<IFBEExportPlugin2> exporter; hr = plugin->QueryInterface(IID_IFBEExportPlugin2, reinterpret_cast<void**>(&exporter));
            Snapshot* snapshot = new Snapshot;
            if(SUCCEEDED(hr)) hr = Call(exporter, host, snapshot);
            snapshot->Release();
            if(SUCCEEDED(hr) && FileSha256(file).size() != 32) hr = E_FAIL;
        } else {
            const std::wstring epub = DeploymentContext::ExecutableDirectory() + files[2];
            ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_IMPORT_EPUB_PATH", epub.c_str());
            CComPtr<IFBEImportPlugin2> importer; hr = plugin->QueryInterface(IID_IFBEImportPlugin2, reinterpret_cast<void**>(&importer));
            CComBSTR name; CComPtr<IStream> stream;
            if(SUCCEEDED(hr)) hr = importer->Import(host, &name, &stream);
            STATSTG stat = {};
            if(SUCCEEDED(hr) && (!name || !stream || FAILED(stream->Stat(&stat, STATFLAG_NONAME)) || !stat.cbSize.QuadPart)) hr = E_FAIL;
        }
        host->Release();
        if(FAILED(hr)) return Die(names[index], hr);
        std::wcout << names[index] << L" real conversion: PASS" << std::endl;
        plugin.Release(); factory.Release(); ::FreeLibrary(module);
    }
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    if(argc < 3) return 2;
    if(FAILED(::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) return 3;
    CRegKey redirected;
    if(wcscmp(argv[1], L"--installed") == 0) {
        wchar_t fixture[512] = {};
        if(!::GetEnvironmentVariableW(L"FBE_NEXT_TEST_REGISTRY_ROOT", fixture, _countof(fixture))) return 5;
        if(redirected.Create(HKEY_CURRENT_USER, fixture) != ERROR_SUCCESS || ::RegOverridePredefKey(HKEY_CURRENT_USER, redirected) != ERROR_SUCCESS) return 6;
    }
    int result = 0;
    if(wcscmp(argv[2], L"interrupt") == 0) {
        FbePluginSettings::Store store(L"store-fixture", L"Software\\FBETeam\\PluginSettingsRuntimeFixture");
        FbeExport::FileTransaction transaction; std::wstring staged;
        if(FAILED(transaction.Stage(store.Path(), staged))) return 7;
        CHandle partial(::CreateFileW(staged.c_str(), GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, 0, NULL));
        DWORD written = 0; const char incomplete[] = "{interrupted";
        if(partial == INVALID_HANDLE_VALUE || !::WriteFile(partial, incomplete, sizeof(incomplete) - 1, &written, NULL)) return 8;
        ::FlushFileBuffers(partial);
        ::ExitProcess(0); // Deliberately skip cleanup and Commit, as after a crash.
    }
    if(wcscmp(argv[2], L"store") == 0) result = StoreChecks();
    else if(argc == 4 && wcscmp(argv[2], L"dialogs") == 0) result = Dialogs(argv[3]);
    else if(argc == 4 && wcscmp(argv[2], L"conversions") == 0) result = Conversions(argv[3]);
    else result = 4;
    if(redirected.m_hKey) ::RegOverridePredefKey(HKEY_CURRENT_USER, NULL);
    ::CoUninitialize(); return result;
}
