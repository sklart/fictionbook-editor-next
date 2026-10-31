if(IsFbeTestScenario(L"real-book-source-selection")) {
 CStringA report;
 bool passed = true;
 for(const wchar_t* id : {L"perf-mid", L"perf-last"}) {
  ShowView(BODY);
  MSHTML::IHTMLDocument2Ptr html(m_doc->m_body.Document());
  MSHTML::IHTMLElementPtr target(html ? MSHTML::IHTMLDocument3Ptr(html)->getElementById(id) : NULL);
  MSHTML::IHTMLBodyElementPtr body(html ? html->body : NULL);
  MSHTML::IHTMLTxtRangePtr range(body ? body->createTextRange() : NULL);
  bool selected = false, transferred = false, textMatches = false, focus = false, typed = false, undone = false, returned = false;
  if(target && range) {
   range->moveToElementText(target); range->collapse(VARIANT_TRUE); range->move(L"character",3); range->moveEnd(L"character",8);
   CString before(static_cast<LPCWSTR>(range->text)); selected = before.GetLength() == 8;
   m_doc->m_body.SetFocus(); range->select(); ShowView(SOURCE);
   const sptr_t start = m_source.SendMessage(SCI_GETSELECTIONSTART), end = m_source.SendMessage(SCI_GETSELECTIONEND);
   const std::string source = FbeScripts::SourceBytes(m_source);
   transferred = IsSourceActive() && m_editor_selection_state.BodySource().bodyToSourceTransferred && start >= 0 && end > start && static_cast<size_t>(end) <= source.size();
   const CStringA expected(CW2A(before, CP_UTF8));
   textMatches = transferred && source.substr(static_cast<size_t>(start), static_cast<size_t>(end-start)) == static_cast<LPCSTR>(expected);
   focus = ::GetFocus() == m_source.m_hWnd;
   if(transferred) {
    std::string changed(source); changed.replace(static_cast<size_t>(start), static_cast<size_t>(end-start), "X");
    ::SendMessageW(m_source, WM_CHAR, L'X', 1); typed = FbeScripts::SourceBytes(m_source) == changed;
    m_source.SendMessage(SCI_UNDO); undone = FbeScripts::SourceBytes(m_source) == source;
    m_source.SendMessage(SCI_SETSEL, start, end);
   }
   ShowView(BODY);
   MSHTML::IHTMLDocument2Ptr restored(m_doc->m_body.Document());
   MSHTML::IHTMLSelectionObjectPtr selection(restored ? restored->selection : NULL);
   MSHTML::IHTMLTxtRangePtr restoredRange(selection ? selection->createRange() : NULL);
   returned = restoredRange && CString(static_cast<LPCWSTR>(restoredRange->text)) == before;
  }
  report.AppendFormat("%S=%d/%d/%d/%d/%d/%d/%d\n", id,selected,transferred,textMatches,focus,typed,undone,returned);
  passed = passed && selected && transferred && textMatches && focus && typed && undone && returned;
 }
 DWORD written=0; output.Write(report,static_cast<DWORD>(report.GetLength()),&written); output.Flush(); output.Close();
 ::PostQuitMessage(passed ? 0 : 1); return 0;
}
if(IsFbeTestScenario(L"community-transition-performance")) {
 CStringA report("kind\trepeat\tbody_source_ms\tsource_body_ms\tindex_ms\tmap_body_ms\tmap_source_ms\tprivate_bytes\tpeak_private_bytes\tindex_live_delta\theartbeat_max_ms\theartbeat_timeouts\r\n");
 auto flush=[&](){DWORD written=0;output.Seek(0,FILE_BEGIN);output.Write(report,static_cast<DWORD>(report.GetLength()),&written);output.Flush();};
 LARGE_INTEGER frequency={};::QueryPerformanceFrequency(&frequency);
 auto now=[&](){LARGE_INTEGER value={};::QueryPerformanceCounter(&value);return value.QuadPart*1000.0/frequency.QuadPart;};
 auto memory=[](){PROCESS_MEMORY_COUNTERS_EX value={};value.cb=sizeof(value);::GetProcessMemoryInfo(::GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&value),sizeof(value));return value;};
 try {
  wchar_t affinity[32]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PERF_AFFINITY",affinity,_countof(affinity));
  const DWORD_PTR mask=static_cast<DWORD_PTR>(_wcstoui64(affinity,nullptr,10));
  if(mask && !::SetThreadAffinityMask(::GetCurrentThread(),mask))CheckError(HRESULT_FROM_WIN32(::GetLastError()));
  wchar_t kind[16]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PERF_KIND",kind,_countof(kind));
  const int first=wcscmp(kind,L"range")==0 ? 1:0, limit=wcscmp(kind,L"caret")==0 ? 1:2;
  for(int selected=first;selected<limit;++selected) for(int repeat=-1;repeat<5;++repeat) {
   ShowView(BODY);
   MSHTML::IHTMLDocument2Ptr html(m_doc->m_body.Document());
   MSHTML::IHTMLElementPtr target(MSHTML::IHTMLDocument3Ptr(html)->getElementById(L"perf-last"));
   if(!target)CheckError(E_FAIL);
   MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(html->body)->createTextRange());
   range->moveToElementText(target);range->collapse(VARIANT_TRUE);range->move(L"character",3);if(selected)range->moveEnd(L"character",8);
   m_doc->m_body.SetFocus();range->select();
   std::atomic_bool stop(false);double heartbeatMax=0;int timeouts=0;
   std::thread heartbeat([&](){while(!stop.load()){const double start=now();DWORD_PTR result=0;const bool ok=::SendMessageTimeout(m_hWnd,WM_NULL,0,0,SMTO_BLOCK|SMTO_ABORTIFHUNG,10000,&result)!=0;heartbeatMax=(std::max)(heartbeatMax,now()-start);if(!ok)++timeouts;::Sleep(10);}});
   const double start=now();ShowView(SOURCE);const bool sourceActive=IsSourceActive();const double sourceAt=now();ShowView(BODY);const bool bodyActive=!IsSourceActive();const double bodyAt=now();
   stop.store(true);
   // Let an outstanding worker probe complete before joining the worker.
   MSG message={};while(::PeekMessage(&message,NULL,0,0,PM_REMOVE)){::TranslateMessage(&message);::DispatchMessage(&message);}heartbeat.join();CheckError(sourceActive && bodyActive ? S_OK:E_FAIL);
   const auto transitionMemory=memory();
   SourceDocumentText text;CheckError(SourceDocumentTransfer::ReadSourceText(m_source,text)==SourceTransitionResult::Success ? S_OK:E_FAIL);
   auto xml=m_source_view_session.DiagnosticSelectionSnapshot();if(!xml)CheckError(E_FAIL);const auto before=memory();SIZE_T indexDelta=0;double indexMs=-1;
   wchar_t legacy[2]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_PERF_LEGACY",legacy,_countof(legacy));
   if(legacy[0]!=L'1') {
    const double indexStart=now();FBEBodySourceTransfer::XmlSelectionPosition index(std::wstring_view(text.text,text.text.GetLength()));indexMs=now()-indexStart;
    if(!index.Valid())CheckError(E_FAIL);const auto live=memory();if(live.PrivateUsage>before.PrivateUsage)indexDelta=live.PrivateUsage-before.PrivateUsage;
   }
   m_editor_selection_state.BodyRange()=range;
   const double mapStart=now();m_source_selection_coordinator.MapBodySelectionToSource(*m_doc,xml,m_source,text.text,m_editor_selection_state,nullptr);const double mapBodyEnd=now();
   CheckError(SourceDocumentTransfer::ReadSourceText(m_source,text)==SourceTransitionResult::Success ? S_OK:E_FAIL);
   const double reverseStart=now();m_source_selection_coordinator.MapSourceSelectionToBody(*m_doc,xml,text,m_editor_selection_state);const double reverseEnd=now();
   CStringA row;row.Format("%s\t%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%I64u\t%I64u\t%I64u\t%.3f\t%d\r\n",selected?"range":"caret",repeat,sourceAt-start,bodyAt-sourceAt,indexMs,mapBodyEnd-mapStart,reverseEnd-reverseStart,static_cast<unsigned __int64>(transitionMemory.PrivateUsage),static_cast<unsigned __int64>(transitionMemory.PeakPagefileUsage),static_cast<unsigned __int64>(indexDelta),heartbeatMax,timeouts);report+=row;flush();
  }
  output.Close();::PostQuitMessage(0);return 0;
 } catch(const _com_error& error){report.AppendFormat("ERROR\t0x%08lX\n",static_cast<unsigned long>(error.Error()));flush();output.Close();::PostQuitMessage(1);return 0;}
}
