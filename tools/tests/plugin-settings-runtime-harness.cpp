// Reuse the existing COM host/snapshot fixture; the plug-ins are still loaded
// by DllGetClassObject, without registering them or changing their ABI.
#define wmain ExistingExportRuntimeMain
#include "export-html-v2-runtime-harness.cpp"
#undef wmain
static unsigned replacementFailures = 0;
static BOOL WINAPI ControlledReplaceFile(LPCWSTR target, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved) {
    if(replacementFailures) { --replacementFailures; ::SetLastError(ERROR_UNABLE_TO_REMOVE_REPLACED); return FALSE; }
    return ::ReplaceFileW(target, replacement, backup, flags, exclude, reserved);
}
#define ReplaceFileW ControlledReplaceFile
#include "../../src/common/PluginSettingsStore.h"
#undef ReplaceFileW
#include <atlbase.h>
#include "../../src/fbe/testing/PluginOptionsDialogProbe.h"

static int peerIndex = -1;
static std::wstring peerDirectory;
static bool SavePeer() {
    wchar_t executable[MAX_PATH] = {}; if(!::GetModuleFileNameW(NULL, executable, _countof(executable))) return false;
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --portable ui-peer" + std::to_wstring(peerIndex) + L" \"" + peerDirectory + L"\"";
    STARTUPINFOW startup = {}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process = {};
    if(!::CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) return false;
    ::CloseHandle(process.hThread); const DWORD wait = ::WaitForSingleObject(process.hProcess, 20000); DWORD code = 1;
    if(wait == WAIT_OBJECT_0) ::GetExitCodeProcess(process.hProcess, &code); else ::TerminateProcess(process.hProcess, 1);
    ::CloseHandle(process.hProcess); return code == 0;
}
static int Dialogs(const wchar_t* directory, const std::wstring& scenario = L"", int only = -1) {
    const wchar_t* names[] = {L"ExportHTML", L"ExportDOCX", L"ImportEPUB", L"ExportEPUB"};
    const wchar_t* ids[] = {L"{C3098839-EF69-4DE5-B27D-1E80051CA843}", L"{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}",
        L"{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}", L"{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}"};
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_MODE", L"1");
    ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_PLUGIN_SETTINGS", L"1");
    Host* host = new Host; Snapshot* snapshot = new Snapshot;
    for(int index = 0; index < 4; ++index) {
        if(only >= 0 && index != only) continue;
        const bool fileDialog = scenario.find(L"ui-file-") == 0;
        if(fileDialog && index != 2) continue;
        ::SetEnvironmentVariableW(L"FBE_NEXT_TEST_PLUGIN_FILE_DIALOG", fileDialog ? L"1" : NULL);
        if((scenario == L"ui-defaults-cancel" || scenario == L"ui-default" || scenario == L"ui-compat" || scenario == L"ui-rich") && index < 2) continue;
        if((scenario == L"ui-compat" || scenario == L"ui-rich") && index != 3) continue;
        const std::wstring dll = std::wstring(directory) + L"\\" + names[index] + L".dll";
        HMODULE module = ::LoadLibraryExW(dll.c_str(), NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module) return Die(L"load DLL", HRESULT_FROM_WIN32(::GetLastError()));
        CLSID clsid = {}; ::CLSIDFromString(ids[index], &clsid);
        GetClassObject get = reinterpret_cast<GetClassObject>(::GetProcAddress(module, "DllGetClassObject"));
        CComPtr<IClassFactory> factory; CComPtr<IUnknown> plugin;
        HRESULT hr = get ? get(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory)) : E_NOINTERFACE;
        if(SUCCEEDED(hr)) hr = factory->CreateInstance(NULL, IID_IUnknown, reinterpret_cast<void**>(&plugin));
        if(FAILED(hr)) return Die(L"local activation", hr);
        std::wcout << names[index] << L" opening settings" << std::endl;
        const bool advanced = true;
        PluginOptionsProbe::Action action = scenario.empty() ? PluginOptionsProbe::Action::Inspect : PluginOptionsProbe::Action::EditSave;
        if(scenario == L"ui-cancel" || scenario == L"ui-file-cancel") action = PluginOptionsProbe::Action::EditCancel;
        else if(scenario == L"ui-defaults-cancel") action = PluginOptionsProbe::Action::DefaultsCancel;
        else if(scenario == L"ui-default") action = PluginOptionsProbe::Action::PresetDefault;
        else if(scenario == L"ui-compat") action = PluginOptionsProbe::Action::PresetCompat;
        else if(scenario == L"ui-rich") action = PluginOptionsProbe::Action::PresetRich;
        else if(scenario == L"ui-readonly" || scenario == L"ui-file-readonly") action = PluginOptionsProbe::Action::ReadOnly;
        else if(scenario == L"ui-conflict") action = PluginOptionsProbe::Action::Conflict;
        else if(scenario == L"ui-corrupt") action = PluginOptionsProbe::Action::Corrupt;
        else if(scenario == L"ui-position-default") action = PluginOptionsProbe::Action::PositionDefault;
        else if(scenario == L"ui-position-move") action = PluginOptionsProbe::Action::PositionMove;
        else if(scenario == L"ui-position-verify") action = PluginOptionsProbe::Action::PositionVerify;
        const wchar_t* profiles[] = {L"export-html", L"export-docx", L"import-epub", L"export-epub"};
        const std::wstring profile = DeploymentContext::SettingsDirectory() + L"Plugins\\" + profiles[index] + L".json";
        if(advanced) {
            peerIndex = index; peerDirectory = directory;
            PluginOptionsProbe::Start(index, action, L"", std::wstring(names[index]) + scenario, profile);
            if(scenario == L"ui-import-separator" && index == 2) PluginOptionsProbe::targetControlId = 2026;
            if(scenario == L"ui-import-title" && index == 2) PluginOptionsProbe::targetControlId = 2027;
            if(scenario == L"ui-import-unify-images" && index == 2) PluginOptionsProbe::targetControlId = 2028;
            if(scenario == L"ui-import-cover-name" && index == 2) PluginOptionsProbe::targetControlId = 2029;
            PluginOptionsProbe::peerSave = scenario == L"ui-conflict" ? SavePeer : NULL;
        }
        const UINT_PTR timer = ::SetTimer(NULL, 0, 80, PluginOptionsProbe::Timer);
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
        if(advanced) PluginOptionsProbe::Stop();
        if(advanced) {
            const bool cancelled = fileDialog || PluginOptionsProbe::PositionProbe() || action == PluginOptionsProbe::Action::EditCancel || action == PluginOptionsProbe::Action::DefaultsCancel ||
                action == PluginOptionsProbe::Action::ReadOnly || action == PluginOptionsProbe::Action::Conflict || action == PluginOptionsProbe::Action::Corrupt;
            if(!PluginOptionsProbe::Passed() || (cancelled ? hr != HRESULT_FROM_WIN32(ERROR_CANCELLED) : FAILED(hr))) {
                std::wcerr << L"UI probe failed line=" << PluginOptionsProbe::failedLine << L" pages=" << PluginOptionsProbe::pages << L" stage=" << PluginOptionsProbe::stage << L" error=" << PluginOptionsProbe::errorSeen << std::endl;
                const auto& text = PluginOptionsProbe::errorText;
                const int length = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
                if(length) {
                    std::string utf8(static_cast<size_t>(length), '\0');
                    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &utf8[0], length, NULL, NULL);
                    DWORD written = 0; ::WriteFile(::GetStdHandle(STD_ERROR_HANDLE), utf8.data(), static_cast<DWORD>(utf8.size()), &written, NULL);
                }
                return Die(names[index], FAILED(hr) ? hr : E_FAIL);
            }
        }
        std::wcout << names[index] << L" settings dialog/save: PASS\n";
        plugin.Release(); factory.Release(); ::FreeLibrary(module);
    }
    host->Release(); snapshot->Release(); return 0;
}

static int ReplacementChecks() {
    using namespace FbePluginSettings;
    Store initial(L"replace-controlled", L"Software\\FBETeam\\PluginReplaceControlledFixture");
    initial.SetDWORDValue(L"Number", 1); if(FAILED(initial.Commit())) return 48;
    Store transient(L"replace-controlled", L"Software\\FBETeam\\PluginReplaceControlledFixture");
    transient.SetDWORDValue(L"Number", 2); replacementFailures = 2;
    if(FAILED(transient.Commit()) || replacementFailures) return 49;
    const auto preserved = PluginOptionsProbe::Read(transient.Path());
    Store permanent(L"replace-controlled", L"Software\\FBETeam\\PluginReplaceControlledFixture");
    permanent.SetDWORDValue(L"Number", 3); replacementFailures = 8;
    const HRESULT refused = permanent.Commit(); const unsigned remaining = replacementFailures; replacementFailures = 0;
    if(refused != HRESULT_FROM_WIN32(ERROR_UNABLE_TO_REMOVE_REPLACED) || remaining != 4 || PluginOptionsProbe::Read(permanent.Path()) != preserved) return 50;
    for(DWORD revision = 0; revision < 100; ++revision) {
        Store repeated(L"replace-fixture", L"Software\\FBETeam\\PluginReplaceRuntimeFixture");
        const DWORD next = ReadDword(repeated, L"Revision", 0) + 1;
        repeated.SetDWORDValue(L"Revision", next);
        const HRESULT hr = repeated.Commit();
        if(FAILED(hr)) {
            HANDLE target = ::CreateFileW(repeated.Path().c_str(), DELETE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
            const DWORD opened = target == INVALID_HANDLE_VALUE ? ::GetLastError() : ERROR_SUCCESS;
            if(target != INVALID_HANDLE_VALUE) ::CloseHandle(target);
            std::wcerr << L"replacement_index=" << revision << L" delete_open=" << opened << L" attributes=" << ::GetFileAttributesW(repeated.Path().c_str()) << std::endl;
            return Die(L"repeated settings replacement", hr);
        }
    }
    return 0;
}
static int StoreChecks() {
    using namespace FbePluginSettings;
    const int replaced = ReplacementChecks(); if(replaced) return replaced;
    {
        HWND window = ::CreateWindowExW(0, L"STATIC", L"placement fixture", WS_POPUP | WS_CAPTION,
            10, 10, 500, 400, NULL, NULL, ::GetModuleHandleW(NULL), NULL);
        if(!window) return 44;
        struct Destroy { HWND window; ~Destroy() { ::DestroyWindow(window); } } destroy = {window};
        Store geometry(L"plugin-windows", L"Software\\FBETeam\\FictionBook Editor Next\\PluginWindowPlacement");
        geometry.SetDWORDValue(L"Fixture.Left", 100000);
        geometry.SetDWORDValue(L"Fixture.Top", 100000);
        geometry.SetDWORDValue(L"Fixture.Width", 500);
        geometry.SetDWORDValue(L"Fixture.Height", 400);
        geometry.SetDWORDValue(L"Fixture.Dpi", 96);
        if(FAILED(geometry.Commit()) || FAILED(FbePluginUi::WindowPlacement(window, L"Fixture", false))) return 45;
        RECT r = {}; ::GetWindowRect(window, &r); MONITORINFO monitor = {sizeof(monitor)};
        if(!::GetMonitorInfoW(::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor) ||
            r.left < monitor.rcWork.left || r.top < monitor.rcWork.top || r.right > monitor.rcWork.right || r.bottom > monitor.rcWork.bottom ||
            r.right - r.left != ::MulDiv(500, FbePluginUi::WindowDpi(window), 96) ||
            r.bottom - r.top != ::MulDiv(400, FbePluginUi::WindowDpi(window), 96)) return 46;
        const auto before = PluginOptionsProbe::Read(geometry.Path());
        CHandle lock(::CreateFileW(geometry.Path().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
        if(lock == INVALID_HANDLE_VALUE || SUCCEEDED(FbePluginUi::WindowPlacement(window, L"Fixture", true)) || PluginOptionsProbe::Read(geometry.Path()) != before) return 47;
        std::wcout << L"Window placement/off-screen recovery/DPI/atomic failure: PASS\n";
    }
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
    std::wcout << L"Store Unicode/atomic/stale/locked/corrupt/interrupted/100 replacements: PASS\n";
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
    else if(wcscmp(argv[2], L"replace") == 0) result = ReplacementChecks();
    else if(argc == 4 && wcscmp(argv[2], L"dialogs") == 0) result = Dialogs(argv[3]);
    else if(argc == 4 && wcscmp(argv[2], L"conversions") == 0) result = Conversions(argv[3]);
    else if(argc == 4 && wcsncmp(argv[2], L"ui-", 3) == 0) {
        const std::wstring scenario(argv[2]);
        const int only = (scenario == L"ui-import-separator" || scenario == L"ui-import-title" || scenario == L"ui-import-unify-images" || scenario == L"ui-import-cover-name") ? 2 : (scenario.find(L"ui-peer") == 0 ? static_cast<int>(scenario.back() - L'0') : -1);
        result = Dialogs(argv[3], scenario, only);
    }
    else result = 4;
    if(redirected.m_hKey) ::RegOverridePredefKey(HKEY_CURRENT_USER, NULL);
    ::CoUninitialize(); return result;
}
