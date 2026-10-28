  auto runExactRanges = [&]() {
   struct RangeCase { LPCWSTR first; int from; LPCWSTR firstXml; LPCWSTR last; int to; LPCWSTR lastXml; };
   const RangeCase ranges[] = {
    {L"position-first",3,L"Rep",L"position-second",5,L"Repea"},
    {L"position-first",3,L"Rep",L"position-third",3,L"Thi"},
    {L"position-title",3,L"Sec",L"position-second",5,L"Repea"},
    {L"position-inline",3,L"Pre",L"position-nested",7,L"INDEPEN"},
    {L"position-third",3,L"Thi",L"position-unicode",17,L"Кириллица Ω &amp; &lt; Ю"},
    {L"position-emoji",1,L"A",L"position-unicode",17,L"Кириллица Ω &amp; &lt; Ю"}
   };
   for(int cycle=0;cycle<3;++cycle) for(const auto& item:ranges) for(int reverse=0;reverse<2;++reverse) {
    MSHTML::IHTMLDocument2Ptr html(m_doc->m_body.Document());
    auto endpoint=[&](LPCWSTR id,int offset) {
     MSHTML::IHTMLElementPtr node(MSHTML::IHTMLDocument3Ptr(html)->getElementById(id));
     MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(html->body)->createTextRange());
     range->moveToElementText(node);range->collapse(VARIANT_TRUE);range->move(L"character",offset);return range;
    };
    auto original=endpoint(item.first,item.from), last=endpoint(item.last,item.to);
    original->setEndPoint(L"EndToStart",last);m_doc->m_body.SetFocus();original->select();
    original=original->duplicate();const bool dirty=m_doc->DocChanged();
    ShowView(SOURCE);pump();
    const int length=static_cast<int>(m_source.SendMessage(SCI_GETLENGTH));std::vector<char> bytes(static_cast<size_t>(length)+1);
    m_source.SendMessage(SCI_GETTEXT,length+1,reinterpret_cast<LPARAM>(bytes.data()));
    // Independent oracle: fixture IDs plus literal serialized XML prefixes,
    // without using the production structural index or its offset mapper.
    auto xmlOffset=[&](LPCWSTR id,LPCWSTR prefix) {
     CStringA key;key.Format("id=\"%s\"",static_cast<LPCSTR>(CStringA(CW2A(id,CP_UTF8))));
     const char* at=strstr(bytes.data(),key);if(!at)return -1;at=strchr(at,'>');if(!at)return -1;++at;
     const CStringA expected(CW2A(prefix,CP_UTF8));
     if(strncmp(at,expected,expected.GetLength())!=0)return -1;
     return static_cast<int>(at-bytes.data())+expected.GetLength();
    };
    const int expectedStart=xmlOffset(item.first,item.firstXml),expectedEnd=xmlOffset(item.last,item.lastXml);
    const bool xmlExact=expectedStart>0 && expectedEnd>expectedStart &&
      m_source.SendMessage(SCI_GETSELECTIONSTART)==expectedStart && m_source.SendMessage(SCI_GETSELECTIONEND)==expectedEnd;
    if(reverse)m_source.SendMessage(SCI_SETSEL,expectedEnd,expectedStart);
    ShowView(BODY);pump();MSHTML::IHTMLTxtRangePtr actual(m_doc->m_body.Document()->selection->createRange());
    const bool exact=original->compareEndPoints(L"StartToStart",actual)==0 && original->compareEndPoints(L"EndToEnd",actual)==0;
    auto& tree=m_document_tree.m_tree.m_tree;HTREEITEM expectedItem=NULL;
    for(MSHTML::IHTMLElementPtr parent(actual->parentElement());parent && !expectedItem;parent=parent->parentElement) {
     std::vector<HTREEITEM> pending;if(tree.GetRootItem())pending.push_back(tree.GetRootItem());
     while(!pending.empty()) {const auto node=pending.back();pending.pop_back();auto* element=reinterpret_cast<MSHTML::IHTMLElement*>(tree.GetItemData(node));
      if(element && CComPtr<MSHTML::IHTMLElement>(element).IsEqualObject(parent)){expectedItem=node;break;}
      if(tree.GetNextSiblingItem(node))pending.push_back(tree.GetNextSiblingItem(node));if(tree.GetChildItem(node))pending.push_back(tree.GetChildItem(node));
     }
    }
    const bool treeExact=expectedItem && tree.CurrentStructureItem()==expectedItem;
    if(!xmlExact || !exact || !treeExact || dirty!=m_doc->DocChanged())report.AppendFormat("range_%S_%S_%d_%d=%d/%d/%d;xml=%d/%d\n",item.first,item.last,cycle,reverse,xmlExact,exact,treeExact,expectedStart,expectedEnd);
    passed=passed && xmlExact && exact && treeExact && dirty==m_doc->DocChanged();++cases;flush();
   }
  };
  runExactRanges();
