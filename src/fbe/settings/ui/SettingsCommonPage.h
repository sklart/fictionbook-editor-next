#pragma once
#include <atlhost.h>
#include "../../resource.h"
#include "SettingsPageLifecycle.h"
#include "SettingsTooltips.h"

class CSettingsCommonPage : public CAxDialogImpl<CSettingsCommonPage>, public ISettingsPage
{
    CComboBox m_language, m_genreCatalog, m_defaultEncoding, m_updateChannel;
    CButton m_keepEncoding, m_restorePosition;
    CSettingsTooltips m_tooltips;
    void FitLocalizedLayout();
public:
    enum { IDD = IDD_SETTINGS_COMMON };
    BEGIN_MSG_MAP(CSettingsCommonPage)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
        COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
        CHAIN_MSG_MAP(CAxDialogImpl<CSettingsCommonPage>)
    END_MSG_MAP()
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&) { return ForwardSettingsDialogCommand(m_hWnd, IDOK); }
    LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&) { return ForwardSettingsDialogCommand(m_hWnd, IDCANCEL); }
    bool Validate() override { return true; }
    void Commit() override;
    bool CancelChanges() override { return true; }
};
