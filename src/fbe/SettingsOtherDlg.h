// SettingsOtherDlg.h : Declaration of the CSettingsOtherDlg

#pragma once

#include "resource.h" 
#include "CFileDialogEx.h"
#include <atlhost.h>


// CSettingsOtherDlg

class CSettingsOtherDlg : 
	public CAxDialogImpl<CSettingsOtherDlg>
{
	CButton		m_keep;
	CComboBox	m_def_enc;
	CButton		m_restore_pos;

	// added by SeNS
	CComboBox   m_nbsp_char;
	CComboBox	m_image_type;
	CUpDownCtrl	m_updown;
	CEdit		m_jpeg_quality;
	CComboBox	m_image_import_format;
	CEdit		m_image_import_jpeg_quality;
	CUpDownCtrl	m_image_import_updown;
	CButton		m_image_import_keep_supported;


public:
	CSettingsOtherDlg();
	~CSettingsOtherDlg();

	enum { IDD = IDD_SETTING_OTHER };

BEGIN_MSG_MAP(CSettingsOtherDlg)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	COMMAND_HANDLER(IDC_SETTINGS_ASKIMAGE, BN_CLICKED, OnBnClickedSettingsAskimage)
	CHAIN_MSG_MAP(CAxDialogImpl<CSettingsOtherDlg>)
END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);	
	LRESULT OnBnClickedRestorePos2(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedSettingsAskimage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};

