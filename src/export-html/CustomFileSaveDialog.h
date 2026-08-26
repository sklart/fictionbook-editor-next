#pragma once

#include "RuntimeLocalization.h"
#include "TemplateResolver.h"

class CCustomSaveDialog : public CFileDialogImpl<CCustomSaveDialog>
{
public:
	HWND	      m_hDlg;
	HWND	      m_hToolTip;
	CString     m_template;
	bool        m_usingCustomTemplate;
	CString     m_customCss;
	CString     m_tooltipTemplate;
	CString     m_tooltipBrowseTemplate;
	CString     m_tooltipDocInfo;
	CString     m_tooltipTocDepth;
	CString     m_tooltipCustomCss;
	CString     m_tooltipBrowseCss;
	CString     m_tooltipImageMaxWidth;
	CString     m_tooltipImageMaxHeight;
	bool	      m_includedesc;
	int	      m_tocdepth;
	int	      m_imageMaxWidth;
	int	      m_imageMaxHeight;

	CCustomSaveDialog(BOOL bOpenFileDialog, // TRUE for FileOpen, FALSE for FileSaveAs
		LPCTSTR lpszDefExt = NULL,
		LPCTSTR lpszFileName = NULL,
		DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		LPCTSTR lpszFilter = NULL,
		HWND hWndParent = NULL)
		: CFileDialogImpl<CCustomSaveDialog>(bOpenFileDialog, lpszDefExt, lpszFileName, dwFlags, lpszFilter, hWndParent),
		m_hDlg(NULL), m_hToolTip(NULL), m_usingCustomTemplate(false), m_includedesc(true), m_tocdepth(1), m_imageMaxWidth(0), m_imageMaxHeight(0)
	{
		m_ofn.lpTemplateName = MAKEINTRESOURCE(IDD_CUSTOMSAVEDLG);
	}

	BEGIN_MSG_MAP(CCustomSaveDialog)
		if (uMsg == WM_INITDIALOG)
			return OnInitDialog(hWnd, uMsg, wParam, lParam);

	MESSAGE_HANDLER(WM_SIZE, OnSize)

	COMMAND_ID_HANDLER(IDC_BROWSE, OnBrowse);
	COMMAND_ID_HANDLER(IDC_BROWSE_CSS, OnBrowseCss);

	CHAIN_MSG_MAP(CFileDialogImpl<CCustomSaveDialog>)
	END_MSG_MAP()

	~CCustomSaveDialog() {
		if (m_hToolTip != NULL) {
			::DestroyWindow(m_hToolTip);
			m_hToolTip = NULL;
		}
	}

	LRESULT OnInitDialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		// save window handles
		m_hDlg = hWnd;

		// read saved template name
		ExportHtmlTemplateSelection selection = ResolveExportHtmlTemplate(_Settings);
		m_template = selection.path;
		m_usingCustomTemplate = selection.custom;
		SetDlgItemText(IDC_TEMPLATE, m_template);
		m_customCss = U::QuerySV(_Settings, _T("CustomCss"), _T(""));
		SetDlgItemText(IDC_CUSTOM_CSS, m_customCss);
		m_includedesc = U::QueryIV(_Settings, _T("IncludeDesc"), 1) != 0;
		SendDlgItemMessage(IDC_DOCINFO, BM_SETCHECK,
			m_includedesc ? BST_CHECKED : BST_UNCHECKED, 0);
		m_tocdepth = U::QueryIV(_Settings, _T("TOCDepth"), 1);
		SetDlgItemInt(IDC_TOCDEPTH, m_tocdepth, FALSE);
		m_imageMaxWidth = U::QueryIV(_Settings, _T("ImageMaxWidth"), 0);
		m_imageMaxHeight = U::QueryIV(_Settings, _T("ImageMaxHeight"), 0);
		SetDlgItemInt(IDC_IMAGE_MAX_WIDTH, m_imageMaxWidth, FALSE);
		SetDlgItemInt(IDC_IMAGE_MAX_HEIGHT, m_imageMaxHeight, FALSE);
		// Подписи шаблона задано через стабильные ID, чтобы внешний JSON-слой
		// мог заменить английский layout для всех поддерживаемых языков.
		SetDlgItemText(IDC_TEMPLATE_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_TEMPLATE_LABEL));
		SetDlgItemText(IDC_DOCINFO, LoadExportHtmlString(IDS_CUSTOM_SAVE_INCLUDE_DESC));
		SetDlgItemText(IDC_TOC_DEPTH_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_TOC_DEPTH));
		SetDlgItemText(IDC_CUSTOM_CSS_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_CUSTOM_CSS));
		SetDlgItemText(IDC_IMAGE_MAX_WIDTH_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_IMAGE_MAX_WIDTH));
		SetDlgItemText(IDC_IMAGE_MAX_HEIGHT_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_IMAGE_MAX_HEIGHT));
		InitTooltips();
		return TRUE;
	}

	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
		// make combobox the same size as std controls
		RECT    rc_std, rc_my;
		HWND    hCB = ::GetDlgItem(m_hDlg, IDC_TEMPLATE);
		::GetWindowRect(hCB, &rc_my);
		::GetWindowRect(GetFileDialogWindow().GetDlgItem(cmb1), &rc_std);
		POINT   pt = { rc_my.left,rc_my.top };
		::ScreenToClient(m_hDlg, &pt);
		::MoveWindow(hCB, pt.x, pt.y, rc_std.right - rc_std.left, rc_my.bottom - rc_my.top, TRUE);
		hCB = ::GetDlgItem(m_hDlg, IDC_BROWSE);
		::GetWindowRect(hCB, &rc_my);
		::MoveWindow(hCB,
			pt.x + rc_std.right - rc_std.left + 10, pt.y,
			rc_my.right - rc_my.left, rc_my.bottom - rc_my.top, TRUE);

		hCB = ::GetDlgItem(m_hDlg, IDC_CUSTOM_CSS);
		::GetWindowRect(hCB, &rc_my);
		POINT cssPt = { rc_my.left, rc_my.top };
		::ScreenToClient(m_hDlg, &cssPt);
		::MoveWindow(hCB, cssPt.x, cssPt.y, rc_std.right - rc_std.left, rc_my.bottom - rc_my.top, TRUE);
		hCB = ::GetDlgItem(m_hDlg, IDC_BROWSE_CSS);
		::GetWindowRect(hCB, &rc_my);
		::MoveWindow(hCB, cssPt.x + rc_std.right - rc_std.left + 10, cssPt.y,
			rc_my.right - rc_my.left, rc_my.bottom - rc_my.top, TRUE);

		return 0;
	}

	LRESULT OnBrowse(WORD, WORD, HWND, BOOL&) {
		CString filter = LoadExportHtmlString(IDS_OPEN_TEMPLATE_FILTER);
		filter.Replace(_T('|'), _T('\0'));
		CFileDialog	dlg(
			TRUE,
			_T("xsl"),
			NULL,
			OFN_HIDEREADONLY | OFN_PATHMUSTEXIST,
			filter
			);

		if (dlg.DoModal(*this) == IDOK)
			::SetDlgItemText(m_hDlg, IDC_TEMPLATE, dlg.m_szFileName);

		return 0;
	}

	LRESULT OnBrowseCss(WORD, WORD, HWND, BOOL&) {
		CFileDialog dlg(TRUE, _T("css"), NULL,
			OFN_HIDEREADONLY | OFN_PATHMUSTEXIST,
			_T("CSS files (*.css)|*.css|All files (*.*)|*.*|"));
		if (dlg.DoModal(*this) == IDOK)
			::SetDlgItemText(m_hDlg, IDC_CUSTOM_CSS, dlg.m_szFileName);
		return 0;
	}

	BOOL OnFileOK(LPOFNOTIFY on) {
		m_template = U::GetWindowText(::GetDlgItem(m_hDlg, IDC_TEMPLATE));
		const CString bundled = U::GetProgDirFile(_T("html.xsl"));
		m_usingCustomTemplate = !ExportHtmlPathsEqual(m_template, bundled);
		_Settings.SetDWORDValue(_T("UseCustomTemplate"), m_usingCustomTemplate ? 1 : 0);
		if (m_usingCustomTemplate)
			_Settings.SetStringValue(_T("Template"), m_template);
		else
			_Settings.DeleteValue(_T("Template"));
		m_customCss = U::GetWindowText(::GetDlgItem(m_hDlg, IDC_CUSTOM_CSS));
		_Settings.SetStringValue(_T("CustomCss"), m_customCss);
		m_includedesc = ::SendDlgItemMessage(m_hDlg, IDC_DOCINFO, BM_GETCHECK, 0, 0) == BST_CHECKED;
		_Settings.SetDWORDValue(_T("IncludeDesc"), m_includedesc);
		m_tocdepth = ::GetDlgItemInt(m_hDlg, IDC_TOCDEPTH, NULL, FALSE);
		if (m_tocdepth<0)
			m_tocdepth = 0;
		if (m_tocdepth>10)
			m_tocdepth = 10;
		_Settings.SetDWORDValue(_T("TOCDepth"), m_tocdepth);
		m_imageMaxWidth = ::GetDlgItemInt(m_hDlg, IDC_IMAGE_MAX_WIDTH, NULL, FALSE);
		m_imageMaxHeight = ::GetDlgItemInt(m_hDlg, IDC_IMAGE_MAX_HEIGHT, NULL, FALSE);
		m_imageMaxWidth = min(m_imageMaxWidth, 10000);
		m_imageMaxHeight = min(m_imageMaxHeight, 10000);
		_Settings.SetDWORDValue(_T("ImageMaxWidth"), m_imageMaxWidth);
		_Settings.SetDWORDValue(_T("ImageMaxHeight"), m_imageMaxHeight);
		return TRUE;
	}

	void InitTooltips() {
		m_hToolTip = ::CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
			WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT, m_hDlg, NULL, _Module.GetModuleInstance(), NULL);
		if (m_hToolTip == NULL)
			return;

		::SendMessage(m_hToolTip, TTM_SETMAXTIPWIDTH, 0, 360);
		m_tooltipTemplate = LoadExportHtmlString(IDS_TOOLTIP_TEMPLATE);
		m_tooltipBrowseTemplate = LoadExportHtmlString(IDS_TOOLTIP_BROWSE_TEMPLATE);
		m_tooltipDocInfo = LoadExportHtmlString(IDS_TOOLTIP_DOCINFO);
		m_tooltipTocDepth = LoadExportHtmlString(IDS_TOOLTIP_TOC_DEPTH);
		m_tooltipCustomCss = LoadExportHtmlString(IDS_TOOLTIP_CUSTOM_CSS);
		m_tooltipBrowseCss = LoadExportHtmlString(IDS_TOOLTIP_BROWSE_CSS);
		m_tooltipImageMaxWidth = LoadExportHtmlString(IDS_TOOLTIP_IMAGE_MAX_WIDTH);
		m_tooltipImageMaxHeight = LoadExportHtmlString(IDS_TOOLTIP_IMAGE_MAX_HEIGHT);
		AddTooltip(IDC_TEMPLATE, m_tooltipTemplate);
		AddTooltip(IDC_TEMPLATE_LABEL, m_tooltipTemplate);
		AddTooltip(IDC_BROWSE, m_tooltipBrowseTemplate);
		AddTooltip(IDC_DOCINFO, m_tooltipDocInfo);
		AddTooltip(IDC_TOCDEPTH, m_tooltipTocDepth);
		AddTooltip(IDC_TOC_DEPTH_LABEL, m_tooltipTocDepth);
		AddTooltip(IDC_CUSTOM_CSS, m_tooltipCustomCss);
		AddTooltip(IDC_CUSTOM_CSS_LABEL, m_tooltipCustomCss);
		AddTooltip(IDC_BROWSE_CSS, m_tooltipBrowseCss);
		AddTooltip(IDC_IMAGE_MAX_WIDTH, m_tooltipImageMaxWidth);
		AddTooltip(IDC_IMAGE_MAX_WIDTH_LABEL, m_tooltipImageMaxWidth);
		AddTooltip(IDC_IMAGE_MAX_HEIGHT, m_tooltipImageMaxHeight);
		AddTooltip(IDC_IMAGE_MAX_HEIGHT_LABEL, m_tooltipImageMaxHeight);
	}

	void AddTooltip(UINT controlId, CString& text) {
		if (m_hToolTip == NULL)
			return;

		HWND hControl = ::GetDlgItem(m_hDlg, controlId);
		if (hControl == NULL)
			return;

		if (text.IsEmpty())
			return;

		TOOLINFO ti = { 0 };
		ti.cbSize = sizeof(ti);
		ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		ti.hwnd = m_hDlg;
		ti.uId = reinterpret_cast<UINT_PTR>(hControl);
		ti.lpszText = const_cast<LPTSTR>((LPCTSTR)text);
		::SendMessage(m_hToolTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
	}
};
