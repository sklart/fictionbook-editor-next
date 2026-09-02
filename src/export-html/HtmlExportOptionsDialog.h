#pragma once
#include "RuntimeLocalization.h"
#include "TemplateResolver.h"
#include "..\\common\\ModernFileDialog.h"
#include <vector>

inline void BuildHtmlModernFileTypes(const CString& serializedFilters,
    std::vector<CString>& labels, std::vector<CString>& patterns,
    std::vector<COMDLG_FILTERSPEC>& filters)
{
    labels.clear();
    patterns.clear();
    filters.clear();

    int position = 0;
    while (position >= 0) {
        const int separator = serializedFilters.Find(_T('|'), position);
        if (separator < 0) break;
        CString label = serializedFilters.Mid(position, separator - position);
        position = separator + 1;
        const int patternSeparator = serializedFilters.Find(_T('|'), position);
        if (patternSeparator < 0) break;
        CString pattern = serializedFilters.Mid(position, patternSeparator - position);
        position = patternSeparator + 1;
        if (label.IsEmpty() || pattern.IsEmpty()) break;
        labels.push_back(label);
        patterns.push_back(pattern);
    }

    filters.reserve(labels.size());
    for (size_t index = 0; index < labels.size(); ++index)
        filters.push_back({ labels[index].GetString(), patterns[index].GetString() });
}

class CHtmlExportOptionsDialog : public CDialogImpl<CHtmlExportOptionsDialog> {
public:
    enum { IDD = IDD_HTML_EXPORT_OPTIONS };
    CString m_template, m_customCss; HWND m_tooltip = NULL; std::vector<CString> m_tooltipTexts; bool m_usingCustomTemplate = false, m_includedesc = true; int m_tocdepth = 1, m_imageMaxWidth = 0, m_imageMaxHeight = 0;
    BEGIN_MSG_MAP(CHtmlExportOptionsDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowse) COMMAND_ID_HANDLER(IDC_BROWSE_CSS, OnBrowseCss)
        COMMAND_ID_HANDLER(IDOK, OnOk) COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    END_MSG_MAP()
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) { ExportHtmlTemplateSelection s = ResolveExportHtmlTemplate(_Settings); m_template=s.path; m_usingCustomTemplate=s.custom; m_customCss=U::QuerySV(_Settings,_T("CustomCss"),_T("")); m_includedesc=U::QueryIV(_Settings,_T("IncludeDesc"),1)!=0; m_tocdepth=U::QueryIV(_Settings,_T("TOCDepth"),1); m_imageMaxWidth=U::QueryIV(_Settings,_T("ImageMaxWidth"),0); m_imageMaxHeight=U::QueryIV(_Settings,_T("ImageMaxHeight"),0); SetWindowText(LoadExportHtmlString(IDS_HTML_EXPORT_OPTIONS_TITLE)); SetDlgItemText(IDC_TEMPLATE_LABEL,LoadExportHtmlString(IDS_CUSTOM_SAVE_TEMPLATE_LABEL)); SetDlgItemText(IDC_DOCINFO,LoadExportHtmlString(IDS_CUSTOM_SAVE_INCLUDE_DESC)); SetDlgItemText(IDC_TOC_DEPTH_LABEL,LoadExportHtmlString(IDS_CUSTOM_SAVE_TOC_DEPTH)); SetDlgItemText(IDC_CUSTOM_CSS_LABEL,LoadExportHtmlString(IDS_CUSTOM_SAVE_CUSTOM_CSS)); SetDlgItemText(IDC_IMAGE_MAX_WIDTH_LABEL,LoadExportHtmlString(IDS_CUSTOM_SAVE_IMAGE_MAX_WIDTH)); SetDlgItemText(IDC_IMAGE_MAX_HEIGHT_LABEL,LoadExportHtmlString(IDS_CUSTOM_SAVE_IMAGE_MAX_HEIGHT)); SetDlgItemText(IDC_TEMPLATE,m_template); SetDlgItemText(IDC_CUSTOM_CSS,m_customCss); CheckDlgButton(IDC_DOCINFO,m_includedesc?BST_CHECKED:BST_UNCHECKED); SetDlgItemInt(IDC_TOCDEPTH,m_tocdepth,FALSE); SetDlgItemInt(IDC_IMAGE_MAX_WIDTH,m_imageMaxWidth,FALSE); SetDlgItemInt(IDC_IMAGE_MAX_HEIGHT,m_imageMaxHeight,FALSE); InitTooltips(); return TRUE; }
    LRESULT OnBrowse(WORD,WORD,HWND,BOOL&) { std::vector<CString> labels,patterns;std::vector<COMDLG_FILTERSPEC> f;BuildHtmlModernFileTypes(LoadExportHtmlString(IDS_OPEN_TEMPLATE_FILTER),labels,patterns,f); ModernFileDialog::Request r; r.fileMustExist=true;r.pathMustExist=true;r.defaultExtension=L"xsl";r.filters=f.data();r.filterCount=static_cast<UINT>(f.size()); auto x=ModernFileDialog::Show(m_hWnd,r);if(x.outcome==ModernFileDialog::Outcome::Cancelled)return 0;if(x.outcome==ModernFileDialog::Outcome::Failed){ModernFileDialog::LogFailure(L"Browse HTML XSL template",x.error);return 0;}SetDlgItemText(IDC_TEMPLATE,x.paths.front().c_str());return 0; }
    LRESULT OnBrowseCss(WORD,WORD,HWND,BOOL&) { std::vector<CString> labels,patterns;std::vector<COMDLG_FILTERSPEC> f;BuildHtmlModernFileTypes(LoadExportHtmlString(IDS_OPEN_CSS_FILTER),labels,patterns,f); ModernFileDialog::Request r; r.fileMustExist=true;r.pathMustExist=true;r.defaultExtension=L"css";r.filters=f.data();r.filterCount=static_cast<UINT>(f.size());auto x=ModernFileDialog::Show(m_hWnd,r);if(x.outcome==ModernFileDialog::Outcome::Cancelled)return 0;if(x.outcome==ModernFileDialog::Outcome::Failed){ModernFileDialog::LogFailure(L"Browse HTML CSS file",x.error);return 0;}SetDlgItemText(IDC_CUSTOM_CSS,x.paths.front().c_str());return 0; }
    LRESULT OnOk(WORD,WORD,HWND,BOOL&) { m_template=U::GetWindowText(GetDlgItem(IDC_TEMPLATE));const CString b=U::GetProgDirFile(_T("html.xsl"));m_usingCustomTemplate=!ExportHtmlPathsEqual(m_template,b);m_customCss=U::GetWindowText(GetDlgItem(IDC_CUSTOM_CSS));m_includedesc=IsDlgButtonChecked(IDC_DOCINFO)==BST_CHECKED;m_tocdepth=min(10,(int)GetDlgItemInt(IDC_TOCDEPTH,NULL,FALSE));m_imageMaxWidth=min(10000,(int)GetDlgItemInt(IDC_IMAGE_MAX_WIDTH,NULL,FALSE));m_imageMaxHeight=min(10000,(int)GetDlgItemInt(IDC_IMAGE_MAX_HEIGHT,NULL,FALSE));EndDialog(IDOK);return 0; }
    void Persist() const { _Settings.SetDWORDValue(_T("UseCustomTemplate"),m_usingCustomTemplate?1:0);if(m_usingCustomTemplate)_Settings.SetStringValue(_T("Template"),m_template);else _Settings.DeleteValue(_T("Template"));_Settings.SetStringValue(_T("CustomCss"),m_customCss);_Settings.SetDWORDValue(_T("IncludeDesc"),m_includedesc);_Settings.SetDWORDValue(_T("TOCDepth"),m_tocdepth);_Settings.SetDWORDValue(_T("ImageMaxWidth"),m_imageMaxWidth);_Settings.SetDWORDValue(_T("ImageMaxHeight"),m_imageMaxHeight); }
    LRESULT OnCancel(WORD,WORD,HWND,BOOL&) { EndDialog(IDCANCEL);return 0; }
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&) { if (m_tooltip) { ::DestroyWindow(m_tooltip); m_tooltip = NULL; } return 0; }
    void InitTooltips() { m_tooltipTexts.reserve(8); m_tooltip = ::CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX, CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,m_hWnd,NULL,_Module.GetModuleInstance(),NULL); if (!m_tooltip) return; AddTooltip(IDC_TEMPLATE, IDS_TOOLTIP_TEMPLATE); AddTooltip(IDC_BROWSE, IDS_TOOLTIP_BROWSE_TEMPLATE); AddTooltip(IDC_DOCINFO, IDS_TOOLTIP_DOCINFO); AddTooltip(IDC_TOCDEPTH, IDS_TOOLTIP_TOC_DEPTH); AddTooltip(IDC_CUSTOM_CSS, IDS_TOOLTIP_CUSTOM_CSS); AddTooltip(IDC_BROWSE_CSS, IDS_TOOLTIP_BROWSE_CSS); AddTooltip(IDC_IMAGE_MAX_WIDTH, IDS_TOOLTIP_IMAGE_MAX_WIDTH); AddTooltip(IDC_IMAGE_MAX_HEIGHT, IDS_TOOLTIP_IMAGE_MAX_HEIGHT); }
    void AddTooltip(UINT id, UINT textId) { HWND control=GetDlgItem(id); if (!control) return; m_tooltipTexts.push_back(LoadExportHtmlString(textId)); CString& text=m_tooltipTexts.back(); if (text.IsEmpty()) return; TOOLINFO ti={}; ti.cbSize=sizeof(ti);ti.uFlags=TTF_IDISHWND|TTF_SUBCLASS;ti.hwnd=m_hWnd;ti.uId=reinterpret_cast<UINT_PTR>(control);ti.lpszText=const_cast<LPTSTR>((LPCTSTR)text);::SendMessage(m_tooltip,TTM_ADDTOOL,0,reinterpret_cast<LPARAM>(&ti)); }
};
