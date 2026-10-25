// External controller: identical, uninstrumented Release editors are driven
// through their existing MSHTML object model and ordinary window commands.
#include <atlbase.h>
#include <atlcom.h>
#include <mshtml.h>
#include <oleacc.h>
#include <psapi.h>
#include <atomic>
#include <thread>
#include <string>
#include <fstream>
#include <cstdio>
#pragma comment(lib, "oleacc.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "psapi.lib")
CComModule module;
void Check(HRESULT hr) { if (FAILED(hr)) throw hr; }
double Now() { LARGE_INTEGER t, f; QueryPerformanceCounter(&t); QueryPerformanceFrequency(&f); return 1000.0 * t.QuadPart / f.QuadPart; }
std::wstring Quote(const std::wstring& text) {
    std::wstring result = L"\"";
    for (auto ch : text) { if (ch == L'\\' || ch == L'\"') result += L'\\'; result += ch; }
    return result + L"\"";
}
struct Controller {
    HANDLE process = nullptr; DWORD pid = 0; HWND frame = nullptr, browser = nullptr;
    std::atomic<bool> done{false}; std::atomic<SIZE_T> peakPrivate{0}; std::thread sampler;
    std::ofstream output;
    std::wstring scriptPath;
    const char* peakPrefix;
    Controller(const wchar_t* report, const char* prefix = "final") : output(report), scriptPath(std::wstring(report)+L".js"), peakPrefix(prefix) {
        if (!output) throw E_FAIL;
        std::ofstream script(scriptPath); script << "// External binary-store benchmark controller context.\n";
        if (!script) throw E_FAIL;
    }
    ~Controller() { done = true; if (sampler.joinable()) sampler.join(); if (process) { TerminateProcess(process, 1); WaitForSingleObject(process, 5000); CloseHandle(process); } }
    void Start(const std::wstring& exe, const std::wstring& book) {
        std::wstring command = L"\"" + exe + L"\" --portable \"" + book + L"\"";
        STARTUPINFOW startup = {}; startup.cb = sizeof(startup); PROCESS_INFORMATION info = {};
        if (!CreateProcessW(exe.c_str(), &command[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &info)) throw HRESULT_FROM_WIN32(GetLastError());
        process = info.hProcess; pid = info.dwProcessId; CloseHandle(info.hThread);
        sampler = std::thread([&]() { while (!done) { PROCESS_MEMORY_COUNTERS_EX counters = {}; if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) { if (counters.PrivateUsage > peakPrivate) peakPrivate = counters.PrivateUsage; } Sleep(10); } });
    }
    void Windows() {
        frame = browser = nullptr;
        EnumWindows([](HWND window, LPARAM context) -> BOOL {
            auto& self = *reinterpret_cast<Controller*>(context); DWORD id = 0; GetWindowThreadProcessId(window, &id);
            if (id != self.pid) return TRUE;
            EnumChildWindows(window, [](HWND child, LPARAM nested) -> BOOL {
                auto& parent = *reinterpret_cast<Controller*>(nested); wchar_t name[128] = {}; GetClassNameW(child, name, 128);
                if (!wcscmp(name, L"Internet Explorer_Server")) { parent.browser = child; parent.frame = GetAncestor(child, GA_ROOT); }
                return TRUE;
            }, context); return TRUE;
        }, reinterpret_cast<LPARAM>(this));
    }
    CComPtr<IHTMLDocument2> Document() {
        Windows(); if (!browser) throw E_PENDING;
        DWORD_PTR result = 0;
        if (!SendMessageTimeoutW(browser, RegisterWindowMessageW(L"WM_HTML_GETOBJECT"), 0, 0, SMTO_ABORTIFHUNG, 30000, &result)) throw E_PENDING;
        CComPtr<IHTMLDocument2> doc;
        Check(ObjectFromLresult(static_cast<LRESULT>(result), IID_IHTMLDocument2, 0, reinterpret_cast<void**>(&doc))); return doc;
    }
    void Ready(long expected) {
        const double deadline = Now() + 180000;
        while (Now() < deadline) {
            if (WaitForSingleObject(process, 0) != WAIT_TIMEOUT) throw E_FAIL;
            try {
                auto doc = Document(); CComQIPtr<IHTMLDocument3> dom(doc); CComPtr<IHTMLElement> container;
                Check(dom->getElementById(CComBSTR(L"binobj"), &container));
                CComQIPtr<IHTMLElement2> element(container); if (element) {
                    CComPtr<IHTMLElementCollection> children; Check(element->getElementsByTagName(CComBSTR(L"DIV"), &children));
                    long count = 0; Check(children->get_length(&count));
                    if (count == expected) { DWORD_PTR ignored; SendMessageTimeoutW(frame, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 30000, &ignored); return; }
                }
            } catch (HRESULT) {}
            Sleep(10);
        }
        throw HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    }
    void Script(const std::wstring& expression) {
        auto doc = Document(); CComPtr<IHTMLWindow2> window; Check(doc->get_parentWindow(&window)); CComVariant result;
        const std::wstring script = L"var fbePerfError='', fbePerfContext=window.external._EnterScriptContext(" + Quote(scriptPath) + L"); try { " + expression + L"; } catch(error) { fbePerfError=String(error.number)+':'+String(error.message); } finally {window.external._LeaveScriptContext(fbePerfContext);}";
        Check(window->execScript(CComBSTR(script.c_str()), CComBSTR(L"JScript"), &result));
        CComPtr<IDispatch> dispatch; Check(doc->get_Script(&dispatch));
        CComDispatchDriver globals(dispatch); CComVariant error;
        Check(globals.GetPropertyByName(L"fbePerfError", &error));
        if (error.vt == VT_BSTR && SysStringLen(error.bstrVal)) { fwprintf(stderr,L"Script: %ls\n",error.bstrVal); throw E_FAIL; }
    }
    void Command(WORD id) { DWORD_PTR result; if (!SendMessageTimeoutW(frame, WM_COMMAND, id, 0, SMTO_ABORTIFHUNG, 180000, &result)) throw HRESULT_FROM_WIN32(GetLastError()); }
    void EditSourceWhitespace() {
        HWND source = nullptr;
        EnumChildWindows(frame, [](HWND child, LPARAM context) -> BOOL {
            wchar_t name[64] = {}; GetClassNameW(child, name, 64);
            if (IsWindowVisible(child) && !wcscmp(name, L"Scintilla")) *reinterpret_cast<HWND*>(context) = child;
            return TRUE;
        }, reinterpret_cast<LPARAM>(&source));
        if (!source) throw E_FAIL;
        DWORD_PTR result = 0;
        // SCI_DOCUMENTEND, then an ordinary typed space: valid trailing XML
        // whitespace forces the real Source Apply path without changing bytes.
        if (!SendMessageTimeoutW(source, 2318, 0, 0, SMTO_ABORTIFHUNG, 30000, &result) ||
            !SendMessageTimeoutW(source, WM_CHAR, L' ', 0, SMTO_ABORTIFHUNG, 30000, &result)) throw E_FAIL;
    }
    void Timing(const char* name, double started) { output << "time\t" << name << "\t" << (Now() - started) << "\n"; output.flush(); }
    void Memory(const char* stage) {
        SIZE_T ws = 0, priv = 0;
        for (int i = 0; i < 10; ++i) {
            Sleep(50); PROCESS_MEMORY_COUNTERS_EX counters = {};
            if (!GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) throw E_FAIL;
            ws += counters.WorkingSetSize; priv += counters.PrivateUsage;
        }
        PROCESS_MEMORY_COUNTERS_EX counters = {}; GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));
        output << "memory\t" << stage << "_working_set\t" << ws / 10 << "\n"
            << "memory\t" << stage << "_private_bytes\t" << priv / 10 << "\n"
            << "memory\t" << stage << "_peak_working_set\t" << counters.PeakWorkingSetSize << "\n"
            << "memory\t" << stage << "_peak_private_commit\t" << counters.PeakPagefileUsage << "\n"
            << "memory\t" << stage << "_sampled_peak_private_bytes\t" << peakPrivate.load() << "\n"; output.flush();
    }
    void Close(bool collect = true) {
        Command(0xE102); if (collect) Memory("after_close");
        PROCESS_MEMORY_COUNTERS_EX counters = {};
        if (!GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) throw E_FAIL;
        output << "memory\t" << peakPrefix << "_peak_working_set\t" << counters.PeakWorkingSetSize << "\n"
            << "memory\t" << peakPrefix << "_peak_private_commit\t" << counters.PeakPagefileUsage << "\n";
        PostMessageW(frame, WM_CLOSE, 0, 0);
        const double deadline = Now() + 30000;
        while (WaitForSingleObject(process, 10) == WAIT_TIMEOUT) {
            if (Now() > deadline) throw HRESULT_FROM_WIN32(WAIT_TIMEOUT);
            // FileClose has already discarded no data. The editor can create
            // an empty New document; discard only that test-owned blank book.
            EnumWindows([](HWND window, LPARAM context) -> BOOL {
                const auto& self = *reinterpret_cast<Controller*>(context); DWORD id = 0; GetWindowThreadProcessId(window, &id);
                if (id == self.pid && IsWindowVisible(window) && GetDlgItem(window, IDNO) && GetDlgItem(window, IDYES))
                    PostMessageW(window, WM_COMMAND, IDNO, 0);
                return TRUE;
            }, reinterpret_cast<LPARAM>(this));
        }
        done = true; sampler.join(); CloseHandle(process); process = nullptr;
    }
};
int wmain(int argc, wchar_t** argv) {
    if (argc != 7) { fputs("driver FBE.exe book.fb2 replacement.png binary-count report.tsv result-prefix\n", stderr); return 2; }
    Check(CoInitialize(nullptr));
    try {
        Controller run(argv[5]); double started = Now(); run.Start(argv[1], argv[2]); run.Ready(_wtol(argv[4])); run.Command(32810); run.Timing("open", started); run.Memory("after_open");
        started = Now(); run.Command(0xE103); run.Timing("save", started);
        if (!CopyFileW(argv[2], (std::wstring(argv[6])+L"-unchanged.fb2").c_str(), FALSE)) throw HRESULT_FROM_WIN32(GetLastError());
        started = Now(); run.Script(L"for(var i=0;i<1000;i++) window.external.GetBinaryInfo('image1.png')"); run.Timing("metadata_1000", started);
        started = Now(); run.Script(L"window.external.ReplaceBinaryFromFile('image1.png'," + Quote(argv[3]) + L",'image/png')"); run.Command(0xE103); run.Timing("replace_save", started);
        if (!CopyFileW(argv[2], (std::wstring(argv[6])+L"-replaced.fb2").c_str(), FALSE)) throw HRESULT_FROM_WIN32(GetLastError());
        started = Now(); run.Command(127); run.Timing("body_source", started); run.Memory("after_source");
        run.EditSourceWhitespace();
        started = Now(); run.Command(32810); run.Ready(_wtol(argv[4])); run.Timing("source_body", started); run.Memory("after_body");
        run.Command(0xE103);
        if (!CopyFileW(argv[2], (std::wstring(argv[6])+L"-source.fb2").c_str(), FALSE)) throw HRESULT_FROM_WIN32(GetLastError());
        started = Now(); run.Script(L"for(var i=0;i<5;i++){window.external.ReplaceBinaryFromFile('image1.png',"+Quote(argv[3])+L",'image/png'); var b=document.all.binobj.getElementsByTagName('DIV')[0]; b.base64data=apiGetBinary('photo.png');}"); run.Timing("mutation_5", started);
        run.Command(0xE103);
        if (!CopyFileW(argv[2], (std::wstring(argv[6])+L"-mutation.fb2").c_str(), FALSE)) throw HRESULT_FROM_WIN32(GetLastError());
        // Restore the known replacement before equivalence checks and roundtrip.
        run.Script(L"window.external.ReplaceBinaryFromFile('image1.png',"+Quote(argv[3])+L",'image/png')");
        started = Now(); run.Command(0xE103);
        run.Close(false); Controller reopened((std::wstring(argv[5])+L".reopen").c_str(), "reopen_final"); reopened.Start(argv[1],argv[2]); reopened.Ready(_wtol(argv[4])); reopened.Command(32810); run.Timing("save_close_process_reopen", started); reopened.Memory("after_reopen");
        reopened.Command(0xE103); reopened.Close();
        run.output << "status\tPASS\t1\n";
    } catch(HRESULT hr) { fprintf(stderr,"Benchmark failed HRESULT=0x%08lx\n",hr); return 1; }
    CoUninitialize(); return 0;
}
