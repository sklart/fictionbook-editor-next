// Addressed title-only contour; the full tree interaction scenario remains below.
if(IsFbeTestScenario(L"document-tree-titles-runtime")) {
    bool passed=false;CStringA report,phase="titles";
    auto require=[&](bool ok){if(!ok)CheckError(E_FAIL);};
    try {
        ShowWindow(SW_SHOWNOACTIVATE);
        ShowView(BODY);CTreeView& tree=m_document_tree.m_tree.m_tree;tree.SetScriptMode(false);
        auto document=m_doc->m_body.Document();
        auto element=[&](LPCWSTR id){return MSHTML::IHTMLElementPtr(MSHTML::IHTMLDocument3Ptr(document)->getElementById(id));};
        auto caption=[&](LPCWSTR id){
            CString text;bool found=false;
            std::function<void(HTREEITEM)> walk=[&](HTREEITEM item){for(;item;item=tree.GetNextSiblingItem(item)){
                MSHTML::IHTMLElementPtr node(reinterpret_cast<MSHTML::IHTMLElement*>(tree.GetItemData(item)));
                if(node&&CString(static_cast<LPCWSTR>(node->id))==id){tree.GetItemText(item,text);found=true;return;}
                walk(tree.GetChildItem(item));if(found)return;
            }};walk(tree.GetRootItem());report.AppendFormat("caption_%s=%s;found=%d\n",static_cast<LPCSTR>(CW2A(id,CP_UTF8)),static_cast<LPCSTR>(CW2A(text,CP_UTF8)),found);require(found);return text;
        };
        auto verify=[&](){
            m_document_tree.GetDocumentStructure(document);
            require(caption(L"head0")==L"Head 0 Heading text");require(caption(L"leaf0")==L"Leaf 0");
            require(caption(L"title-unicode")==L"СТАДИЯ ПЕРВАЯ Глава первая");
            require(caption(L"title-empty")==L"<section>");require(caption(L"title-parent-empty")==L"<section>");
        };
        verify();
        MSHTML::IHTMLElementPtr probe(document->createElement(L"DIV"));
        const LPCWSTR prefixes[]={L"",L" \r\n",L"<!-- comment -->",L"<DIV class='image'></DIV>",L" \n<!-- comment --><DIV class='image'></DIV> \n"};
        phase="DOM-prefixes";for(LPCWSTR prefix:prefixes){probe->innerHTML=_bstr_t(CString(prefix)+L"<DIV class='title'><P>Введение <EM>книга</EM></P><P>Часть 2</P></DIV>");
            CString title=U::FindTitle(probe);U::NormalizeInplace(title);title.Remove(L' ');require(title==L"ВведениекнигаЧасть2");}
        probe->innerHTML=L"<DIV class='section'><DIV class='title'><P>Child title</P></DIV></DIV>";require(!U::FindTitleNode(probe));
        const CString before(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));const bool dirty=m_doc->DocChanged();
        ShowView(SOURCE);ShowView(BODY);verify();
        require(CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml))==before&&dirty==m_doc->DocChanged());
        phase="edit/undo/redo";
        m_doc->m_body.MarkupServices()->BeginUndoUnit(L"Title test");element(L"head-title0")->insertAdjacentText(L"beforeEnd",L" changed");m_doc->m_body.MarkupServices()->EndUndoUnit();
        tree.UpdateDocumentStructure(document,element(L"head-title0"));require(caption(L"head0")==L"Head 0 changed Heading text");
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));verify();
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));m_document_tree.GetDocumentStructure(document);require(caption(L"head0")==L"Head 0 changed Heading text");
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));verify();
        phase="save/reopen";const CString path=U::GetProgDir()+L"title-probe.fb2";require(m_doc->Save(path));require(LoadFile(path)==OK);document=m_doc->m_body.Document();verify();
        require(CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml))==before);
        passed=true;
    }catch(const _com_error& error){report.AppendFormat("hr=0x%08lX\n",static_cast<unsigned long>(error.Error()));}
    report.AppendFormat("phase=%s\nresult=%s\n",phase.GetString(),passed?"pass":"fail");DWORD n=0;output.Write(report,report.GetLength(),&n);output.Close();::PostQuitMessage(passed?0:1);return 0;
}
if(IsFbeTestScenario(L"document-tree-window-runtime"))
{
    CStringA report, phase("setup");
    bool passed = false;
    auto checkpoint = [&]() {
        CStringA row; row.Format("stage=%s\n", phase.GetString());
        DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
    };
    try {
        CTreeView& tree = m_document_tree.m_tree.m_tree;
        ShowWindow(SW_SHOWNORMAL);
        ShowView(BODY);
        if(!_Settings.ViewDocumentTree()) SendMessage(WM_COMMAND, ID_VIEW_TREE);
        tree.SetScriptMode(false);
        m_document_tree.GetDocumentStructure(m_doc->m_body.Document());
        UpdateLayout();
        ::SetForegroundWindow(m_hWnd);
        InvalidateUi(UiDirtyAll); m_sel_changed = true; OnIdle();
        auto document = m_doc->m_body.Document();
        auto element = [&](LPCWSTR id) { return MSHTML::IHTMLElementPtr(MSHTML::IHTMLDocument3Ptr(document)->getElementById(id)); };
        auto find = [&](LPCWSTR id) -> HTREEITEM {
            const auto target = element(id);
            std::function<HTREEITEM(HTREEITEM)> walk = [&](HTREEITEM first) -> HTREEITEM {
                for(HTREEITEM item = first; item; item = tree.GetNextSiblingItem(item)) {
                    MSHTML::IHTMLElementPtr node(reinterpret_cast<MSHTML::IHTMLElement*>(tree.GetItemData(item)));
                    if(node && CString(static_cast<LPCWSTR>(node->id)) == id) return item;
                    if(HTREEITEM child = walk(tree.GetChildItem(item))) return child;
                }
                return NULL;
            };
            return target ? walk(tree.GetRootItem()) : NULL;
        };
        auto require = [&](bool value, int line) { if(!value) { report.AppendFormat("failed_line=%d\n", line); CheckError(E_FAIL); } };
#define FBE_TREE_CHECK(value) require((value), __LINE__)
        auto paint = [&]() { tree.UpdateWindow(); ::GdiFlush(); };
        auto verifyMarker = [&](HTREEITEM item) {
            RECT marker = {}; FBE_TREE_CHECK(tree.StructureMarkerRect(item, marker));
            RECT label = {}, client = {}, expectedRect = {};
            FBE_TREE_CHECK(tree.GetItemRect(item, &label, TRUE) != FALSE);
            tree.GetClientRect(&client);
            const int thickness = UiMetrics::ScaleForDpi(tree.StructureHighlight(item) == 2 ? 3 : 1, UiMetrics::DpiForWindow(tree));
            label.top = (std::max)(label.top, label.bottom - thickness);
            FBE_TREE_CHECK(::IntersectRect(&expectedRect, &label, &client) != FALSE);
            FBE_TREE_CHECK(::EqualRect(&marker, &expectedRect) != FALSE);
            const bool selectedItem = (tree.GetItemState(item, TVIS_SELECTED) & TVIS_SELECTED) != 0;
            const COLORREF expected = ThemeManager::IsHighContrast() ? ::GetSysColor(selectedItem ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT) :
                (selectedItem ? ThemeManager::SelectionTextColor() : (tree.StructureHighlight(item) == 2 ? ThemeManager::AccentColor() : ThemeManager::SecondaryTextColor()));
            HDC dc = tree.GetDC(); const COLORREF actual = ::GetPixel(dc, marker.left, marker.bottom - 1);
            bool entireLine = true;
            for(int y = marker.top; y < marker.bottom; ++y)
                for(int x = marker.left; x < marker.right; ++x)
                    if(::GetPixel(dc, x, y) != expected) entireLine = false;
            tree.ReleaseDC(dc);
            if(actual != expected) report.AppendFormat("marker_rank=%d\nmarker_rect=%ld,%ld,%ld,%ld\nmarker_actual=0x%08lX\nmarker_expected=0x%08lX\n",
                tree.StructureHighlight(item), marker.left, marker.top, marker.right, marker.bottom, actual, expected);
            FBE_TREE_CHECK(actual == expected);
            FBE_TREE_CHECK(entireLine);
        };
        auto input = [&](HTREEITEM item, UINT modifiers, bool button, UINT down = WM_LBUTTONDOWN) {
            FBE_TREE_CHECK(item != NULL);
            tree.EnsureVisible(item);
            RECT label = {}; FBE_TREE_CHECK(tree.GetItemRect(item, &label, TRUE) != FALSE);
            POINT point = {label.left + 2, (label.top + label.bottom) / 2};
            if(button) {
                bool found = false;
                for(int x = 0; x < label.left; ++x) {
                    UINT flags = 0;
                    if(tree.HitTest(CPoint(x, point.y), &flags) == item && (flags & TVHT_ONITEMBUTTON)) { point.x = x; found = true; break; }
                }
                FBE_TREE_CHECK(found);
            }
            // Queue the release before dispatching the press: native TreeView
            // can enter DragDetect's message loop on an already focused row.
            tree.PostMessage(down, MK_LBUTTON | modifiers, MAKELPARAM(point.x, point.y));
            tree.PostMessage(WM_LBUTTONUP, modifiers, MAKELPARAM(point.x, point.y));
            MSG message = {};
            while(::PeekMessage(&message, tree, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) ::DispatchMessage(&message);
            paint();
        };
        int caretProbes = 0;
        auto selectCaret = [&](LPCWSTR id) {
            const auto target = element(id); FBE_TREE_CHECK(target != NULL);
            m_doc->m_body.SetFocus();
            MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
            range->moveToElementText(target); range->collapse(VARIANT_TRUE);
            // Put the caret inside the text. A collapsed range at the exact
            // boundary can belong to the end of the preceding paragraph.
            range->move(L"character", 1); range->select();
            if(caretProbes++ < 3) {
                MSHTML::IHTMLTxtRangePtr actual(document->selection->createRange());
                MSHTML::IHTMLElementPtr parent(actual->parentElement());
                report.AppendFormat("raw_caret=%s\n", CStringA(static_cast<LPCWSTR>(parent->id)).GetString());
            }
            m_doc->m_body.OnSelChange(NULL);
            OnIdle();
            paint();
        };
        auto xml = [&]() { return CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)); };
        phase = "section-titles";
        auto caption = [&](LPCWSTR id) {
            CString text; FBE_TREE_CHECK(tree.GetItemText(find(id), text)); return text;
        };
        FBE_TREE_CHECK(caption(L"head0") == L"Head 0 Heading text");
        FBE_TREE_CHECK(caption(L"leaf0") == L"Leaf 0");
        FBE_TREE_CHECK(caption(L"merge-first") == L"<section>");
        FBE_TREE_CHECK(caption(L"title-unicode") == L"СТАДИЯ ПЕРВАЯ Глава первая");
        FBE_TREE_CHECK(caption(L"title-empty") == L"<section>");
        FBE_TREE_CHECK(caption(L"title-parent-empty") == L"<section>");
        MSHTML::IHTMLElementPtr probe(document->createElement(L"DIV"));
        probe->innerHTML = L" \r\n<!--space--><DIV class='image'></DIV> \n<DIV class='title'><P>Title</P></DIV>";
        FBE_TREE_CHECK(U::FindTitle(probe) == L"Title");
        probe->innerHTML = L"<DIV class='image'></DIV>";
        FBE_TREE_CHECK(!U::FindTitleNode(probe));
        probe->innerHTML = L"<DIV class='section'><DIV class='title'><P>Nested</P></DIV></DIV>";
        FBE_TREE_CHECK(!U::FindTitleNode(probe));
        FBE_TREE_CHECK(!U::FindTitleNode(NULL));
        report += "section_title_extraction=1\n";
        phase = "title-layout";
        const CString layoutXml = xml();
        const bool layoutDirty = m_doc->DocChanged(), originalFast = m_doc->GetFastMode();
        for(bool fast : {false, true}) {
            m_doc->SetFastMode(fast);
            const auto title = U::FindTitleNode(element(L"head0"));
            MSHTML::IHTMLCurrentStylePtr style(MSHTML::IHTMLElement2Ptr(title)->currentStyle);
            FBE_TREE_CHECK(CString(static_cast<LPCWSTR>(_bstr_t(style->paddingTop))) == L"0px");
            FBE_TREE_CHECK(CString(static_cast<LPCWSTR>(_bstr_t(style->paddingBottom))) == L"0px");
            const auto first = element(L"head-title0"), second = element(L"head-text0");
            FBE_TREE_CHECK(first->offsetHeight > 0 && second->offsetHeight > 0);
            FBE_TREE_CHECK(second->offsetTop >= first->offsetTop + first->offsetHeight);
            FBE_TREE_CHECK(second->offsetTop - first->offsetTop - first->offsetHeight <= 1);
            FBE_TREE_CHECK(title->offsetHeight >= first->offsetHeight + second->offsetHeight);
            FBE_TREE_CHECK(xml() == layoutXml && m_doc->DocChanged() == layoutDirty);
        }
        m_doc->SetFastMode(originalFast);
        ShowView(SOURCE); ShowView(BODY);
        m_document_tree.GetDocumentStructure(document);
        FBE_TREE_CHECK(xml() == layoutXml && m_doc->DocChanged() == layoutDirty);
        report += "title_normal_fast_layout=1\ntitle_source_roundtrip=1\n";
        const CString before = xml();
        const bool dirty = m_doc->DocChanged();
        const auto revision = m_doc->m_body.GetVersionNumber();
        CComPtr<IOleUndoManager> undoManager; IServiceProviderPtr services(document);
        CheckError(services->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&undoManager)));
        auto history = [&](bool redo) {
            CComPtr<IEnumOleUndoUnits> entries; CheckError(redo ? undoManager->EnumRedoable(&entries) : undoManager->EnumUndoable(&entries));
            std::vector<CComPtr<IUnknown>> result;
            for(;;) {
                CComPtr<IOleUndoUnit> unit; const HRESULT next = entries->Next(1, &unit, NULL);
                if(next == S_FALSE) break; CheckError(next);
                CComPtr<IUnknown> identity; CheckError(unit->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity)));
                result.push_back(identity);
            }
            return result;
        };
        const auto undoBefore = history(false), redoBefore = history(true);
        auto selected = [&]() {
            std::set<HTREEITEM> result;
            std::function<void(HTREEITEM)> walk = [&](HTREEITEM first) {
                for(HTREEITEM item = first; item; item = tree.GetNextSiblingItem(item)) {
                    if(tree.GetItemState(item, TVIS_SELECTED) & TVIS_SELECTED) result.insert(item);
                    walk(tree.GetChildItem(item));
                }
            };
            walk(tree.GetRootItem()); return result;
        };
        const HTREEITEM head = find(L"head0"), child = find(L"child0"), leaf = find(L"leaf0"), other = find(L"head1");
        report.AppendFormat("nodes=%u\nhead=%d\nchild=%d\nleaf=%d\nother=%d\n", tree.GetCount(), head != NULL, child != NULL, leaf != NULL, other != NULL);
        FBE_TREE_CHECK(head && child && leaf && other && tree.GetCount() >= 100);
        phase = "initial-window-selection";
        tree.EnsureVisible(child);
        input(head, 0, false); input(other, MK_CONTROL, false);
        const auto originalSelection = selected();
        report.AppendFormat("initial_selected=%u\n", static_cast<unsigned>(originalSelection.size()));
        FBE_TREE_CHECK(originalSelection.size() == 2);
        phase = "expand-window-messages";
        for(int index = 0; index < 20; ++index) {
            const bool expanded = (tree.GetItemState(child, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
            input(child, index % 3 == 1 ? MK_CONTROL : (index % 3 == 2 ? MK_SHIFT : 0), true);
            FBE_TREE_CHECK(expanded != ((tree.GetItemState(child, TVIS_EXPANDED) & TVIS_EXPANDED) != 0));
            FBE_TREE_CHECK(selected() == originalSelection);
        }
        report += "toggle_20=1\nbutton_modifiers_selection=1\n";
        phase = "double-click-button";
        const bool expanded = (tree.GetItemState(child, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
        input(child, 0, true, WM_LBUTTONDBLCLK);
        FBE_TREE_CHECK(expanded != ((tree.GetItemState(child, TVIS_EXPANDED) & TVIS_EXPANDED) != 0));
        FBE_TREE_CHECK(selected() == originalSelection);
        report += "double_click_button=1\n";
        phase = "keyboard-selection-focus";
        tree.EnsureVisible(leaf); tree.SetFocus();
        BYTE keys[256] = {}; FBE_TREE_CHECK(::GetKeyboardState(keys) != FALSE);
        BYTE controlKeys[256]; memcpy(controlKeys, keys, sizeof(keys)); controlKeys[VK_CONTROL] |= 0x80;
        FBE_TREE_CHECK(::SetKeyboardState(controlKeys) != FALSE);
        tree.SendMessage(WM_KEYDOWN, 'A');
        ::SetKeyboardState(keys);
        const auto all = selected(); report.AppendFormat("ctrl_a_selected=%u\n", static_cast<unsigned>(all.size())); FBE_TREE_CHECK(all.size() > 5);
        phase = "button-after-ctrl-a";
        input(child, 0, true); FBE_TREE_CHECK(selected() == all);
        input(child, MK_CONTROL, true); FBE_TREE_CHECK(selected() == all);
        report += "button_after_ctrl_a=1\n";
        phase = "ctrl-a-exclude";
        input(other, MK_CONTROL, false);
        auto excluded = all; excluded.erase(other); FBE_TREE_CHECK(selected() == excluded);
        phase = "ctrl-a-include";
        input(other, MK_CONTROL, false); FBE_TREE_CHECK(selected() == all);
        phase = "focus-return";
        m_doc->m_body.SetFocus(); tree.SetFocus(); FBE_TREE_CHECK(selected() == all);
        phase = "shift-selection";
        input(head, 0, false); input(child, MK_SHIFT, false);
        FBE_TREE_CHECK((tree.GetItemState(head, TVIS_SELECTED) & TVIS_SELECTED) && (tree.GetItemState(child, TVIS_SELECTED) & TVIS_SELECTED));
        report += "ctrl_a_exclude=1\nshift_focus=1\n";
        phase = "document-context-menu";
        const auto beforeMenu = selected();
        RECT contextLabel = {}; FBE_TREE_CHECK(tree.GetItemRect(child, &contextLabel, TRUE) != FALSE);
        POINT contextPoint = {contextLabel.left + 2, (contextLabel.top + contextLabel.bottom) / 2};
        tree.ClientToScreen(&contextPoint);
        const UINT_PTR menuTimer = ::SetTimer(NULL, 0, 100, CloseNavigationPopupForTest);
        FBE_TREE_CHECK(menuTimer != 0);
        const LRESULT menuResult = tree.SendMessage(WM_CONTEXTMENU, reinterpret_cast<WPARAM>(tree.m_hWnd), MAKELPARAM(contextPoint.x, contextPoint.y));
        ::KillTimer(NULL, menuTimer);
        FBE_TREE_CHECK(menuResult == 1 && selected() == beforeMenu);
        report += "document_context_menu=1\n";
        phase = "caret-path";
        const auto selection = selected();
        for(LPCWSTR id : {L"leaf-title0", L"leaf-text0", L"leaf0"}) {
            phase = CStringA("caret-") + CStringA(id);
            selectCaret(id);
            MSHTML::IHTMLElementPtr current(tree.CurrentStructureItem() ? reinterpret_cast<MSHTML::IHTMLElement*>(tree.GetItemData(tree.CurrentStructureItem())) : NULL);
            report.AppendFormat("caret_item=%s\n", current ? CStringA(static_cast<LPCWSTR>(current->id)).GetString() : "<none>");
            FBE_TREE_CHECK(tree.CurrentStructureItem() == leaf);
            FBE_TREE_CHECK(tree.StructureHighlight(leaf) == 2 && tree.StructureHighlight(child) == 1 && tree.StructureHighlight(head) == 1);
            FBE_TREE_CHECK(selected() == selection);
        }
        phase = "indexed-caret-lookup";
        const ULONGLONG fallbacks = tree.StructureLinearFallbackCount();
        for(int index = 0; index < 20; ++index) m_document_tree.HighlightItemAtPos(element(index % 2 ? L"head-title0" : L"leaf-text0"));
        FBE_TREE_CHECK(tree.StructureLinearFallbackCount() == fallbacks);
        report += "indexed_caret_lookup=1\n";
        selectCaret(L"head-title0"); FBE_TREE_CHECK(tree.CurrentStructureItem() == head);
        selectCaret(L"child-text0"); FBE_TREE_CHECK(tree.CurrentStructureItem() == child);
        tree.Expand(head, TVE_COLLAPSE);
        selectCaret(L"leaf-text0");
        FBE_TREE_CHECK(tree.CurrentStructureItem() == leaf && tree.StructureHighlight(head) == 2);
        FBE_TREE_CHECK(!(tree.GetItemState(head, TVIS_EXPANDED) & TVIS_EXPANDED));
        verifyMarker(head);
        tree.Expand(head, TVE_EXPAND); tree.Expand(child, TVE_COLLAPSE); paint();
        FBE_TREE_CHECK(tree.StructureHighlight(child) == 2);
        verifyMarker(child); verifyMarker(head);
        report += "caret_title_section_paragraph=1\nancestor_chain=1\ncollapsed_parent=1\n";
        phase = "incremental-paint";
        auto pixels = [&]() {
            RECT client = {}; tree.GetClientRect(&client);
            const int width = client.right, height = client.bottom; FBE_TREE_CHECK(width > 0 && height > 0);
            BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
            void* bits = NULL; HDC source = tree.GetDC(); HDC memory = ::CreateCompatibleDC(source);
            HBITMAP bitmap = ::CreateDIBSection(source, &info, DIB_RGB_COLORS, &bits, NULL, 0);
            FBE_TREE_CHECK(source && memory && bitmap && bits);
            HGDIOBJ old = ::SelectObject(memory, bitmap);
            const BOOL copied = ::BitBlt(memory, 0, 0, width, height, source, 0, 0, SRCCOPY);
            ::GdiFlush();
            std::vector<DWORD> result(static_cast<DWORD*>(bits), static_cast<DWORD*>(bits) + width * height);
            ::SelectObject(memory, old); ::DeleteObject(bitmap); ::DeleteDC(memory); tree.ReleaseDC(source);
            FBE_TREE_CHECK(copied != FALSE); return result;
        };
        for(InterfaceTheme theme : {INTERFACE_THEME_LIGHT, INTERFACE_THEME_DARK}) {
            ThemeManager::SetSelectedTheme(theme); ThemeManager::ApplyToWindow(m_hWnd);
            tree.EnsureVisible(leaf); tree.EnsureVisible(head); tree.SetFocus();
            phase = "marker-text-width";
            selectCaret(L"leaf-text0");
            WCHAR originalLabel[1024] = {};
            FBE_TREE_CHECK(tree.GetItemText(leaf, originalLabel, _countof(originalLabel)) != FALSE);
            CString longLabel;
            for(int repeat = 0; repeat < 100; ++repeat) longLabel += L"Long section title ";
            for(const CString& text : {CString(L"<section>"), CString(L"\x0413\x043b\x0430\x0432\x0430 \x043f\x0435\x0440\x0432\x0430\x044f"), longLabel}) {
                FBE_TREE_CHECK(tree.SetItemText(leaf, text) != FALSE);
                tree.SendMessage(WM_HSCROLL, SB_LEFT);
                tree.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
                RECT label = {}, client = {};
                FBE_TREE_CHECK(tree.GetItemRect(leaf, &label, TRUE) != FALSE);
                tree.GetClientRect(&client);
                if(text == longLabel) FBE_TREE_CHECK(label.right > client.right);
                else {
                    FBE_TREE_CHECK(label.right <= client.right);
                    FBE_TREE_CHECK(label.right - label.left > UiMetrics::ScaleForDpi(24, UiMetrics::DpiForWindow(tree)));
                }
                verifyMarker(leaf); verifyMarker(child); verifyMarker(head);
                report.AppendFormat("marker_label_width=%ld\nmarker_visible_width=%ld\n", label.right - label.left,
                    (std::min)(label.right, client.right) - (std::max)(label.left, client.left));
                if(text == longLabel) {
                    tree.SendMessage(WM_HSCROLL, SB_RIGHT); paint();
                    FBE_TREE_CHECK(tree.GetItemRect(leaf, &label, TRUE) != FALSE && label.left < client.left);
                    verifyMarker(leaf);
                }
            }
            FBE_TREE_CHECK(tree.SetItemText(leaf, originalLabel) != FALSE);
            tree.SendMessage(WM_HSCROLL, SB_LEFT);
            tree.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
            report.AppendFormat("marker_text_width_theme_%d=1\n", static_cast<int>(theme));
            for(int index = 0; index < 24; ++index) {
                phase.Format("paint-%d-%d", static_cast<int>(theme), index);
                selectCaret(index % 2 ? L"head-text0" : L"leaf-text0");
                const auto incremental = pixels();
                tree.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
                const auto complete = pixels();
                if(incremental != complete) {
                    size_t count = 0; for(size_t pixel = 0; pixel < incremental.size(); ++pixel) if(incremental[pixel] != complete[pixel]) ++count;
                    report.AppendFormat("different_pixels=%u\n", static_cast<unsigned>(count));
                }
                FBE_TREE_CHECK(incremental == complete);
                RECT marker = {}; FBE_TREE_CHECK(tree.StructureMarkerRect(tree.CurrentStructureItem(), marker));
                RECT label = {}; FBE_TREE_CHECK(tree.GetItemRect(tree.CurrentStructureItem(), &label, TRUE) != FALSE);
                FBE_TREE_CHECK(marker.left == label.left && marker.top >= label.top && marker.bottom == label.bottom);
                verifyMarker(tree.CurrentStructureItem()); verifyMarker(head);
                if(tree.StructureHighlight(child)) verifyMarker(child);
                else { RECT cleared = {}; FBE_TREE_CHECK(!tree.StructureMarkerRect(child, cleared)); }
            }
            tree.SendMessage(WM_VSCROLL, SB_BOTTOM); paint();
            tree.SendMessage(WM_VSCROLL, SB_TOP); paint();
            selectCaret(L"leaf-text0");
            const auto scrolled = pixels();
            tree.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
            FBE_TREE_CHECK(scrolled == pixels());
        }
        report.AppendFormat("paint_light_dark=1\nscroll_clear=1\ndpi=%u\nhigh_contrast=%s\n", UiMetrics::DpiForWindow(tree), ThemeManager::IsHighContrast() ? "RUN" : "NOT RUN");
        phase = "document-preserved";
        report.AppendFormat("xml_preserved=%d\ndirty_preserved=%d\nversion_before=%ld\nversion_after=%ld\nundo_preserved=%d\nredo_preserved=%d\n",
            xml() == before, m_doc->DocChanged() == dirty, revision, m_doc->m_body.GetVersionNumber(), undoBefore == history(false), redoBefore == history(true));
        FBE_TREE_CHECK(xml() == before && m_doc->DocChanged() == dirty && undoBefore == history(false) && redoBefore == history(true));
        report += "document_preserved=1\n";
        phase = "rebuild-current-position";
        tree.DeleteAllItems(); FBE_TREE_CHECK(tree.CurrentStructureItem() == NULL);
        m_document_tree.GetDocumentStructure(document);
        FBE_TREE_CHECK(tree.CurrentStructureItem() == find(L"leaf0"));
        report += "rebuild_current=1\n";
        phase = "structure-undo-redo";
        checkpoint();
        tree.EnsureVisible(find(L"head1")); input(find(L"head1"), 0, false);
        tree.SendMessage(WM_COMMAND, ID_DT_RIGHT_ONE);
        const CString moved = xml(); FBE_TREE_CHECK(moved != before && m_doc->DocChanged());
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
        FBE_TREE_CHECK(xml() == before && m_doc->DocChanged() == dirty);
        m_document_tree.GetDocumentStructure(document);
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));
        FBE_TREE_CHECK(xml() == moved && m_doc->DocChanged());
        m_document_tree.GetDocumentStructure(document);
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
        FBE_TREE_CHECK(xml() == before);
        selectCaret(L"leaf-text0");
        m_document_tree.GetDocumentStructure(document);
        FBE_TREE_CHECK(tree.CurrentStructureItem() == find(L"leaf0"));
        report += "structure_move_undo_redo=1\n";
        auto roundTrip = [&](const std::function<void()>& operation) {
            const CString original = xml(); const bool originalDirty = m_doc->DocChanged();
            operation(); const CString changed = xml(); FBE_TREE_CHECK(changed != original && m_doc->DocChanged());
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO))); FBE_TREE_CHECK(xml() == original && m_doc->DocChanged() == originalDirty);
            m_document_tree.GetDocumentStructure(document);
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO))); FBE_TREE_CHECK(xml() == changed);
            m_document_tree.GetDocumentStructure(document);
            CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO))); FBE_TREE_CHECK(xml() == original);
            m_document_tree.GetDocumentStructure(document);
        };
        phase = "title-edit-undo-redo";
        const auto titleParagraph = element(L"head-title0");
        m_doc->m_body.SetFocus();
        m_doc->m_body.MarkupServices()->BeginUndoUnit(L"Edit section title");
        titleParagraph->insertAdjacentText(L"beforeEnd", L" changed");
        m_doc->m_body.MarkupServices()->EndUndoUnit();
        tree.UpdateDocumentStructure(document, titleParagraph);
        report.AppendFormat("title_after_edit=%s\n", static_cast<LPCSTR>(CW2A(caption(L"head0"), CP_UTF8)));
        FBE_TREE_CHECK(caption(L"head0") == L"Head 0 changed Heading text");
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
        tree.UpdateDocumentStructure(document, element(L"head0"));
        report.AppendFormat("title_after_undo=%s\ntitle_undo_xml=%d\n", static_cast<LPCSTR>(CW2A(caption(L"head0"), CP_UTF8)), xml() == before);
        FBE_TREE_CHECK(caption(L"head0") == L"Head 0 Heading text" && xml() == before);
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_REDO)));
        tree.UpdateDocumentStructure(document, element(L"head0"));
        FBE_TREE_CHECK(caption(L"head0") == L"Head 0 changed Heading text");
        CheckError(static_cast<HRESULT>(m_doc->m_body.ExecCommand(IDM_UNDO)));
        m_document_tree.GetDocumentStructure(document);
        FBE_TREE_CHECK(xml() == before);
        report += "title_edit_undo_redo=1\n";
        for(UINT command : {ID_DT_RIGHT_SMART, ID_DT_LEFT, ID_DT_MERGE}) {
            phase.Format("structure-command-%u", command);
            checkpoint();
            const LPCWSTR id = command == ID_DT_RIGHT_SMART ? L"head1" : (command == ID_DT_LEFT ? L"leaf1" : L"merge-first");
            input(find(id), 0, false);
            roundTrip([&]() { tree.SendMessage(WM_COMMAND, command); });
        }
        report += "structure_smart_left_merge=1\n";
        phase = "native-drag-drop";
        checkpoint();
        input(find(L"head1"), 0, false);
        tree.EnsureVisible(find(L"head0"));
        roundTrip([&]() {
            RECT from = {}, to = {};
            FBE_TREE_CHECK(tree.GetItemRect(find(L"head1"), &from, TRUE) && tree.GetItemRect(find(L"head0"), &to, TRUE));
            BYTE savedKeys[256] = {}; FBE_TREE_CHECK(::GetKeyboardState(savedKeys) != FALSE);
            BYTE dragKeys[256]; memcpy(dragKeys, savedKeys, sizeof(savedKeys)); dragKeys[VK_LBUTTON] |= 0x80;
            ::SetKeyboardState(dragKeys);
            const int y = (from.top + from.bottom) / 2;
            tree.PostMessage(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(from.left + 2, y));
            tree.PostMessage(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(from.left + 20, y));
            for(int index = 0; index < 2; ++index) tree.PostMessage(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(to.left + 2, (to.top + to.bottom) / 2));
            tree.PostMessage(WM_LBUTTONUP, 0, MAKELPARAM(to.left + 2, (to.top + to.bottom) / 2));
            bool started = false; MSG message = {};
            while(::PeekMessage(&message, tree, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) {
                ::DispatchMessage(&message); started = started || tree.IsStructuralDragActive();
            }
            ::SetKeyboardState(savedKeys);
            FBE_TREE_CHECK(started && !tree.IsStructuralDragActive());
        });
        report += "native_drag_drop_undo_redo=1\n";
        phase = "clone-section";
        checkpoint();
        roundTrip([&]() {
            m_doc->m_body.Call(L"CloneContainer", element(L"head0"));
            MSHTML::IHTMLDOMNodePtr clone(MSHTML::IHTMLDOMNodePtr(element(L"head0"))->nextSibling);
            while(clone && clone->nodeType != 1) clone = clone->nextSibling;
            FBE_TREE_CHECK(clone != NULL && U::FindTitle(clone).IsEmpty());
            m_document_tree.GetDocumentStructure(document);
            m_document_tree.HighlightItemAtPos(MSHTML::IHTMLElementPtr(clone));
            CString text; FBE_TREE_CHECK(tree.GetItemText(tree.CurrentStructureItem(), text));
            FBE_TREE_CHECK(text == L"<section>");
        });
        report += "clone_section_caption_undo_redo=1\n";
        phase = "save-reopen";
        checkpoint();
        const CString saveXml = xml(), saveCaption = caption(L"head0");
        const CString savedPath = U::GetProgDir() + L"tree-title-roundtrip.fb2";
        int saveErrorLine = 0, saveErrorColumn = 0;
        const bool saveValid = m_doc->Validate(saveErrorLine, saveErrorColumn);
        report.AppendFormat("save_validation=%d,line:%d,column:%d\n", saveValid, saveErrorLine, saveErrorColumn);
        FBE_TREE_CHECK(saveValid);
        FBE_TREE_CHECK(m_doc->Save(savedPath));
        FBE_TREE_CHECK(LoadFile(savedPath) == OK);
        document = m_doc->m_body.Document();
        m_document_tree.GetDocumentStructure(document);
        FBE_TREE_CHECK(xml() == saveXml && caption(L"head0") == saveCaption);
        report += "title_save_reopen_xml=1\n";
        phase = "shifted-source-index";
        checkpoint();
        // Deliberately change this disposable fixture without notifying the
        // tree, so the old sourceIndex keys must not resolve a wrong item.
        element(L"head-title0")->insertAdjacentHTML(L"afterBegin", L"<EM>Index shift</EM>");
        m_document_tree.HighlightItemAtPos(element(L"leaf-text0"));
        FBE_TREE_CHECK(tree.CurrentStructureItem() == find(L"leaf0"));
        report += "shifted_source_index=1\n";
        passed = true;
    } catch(_com_error& error) { report.AppendFormat("hresult=0x%08lX\n", static_cast<unsigned long>(error.Error())); }
      catch(...) { report += "exception=1\n"; }
    report.AppendFormat("phase=%s\nresult=%s\n", phase.GetString(), passed ? "pass" : "fail");
    DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
    output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
#undef FBE_TREE_CHECK
}
