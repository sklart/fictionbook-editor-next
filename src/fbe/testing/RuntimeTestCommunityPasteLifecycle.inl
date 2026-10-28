if(IsFbeTestScenario(L"html-paste-runtime")) {
 wchar_t name[64]={};::GetEnvironmentVariableW(L"FBE_NEXT_TEST_HTML_PASTE_CASE",name,_countof(name));
 if(wcscmp(name,L"queue-reload")==0 || wcscmp(name,L"queue-reload-idle")==0) {
  bool passed=true;CComPtr<IDataObject> clipboard;::OleGetClipboard(&clipboard);const CString filename(m_doc->m_filename);
  try {
   {
    ShowView(BODY);MSHTML::IHTMLDocument2Ptr html(m_doc->m_body.Document());
    MSHTML::IHTMLElementPtr target(MSHTML::IHTMLDocument3Ptr(html)->getElementById(L"paste-target"));
    MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(html->body)->createTextRange());range->moveToElementText(target);range->collapse(VARIANT_FALSE);range->select();m_doc->m_body.SetFocus();
    for(int i=1;i<=3;++i) {
     CheckError(::OpenClipboard(m_doc->m_body)?S_OK:E_FAIL);::EmptyClipboard();
     CString text;text.Format(L"Queue%d",i);HGLOBAL data=::GlobalAlloc(GMEM_MOVEABLE,(text.GetLength()+1)*sizeof(wchar_t));
     auto* bytes=static_cast<wchar_t*>(::GlobalLock(data));if(!bytes){::CloseClipboard();CheckError(E_OUTOFMEMORY);}memcpy(bytes,static_cast<LPCWSTR>(text),(text.GetLength()+1)*sizeof(wchar_t));::GlobalUnlock(data);
     const bool written=::SetClipboardData(CF_UNICODETEXT,data)!=NULL;::CloseClipboard();CheckError(written?S_OK:E_FAIL);
     if(wcscmp(name,L"queue-reload-idle")==0)::SetEnvironmentVariableW(L"FBE_NEXT_UNDO_FAILURE",L"paste-post");
     html->execCommand(L"Paste",VARIANT_FALSE,_variant_t());
    }
    MSHTML::IHTMLElementPtr root(MSHTML::IHTMLDocument3Ptr(html)->getElementById(L"fbw_body"));
    passed=CString(static_cast<LPCWSTR>(root->innerText)).Find(L"Queue")==-1;
   }
   // Release every test probe of the old view before normal document replacement.
   passed=LoadFile(filename)==OK && passed;ShowView(BODY);
   MSG message={};while(::PeekMessage(&message,NULL,0,0,PM_REMOVE)){::TranslateMessage(&message);::DispatchMessage(&message);}
   _Module.GetMessageLoop()->OnIdle(0);_Module.GetMessageLoop()->OnIdle(0);
   MSHTML::IHTMLElementPtr root(MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"fbw_body"));
   const CString current(static_cast<LPCWSTR>(root->innerText));passed=passed && current.Find(L"Queue")==-1 && current.Find(L"Before")>=0 && !m_doc->DocChanged();
  } catch(const _com_error&){passed=false;}
  const HRESULT restored=::OleSetClipboard(clipboard);passed=passed && SUCCEEDED(restored);if(clipboard)passed=SUCCEEDED(::OleFlushClipboard()) && passed;
  CStringA report;report.Format("case\tresult\r\n%S\t%s\r\n",name,passed?"pass":"fail");DWORD written=0;output.Write(report,static_cast<DWORD>(report.GetLength()),&written);output.Close();::PostQuitMessage(passed?0:1);return 0;
 }
}
