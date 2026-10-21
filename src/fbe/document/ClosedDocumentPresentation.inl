// Frame presentation for the explicit closed state. The private FB::Doc only
// keeps legacy MSHTML/configuration code alive; it is never an open book.
bool CMainFrame::RequiresOpenDocument(UINT command)
{
    if ((command >= ID_PLUGIN_IMPORT_FIRST && command <= ID_PLUGIN_IMPORT_LAST) ||
        (command >= ID_FILE_MRU_FIRST && command <= ID_FILE_MRU_LAST) ||
        (command >= ATL_IDW_BAND_FIRST && command <= ATL_IDW_BAND_FIRST + 8) ||
        (command >= ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_FIRST && command <= ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_LAST) ||
        (command >= ID_STATUS_PANE_POSITION && command <= ID_STATUS_PANE_INSERT_MODE)) return false;
    switch (command) {
    case ID_FILE_NEW: case ID_FILE_OPEN: case ID_APP_EXIT: case ID_FILE_RESTART:
    case ID_APP_ABOUT: case ID_VIEW_OPTIONS: case ID_TOOLS_OPTIONS:
    case ID_VIEW_STATUS_BAR: case ID_VIEW_SCRIPT_TOOLBARS_MANAGE:
    case ID_VIEW_TREE: case ID_PANE_CLOSE:
    case ID_TOOLS_REFRESH_SCRIPTS: case ID_TOOLS_CUSTOMIZE:
    case ID_TOOLS_DIAGNOSTIC_TRACE: case ID_TOOLS_OPEN_DIAGNOSTIC_LOG:
    case ID_TOOLS_OPEN_DIAGNOSTIC_FOLDER: case ID_TOOLS_COPY_DIAGNOSTIC_LOG_PATH:
    case ID_TOOLS_CLEAR_DIAGNOSTIC_LOGS: case ID_TOOLS_CREATE_DIAGNOSTIC_PACKAGE:
    case ID_FILE_IMPORT_PLACEHOLDER:
    case IDCANCEL:
        return false;
    default: return true;
    }
}

void CMainFrame::SetDocumentCommandAvailability(bool available)
{
    for (const auto* item = GetUpdateUIMap(); item->m_nID != static_cast<WORD>(-1); ++item)
        if (RequiresOpenDocument(item->m_nID)) UIEnable(item->m_nID, available);
    const auto updateMenu = [&](const auto& self, HMENU menu) -> void {
        for (int i = 0; i < ::GetMenuItemCount(menu); ++i) {
            HMENU child = ::GetSubMenu(menu, i);
            if (child) self(self, child);
            else {
                const UINT command = ::GetMenuItemID(menu, i);
                if (command && command != static_cast<UINT>(-1) && RequiresOpenDocument(command))
                    ::EnableMenuItem(menu, command, MF_BYCOMMAND | (available ? MF_ENABLED : MF_GRAYED));
            }
        }
    };
    updateMenu(updateMenu, m_MenuBar.GetMenu());
    struct ToolbarContext {HWND menu; bool enabled;} context{m_MenuBar, available};
    ::EnumChildWindows(m_hWnd, [](HWND window, LPARAM data) -> BOOL {
        const auto& state = *reinterpret_cast<ToolbarContext*>(data);
        if (window == state.menu) return TRUE;
        wchar_t name[64] = {}; ::GetClassNameW(window, name, _countof(name));
        if (!wcscmp(name, TOOLBARCLASSNAMEW)) {
            const int count = static_cast<int>(::SendMessageW(window, TB_BUTTONCOUNT, 0, 0));
            for (int i = 0; i < count; ++i) {
                TBBUTTON button = {};
                if (::SendMessageW(window, TB_GETBUTTON, i, reinterpret_cast<LPARAM>(&button)) &&
                    !(button.fsStyle & BTNS_SEP) && RequiresOpenDocument(button.idCommand))
                    ::SendMessageW(window, TB_ENABLEBUTTON, button.idCommand, state.enabled);
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    UIUpdateToolBar();
}

void CMainFrame::DetachDocumentForClose()
{
    StopIncSearch(true);
    HideCaretMarker();
    if (m_Speller) m_Speller->DetachDocument();
    m_doc->m_body.CloseFindDialog(m_doc->m_body.m_find_dlg);
    m_doc->m_body.CloseFindDialog(m_doc->m_body.m_replace_dlg);
    m_doc->m_body.CloseFindDialog(m_sci_find_dlg);
    m_doc->m_body.CloseFindDialog(m_sci_replace_dlg);
    m_find_results_pane.Detach(nullptr);
    HideFindResultsPane();
    m_document_tree.m_tree.ClearDocument();
    InvalidateSelectionContext();
    m_editor_selection_state.Reset();
    m_source_view_session.ResetDocument();
    FbeScripts::Status::Invalidate(m_hWnd);
}

void CMainFrame::PresentClosedDocument()
{
    m_closed_document_ui = true;
    FB::Doc::m_active_doc = nullptr;
    m_view.HideActiveWnd();
    m_source.ShowWindow(SW_HIDE);
    m_doc->m_body.ShowWindow(SW_HIDE);
    m_editor_view_state.Reset(EditorView::Body, EditorView::Description);
    m_document_tree.EnableWindow(TRUE);
    m_document_tree.m_tree.SetDocumentAvailable(false);
    m_contextAttributeBars.ClearLinkState(); m_contextAttributeBars.ClearTableState();
    m_contextAttributeBars.SetLinkAvailability({false, false, false, false});
    m_contextAttributeBars.SetTableAvailability({false, false, false, false, false, false, false, false, false});
    m_change_state = false; m_need_title_update = false;
    m_doc_changed = false; m_sel_changed = false; m_want_focus = 0;
    SetWindowText(L"FictionBook Editor Next");
    UISetCheck(ID_VIEW_DESC, false);
    UISetCheck(ID_VIEW_BODY, false);
    UISetCheck(ID_VIEW_SOURCE, false);
    SetDocumentCommandAvailability(false);
    ResetStatusForDocument();
    m_view.Invalidate(TRUE);
}

LRESULT CMainFrame::OnFileClose(WORD, WORD, HWND, BOOL&)
{
    if (!m_document_session.HasDocument() || m_document_close_active ||
        !FbeScripts::ExecutionContexts().empty() || FbeScripts::HasDocumentUpdates() || m_doc->IsSaveTransactionActive()) return 0;
    m_document_close_active = true;
    struct Guard {bool& value; ~Guard() {value = false;}} guard{m_document_close_active};
    if (!DiscardChanges()) return 0;
    try {
        DocumentLifecycleController lifecycle(*this, m_doc, m_document_session, m_view);
        if (!lifecycle.CloseDocument().Succeeded()) { U::ReportError(E_FAIL); return 0; }
        m_source.SendMessage(SCI_SETREADONLY, FALSE);
        m_source.SendMessage(SCI_CLEARALL);
        m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
        m_source.SendMessage(SCI_SETSAVEPOINT);
        ClearSourceValidationAnnotations();
        m_bad_xml = false; m_bad_filename.Empty();
        m_recovery.DeleteIfWritten();
        m_recovery_saved_generation = m_recovery_generation;
        PresentClosedDocument();
        m_view.SetFocus();
    } catch (_com_error& error) { U::ReportError(error); }
    catch (const std::bad_alloc&) { U::ReportError(E_OUTOFMEMORY); }
    return 0;
}
