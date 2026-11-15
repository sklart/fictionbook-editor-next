if (IsFbeTestScenario(L"replace-one-matrix"))
{
    CStringA report;
    bool opened = false, selected = false, changed = false, undo = false, redo = false, saved = false, reopened = false, xmlMatches = false, rejected = false;
    int dismissed = 0;
    auto pump = []() {
        const ULONGLONG until = ::GetTickCount64() + 100;
        while (::GetTickCount64() < until) {
            MSG message = {};
            while (::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) { ::TranslateMessage(&message); ::DispatchMessageW(&message); }
            ::Sleep(1);
        }
    };
    try {
        wchar_t pattern[1024] = {}, replacement[1024] = {}, directory[MAX_PATH] = {}, mode[64] = {};
        if (!::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MATRIX_PATTERN", pattern, _countof(pattern)) ||
            !::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MATRIX_OUTPUT", directory, _countof(directory))) CheckError(E_INVALIDARG);
        ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MATRIX_REPLACEMENT", replacement, _countof(replacement));
        ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_MATRIX_MODE", mode, _countof(mode));
        ShowView(BODY); m_doc->m_body.SetFocus(); pump();
        MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
        MSHTML::IHTMLElementPtr body(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"fbw_body"));
        if (!body) CheckError(E_FAIL);
        const CString beforeHtml(static_cast<LPCWSTR>(body->innerHTML));
        const CString beforeXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
        {
            CAtlFile file;
            if (SUCCEEDED(file.Create(CString(directory) + L"\\before.html", GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) {
                const CStringA utf8(CW2A(beforeHtml, CP_UTF8)); DWORD written = 0;
                file.Write(static_cast<LPCSTR>(utf8), utf8.GetLength(), &written);
            }
        }
        if (wcscmp(mode, L"selection") == 0 || wcscmp(mode, L"section") == 0 || wcscmp(mode, L"up") == 0) {
            MSHTML::IHTMLBodyElementPtr bodyElement(document->body);
            MSHTML::IHTMLTxtRangePtr caret(bodyElement->createTextRange());
            MSHTML::IHTMLElementPtr target(MSHTML::IHTMLDocument3Ptr(document)->getElementById(L"target"));
            if (!target || !caret) CheckError(E_FAIL);
            caret->moveToElementText(target);
            if (wcscmp(mode, L"selection") == 0) {
                if (!caret->findText(L"12", 0, 0)) CheckError(E_FAIL);
            } else if (wcscmp(mode, L"up") == 0) caret->collapse(VARIANT_FALSE);
            else { caret->moveStart(L"character", 2); caret->collapse(VARIANT_TRUE); }
            caret->select(); pump();
        }
        BOOL handled = FALSE; m_doc->m_body.OnReplace(0, ID_EDIT_REPLACE, m_doc->m_body, handled); pump();
        HWND dialog = m_doc->m_body.m_replace_dlg ? m_doc->m_body.m_replace_dlg->m_hWnd : NULL;
        opened = dialog != NULL;
        static int* dismissedPtr = nullptr; dismissedPtr = &dismissed;
        const UINT_PTR timer = ::SetTimer(NULL, 0, 30, [](HWND, UINT, UINT_PTR, DWORD) {
            ::EnumThreadWindows(::GetCurrentThreadId(), [](HWND window, LPARAM) -> BOOL {
                wchar_t name[64] = {}; ::GetClassNameW(window, name, _countof(name));
                if ((wcscmp(name, L"#32770") == 0 || wcscmp(name, L"FBEThemedMessageDialog") == 0) && !::GetDlgItem(window, IDC_TEXT)) {
                    const int id = ::GetDlgItem(window, IDOK) ? IDOK : ::GetDlgItem(window, IDCANCEL) ? IDCANCEL : 0;
                    if (id) { ++*dismissedPtr; ::PostMessageW(window, WM_COMMAND, id, 0); }
                }
                return TRUE;
            }, 0);
        });
        if (!opened || !timer) CheckError(E_FAIL);
        ::SetWindowTextW(::GetDlgItem(dialog, IDC_TEXT), pattern);
        ::SetWindowTextW(::GetDlgItem(dialog, IDC_REPLACE), replacement);
        ::CheckDlgButton(dialog, IDC_REGEXP, BST_CHECKED);
        ::SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_REGEXP, BN_CLICKED), reinterpret_cast<LPARAM>(::GetDlgItem(dialog, IDC_REGEXP)));
        ::CheckDlgButton(dialog, IDC_FIND_UNICODE_PROPERTIES, BST_CHECKED);
        ::SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_FIND_UNICODE_PROPERTIES, BN_CLICKED), reinterpret_cast<LPARAM>(::GetDlgItem(dialog, IDC_FIND_UNICODE_PROPERTIES)));
        if (wcscmp(mode, L"selection") == 0 || wcscmp(mode, L"section") == 0) {
            const int index = wcscmp(mode, L"selection") == 0 ? 2 : 1;
            ::SendMessageW(::GetDlgItem(dialog, IDC_FIND_SCOPE), CB_SETCURSEL, index, 0);
            ::SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_FIND_SCOPE, CBN_SELCHANGE), reinterpret_cast<LPARAM>(::GetDlgItem(dialog, IDC_FIND_SCOPE)));
        }
        if (wcscmp(mode, L"up") == 0) {
            ::CheckRadioButton(dialog, IDC_UP, IDC_DOWN, IDC_UP);
            ::SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDC_UP, BN_CLICKED), reinterpret_cast<LPARAM>(::GetDlgItem(dialog, IDC_UP)));
            ::SendMessageW(::GetDlgItem(dialog, ID_FIND_NEXT), BM_CLICK, 0, 0);
        } else ::SendMessageW(::GetDlgItem(dialog, IDC_FIND_FROM_START), BM_CLICK, 0, 0);
        pump();
        selected = dismissed == 0;
        if (selected) {
            MSHTML::IHTMLTxtRangePtr found(document->selection->createRange());
            if (found) {
                const _bstr_t foundText(found->text), foundHtml(found->htmlText);
                MSHTML::IHTMLElementPtr parent(found->parentElement());
                report.AppendFormat("found-length=%u\nfound-html-length=%u\nfound-parent=%d\n", foundText.length(), foundHtml.length(), parent ? 1 : 0);
            }
        }
        if (selected) {
            ::SendMessageW(::GetDlgItem(dialog, IDC_REPLACE_ONE), BM_CLICK, 0, 0); pump();
            if (wcscmp(mode, L"repeat") == 0) { ::SendMessageW(::GetDlgItem(dialog, IDC_REPLACE_ONE), BM_CLICK, 0, 0); pump(); }
        }
        ::KillTimer(NULL, timer); dismissedPtr = nullptr;
        const CString afterHtml(static_cast<LPCWSTR>(body->innerHTML));
        const CString afterXml(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml));
        changed = beforeXml != afterXml;
        rejected = wcscmp(mode, L"cross-paragraph") == 0 && selected && dismissed > 0 && beforeHtml == afterHtml && !changed;
        ::SendMessageW(dialog, WM_CLOSE, 0, 0); pump();
        if (changed) {
            m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump();
            if (wcscmp(mode, L"repeat") == 0) { SendMessage(WM_COMMAND, ID_EDIT_UNDO, 0); pump(); }
            undo = CString(static_cast<LPCWSTR>(body->innerHTML)) == beforeHtml && CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == beforeXml;
            m_doc->m_body.SetFocus(); SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump();
            if (wcscmp(mode, L"repeat") == 0) { SendMessage(WM_COMMAND, ID_EDIT_REDO, 0); pump(); }
            redo = CString(static_cast<LPCWSTR>(body->innerHTML)) == afterHtml && CString(static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml)) == afterXml;
        }
        saved = !!m_doc->Save(CString(directory) + L"\\after.fb2");
        reopened = saved && LoadFile(CString(directory) + L"\\after.fb2") == OK; pump();
        const CString reopenedXml(reopened ? static_cast<LPCWSTR>(m_doc->CreateDOMForAnalysis()->xml) : L"");
        xmlMatches = reopened && reopenedXml == afterXml;
        if (!xmlMatches && reopened) {
            CString withoutEmptyFormatting(afterXml);
            withoutEmptyFormatting.Replace(L"<emphasis/>", L"");
            withoutEmptyFormatting.Replace(L"<strong/>", L"");
            xmlMatches = withoutEmptyFormatting == reopenedXml;
            if (xmlMatches) report += "normalized-empty-formatting=1\n";
        }
        if (!xmlMatches && reopened) {
            auto saveXml = [](const CString& path, const CString& text) {
                CAtlFile file; if (FAILED(file.Create(path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return;
                const CStringA utf8(CW2A(text, CP_UTF8)); DWORD written = 0; file.Write(static_cast<LPCSTR>(utf8), utf8.GetLength(), &written);
            };
            saveXml(CString(directory) + L"\\snapshot-before.xml", afterXml);
            saveXml(CString(directory) + L"\\snapshot-reopened.xml", reopenedXml);
        }
    } catch (const _com_error& e) { report.AppendFormat("hresult=0x%08lx\n", static_cast<unsigned long>(e.Error())); }
      catch (...) { report += "exception=1\n"; }
    report.AppendFormat("opened=%d\nselected=%d\nchanged=%d\nundo=%d\nredo=%d\nsaved=%d\nreopened=%d\nxml-matches=%d\nrejected=%d\ndismissed=%d\n", opened, selected, changed, undo, redo, saved, reopened, xmlMatches, rejected, dismissed);
    DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Flush(); output.Close();
    ::PostQuitMessage(opened && selected && (rejected || (changed && undo && redo)) && saved && reopened && xmlMatches ? 0 : 1); return 0;
}
