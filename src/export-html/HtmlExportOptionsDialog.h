#pragma once
#include "RuntimeLocalization.h"
#include "TemplateResolver.h"
#include "..\\common\\ModernFileDialog.h"

class CHtmlExportOptionsDialog : public CDialogImpl<CHtmlExportOptionsDialog> {
public:
    enum { IDD = IDD_HTML_EXPORT_OPTIONS };
    CString m_template, m_customCss; bool m_usingCustomTemplate = false, m_includedesc = true; int m_tocdepth = 1, m_imageMaxWidth = 0, m_imageMaxHeight = 0;
    BEGIN_MSG_MAP(CHtmlExportOptionsDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowse) COMMAND_ID_HANDLER(IDC_BROWSE_CSS, OnBrowseCss)
        COMMAND_ID_HANDLER(IDOK, OnOk) COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) { ExportHtmlTemplateSelection s = ResolveExportHtmlTemplate(_Settings); m_template=s.path; m_usingCustomTemplate=s.custom; m_customCss=U::QuerySV(_Settings,_T("CustomCss"),_T("")); m_includedesc=U::QueryIV(_Settings,_T("IncludeDesc"),1)!=0; m_tocdepth=U::QueryIV(_Settings,_T("TOCDepth"),1); m_imageMaxWidth=U::QueryIV(_Settings,_T("ImageMaxWidth"),0); m_imageMaxHeight=U::QueryIV(_Settings,_T("ImageMaxHeight"),0); SetDlgItemText(IDC_TEMPLATE,m_template); SetDlgItemText(IDC_CUSTOM_CSS,m_customCss); CheckDlgButton(IDC_DOCINFO,m_includedesc?BST_CHECKED:BST_UNCHECKED); SetDlgItemInt(IDC_TOCDEPTH,m_tocdepth,FALSE); SetDlgItemInt(IDC_IMAGE_MAX_WIDTH,m_imageMaxWidth,FALSE); SetDlgItemInt(IDC_IMAGE_MAX_HEIGHT,m_imageMaxHeight,FALSE); return TRUE; }
    LRESULT OnBrowse(WORD,WORD,HWND,BOOL&) { const COMDLG_FILTERSPEC f[]={{L"XSL files (*.xsl)",L"*.xsl"},{L"All files (*.*)",L"*.*"}}; ModernFileDialog::Request r; r.fileMustExist=true;r.pathMustExist=true;r.defaultExtension=L"xsl";r.filters=f;r.filterCount=_countof(f); auto x=ModernFileDialog::Show(m_hWnd,r);if(x.outcome==ModernFileDialog::Outcome::Accepted) SetDlgItemText(IDC_TEMPLATE,x.paths.front().c_str());return 0; }
    LRESULT OnBrowseCss(WORD,WORD,HWND,BOOL&) { const COMDLG_FILTERSPEC f[]={{L"CSS files (*.css)",L"*.css"},{L"All files (*.*)",L"*.*"}}; ModernFileDialog::Request r; r.fileMustExist=true;r.pathMustExist=true;r.defaultExtension=L"css";r.filters=f;r.filterCount=_countof(f);auto x=ModernFileDialog::Show(m_hWnd,r);if(x.outcome==ModernFileDialog::Outcome::Accepted) SetDlgItemText(IDC_CUSTOM_CSS,x.paths.front().c_str());return 0; }
    LRESULT OnOk(WORD,WORD,HWND,BOOL&) { m_template=U::GetWindowText(GetDlgItem(IDC_TEMPLATE));const CString b=U::GetProgDirFile(_T("html.xsl"));m_usingCustomTemplate=!ExportHtmlPathsEqual(m_template,b);_Settings.SetDWORDValue(_T("UseCustomTemplate"),m_usingCustomTemplate?1:0);if(m_usingCustomTemplate)_Settings.SetStringValue(_T("Template"),m_template);else _Settings.DeleteValue(_T("Template"));m_customCss=U::GetWindowText(GetDlgItem(IDC_CUSTOM_CSS));_Settings.SetStringValue(_T("CustomCss"),m_customCss);m_includedesc=IsDlgButtonChecked(IDC_DOCINFO)==BST_CHECKED;_Settings.SetDWORDValue(_T("IncludeDesc"),m_includedesc);m_tocdepth=min(10,(int)GetDlgItemInt(IDC_TOCDEPTH,NULL,FALSE));m_imageMaxWidth=min(10000,(int)GetDlgItemInt(IDC_IMAGE_MAX_WIDTH,NULL,FALSE));m_imageMaxHeight=min(10000,(int)GetDlgItemInt(IDC_IMAGE_MAX_HEIGHT,NULL,FALSE));_Settings.SetDWORDValue(_T("TOCDepth"),m_tocdepth);_Settings.SetDWORDValue(_T("ImageMaxWidth"),m_imageMaxWidth);_Settings.SetDWORDValue(_T("ImageMaxHeight"),m_imageMaxHeight);EndDialog(IDOK);return 0; }
    LRESULT OnCancel(WORD,WORD,HWND,BOOL&) { EndDialog(IDCANCEL);return 0; }
};
