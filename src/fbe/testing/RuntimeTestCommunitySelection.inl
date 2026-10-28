if (IsFbeTestScenario(L"community-selection-positions"))
{
 ShowView(BODY);
 CStringA report; bool passed = true; int cases = 0;
 auto flush = [&]() { DWORD written=0; output.Seek(0,FILE_BEGIN); output.Write(report,static_cast<DWORD>(report.GetLength()),&written);output.Flush(); };
 auto check = [&](const char* name, bool value) { report.AppendFormat("%s=%d\n", name, value ? 1 : 0); passed = passed && value; flush(); };
 auto pump = [&]() { MSG message={}; for(int count=0;count<1000 && ::PeekMessage(&message,NULL,0,0,PM_REMOVE);++count) {::TranslateMessage(&message);::DispatchMessage(&message);} };
 try {
  const bool treeEnabled = _Settings.ViewDocumentTree();
  _Settings.SetViewDocumentTree(true);
  m_document_tree.GetDocumentStructure(m_doc->m_body.Document());
  m_doc->MarkSavePoint();
  const wchar_t* ids[] = {L"position-title",L"position-first",L"position-second",L"position-inline",L"position-nested",L"position-annotation",L"position-note",L"position-comment",L"position-unicode",L"position-emoji"};
  for (int cycle = 0; cycle < 3; ++cycle) for (LPCWSTR id : ids) {
   MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
   MSHTML::IHTMLElementPtr element(MSHTML::IHTMLDocument3Ptr(document)->getElementById(id));
   if (!element) { check("fixture_element",false); continue; }
   const CString allText(static_cast<LPCWSTR>(element->innerText));
   const int offsets[] = {0,allText.GetLength()/2,allText.GetLength()};
   for (int offset : offsets) for (int selected = 0; selected < 2; ++selected) {
    m_doc->m_body.SetFocus();
    MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
    int beginOffset=selected ? (std::min)(offset,(std::max)(0,allText.GetLength()-3)) : offset;
    if(beginOffset>0 && beginOffset<allText.GetLength() && allText[beginOffset]>=0xDC00 && allText[beginOffset]<=0xDFFF) --beginOffset;
    int length = selected ? (std::min)(3,allText.GetLength()-beginOffset) : 0;
    if(length>0 && beginOffset+length<allText.GetLength() && allText[beginOffset+length]>=0xDC00 && allText[beginOffset+length]<=0xDFFF) ++length;
    if(beginOffset==0) {
     m_doc->m_body.GoTo(element,false);
     range=document->selection->createRange();
    } else {
     range->moveToElementText(element);range->collapse(VARIANT_TRUE);range->move(L"character",beginOffset);
    }
    if(length>0) range->moveEnd(L"character",length); range->select(); pump();
    const MSHTML::IHTMLTxtRangePtr before(range->duplicate());
    const bool dirty = m_doc->DocChanged();
    ShowView(SOURCE); pump();
    const int start = static_cast<int>(m_source.SendMessage(SCI_GETSELECTIONSTART));
    const int end = static_cast<int>(m_source.SendMessage(SCI_GETSELECTIONEND));
    const bool mapped = m_editor_selection_state.BodySource().bodyToSourceTransferred && start > 0 && end >= start && ((length == 0) == (start == end));
    ShowView(BODY); pump();
    MSHTML::IHTMLTxtRangePtr after(m_doc->m_body.Document()->selection->createRange());
    MSHTML::IHTMLElementPtr owner(after ? after->parentElement() : MSHTML::IHTMLElementPtr());
    while(owner && _wcsicmp(owner->tagName,L"P")!=0) owner=owner->parentElement;
    const bool sameOwner=owner && CString(static_cast<LPCWSTR>(owner->id))==id;
    const bool exact = after && sameOwner && before->compareEndPoints(L"StartToStart",after)==0 && before->compareEndPoints(L"EndToEnd",after)==0;
    auto& tree = m_document_tree.m_tree.m_tree;
    HTREEITEM expectedItem = NULL;
    for (MSHTML::IHTMLElementPtr parent(after->parentElement()); parent && !expectedItem; parent=parent->parentElement) {
     std::vector<HTREEITEM> items; if(tree.GetRootItem()) items.push_back(tree.GetRootItem());
     while(!items.empty()) {
      const HTREEITEM item=items.back();items.pop_back();
      auto* node=reinterpret_cast<MSHTML::IHTMLElement*>(tree.GetItemData(item));
      if(node && CComPtr<MSHTML::IHTMLElement>(node).IsEqualObject(parent)) {expectedItem=item;break;}
      if(tree.GetNextSiblingItem(item)) items.push_back(tree.GetNextSiblingItem(item));
      if(tree.GetChildItem(item)) items.push_back(tree.GetChildItem(item));
     }
    }
    const bool treeMatches = expectedItem && tree.CurrentStructureItem() == expectedItem;
    const bool unchanged = m_doc->DocChanged() == dirty;
    if ((!mapped || !exact) && cases<10) { auto node=before->parentElement(); auto previous=before->duplicate(); previous->moveToElementText(node); auto target=before->duplicate();target->moveToElementText(element);report.AppendFormat("boundary=%ld/%ld\n",previous->compareEndPoints(L"EndToStart",before),target->compareEndPoints(L"StartToStart",before)); report.AppendFormat("endpoint=%S/%S/%S;source=%d/%d;flags=%d/%d\n",static_cast<LPCWSTR>(node->tagName),static_cast<LPCWSTR>(node->id),static_cast<LPCWSTR>(node->className),start,end,m_editor_selection_state.BodySource().bodyToSourceTransferred,m_editor_selection_state.BodySource().sourceToBodyTransferred); }
    if (!mapped || !exact || !treeMatches || !unchanged) report.AppendFormat("case_%S_%d_%d_%d=%d/%d/%d/%d\n",id,cycle,offset,length,mapped,exact,treeMatches,unchanged);
    passed = passed && mapped && exact && treeMatches && unchanged; ++cases;
    report.AppendFormat("progress=%d\n",cases); DWORD progressWritten=0;
    output.Seek(0,FILE_BEGIN);output.Seek(0,FILE_BEGIN); output.Write(report,static_cast<DWORD>(report.GetLength()),&progressWritten);output.Flush();
   }
  }
#include "RuntimeTestCommunitySelectionRanges.inl"
  check("matrix_complete",true);
  if(passed) {
  // Select a source position independently of the outgoing Body selection.
  ShowView(SOURCE); pump();
  const int sourceLength = static_cast<int>(m_source.SendMessage(SCI_GETLENGTH));
  std::vector<char> source(static_cast<size_t>(sourceLength)+1);
  m_source.SendMessage(SCI_GETTEXT,sourceLength+1,reinterpret_cast<LPARAM>(source.data()));
  const char* marker = strstr(source.data(),"INDEPENDENT_SOURCE_TARGET");
  check("independent_source_marker",marker!=NULL);
  if (marker) {
   const int position = static_cast<int>(marker-source.data())+7;
   m_source.SendMessage(SCI_SETSEL,position,position);
   ShowView(BODY); pump(); check("independent_body_ready",!IsSourceActive());
   MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
   MSHTML::IHTMLElementPtr target(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"position-nested"));
   MSHTML::IHTMLTxtRangePtr expected(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
   expected->moveToElementText(target); expected->collapse(VARIANT_TRUE); expected->move(L"character",7);
   MSHTML::IHTMLTxtRangePtr actual(document->selection->createRange());
   check("independent_source_caret",expected->compareEndPoints(L"StartToStart",actual)==0 && actual->compareEndPoints(L"StartToEnd",actual)==0);
  }
  {
   ShowView(SOURCE); pump();
   const char* markup=strstr(source.data(),"id=\"position-nested\"");
   check("markup_fixture",markup!=NULL);
   if(markup) {
    const int at=static_cast<int>(markup-source.data())+4;m_source.SendMessage(SCI_SETSEL,at,at);ShowView(BODY);pump();
    check("markup_approximate",m_editor_selection_state.BodySource().sourceToBodyTransferred && m_editor_selection_state.BodySource().sourceToBodyApproximate);
    MSHTML::IHTMLTxtRangePtr actual(m_doc->m_body.Document()->selection->createRange());
    check("markup_scope",CString(static_cast<LPCWSTR>(actual->parentElement()->id))==L"position-nested");
   }
   MSHTML::IHTMLTxtRangePtr retained(m_doc->m_body.Document()->selection->createRange());
   ShowView(SOURCE);pump();const char* metadata=strstr(source.data(),"COMMUNITY_SMOKE_3010");check("metadata_fixture",metadata!=NULL);
   if(metadata) {
    const int at=static_cast<int>(metadata-source.data())+2;m_source.SendMessage(SCI_SETSEL,at,at);ShowView(BODY);pump();
    MSHTML::IHTMLTxtRangePtr actual(m_doc->m_body.Document()->selection->createRange());
    check("metadata_unavailable",!m_editor_selection_state.BodySource().sourceToBodyTransferred && retained->compareEndPoints(L"StartToStart",actual)==0);
   }
  }
  MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
  MSHTML::IHTMLElementPtr first(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"position-first"));
  MSHTML::IHTMLElementPtr second(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"position-second"));
  MSHTML::IHTMLTxtRangePtr begin(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange()), end(begin->duplicate());
  begin->moveToElementText(first); begin->collapse(VARIANT_TRUE); begin->move(L"character",3);
  end->moveToElementText(second); end->collapse(VARIANT_TRUE); end->move(L"character",5);
  begin->setEndPoint(L"EndToStart",end); begin->select();
  const CString expectedText(static_cast<LPCWSTR>(begin->text));
  ShowView(SOURCE); pump(); ShowView(BODY); pump();
  check("paragraph_pair",CString(static_cast<LPCWSTR>(MSHTML::IHTMLTxtRangePtr(m_doc->m_body.Document()->selection->createRange())->text))==expectedText);
  ShowView(SOURCE); pump();
  const int edit = static_cast<int>(m_source.SendMessage(SCI_GETSELECTIONSTART));
  m_source.SendMessage(SCI_SETSEL,edit,edit); const CStringA insertion(CW2A(L"\x0416\x03A9",CP_UTF8));
  const int beforeEditLength=static_cast<int>(m_source.SendMessage(SCI_GETLENGTH));
  m_source.SendMessage(SCI_REPLACESEL,0,reinterpret_cast<LPARAM>(static_cast<LPCSTR>(insertion)));
  const int editedLength=static_cast<int>(m_source.SendMessage(SCI_GETLENGTH));
  m_source.SendMessage(SCI_UNDO);check("source_edit_undo",m_source.SendMessage(SCI_GETLENGTH)==beforeEditLength);
  m_source.SendMessage(SCI_REDO);check("source_edit_redo",m_source.SendMessage(SCI_GETLENGTH)==editedLength);
  const int editStart=static_cast<int>(m_source.SendMessage(SCI_GETSELECTIONSTART));
  const int editEnd=static_cast<int>(m_source.SendMessage(SCI_GETSELECTIONEND));
  check("source_edit_offsets",editStart>=edit && editEnd<=edit+insertion.GetLength());
  const int beginChars=3+::MultiByteToWideChar(CP_UTF8,0,insertion,(std::max)(0,(std::min)(insertion.GetLength(),editStart-edit)),NULL,0);
  const int endChars=3+::MultiByteToWideChar(CP_UTF8,0,insertion,(std::max)(0,(std::min)(insertion.GetLength(),editEnd-edit)),NULL,0);
  ShowView(BODY); pump(); check("source_edit_dirty",m_doc->DocChanged());
  check("source_edit_range_retained",static_cast<bool>(m_editor_selection_state.BodyRange()));
  {
   MSHTML::IHTMLDocument2Ptr current(m_doc->m_body.Document());
   MSHTML::IHTMLElementPtr edited(MSHTML::IHTMLDocument3Ptr(current)->getElementById(L"position-first"));
   MSHTML::IHTMLTxtRangePtr expected(MSHTML::IHTMLBodyElementPtr(current->body)->createTextRange());
   expected->moveToElementText(edited);expected->collapse(VARIANT_TRUE);expected->move(L"character",beginChars);
   if(endChars>beginChars) expected->moveEnd(L"character",endChars-beginChars);
   MSHTML::IHTMLTxtRangePtr actual(current->selection->createRange());
   check("source_edit_position",expected->compareEndPoints(L"StartToStart",actual)==0 && expected->compareEndPoints(L"EndToEnd",actual)==0);
   ShowView(SOURCE);pump();ShowView(BODY);pump();
   actual=m_doc->m_body.Document()->selection->createRange();
   runExactRanges();
   check("source_edit_cycle",expected->compareEndPoints(L"StartToStart",actual)==0 && expected->compareEndPoints(L"EndToEnd",actual)==0 && m_doc->DocChanged());
  }
  MSHTML::IHTMLElementPtr editedRoot(MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"fbw_body"));
  const CString editedText(static_cast<LPCWSTR>(editedRoot->innerText));
  const CString filename(m_doc->m_filename); check("save",m_doc->Save());
  // A reopened document owns a new MSHTML view. Release probes of the old view
  // before destroying it, exactly as normal editor orchestration does.
  editedRoot=NULL;begin=NULL;end=NULL;first=NULL;second=NULL;document=NULL;
  check("reopen",LoadFile(filename)==OK);
  MSHTML::IHTMLElementPtr reopenedRoot(MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->getElementById(L"fbw_body"));
  check("reopen_text",editedText==CString(static_cast<LPCWSTR>(reopenedRoot->innerText)));
  } else report += "subsequent_checks=NOT_RUN\n";
  _Settings.SetViewDocumentTree(treeEnabled);
 } catch(const _com_error& error) { report.AppendFormat("hresult=0x%08lX\n",static_cast<unsigned long>(error.Error())); passed=false; }
 report.AppendFormat("cases=%d\nresult=%s\n",cases,passed?"PASS":"FAIL");
 DWORD written=0; output.Seek(0,FILE_BEGIN); output.Write(report,static_cast<DWORD>(report.GetLength()),&written); output.Close();
 ::PostQuitMessage(passed?0:1); return 0;
}
