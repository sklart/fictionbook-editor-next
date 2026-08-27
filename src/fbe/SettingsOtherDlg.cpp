// SettingsOtherDlg.cpp : Implementation of CSettingsOtherDlg

#include "stdafx.h"
#include "SettingsOtherDlg.h"
#include "utils.h"
#include "Settings.h"
#include "res1.h"
#include "RuntimeLocalization.h"

extern CSettings _Settings;

static void SetRuntimeSettingsOtherText(HWND dialog, int controlId, LPCWSTR key, LPCWSTR fallback)
{
	const CString text = FbeLoadRuntimeStringByKey(key, fallback);
	if (!text.IsEmpty())
		::SetDlgItemText(dialog, controlId, text);
}

// CSettingsOtherDlg

CSettingsOtherDlg::CSettingsOtherDlg()
{
}

CSettingsOtherDlg::~CSettingsOtherDlg()
{
}

LRESULT CSettingsOtherDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsOtherDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTING_OTHER);
	m_keep = GetDlgItem(IDC_KEEP);
	m_def_enc = GetDlgItem(IDC_DEFAULT_ENC);
	m_restore_pos = GetDlgItem(IDC_RESTORE_POS);

	// added by SeNS
	m_nbsp_char = GetDlgItem(IDC_NBSP_CHAR);

	m_image_type = GetDlgItem(IDC_IMAGETYPE);
	m_jpeg_quality = GetDlgItem(IDC_JPEGQUALITY);
	m_updown = GetDlgItem(IDC_JPEGSPIN);
	m_image_import_format = GetDlgItem(IDC_IMAGE_IMPORT_FORMAT);
	m_image_import_jpeg_quality = GetDlgItem(IDC_IMAGE_IMPORT_JPEG_QUALITY);
	m_image_import_updown = GetDlgItem(IDC_IMAGE_IMPORT_JPEG_SPIN);
	m_image_import_keep_supported = GetDlgItem(IDC_IMAGE_IMPORT_KEEP_SUPPORTED);


	SetRuntimeSettingsOtherText(m_hWnd, IDC_KEEP, L"fbe.dialog.idd_setting_other.keep_manual", L"Keep manual");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_RESTORE_POS, L"fbe.dialog.idd_setting_other.restore_position", L"Restore position");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_SETTINGS_ASKIMAGE, L"fbe.dialog.idd_setting_other.ask_image", L"Ask for non clear image insertion");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_OPTIONS_CLEARIMGS, L"fbe.dialog.idd_setting_other.clear_images", L"Insert clear images");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_IMAGE_IMPORT_KEEP_SUPPORTED, L"fbe.dialog.idd_setting_other.keep_supported", L"Keep JPEG/PNG without recompression");

	::SendMessage(GetDlgItem(IDC_SETTINGS_ASKIMAGE), BM_SETCHECK, 
				_Settings.GetInsImageAsking() ? BST_CHECKED : BST_UNCHECKED, 0);

	::EnableWindow(GetDlgItem(IDC_OPTIONS_CLEARIMGS), !IsDlgButtonChecked(IDC_SETTINGS_ASKIMAGE));
	::SendMessage(GetDlgItem(IDC_OPTIONS_CLEARIMGS), BM_SETCHECK, 
				_Settings.GetIsInsClearImage() ? BST_CHECKED : BST_UNCHECKED, 0);
	
    wchar_t buf[MAX_LOAD_STRING + 1];
	if (FbeLoadString(_Module.GetResourceInstance(),IDS_ENCODINGS,buf,sizeof(buf)/sizeof(buf[0])))
	{
		TCHAR   *cp=buf;
		while (*cp) 
		{
			size_t len=_tcscspn(cp,_T(","));
			if (cp[len])
			cp[len++]=_T('\0');
			if (*cp)
			{
				m_def_enc.AddString(cp);
			}
			cp+=len;
		}
	}
	
	m_def_enc.SelectString(0, _Settings.GetDefaultEncoding());
	m_keep.SetCheck(_Settings.KeepEncoding() ? BST_CHECKED : BST_UNCHECKED);
	m_restore_pos.SetCheck(_Settings.RestoreFilePosition() ? BST_CHECKED : BST_UNCHECKED);


	// added by SeNS
	// Используем Unicode-экранирование: исходный файл исторически собирался
	// в разных кодировках, из-за чего сами символы превращались в вопросы.
	m_nbsp_char.AddString(L"\u25A1");  // □
	m_nbsp_char.AddString(L"\u25AB");  // ▫
	m_nbsp_char.AddString(L"\u25E6");  // ◦
	m_nbsp_char.AddString(L"\u00A0");  // original nbsp
	m_nbsp_char.SelectString (0, _Settings.GetNBSPChar());

	m_image_type.AddString(L"PNG");
	m_image_type.AddString(L"JPEG");
	m_image_type.SetCurSel(_Settings.GetImageType());
	CString quality;
	quality.Format(L"%d", static_cast<int>(_Settings.GetJpegQuality()));
	m_jpeg_quality.SetWindowText(quality);
	m_updown.SetRange(20, 100);
	m_image_import_format.AddString(FbeLoadRuntimeStringByKey(L"fbe.image_import.output_auto", L"Auto"));
	m_image_import_format.AddString(FbeLoadRuntimeStringByKey(L"fbe.image_import.output_jpeg", L"JPEG"));
	m_image_import_format.AddString(FbeLoadRuntimeStringByKey(L"fbe.image_import.output_png", L"PNG"));
	m_image_import_format.SetCurSel(_Settings.GetImageImportFormat());
	quality.Format(L"%d", static_cast<int>(_Settings.GetImageImportJpegQuality()));
	m_image_import_jpeg_quality.SetWindowText(quality);
	m_image_import_updown.SetRange(1, 100);
	m_image_import_keep_supported.SetCheck(_Settings.GetImageImportKeepSupported() ? BST_CHECKED : BST_UNCHECKED);

	
	return 1;
}

LRESULT CSettingsOtherDlg::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CString def_enc;
	m_def_enc.GetLBText(m_def_enc.GetCurSel(), def_enc);

	_Settings.SetDefaultEncoding(def_enc);
	_Settings.SetKeepEncoding(m_keep.GetState() != 0);
	_Settings.SetRestoreFilePosition(m_restore_pos.GetState() != 0);
	

	_Settings.SetInsImageAsking(IsDlgButtonChecked(IDC_SETTINGS_ASKIMAGE) != 0);
	_Settings.SetIsInsClearImage(IsDlgButtonChecked(IDC_OPTIONS_CLEARIMGS) != 0);

	// Added by SeNS
	CString s;
	m_nbsp_char.GetWindowText (s);
	_Settings.SetNBSPChar(s);

	_Settings.SetImageType(m_image_type.GetCurSel());
	_Settings.SetJpegQuality(m_updown.GetPos());
	_Settings.SetImageImportFormat(m_image_import_format.GetCurSel());
	_Settings.SetImageImportJpegQuality(m_image_import_updown.GetPos());
	_Settings.SetImageImportKeepSupported(m_image_import_keep_supported.GetCheck() == BST_CHECKED);


	EndDialog(wID);
	return 0;
}

LRESULT CSettingsOtherDlg::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}

LRESULT CSettingsOtherDlg::OnBnClickedSettingsAskimage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::EnableWindow(GetDlgItem(IDC_OPTIONS_CLEARIMGS), !IsDlgButtonChecked(IDC_SETTINGS_ASKIMAGE));
	if(IsDlgButtonChecked(IDC_SETTINGS_ASKIMAGE))
		::SendMessage(GetDlgItem(IDC_OPTIONS_CLEARIMGS), BM_SETCHECK, BST_UNCHECKED, 0);
	return 0;
}
