#ifdef FBE_BINARY_STORE_MEMORY_ATTRIBUTION
#include "../binary/BinaryMemoryAttribution.h"
#include <functional>
namespace {
void C41Idle(DWORD milliseconds) {
    const auto until = ::GetTickCount64() + milliseconds;
    while (::GetTickCount64() < until) {
        MSG message = {};
        while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) continue;
            ::TranslateMessage(&message); ::DispatchMessage(&message);
        }
        ::MsgWaitForMultipleObjects(0, nullptr, FALSE, 20, QS_ALLINPUT);
    }
}
unsigned long long C41ArrayBytes(const CComVariant& value) {
    if (value.vt != (VT_ARRAY | VT_UI1) || !value.parray) CheckError(E_UNEXPECTED);
    return value.parray->rgsabound[0].cElements;
}
}
__declspec(noinline) LRESULT CMainFrame::RunBinaryStoreC41(CAtlFile& output) {
    bool passed = false;
    CStringA details("start");
    unsigned long long nativeLive = 0, jsReachable = 0, xmlBstrLive = 0;
    bool xmlDomLive = false;
    const auto started = ::GetTickCount64();
    using Emit = std::function<void(const char*, unsigned long long)>;
    Emit emit = [&](const char* stage, unsigned long long knownBytes) {
        PROCESS_MEMORY_COUNTERS_EX memory = {}; memory.cb = sizeof(memory);
        if (!::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory))) CheckError(E_FAIL);
        const auto owner = m_doc->m_binaryStore.payloads.DiagnosticStatistics();
        const auto current = m_doc->m_binaryStore.DiagnosticCurrentPayloads();
        const auto counters = FbeBinary::Diagnostics::Get();
        unsigned long long privateCommitted = 0, privateReserved = 0;
        SYSTEM_INFO system = {}; ::GetSystemInfo(&system);
        auto address = reinterpret_cast<ULONG_PTR>(system.lpMinimumApplicationAddress);
        const auto maximum = reinterpret_cast<ULONG_PTR>(system.lpMaximumApplicationAddress);
        while (address < maximum) {
            MEMORY_BASIC_INFORMATION region = {};
            if (!::VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region))) break;
            if (region.Type == MEM_PRIVATE) {
                if (region.State == MEM_COMMIT) privateCommitted += region.RegionSize;
                if (region.State == MEM_RESERVE) privateReserved += region.RegionSize;
            }
            const auto next = reinterpret_cast<ULONG_PTR>(region.BaseAddress) + region.RegionSize;
            if (next <= address) break;
            address = next;
        }
        LARGE_INTEGER qpc = {}; ::QueryPerformanceCounter(&qpc);
        CStringA row;
        row.Format("stage=%s;elapsed_ms=%llu;qpc=%lld;known_point_bytes=%llu;private_bytes=%llu;working_set=%llu;peak_private_bytes=%llu;peak_working_set=%llu;virtualquery_private_commit=%llu;virtualquery_private_reserve=%llu;canonical_bytes=%llu;history_only_bytes=%llu;owner_unique_bytes=%llu;underlying_blocks=%llu;native_returned_live_bytes=%llu;js_reachable_returned_bytes=%llu;local_xml_dom_live=%u;local_xml_bstr_live_bytes=%llu;source_cached_utf16_bytes=%llu;source_cached_capacity_bytes=%llu;source_saved_xml_live=%u;source_serialized_xml_live=%u;source_utf8_bytes=%llu;image_decode_calls=%llu;image_bitmap_bytes_total=%llu;max_single_bitmap_bytes=%llu;facade_get_calls=%llu\r\n",
            stage, ::GetTickCount64() - started, qpc.QuadPart, knownBytes,
            static_cast<unsigned long long>(memory.PrivateUsage), static_cast<unsigned long long>(memory.WorkingSetSize),
            static_cast<unsigned long long>(memory.PeakPagefileUsage), static_cast<unsigned long long>(memory.PeakWorkingSetSize),
            privateCommitted, privateReserved, current.canonicalBytes, owner.historyOnlyBytes, owner.uniqueBytes, owner.uniquePayloadBlocks,
            nativeLive, jsReachable, xmlDomLive ? 1u : 0u, xmlBstrLive,
            m_source_view_session.DiagnosticCachedTextBytes(), m_source_view_session.DiagnosticCachedTextCapacityBytes(),
            m_source_view_session.DiagnosticSavedXmlLive() ? 1u : 0u, m_source_view_session.DiagnosticSerializedXmlLive() ? 1u : 0u,
            static_cast<unsigned long long>(m_source.SendMessage(SCI_GETLENGTH)), counters.imageDecodeCalls,
            counters.imageDecodedBitmapBytesTotal, counters.maxSingleDecodedBitmapBytes, counters.facadeGets);
        C4Write(output, row); CheckError(output.Flush());
    };
    struct ObserverReset { ~ObserverReset() {
        FbeBinary::MemoryAttribution::Callback() = nullptr; FbeBinary::MemoryAttribution::Context() = nullptr;
    } } reset;
    FbeBinary::MemoryAttribution::Context() = &emit;
    FbeBinary::MemoryAttribution::Callback() = [](void* context, const char* stage, unsigned long long bytes) { (*static_cast<Emit*>(context))(stage, bytes); };
    auto point = [&](const char* stage) { emit(stage, 0); };
    auto script = [&](const wchar_t* text) { CheckError(m_doc->m_body.Document()->parentWindow->execScript(text, L"JScript")); };
    auto gc = [&]() { script(L"CollectGarbage();"); };
    try {
        wchar_t inputBuffer[32768] = {}, rootBuffer[32768] = {}, mode[32] = {};
        if (!::GetEnvironmentVariableW(L"FBE_C41_INPUT", inputBuffer, _countof(inputBuffer))) CheckError(E_INVALIDARG);
        ::GetEnvironmentVariableW(L"FBE_NEXT_SCRIPT_API_ROOT", rootBuffer, _countof(rootBuffer));
        ::GetEnvironmentVariableW(L"FBE_C41_MODE", mode, _countof(mode));
        const CString root(rootBuffer);
        point("process_initialized");
        details = "Open";
        // Only the freshly created, private blank test document is discarded.
        m_doc->MarkSavePoint();
        if (LoadFile(inputBuffer) != OK) CheckError(E_FAIL);
        point("after_open"); gc(); point("after_open_gc"); C41Idle(1500); point("after_open_gc_idle");
        if (_wcsicmp(mode, L"content-only") != 0) {
            const auto canonical = m_doc->m_binaryStore.DiagnosticCurrentPayloads().canonicalBytes;
            if (_wcsicmp(mode, L"source-only") != 0) {
            details = "native facade retention";
            auto objects = FbeScripts::Binary::Objects(m_doc->m_body.Document());
            if (objects.empty()) CheckError(E_UNEXPECTED);
            point("before_native_one");
            {
                CComVariant value; CComDispatchDriver driver(objects.front().element);
                CheckError(driver.GetPropertyByName(L"base64data", &value)); nativeLive = C41ArrayBytes(value);
                point("native_one_variant_live");
            }
            nativeLive = 0; point("native_one_variant_released"); gc(); point("native_one_after_gc");
            for (const auto& object : objects) { CComVariant value; CComDispatchDriver driver(object.element); CheckError(driver.GetPropertyByName(L"base64data", &value)); }
            point("native_all_sequential_released");
            {
                std::vector<CComVariant> values(objects.size());
                for (size_t i = 0; i < objects.size(); ++i) { CComDispatchDriver driver(objects[i].element); CheckError(driver.GetPropertyByName(L"base64data", &values[i])); nativeLive += C41ArrayBytes(values[i]); }
                if (nativeLive != canonical) CheckError(E_UNEXPECTED);
                point("native_all_variants_live");
            }
            nativeLive = 0; point("native_all_variants_released"); gc(); point("native_all_after_gc"); C41Idle(1500); point("native_all_after_idle");
            objects.clear();
            details = "JScript facade retention";
            script(L"var _c41_values=null; function _c41_discard_one(){var n=document.all.binobj.getElementsByTagName('DIV');var x=n[0].base64data;x=null;} function _c41_discard_all(){var n=document.all.binobj.getElementsByTagName('DIV');for(var i=0;i<n.length;i++){var x=n[i].base64data;x=null;}} function _c41_retain_all(){var n=document.all.binobj.getElementsByTagName('DIV');_c41_values=[];for(var i=0;i<n.length;i++)_c41_values.push(n[i].base64data);}");
            point("js_no_extra_access"); script(L"_c41_discard_one();"); point("js_one_discarded");
            script(L"_c41_discard_all();"); point("js_all_discarded"); gc(); point("js_discard_after_gc");
            script(L"_c41_retain_all();"); jsReachable = canonical; point("js_all_retained");
            script(L"_c41_values=null;"); jsReachable = 0; point("js_array_cleared");
            gc(); point("js_clear_after_gc"); C41Idle(1500); point("js_clear_after_idle");
            details = "CreateDOM/BSTR separation";
            point("before_create_dom");
            {
                auto xml = m_doc->CreateDOM(L"utf-8"); xmlDomLive = true;
                point("created_xml_dom_live_no_xml_getter");
                {
                    _bstr_t text(xml->xml); xmlBstrLive = static_cast<unsigned long long>(text.length()) * sizeof(wchar_t);
                    point("xml_dom_and_bstr_live");
                }
                xmlBstrLive = 0; point("xml_bstr_released_dom_live");
            }
            xmlDomLive = false; point("xml_dom_released"); gc(); point("xml_release_after_gc"); C41Idle(1500); point("xml_release_after_idle");
            }
            details = "image metadata/rendering";
            script(L"RebuildImagesInfo();"); point("after_rebuild_images_info");
            ::RedrawWindow(m_doc->m_body, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            C41Idle(1500); point("after_body_render_idle"); gc(); point("before_source_gc");
            details = "Source";
            ShowView(SOURCE); point("source_visible"); C41Idle(1500); point("source_stable");
            ShowView(BODY); point("body_after_readonly_source"); gc(); point("body_after_source_gc"); C41Idle(1500); point("body_after_source_idle");
            // Controlled release of this test document's caches; never a production fix.
            m_source_view_session.ResetDocument(); point("source_xml_and_text_cache_released");
            const auto fresh = m_source.SendMessage(SCI_CREATEDOCUMENT, 0, 0);
            if (!fresh) CheckError(E_OUTOFMEMORY);
            m_source.SendMessage(SCI_SETDOCPOINTER, 0, fresh); m_source.SendMessage(SCI_RELEASEDOCUMENT, 0, fresh);
            point("source_scintilla_document_released"); gc(); point("source_release_after_gc"); C41Idle(1500); point("source_release_after_idle");
            details = "Undo discard";
            { CComPtr<IOleUndoManager> manager; IServiceProviderPtr services(m_doc->m_body.Document()); CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager))); CheckError(manager->DiscardFrom(nullptr)); }
            point("after_undo_discard"); gc(); point("after_undo_discard_gc");
            if (m_doc->m_binaryStore.DiagnosticCurrentPayloads().canonicalBytes != canonical) CheckError(E_UNEXPECTED);
        }
        point("before_close");
        // Verification save follows all measured memory points.
        if (_wcsicmp(mode, L"source-only") == 0 && !m_doc->Save(root + L"\\c41-roundtrip.fb2")) CheckError(E_FAIL);
        passed = true; details = "C4.1 controlled lifetime timeline complete";
    } catch (const _com_error& error) { CStringA code; code.Format(" hr=0x%08lX", static_cast<unsigned long>(error.Error())); details += code; }
      catch (...) { details += " native exception"; }
    CStringA result; result.Format("result=%s\r\ndetails=%s\r\n", passed ? "PASS" : "FAIL", details.GetString()); C4Write(output, result);
    output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
}
#endif
