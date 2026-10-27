if (IsFbeTestScenario(L"community-href-edit"))
{
 ShowView(BODY); CStringA report; bool passed = true;
 auto check = [&](const char* name, bool value) { report.AppendFormat("%s=%d\n",name,value?1:0); passed = passed && value; };
 try {
  MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
  MSHTML::IHTMLElementPtr paragraph(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"href-tests"));
  MSHTML::IHTMLElementCollectionPtr links(MSHTML::IHTMLElement2Ptr(paragraph)->getElementsByTagName(L"a"));
  for (long index=0;index<links->length;++index) {
   MSHTML::IHTMLElementPtr anchor(links->item(_variant_t(index),_variant_t()));
   MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
   m_doc->m_body.SetFocus(); range->moveToElementText(anchor); range->select();
   InvalidateSelectionContext(); m_sel_changed = true; OnIdle();
   m_contextAttributeBars.FocusLinkField(LinkAttributeField::Href);
   COMBOBOXINFO info = {}; info.cbSize = sizeof(info);
   check("native_edit",::GetComboBoxInfo(m_contextAttributeBars.HrefCatalogForPopulation(),&info)!=FALSE && ::IsWindowEnabled(info.hwndItem));
   auto href = [&]() { CString value(static_cast<LPCWSTR>(_bstr_t(anchor->getAttribute(L"href",2)))); if(value.Left(5)==L"file:" && value.Find(L'#')>=0) value=value.Mid(value.ReverseFind(L'#')); return value; };
   const CString original(href());
   for (int step = 0; step < 3; ++step) {
    m_doc->m_body.SetFocus(); range->moveToElementText(anchor); range->select();
    InvalidateSelectionContext(); m_sel_changed=true; OnIdle();
    m_contextAttributeBars.FocusLinkField(LinkAttributeField::Href);
    const int offset = step==0 ? 0 : (step==1 ? original.GetLength()/2 : original.GetLength());
    // Test the real edit notification/DOM path with a selected replacement.
    // A leading '#' is kept for internal links and a scheme for external ones.
    const int at = (std::max)(1,offset);
    ::SendMessage(info.hwndItem,EM_SETSEL,at,at);
    ::SendMessage(info.hwndItem,WM_CHAR,L'z',0);
    OnIdle(); OnIdle();
    DWORD start=0,end=0; ::SendMessage(info.hwndItem,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));
    CString edited(original); edited.Insert(at,L'z');
    const CString attribute(href());
    // The existing handler inserts http:// when editing an incomplete scheme.
    CString expected(edited); if(expected[0]!=L'#' && expected.Find(L"://")<0) expected=L"http://"+expected;
    check("caret_after_idle",start==static_cast<DWORD>(at+1) && end==start);
    check("href_applied",attribute==expected);
    CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
    check("href_undo",href()==original);
    CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));
    check("href_redo",href()==expected);
    // Remove exactly the inserted character through the edit notification.
    ::SendMessage(info.hwndItem,EM_SETSEL,at,at+1); ::SendMessage(info.hwndItem,WM_CHAR,VK_BACK,0);
    OnIdle();
    check("href_delete",href()==original);
   }
  }
  // Leave a real external href edit applied, then verify its serialized value.
  MSHTML::IHTMLElementPtr persistent(links->item(_variant_t(1L),_variant_t()));
  m_doc->m_body.SetFocus();
  MSHTML::IHTMLTxtRangePtr persistentRange(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
  persistentRange->moveToElementText(persistent);persistentRange->select();
  InvalidateSelectionContext();m_sel_changed=true;OnIdle();m_contextAttributeBars.FocusLinkField(LinkAttributeField::Href);
  COMBOBOXINFO editInfo={};editInfo.cbSize=sizeof(editInfo);::GetComboBoxInfo(m_contextAttributeBars.HrefCatalogForPopulation(),&editInfo);
  const CString previous(m_contextAttributeBars.GetLinkState().href);
  ::SendMessage(editInfo.hwndItem,EM_SETSEL,previous.GetLength(),previous.GetLength());::SendMessage(editInfo.hwndItem,WM_CHAR,L'z',0);OnIdle();
  const CString persisted(static_cast<LPCWSTR>(_bstr_t(persistent->getAttribute(L"href",2))));
  check("modified_before_save",persisted==previous+L"z");
  const CString filename(m_doc->m_filename); check("save",m_doc->Save());
  persistentRange=NULL;persistent=NULL;links=NULL;paragraph=NULL;document=NULL;
  check("reopen",LoadFile(filename)==OK);
  MSHTML::IHTMLElementPtr reopened(MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"href-tests"));
  MSHTML::IHTMLElementCollectionPtr reopenedLinks(MSHTML::IHTMLElement2Ptr(reopened)->getElementsByTagName(L"a"));
  MSHTML::IHTMLElementPtr reopenedLink(reopenedLinks->item(_variant_t(1L),_variant_t()));
  check("reopened_href",CString(static_cast<LPCWSTR>(_bstr_t(reopenedLink->getAttribute(L"href",2))))==persisted);
 } catch(const _com_error& error) {report.AppendFormat("hresult=0x%08lX\n",static_cast<unsigned long>(error.Error()));passed=false;}
 report.AppendFormat("result=%s\n",passed?"PASS":"FAIL");DWORD written=0;
 output.Write(report,static_cast<DWORD>(report.GetLength()),&written);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
