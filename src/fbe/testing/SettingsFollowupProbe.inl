#include "../XmlSourceThemes.h"
#include <array>
// Called only by the isolated settings runtime scenario.
namespace {
int g_followupWarnings = 0;
CString g_followupMessage;
HWND g_followupDialog = NULL;
BOOL CALLBACK FollowupCloseMessage(HWND window, LPARAM)
{
    if(window != g_followupDialog && (::GetDlgItem(window, IDOK) || ::GetDlgItem(window, IDCANCEL)) &&
        (::GetWindowLongPtrW(window, GWL_STYLE) & WS_POPUP) &&
        !::GetPropW(window, L"FBE_FOLLOWUP_CLOSED"))
    {
        ::SetPropW(window, L"FBE_FOLLOWUP_CLOSED", reinterpret_cast<HANDLE>(1));
        ++g_followupWarnings;
        wchar_t text[4096] = {};
        ::GetDlgItemTextW(window, 65535, text, _countof(text));
        g_followupMessage = text;
        ::PostMessageW(window, WM_COMMAND, ::GetDlgItem(window, IDOK) ? IDOK : IDCANCEL, 0);
    }
    return TRUE;
}
VOID CALLBACK FollowupMessageTimer(HWND, UINT, UINT_PTR, DWORD)
{
    ::EnumThreadWindows(::GetCurrentThreadId(), FollowupCloseMessage, 0);
}

bool FollowupWrite(const CString& path, const char* text)
{
    CAtlFile file;
    if(FAILED(file.Create(path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS))) return false;
    return SUCCEEDED(file.Write(text, static_cast<DWORD>(strlen(text)))) && SUCCEEDED(file.Flush());
}
std::vector<char> FollowupRead(const CString& path)
{
    CAtlFile file; ULONGLONG size = 0;
    if(FAILED(file.Create(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING)) ||
        FAILED(file.GetSize(size)) || size > 8 * 1024 * 1024) return {};
    std::vector<char> data(static_cast<size_t>(size));
    if(size && FAILED(file.Read(data.data(), static_cast<DWORD>(size)))) return {};
    return data;
}
}

bool CSettingsDlg::ProbeFollowup()
{
    SettingsEditSession restoreGlobals(_Settings);
    bool passed = true;
    const HWND owner = ::GetParent(m_hWnd);
    ::RemovePropW(owner, L"FBE_SETTINGS_FOLLOWUP_PHASE");
    auto checkpoint = [&](int phase) {
        if(!passed && !::GetPropW(owner, L"FBE_SETTINGS_FOLLOWUP_PHASE")) {
            ::SetPropW(owner, L"FBE_SETTINGS_FOLLOWUP_PHASE", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(phase)));
        }
    };
    CString error;
    const CString settingsPath = FbeSettings::SettingsFilePath();
    const CString fixture = SettingsFileTransaction::Temporary(settingsPath);
    if(fixture.IsEmpty()) return false;
    ::DeleteFileW(fixture);
    if(!::CreateDirectoryW(fixture, NULL)) return false;
    const CString prefix = CString(::PathFindFileNameW(fixture)) + L"-dictionary.dic";
    const CString source = fixture + L"\\" + prefix;
    const CString target = U::ResolveUserDataFile(prefix);
    const CString legacy = m_spellingPage->m_legacyDirectory;
    m_spellingPage->m_legacyDirectory = fixture + L"\\";
    m_spellingPage->CheckDlgButton(IDC_USESPELLCHECKER, BST_CHECKED);
    m_spellingPage->SetDlgItemText(IDC_CUSTOM_DICT, prefix);
    passed = FollowupWrite(source, "legacy-one\nlegacy-two\nlegacy-three\n") && passed;
    {
        SettingsFileTransaction files;
        passed = m_spellingPage->PrepareCommit(files, error) && files.Commit() && passed;
    }
    passed = !FollowupRead(target).empty() && FollowupRead(target) == FollowupRead(source) && passed;
    passed = FollowupWrite(target, "existing-user-word\n") && passed;
    {
        SettingsFileTransaction files;
        passed = m_spellingPage->PrepareCommit(files, error) && files.Commit() && passed;
    }
    passed = FollowupRead(target) != FollowupRead(source) && passed;
    ::DeleteFileW(target);
    { SettingsFileTransaction files; passed = m_spellingPage->PrepareCommit(files, error) && passed; }
    passed = ::GetFileAttributesW(target) == INVALID_FILE_ATTRIBUTES && !FollowupRead(source).empty() && passed;
    checkpoint(1);
    // A read-only dictionary warns once per changed path, never for the initial unchanged path.
    ::SetFileAttributesW(source, FILE_ATTRIBUTE_READONLY);
    const CString initial = m_spellingPage->m_initialDictionaryPath;
    m_spellingPage->m_initialDictionaryPath = source;
    m_spellingPage->SetDlgItemText(IDC_CUSTOM_DICT, source);
    g_followupWarnings = 0; g_followupDialog = m_hWnd;
    const UINT_PTR timer = ::SetTimer(NULL, 0, 20, FollowupMessageTimer);
    if(timer)
    {
        passed = m_spellingPage->Validate() && g_followupWarnings == 0 && passed;
        m_spellingPage->m_initialDictionaryPath = L"another-dictionary";
        passed = m_spellingPage->Validate() && m_spellingPage->Validate() && g_followupWarnings == 1 && passed;
        m_spellingPage->SetDlgItemText(IDC_CUSTOM_DICT, target);
        m_spellingPage->SetDlgItemText(IDC_CUSTOM_DICT, source);
        passed = m_spellingPage->Validate() && g_followupWarnings == 2 && passed;
        ::KillTimer(NULL, timer);
    }
    else passed = false;
    checkpoint(2);
    ::SetFileAttributesW(source, FILE_ATTRIBUTE_NORMAL);
    m_spellingPage->m_initialDictionaryPath = initial;
    m_spellingPage->m_legacyDirectory = legacy;
    m_spellingPage->SetDlgItemText(IDC_CUSTOM_DICT, L"");
    m_spellingPage->CheckDlgButton(IDC_USESPELLCHECKER, BST_UNCHECKED);
    ::DeleteFileW(source); ::RemoveDirectoryW(fixture);

    const CString opening = m_sourcePage->m_openingThemeId;
    const auto drafts = m_sourcePage->m_themeDrafts;
    const auto history = m_sourcePage->m_themeSelectionHistory;
    DWORD colors[XML_SRC_STYLE_TOKEN_COUNT] = {};
    for(int i = 0; i < XML_SRC_STYLE_TOKEN_COUNT; ++i)
        XmlSourceThemes::GetThemeColor(opening, static_cast<XmlSrcStyleToken>(i), colors[i]);
    CString first, second;
    const bool firstSaved = XmlSourceThemes::SaveThemeAsUser(L"Followup first", colors, first, error);
    const bool secondSaved = XmlSourceThemes::SaveThemeAsUser(L"Followup second", colors, second, error);
    passed = firstSaved && secondSaved && passed;
    unsigned int themeSteps = (firstSaved ? 1u : 0u) | (secondSaved ? 2u : 0u);
    DWORD themeError = 0;
    {
        SettingsFileTransaction files;
        const bool prepared = XmlSourceThemes::PrepareSettingsEdit(files, error);
        if(!prepared) {
            const CStringA diagnostic(CW2A(error, CP_UTF8));
            FollowupWrite(U::GetSettingsDir() + L"settings-theme-failure.txt", diagnostic);
        }
        const bool committed = prepared && files.Commit();
        themeSteps |= (prepared ? 4u : 0u) | (committed ? 8u : 0u);
        themeError = files.Error();
        passed = committed && passed;
    }
    ::SetPropW(owner, L"FBE_SETTINGS_FOLLOWUP_THEME_STEPS", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(themeSteps)));
    ::SetPropW(owner, L"FBE_SETTINGS_FOLLOWUP_THEME_ERROR", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(themeError)));
    const CString firstPath = U::GetSettingsDir() + L"Themes\\" + first + L".fbetheme";
    const CString secondPath = U::GetSettingsDir() + L"Themes\\" + second + L".fbetheme";
    const auto firstBytes = FollowupRead(firstPath), secondBytes = FollowupRead(secondPath);
    checkpoint(3);
    auto select = [&](const CString& id) {
        m_sourcePage->ReloadSourceThemes(id);
        BOOL handled = FALSE; m_sourcePage->OnSourcePaletteChanged(0, 0, NULL, handled);
    };
    m_sourcePage->m_source_colors[XML_SRC_COLOR_COMMENT].SetColor(RGB(21, 43, 65));
    select(second);
    passed = m_sourcePage->DeleteTheme(second, error) && m_sourcePage->m_draftThemeId == opening &&
        m_sourcePage->m_source_colors[XML_SRC_COLOR_COMMENT].GetColor() == RGB(21, 43, 65) && passed;
    XmlSourceThemes::EndSettingsEdit(); XmlSourceThemes::BeginSettingsEdit();
    passed = !secondBytes.empty() && FollowupRead(secondPath) == secondBytes && passed;
    checkpoint(4);
    // A later selection survives deleting a different inactive theme.
    select(first); select(second);
    passed = m_sourcePage->DeleteTheme(second, error) && m_sourcePage->m_draftThemeId == first && passed;
    checkpoint(5);
    XmlSourceThemes::EndSettingsEdit(); XmlSourceThemes::BeginSettingsEdit();
    // Deleting the original active user theme selects a built-in fallback.
    m_sourcePage->m_openingThemeId = first;
    select(first);
    passed = m_sourcePage->DeleteTheme(first, error) && !XmlSourceThemes::IsUserTheme(m_sourcePage->m_draftThemeId) && passed;
    checkpoint(6);
    XmlSourceThemes::EndSettingsEdit(); XmlSourceThemes::BeginSettingsEdit();
    select(first);
    passed = m_sourcePage->DeleteTheme(first, error) && passed;

    // Settings.xml is last in the batch: a write denial must undo Words.xml and theme deletion.
    passed = _Settings.TrySave(error) && passed;
    checkpoint(7);
    const auto settingsBefore = FollowupRead(settingsPath);
    const CString wordsPath = FbeSettings::WordsFilePath();
    const auto wordsBefore = FollowupRead(wordsPath);
    const auto words = m_wordsPage->m_words;
    const bool dirty = m_wordsPage->m_wordsDirty;
    m_wordsPage->m_words.push_back(WordsItem(L"followup-word", 37)); m_wordsPage->m_wordsDirty = true;
    const bool backup = _Settings.GetCreateBackupFile();
    const bool restart = _Settings.NeedRestart();
    m_generalPage->CheckDlgButton(IDC_CREATE_BACKUP_FILE, backup ? BST_UNCHECKED : BST_CHECKED);
    ::SetFileAttributesW(settingsPath, FILE_ATTRIBUTE_READONLY);
    passed = !TryCommit(error) && !error.IsEmpty() && ::IsWindow(m_hWnd) &&
        _Settings.GetCreateBackupFile() == backup && _Settings.NeedRestart() == restart &&
        FollowupRead(settingsPath) == settingsBefore && FollowupRead(wordsPath) == wordsBefore &&
        FollowupRead(firstPath) == firstBytes && passed;
    checkpoint(8);
    ::SetFileAttributesW(settingsPath, FILE_ATTRIBUTE_NORMAL);
    // Failed standalone serialization is observable and cannot rewrite on destruction.
    HANDLE lock = ::CreateFileW(settingsPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    passed = lock != INVALID_HANDLE_VALUE && !_Settings.TrySave(error) && FollowupRead(settingsPath) == settingsBefore && passed;
    checkpoint(9);
    if(lock != INVALID_HANDLE_VALUE) ::CloseHandle(lock);
    m_generalPage->CheckDlgButton(IDC_CREATE_BACKUP_FILE, BST_CHECKED);
    m_wordsPage->m_words = words; m_wordsPage->m_wordsDirty = dirty;
    XmlSourceThemes::EndSettingsEdit(); XmlSourceThemes::BeginSettingsEdit();
    passed = FollowupRead(firstPath) == firstBytes && FollowupRead(secondPath) == secondBytes && passed;
    checkpoint(10);
    ::DeleteFileW(firstPath); ::DeleteFileW(secondPath);
    XmlSourceThemes::EndSettingsEdit(); XmlSourceThemes::BeginSettingsEdit();
    m_sourcePage->m_openingThemeId = opening;
    m_sourcePage->m_draftThemeId = opening;
    m_sourcePage->m_themeDrafts = drafts; m_sourcePage->m_themeSelectionHistory = history;
    m_sourcePage->LoadSourceThemeControlsFromSettings();
    ::SetPropW(::GetParent(m_hWnd), L"FBE_SETTINGS_FOLLOWUP_PASSED", reinterpret_cast<HANDLE>(passed ? 1 : 2));
    return passed;
}

// Focused regression: no theme or dictionary operations, no layout probe.
int CSettingsDlg::ProbeResidual()
{
    SettingsEditSession restoreGlobals(_Settings);
    int result = 0;
    CString error;
    const CString wordsPath = FbeSettings::WordsFilePath();
    const auto words = m_wordsPage->m_words;
    const bool dirty = m_wordsPage->m_wordsDirty;
    // An unmatched tag makes the real Words PrepareCommit serialization fail.
    m_wordsPage->m_words = { WordsItem(CString(L"<broken>"), 1) };
    m_wordsPage->m_wordsDirty = true;
    g_followupDialog = m_hWnd; g_followupWarnings = 0; g_followupMessage.Empty();
    const UINT_PTR timer = ::SetTimer(NULL, 0, 20, FollowupMessageTimer);
    if(timer)
    {
        BOOL handled = FALSE;
        OnClickedOK(0, IDOK, GetDlgItem(IDOK), handled);
        ::KillTimer(NULL, timer);
        if(g_followupWarnings == 1 && g_followupMessage.Find(wordsPath) >= 0 && ::IsWindow(m_hWnd)) result |= 1;
    }
    CStringA diagnostic;
    diagnostic.Format("messages=%d\nreason_path=%d\ndialog_open=%d\n", g_followupWarnings, g_followupMessage.Find(wordsPath) >= 0, ::IsWindow(m_hWnd));
    FollowupWrite(U::GetSettingsDir() + L"settings-error-probe.txt", diagnostic);
    m_wordsPage->m_words = words; m_wordsPage->m_wordsDirty = dirty;

    // Keep the prior NBSP distinct from the current one to catch incomplete rollback.
    _Settings.SetNBSPChar(L"\u00A0");
    _Settings.SetNBSPChar(L"\u25A1");
    const CString current = _Settings.GetNBSPChar(), previous = _Settings.GetOldNBSPChar();
    m_editorPage->SendDlgItemMessage(IDC_NBSP_CHAR, CB_SETCURSEL, 1, 0); // small square
    const CString settingsPath = FbeSettings::SettingsFilePath();
    const bool saved = _Settings.TrySave(error);
    const auto before = FollowupRead(settingsPath);
    if(saved && ::SetFileAttributesW(settingsPath, FILE_ATTRIBUTE_READONLY))
    {
        if(!TryCommit(error) && !error.IsEmpty() && _Settings.GetNBSPChar() == current &&
            _Settings.GetOldNBSPChar() == previous && FollowupRead(settingsPath) == before) result |= 2;
        ::SetFileAttributesW(settingsPath, FILE_ATTRIBUTE_NORMAL);
        if(TryCommit(error) && _Settings.GetNBSPChar() == L"\u25AB" && _Settings.GetOldNBSPChar() == current)
        {
            CString text = L"first" + current + L"second";
            text.Replace(_Settings.GetOldNBSPChar(), _Settings.GetNBSPChar());
            if(text == L"first\u25ABsecond" && FollowupRead(settingsPath) != before) result |= 4;
        }
    }
    return result;
}

// Narrow HWND/caret regression, separate from the larger settings probes.
bool CSettingsDlg::ProbeUiDefects()
{
    bool passed = true;
    CStringA report;
    struct TooltipProbe {HWND page;HWND tooltip;} tips{m_generalPage->m_hWnd,NULL};
    ::EnumThreadWindows(::GetCurrentThreadId(),[](HWND window,LPARAM data)->BOOL {
        auto& probe=*reinterpret_cast<TooltipProbe*>(data);wchar_t name[64]={};::GetClassNameW(window,name,_countof(name));
        if(!wcscmp(name,TOOLTIPS_CLASSW)) {
            wchar_t text[2048]={};CToolInfo info(TTF_IDISHWND,::GetDlgItem(probe.page,IDC_SCRIPT_ICON_SIZE),0,NULL,text);
            if(::SendMessageW(window,TTM_GETTOOLINFOW,0,reinterpret_cast<LPARAM>(&info))) {probe.tooltip=window;return FALSE;}
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&tips));
    const CString expectedTip=FbeLoadRuntimeStringByKey(L"fbe.settings.tooltip.general.script_icon_size");
    for(int id:{IDC_SCRIPT_ICON_SIZE,IDC_SCRIPT_ICON_SIZE_LABEL}) {
        wchar_t text[2048]={};CToolInfo info(TTF_IDISHWND,m_generalPage->GetDlgItem(id),0,NULL,text);
        const bool ok=tips.tooltip&&::SendMessageW(tips.tooltip,TTM_GETTOOLINFOW,0,reinterpret_cast<LPARAM>(&info))&&
            info.lpszText&&expectedTip==info.lpszText&&!expectedTip.IsEmpty();
        report.AppendFormat("tooltip_%d=%d\n",id,ok);passed=ok&&passed;
    }
    auto caret = [&](const char* phase, HWND expected) {
        GUITHREADINFO info = {}; info.cbSize = sizeof(info);
        const bool got = ::GetGUIThreadInfo(::GetCurrentThreadId(), &info) != FALSE;
        RECT client = {}; if(info.hwndCaret) ::GetClientRect(info.hwndCaret, &client);
        const bool inside = !info.hwndCaret || (info.rcCaret.left >= 0 && info.rcCaret.top >= 0 &&
            info.rcCaret.right <= client.right && info.rcCaret.bottom <= client.bottom && ::IsWindowVisible(info.hwndCaret));
        const bool needsCaret=expected==m_hotkeysPage->m_search.m_hWnd;
        const bool ok = got && info.hwndFocus == expected && ::GetFocus() == expected && inside &&
            (needsCaret ? info.hwndCaret==expected&&info.rcCaret.right>info.rcCaret.left&&info.rcCaret.bottom>info.rcCaret.top :
                !info.hwndCaret||info.hwndCaret==expected);
        report.AppendFormat("caret_%s=%d; focus=%d; owner=%d; rect=%ld,%ld,%ld,%ld\n", phase, ok,
            ::GetDlgCtrlID(info.hwndFocus), ::GetDlgCtrlID(info.hwndCaret),
            info.rcCaret.left, info.rcCaret.top, info.rcCaret.right, info.rcCaret.bottom);
        passed = ok && passed;
    };
    report.AppendFormat("modal_queue_ctrl_f=%d\n",m_uiModalKeysPassed);
    caret("initial", m_navigation);
    passed=!m_sourcePage->m_preview.Editor().Send(SCI_GETFOCUS)&&passed;
    SelectPage(SettingsPageId::Source);
    passed=!m_sourcePage->m_preview.Editor().Send(SCI_GETFOCUS)&&passed;
    const HWND preview=m_sourcePage->m_preview.Editor().Handle();
    ::SetFocus(preview);caret("xml_preview",preview);
    passed=m_sourcePage->m_preview.Editor().Send(SCI_GETFOCUS)&&passed;
    m_navigation.SetFocus();
    passed=!m_sourcePage->m_preview.Editor().Send(SCI_GETFOCUS)&&passed;
    SelectPage(SettingsPageId::Keyboard);caret("keyboard_selected", m_navigation);
    m_hotkeysPage->m_search.SetFocus(); caret("search", m_hotkeysPage->m_search);
    m_hotkeysPage->m_search.SendMessage(WM_CHAR, 'a', 1); caret("typed", m_hotkeysPage->m_search);
    m_hotkeysPage->m_search.SetWindowText(L""); caret("cleared", m_hotkeysPage->m_search);
    CRect original; GetWindowRect(original);
    SetWindowPos(NULL, 0, 0, original.Width()+80, original.Height()+40, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    caret("resized", m_hotkeysPage->m_search);
    const UINT dpi=m_layoutDpi;
    SendMessage(WM_DPICHANGED,MAKEWPARAM(120,120),0);caret("dpi120",m_hotkeysPage->m_search);
    SendMessage(WM_DPICHANGED,MAKEWPARAM(144,144),0);caret("dpi144",m_hotkeysPage->m_search);
    SendMessage(WM_DPICHANGED,MAKEWPARAM(dpi,dpi),0);caret("dpi_restored",m_hotkeysPage->m_search);
    SelectPage(SettingsPageId::General); caret("general", m_navigation);
    SelectPage(SettingsPageId::Keyboard);
    m_navigation.SetFocus();
    caret("navigation",m_navigation);
    m_hotkeysPage->m_hkGroups.SetFocus();
    m_hotkeysPage->m_hkGroups.SetCurSel(0);
    m_hotkeysPage->SendMessage(WM_COMMAND, MAKEWPARAM(IDC_LIST_HOTKEYS_GROUPS,LBN_SELCHANGE), reinterpret_cast<LPARAM>(m_hotkeysPage->m_hkGroups.m_hWnd));
    caret("group", m_hotkeysPage->m_hkGroups);
    m_hotkeysPage->m_hotkeys.SetFocus(); caret("commands", m_hotkeysPage->m_hotkeys);
    m_hotkeysPage->m_editHotkey.SetFocus(); caret("recording", m_hotkeysPage->m_editHotkey);
    for(UINT vk : std::array<UINT,4>{VK_CONTROL,VK_SHIFT,VK_MENU,'Q'}) m_hotkeysPage->m_editHotkey.SendMessage(WM_KEYDOWN,vk,0);
    passed = m_hotkeysPage->m_accel.key=='Q' && (m_hotkeysPage->m_accel.fVirt&(FCONTROL|FSHIFT|FALT))==(FCONTROL|FSHIFT|FALT) && passed;
    for(UINT vk : std::array<UINT,4>{'Q',VK_MENU,VK_SHIFT,VK_CONTROL}) m_hotkeysPage->m_editHotkey.SendMessage(WM_KEYUP,vk,0);
    caret("recorded", m_hotkeysPage->m_editHotkey);
    const WORD command=m_hotkeysPage->m_accel.cmd;
    m_hotkeysPage->SendMessage(WM_COMMAND,MAKEWPARAM(IDC_BUTTON_HOTKEY_ASSIGN,BN_CLICKED),0);
    bool assigned=false;
    for(const auto& group:_Settings.m_hotkey_groups)for(const auto& hotkey:group.m_hotkeys)
        if(hotkey.m_accel.cmd==command)assigned=hotkey.m_accel.key=='Q'&&(hotkey.m_accel.fVirt&(FCONTROL|FSHIFT|FALT))==(FCONTROL|FSHIFT|FALT);
    report.AppendFormat("assigned_ctrl_shift_alt=%d\n",assigned);passed=assigned&&passed;
    const HWND focused=::GetFocus();m_hotkeysPage->m_search.SendMessage(WM_MOUSEMOVE,0,MAKELPARAM(2,2));
    passed = ::GetFocus()==focused && passed;
    SetWindowPos(NULL, 0, 0, original.Width(), original.Height(), SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    caret("restored", m_hotkeysPage->m_editHotkey);
    FollowupWrite(U::GetSettingsDir()+L"ui-caret-probe.txt",report);
    return passed;
}
