#ifdef FBE_SOURCE_MEMORY_BENCHMARK
#include "../binary/BinaryMemoryAttribution.h"
#include <functional>

namespace {
unsigned long long C5DiscardedPumpInput = 0;
void C5Pump(DWORD milliseconds) {
    const auto until = ::GetTickCount64() + milliseconds;
    do {
        MSG message = {};
        while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) continue;
            if (IsSourceBenchmarkUserInput(message.message)) { ++C5DiscardedPumpInput; continue; }
            ::TranslateMessage(&message); ::DispatchMessage(&message);
        }
        ::MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    } while (::GetTickCount64() < until);
}
long long C5Clock() { LARGE_INTEGER value = {}; ::QueryPerformanceCounter(&value); return value.QuadPart; }
double C5Milliseconds(long long first) {
    LARGE_INTEGER frequency = {}; ::QueryPerformanceFrequency(&frequency);
    return static_cast<double>(C5Clock() - first) * 1000.0 / frequency.QuadPart;
}
void C5Paint(HWND window) { ::RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN); }
}

__declspec(noinline) LRESULT CMainFrame::RunSourceMemoryC5(CAtlFile& output) {
    // This process drives controls through messages. Ignore physical desktop input
    // while retaining real layout/paint, so typing in other apps cannot edit it.
    ::EnableWindow(m_hWnd, FALSE);
    m_source.IsolateBenchmarkInput();
    bool passed = false;
    CStringA details("initialization");
    auto require = [&](bool value, const char* message) { if (!value) { details = message; CheckError(E_FAIL); } };
    const auto started = C5Clock();
    using Emit = std::function<void(const char*, unsigned long long)>;
    Emit emit = [&](const char* stage, unsigned long long known) {
        PROCESS_MEMORY_COUNTERS_EX memory = {}; memory.cb = sizeof(memory);
        CheckError(::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) ? S_OK : E_FAIL);
        unsigned long long committed = 0, reserved = 0, imageMapped = 0;
        SYSTEM_INFO system = {}; ::GetSystemInfo(&system);
        auto address = reinterpret_cast<ULONG_PTR>(system.lpMinimumApplicationAddress);
        const auto maximum = reinterpret_cast<ULONG_PTR>(system.lpMaximumApplicationAddress);
        while (address < maximum) {
            MEMORY_BASIC_INFORMATION region = {};
            if (!::VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region))) break;
            if (region.State == MEM_COMMIT || region.State == MEM_RESERVE) {
                if (region.Type == MEM_PRIVATE) {
                    if (region.State == MEM_COMMIT) committed += region.RegionSize;
                    else reserved += region.RegionSize;
                } else imageMapped += region.RegionSize;
            }
            const auto next = reinterpret_cast<ULONG_PTR>(region.BaseAddress) + region.RegionSize;
            if (next <= address) break;
            address = next;
        }
        RECT viewport = {}; ::GetClientRect(m_source, &viewport);
        const auto canonicalBytes = m_doc ? m_doc->m_binaryStore.DiagnosticCurrentPayloads().canonicalBytes : 0ULL;
        CStringA row;
        row.Format("stage=%s;elapsed_ms=%.4f;qpc=%lld;private_bytes=%llu;working_set=%llu;peak_private_bytes=%llu;peak_working_set=%llu;private_commit=%llu;private_reserve=%llu;virtual_size=%llu;source_utf8_bytes=%llu;source_lines=%llu;source_cached_utf16_bytes=%llu;source_cached_capacity_bytes=%llu;source_xml_live=%u;source_modified=%lld;input_messages_discarded=%llu;layout_cache=%lld;end_styled=%lld;viewport_width=%ld;viewport_height=%ld;canonical_bytes=%llu;known_point_bytes=%llu\r\n",
            stage, C5Milliseconds(started), C5Clock(), static_cast<unsigned long long>(memory.PrivateUsage),
            static_cast<unsigned long long>(memory.WorkingSetSize), static_cast<unsigned long long>(memory.PeakPagefileUsage),
            static_cast<unsigned long long>(memory.PeakWorkingSetSize), committed, reserved, committed + reserved + imageMapped,
            static_cast<unsigned long long>(m_source.SendMessage(SCI_GETLENGTH)), static_cast<unsigned long long>(m_source.SendMessage(SCI_GETLINECOUNT)),
            m_source_view_session.DiagnosticCachedTextBytes(), m_source_view_session.DiagnosticCachedTextCapacityBytes(),
            m_source_view_session.DiagnosticSavedXmlLive() ? 1u : 0u, static_cast<long long>(m_source.SendMessage(SCI_GETMODIFY)),
            C5DiscardedPumpInput + m_source.BenchmarkDiscardedInputMessages(), static_cast<long long>(m_source.SendMessage(SCI_GETLAYOUTCACHE)),
            static_cast<long long>(m_source.SendMessage(SCI_GETENDSTYLED)), viewport.right, viewport.bottom, canonicalBytes, known);
        C4Write(output, row); CheckError(output.Flush());
    };
    const char* phase = "initial";
    Emit observed = [&](const char* stage, unsigned long long bytes) { CStringA name; name.Format("internal_%s_%s", phase, stage); emit(name, bytes); };
    struct Reset { ~Reset() { FbeBinary::MemoryAttribution::Callback() = nullptr; FbeBinary::MemoryAttribution::Context() = nullptr; } } reset;
    FbeBinary::MemoryAttribution::Context() = &observed;
    FbeBinary::MemoryAttribution::Callback() = [](void* context, const char* stage, unsigned long long bytes) { (*static_cast<Emit*>(context))(stage, bytes); };
    auto point = [&](const char* stage) { emit(stage, 0); };
    auto timed = [&](const char* operation, const std::function<void()>& action) {
        phase = operation; const auto first = C5Clock(); action(); const double milliseconds = C5Milliseconds(first);
        CStringA row; row.Format("time=%s;milliseconds=%.5f\r\n", operation, milliseconds); C4Write(output, row); CheckError(output.Flush());
    };
    auto stabilize = [&]() {
        // Full syntax styling plus synchronous viewport paint; then a bounded quiet
        // period, rather than assuming a fixed sleep proves internal layout completion.
        m_source.SendMessage(SCI_COLOURISE, 0, -1); C5Paint(m_source);
        unsigned long long previous = 0; int quiet = 0;
        const auto limit = ::GetTickCount64() + 30000;
        while (quiet < 3 && ::GetTickCount64() < limit) {
            C5Pump(100); C5Paint(m_source);
            PROCESS_MEMORY_COUNTERS_EX memory = {}; memory.cb = sizeof(memory);
            ::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory));
            const auto current = static_cast<unsigned long long>(memory.PrivateUsage);
            const auto difference = current > previous ? current - previous : previous - current;
            quiet = difference <= 65536 ? quiet + 1 : 0; previous = current;
        }
        require(quiet == 3, "Source did not reach the defined quiet window");
    };
    try {
        wchar_t input[32768] = {}, directory[32768] = {}, release[32] = {};
        require(::GetEnvironmentVariableW(L"FBE_C5_INPUT", input, _countof(input)) != 0, "input missing");
        require(::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", directory, _countof(directory)) != 0, "runtime root missing");
        ::GetEnvironmentVariableW(L"FBE_C5_RELEASE_BODY", release, _countof(release));
        const CString root(directory);
        point("process_initialized"); m_doc->MarkSavePoint();
        timed("open_body", [&]() { require(LoadFile(input) == OK, "open failed"); C5Paint(m_doc->m_body); });
        const auto canonical = m_doc->m_binaryStore.DiagnosticCurrentPayloads().canonicalBytes;
        point("open_body");
        const auto sourceEntryStarted = C5Clock();
        timed("body_to_source", [&]() { ShowView(SOURCE); require(IsSourceActive(), "Source not active"); });
        const auto firstLength = m_source.SendMessage(SCI_GETLENGTH);
        const auto firstModified = m_source.SendMessage(SCI_GETMODIFY);
        point("source_generated");
        timed("first_layout", [&]() { C5Paint(m_source); }); point("first_paint_complete");
        timed("source_stabilization_wait", stabilize);
        require(m_source.SendMessage(SCI_GETLENGTH) == firstLength && m_source.SendMessage(SCI_GETMODIFY) == firstModified,
            "Source changed while no edits were scheduled (first entry)");
        { CStringA time; time.Format("time=time_to_source_stable;milliseconds=%.5f\r\n", C5Milliseconds(sourceEntryStarted)); C4Write(output, time); }
        point("initial_layout_complete"); point("source_stable");
        const sptr_t length = static_cast<sptr_t>(m_source.SendMessage(SCI_GETLENGTH));
        const auto lines = m_source.SendMessage(SCI_GETLINECOUNT);
        sptr_t longest = 0;
        for (sptr_t line = 0; line < lines; ++line) longest = (std::max)(longest, static_cast<sptr_t>(m_source.SendMessage(SCI_LINELENGTH, line)));
        CStringA shape; shape.Format("shape=max_line_bytes;value=%lld\r\n", static_cast<long long>(longest)); C4Write(output, shape);
        timed("scroll_workload", [&]() {
            for (int repeat = 0; repeat < 3; ++repeat) {
                for (const auto position : {sptr_t(0), length / 2, length > 0 ? length - 1 : 0}) {
                    m_source.SendMessage(SCI_GOTOPOS, position); m_source.SendMessage(SCI_SCROLLCARET); C5Paint(m_source);
                    for (int page = 0; page < 2; ++page) { m_source.SendMessage(SCI_PAGEDOWN); C5Paint(m_source); }
                    for (int page = 0; page < 2; ++page) { m_source.SendMessage(SCI_PAGEUP); C5Paint(m_source); }
                }
            }
        }); point("after_scroll");
        timed("caret_workload", [&]() {
            for (const auto position : {sptr_t(0), length / 2, length > 100 ? length - 100 : 0}) {
                m_source.SendMessage(SCI_GOTOPOS, position);
                for (int move = 0; move < 40; ++move) { m_source.SendMessage(move % 2 ? SCI_CHARLEFT : SCI_CHARRIGHT); C5Paint(m_source); }
            }
        }); point("after_caret");
        auto objects = FbeScripts::Binary::Objects(m_doc->m_body.Document());
        require(!objects.empty(), "fixture has no binary objects");
        const CString middle = L"id=\"" + objects[objects.size() / 2].id + L"\"";
        objects.clear();
        timed("find_workload", [&]() {
            for (const auto& pattern : {CString(L"<description>"), middle, CString(L"</FictionBook>")}) {
                m_doc->m_body.SetSourceBenchmarkQuery(pattern);
                m_source.SendMessage(SCI_SETSEL, 0, 0);
                require(m_doc->m_body.SciFindNext(m_source, false, false), "Find failed");
                m_source.SendMessage(SCI_SCROLLCARET); C5Paint(m_source);
            }
        }); point("after_find");
        // Replace uses the same Scintilla selection/target path as CSciReplaceDlg;
        // dialog opening is not included in the operation latency.
        timed("replace", [&]() {
            m_doc->m_body.SetSourceBenchmarkQuery(L" ");
            m_source.SendMessage(SCI_SETSEL, 0, 0);
            require(m_doc->m_body.SciFindNext(m_source, false, false), "replace target missing");
            m_source.SendMessage(SCI_TARGETFROMSELECTION); m_source.SendMessage(SCI_REPLACETARGET, 2, reinterpret_cast<LPARAM>("  "));
            require(m_source.SendMessage(SCI_GETLENGTH) == length + 1, "replace length"); C5Paint(m_source);
        }); point("after_replace");
        timed("small_edit", [&]() {
            m_source.SendMessage(SCI_SETSEL, 5, 5); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(" ")); C5Paint(m_source);
        });
        require(m_source.SendMessage(SCI_GETLENGTH) == length + 2, "edit length");
        m_source.SendMessage(SCI_UNDO); require(m_source.SendMessage(SCI_GETLENGTH) == length + 1, "Source undo");
        m_source.SendMessage(SCI_REDO); require(m_source.SendMessage(SCI_GETLENGTH) == length + 2, "Source redo");
        point("after_small_edit");
        timed("source_to_body", [&]() { ShowView(BODY); require(!IsSourceActive(), "Source apply failed"); C5Paint(m_doc->m_body); });
        point("body_after_source"); C5Pump(500); point("body_idle");
        const auto originalPolicy = m_source.SendMessage(SCI_GETLAYOUTCACHE);
        if (_wcsicmp(release, L"layout") == 0) { m_source.SendMessage(SCI_SETLAYOUTCACHE, SC_CACHE_NONE); point("body_layout_released"); }
        m_source.SendMessage(SCI_SETLAYOUTCACHE, originalPolicy);
        timed("second_body_to_source", [&]() { ShowView(SOURCE); require(IsSourceActive(), "second Source failed"); C5Paint(m_source); });
        const auto secondLength = m_source.SendMessage(SCI_GETLENGTH);
        const auto secondModified = m_source.SendMessage(SCI_GETMODIFY);
        point("second_source_generated");
        timed("second_source_stable", stabilize); point("second_source_stable");
        require(m_source.SendMessage(SCI_GETLENGTH) == secondLength && m_source.SendMessage(SCI_GETMODIFY) == secondModified,
            "Source changed while no edits were scheduled");
        require(m_source.SendMessage(SCI_GETLENGTH) > 0, "second Source empty");
        timed("return_body", [&]() { ShowView(BODY); require(!IsSourceActive(), "second Body failed"); });
        C5Pump(300); point("return_body_idle");
        require(m_doc->m_binaryStore.DiagnosticCurrentPayloads().canonicalBytes == canonical, "payload bytes changed");
        // Verification Save is outside editor-operation timers; its separate memory
        // point and close are included in the overall scenario peak.
        point("before_close"); require(m_doc->Save(root + L"\\c5-roundtrip.fb2"), "verification save failed"); point("after_verification_save");
        phase = "close"; m_doc->MarkSavePoint(); BOOL handled = FALSE; OnFileClose(0, 0, nullptr, handled);
        point("after_close"); passed = true; details = "C5 Source workload complete";
    } catch (const _com_error& error) { CStringA code; code.Format(" hr=0x%08lX", static_cast<unsigned long>(error.Error())); details += code; }
      catch (...) { details += " native exception"; }
    if (!passed) {
        // Preserve the actual failing buffer locally; never print its contents.
        try {
            wchar_t directory[32768] = {};
            const auto length = m_source.SendMessage(SCI_GETLENGTH);
            if (length > 0 && length < INT_MAX && ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", directory, _countof(directory))) {
                std::vector<char> bytes(static_cast<size_t>(length) + 1);
                m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(bytes.data()));
                CAtlFile failedSource;
                if (SUCCEEDED(failedSource.Create(CString(directory) + L"\\unexpected-source.xml", GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS)))
                    failedSource.Write(bytes.data(), static_cast<DWORD>(length));
            }
        } catch (...) { }
    }
    CStringA result; result.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString()); C4Write(output, result);
    output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
}
#endif
