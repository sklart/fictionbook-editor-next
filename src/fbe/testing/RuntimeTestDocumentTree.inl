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
        const auto document = m_doc->m_body.Document();
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
            const bool selectedItem = (tree.GetItemState(item, TVIS_SELECTED) & TVIS_SELECTED) != 0;
            const COLORREF expected = ThemeManager::IsHighContrast() ? ::GetSysColor(selectedItem ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT) :
                (selectedItem ? ThemeManager::SelectionTextColor() : (tree.StructureHighlight(item) == 2 ? ThemeManager::AccentColor() : ThemeManager::SecondaryTextColor()));
            HDC dc = tree.GetDC(); const COLORREF actual = ::GetPixel(dc, marker.left, marker.bottom - 1); tree.ReleaseDC(dc);
            if(actual != expected) report.AppendFormat("marker_rank=%d\nmarker_rect=%ld,%ld,%ld,%ld\nmarker_actual=0x%08lX\nmarker_expected=0x%08lX\n",
                tree.StructureHighlight(item), marker.left, marker.top, marker.right, marker.bottom, actual, expected);
            FBE_TREE_CHECK(actual == expected);
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
