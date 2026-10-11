#pragma once

#include "HtmlExportSettings.h"
#include "resource.h"
#include <wtl/atlctrls.h>
#include <vector>

class HtmlExportGeneralPage : public CDialogImpl<HtmlExportGeneralPage>
{
public:
    enum { IDD = IDD_HTML_EXPORT_GENERAL_PAGE };
    void Attach(HtmlExportSettings* settings) { m_settings = settings; }
    void LoadFromSettings();
    bool SaveToSettings(HtmlExportSettings& candidate, CString& error);
    void UpdateEnabledState();
    void SetSplitSupported(bool value);
    void RefreshStructureTooltipArea();

    BEGIN_MSG_MAP(HtmlExportGeneralPage)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        COMMAND_ID_HANDLER(IDC_INCLUDE_TOC, OnIncludeToc)
        COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowseTemplate)
        COMMAND_HANDLER(IDC_TEMPLATE, EN_CHANGE, OnTemplateChanged)
    END_MSG_MAP()

private:
    HtmlExportSettings* m_settings = NULL;
    HWND m_tooltip = NULL;
    bool m_splitSupported = true;
    bool m_templateSupportsSplit = true;
    std::vector<CString> m_tooltipTexts;
    CString m_structureTooltipText;
    void InitTooltips();
    void AddTooltip(UINT id, UINT textId);
    void AddDisabledStructureTooltip();
    void UpdateStructureTooltip();
    UINT StructureTooltipTextId() const;
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnIncludeToc(WORD, WORD, HWND, BOOL&);
    LRESULT OnBrowseTemplate(WORD, WORD, HWND, BOOL&);
    LRESULT OnTemplateChanged(WORD, WORD, HWND, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
};
