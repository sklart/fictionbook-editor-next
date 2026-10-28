if(IsFbeTestScenario(L"community-transition-performance")) {
 CStringA report("kind\trepeat\tbody_source_ms\tsource_body_ms\tindex_ms\tmap_body_ms\tmap_source_ms\tprivate_bytes\tpeak_private_bytes\tindex_live_delta\theartbeat_max_ms\theartbeat_timeouts\r\n");
 auto flush=[&](){DWORD written=0;output.Seek(0,FILE_BEGIN);output.Write(report,static_cast<DWORD>(report.GetLength()),&written);output.Flush();};
 LARGE_INTEGER frequency={};::QueryPerformanceFrequency(&frequency);
 auto now=[&](){LARGE_INTEGER value={};::QueryPerformanceCounter(&value);return value.QuadPart*1000.0/frequency.QuadPart;};
 auto memory=[](){PROCESS_MEMORY_COUNTERS_EX value={};value.cb=sizeof(value);::GetProcessMemoryInfo(::GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&value),sizeof(value));return value;};
 try {
  for(int selected=0;selected<2;++selected) for(int repeat=-1;repeat<5;++repeat) {
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
