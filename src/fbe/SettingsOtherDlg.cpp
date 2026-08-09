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

CSettingsOtherDlg::CSettingsOtherDlg() : m_scripts_switched(false)
{
}

CSettingsOtherDlg::~CSettingsOtherDlg()
{
}

LRESULT CSettingsOtherDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsOtherDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	m_keep = GetDlgItem(IDC_KEEP);
	m_def_enc = GetDlgItem(IDC_DEFAULT_ENC);
	m_restore_pos = GetDlgItem(IDC_RESTORE_POS);
	m_def_scripts_fld = GetDlgItem(IDC_DEFAULT_SCRIPTS_FOLDER);
	m_scripts_folder = GetDlgItem(IDC_SCRIPTS_FOLDER_PATH);
	m_scripts_folder_sel = GetDlgItem(IDC_SELECT_SCRIPTS_FOLDER_BUTTON);

	// added by SeNS
	m_nbsp_char = GetDlgItem(IDC_NBSP_CHAR);
	m_change_keyb = GetDlgItem(IDC_CHANGE_KEYB);

	m_image_type = GetDlgItem(IDC_IMAGETYPE);
	m_jpeg_quality = GetDlgItem(IDC_JPEGQUALITY);
	m_updown = GetDlgItem(IDC_JPEGSPIN);
	m_image_import_format = GetDlgItem(IDC_IMAGE_IMPORT_FORMAT);
	m_image_import_jpeg_quality = GetDlgItem(IDC_IMAGE_IMPORT_JPEG_QUALITY);
	m_image_import_updown = GetDlgItem(IDC_IMAGE_IMPORT_JPEG_SPIN);
	m_image_import_keep_supported = GetDlgItem(IDC_IMAGE_IMPORT_KEEP_SUPPORTED);

	m_keyb_layout = GetDlgItem(IDC_KEYB_LAYOUT);

	SetRuntimeSettingsOtherText(m_hWnd, IDC_KEEP, L"fbe.dialog.idd_setting_other.keep_manual", L"Keep manual");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_RESTORE_POS, L"fbe.dialog.idd_setting_other.restore_position", L"Restore position");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_DEFAULT_SCRIPTS_FOLDER, L"fbe.dialog.idd_setting_other.default_scripts_folder", L"Default folder");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_SELECT_SCRIPTS_FOLDER_BUTTON, L"fbe.dialog.idd_setting_other.browse", L"...");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_SETTINGS_ASKIMAGE, L"fbe.dialog.idd_setting_other.ask_image", L"Ask for non clear image insertion");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_OPTIONS_CLEARIMGS, L"fbe.dialog.idd_setting_other.clear_images", L"Insert clear images");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_CHANGE_KEYB, L"fbe.dialog.idd_setting_other.change_keyboard", L"Change keyboard layout automatically");
	SetRuntimeSettingsOtherText(m_hWnd, IDC_IMAGE_IMPORT_KEEP_SUPPORTED, L"fbe.dialog.idd_setting_other.image_import_keep_supported", L"Keep JPEG/PNG without recompression");

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

	_Settings.m_initial_scripts_folder = _Settings.GetScriptsFolder();
	m_def_scripts_fld.SetCheck(_Settings.IsDefaultScriptsFolder());
	m_scripts_folder.SetWindowText(_Settings.m_initial_scripts_folder);
	m_scripts_folder.SetReadOnly(_Settings.IsDefaultScriptsFolder());
	m_scripts_folder_sel.EnableWindow(!_Settings.IsDefaultScriptsFolder());
	m_scripts_switched = _Settings.IsDefaultScriptsFolder();

	// added by SeNS
	// Используем Unicode-экранирование: исходный файл исторически собирался
	// в разных кодировках, из-за чего сами символы превращались в вопросы.
	m_nbsp_char.AddString(L"\u25A1");  // □
	m_nbsp_char.AddString(L"\u25AB");  // ▫
	m_nbsp_char.AddString(L"\u25E6");  // ◦
	m_nbsp_char.AddString(L"\u00A0");  // original nbsp
	m_nbsp_char.SelectString (0, _Settings.GetNBSPChar());
	m_change_keyb.SetCheck(_Settings.GetChangeKeybLayout());

	m_image_type.AddString(L"PNG");
	m_image_type.AddString(L"JPEG");
	m_image_type.SetCurSel(_Settings.GetImageType());
	CString quality;
	quality.Format(L"%d", static_cast<int>(_Settings.GetJpegQuality()));
	m_jpeg_quality.SetWindowText(quality);
	m_updown.SetRange(20, 100);
	m_image_import_format.AddString(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_setting_other.image_import_auto", L"Auto"));
	m_image_import_format.AddString(L"JPEG");
	m_image_import_format.AddString(L"PNG");
	m_image_import_format.SetCurSel(_Settings.GetImageImportFormat());
	quality.Format(L"%d", static_cast<int>(_Settings.GetImageImportJpegQuality()));
	m_image_import_jpeg_quality.SetWindowText(quality);
	m_image_import_updown.SetRange(1, 100);
	m_image_import_keep_supported.SetCheck(_Settings.GetImageImportKeepSupported() ? BST_CHECKED : BST_UNCHECKED);

	// process keyboard layouts
	TCHAR name[255];
	HKL hLayouts[16];
	int nLayouts = GetKeyboardLayoutList(16, &hLayouts[0]);
	for (int i=0; i<nLayouts; i++)
	{
        // bottom 16 bit of HKL is LANGID
		LANGID language = (LANGID)(((UINT)hLayouts[i]) & 0x0000FFFF);
		LCID locale = MAKELCID(language, SORT_DEFAULT);
		GetLocaleInfo(locale, LOCALE_SLANGUAGE, name, 255);
		CString layoutName(name);
		m_keyb_layout.AddString(layoutName);
		m_keyb_layout.SetItemData(i, locale);
	}
	m_keyb_layout.SetCurSel(0);
	for (int i=0; i<nLayouts; i++)
		if (m_keyb_layout.GetItemData(i) == _Settings.GetKeybLayout())
		{
			m_keyb_layout.SetCurSel(i);
			break;
		}
	
	m_scripts_fld_dlg_msg = FbeLoadCString(IDS_CHOOSE_SCRIPTS_FLD);

	return 1;
}

LRESULT CSettingsOtherDlg::OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CString def_enc;
	m_def_enc.GetLBText(m_def_enc.GetCurSel(), def_enc);

	_Settings.SetDefaultEncoding(def_enc);
	_Settings.SetKeepEncoding(m_keep.GetState() != 0);
	_Settings.SetRestoreFilePosition(m_restore_pos.GetState() != 0);
	
	CString folderPath;
	m_scripts_folder.GetWindowText(folderPath);
	_Settings.SetScriptsFolder(folderPath.IsEmpty() ? _Settings.GetDefaultScriptsFolder() : folderPath, true);

	if(_Settings.m_initial_scripts_folder != _Settings.GetScriptsFolder())
	{
		_Settings.SetNeedRestart();
	}

	_Settings.SetInsImageAsking(IsDlgButtonChecked(IDC_SETTINGS_ASKIMAGE) != 0);
	_Settings.SetIsInsClearImage(IsDlgButtonChecked(IDC_OPTIONS_CLEARIMGS) != 0);

	// Added by SeNS
	CString s;
	m_nbsp_char.GetWindowText (s);
	_Settings.SetNBSPChar(s);
	_Settings.SetChangeKeybLayout(IsDlgButtonChecked(IDC_CHANGE_KEYB) != 0);

	_Settings.SetImageType(m_image_type.GetCurSel());
	_Settings.SetJpegQuality(m_updown.GetPos());
	_Settings.SetImageImportFormat(m_image_import_format.GetCurSel());
	_Settings.SetImageImportJpegQuality(m_image_import_updown.GetPos());
	_Settings.SetImageImportKeepSupported(m_image_import_keep_supported.GetCheck() == BST_CHECKED);

	int n = m_keyb_layout.GetCurSel();
	_Settings.SetKeybLayout(m_keyb_layout.GetItemData(n));

	EndDialog(wID);
	return 0;
}

LRESULT CSettingsOtherDlg::OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	EndDialog(wID);
	return 0;
}

LRESULT CSettingsOtherDlg::OnBnClickedDefaultScriptsFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_scripts_switched)
	{
		CString path;
		m_scripts_folder.GetWindowText(path);
		if(path.CompareNoCase(_Settings.GetDefaultScriptsFolder()) != 0)
		{
			m_scripts_folder.SetWindowText(_Settings.GetDefaultScriptsFolder());
		}
		
		_Settings.SetScriptsFolder(_Settings.GetDefaultScriptsFolder(), true);
		m_scripts_folder.SetReadOnly(true);
		m_scripts_folder_sel.EnableWindow(false);
	}
	else
	{
		m_scripts_folder.SetReadOnly(false);
		m_scripts_folder_sel.EnableWindow(true);
	}

	m_scripts_switched = !m_scripts_switched;
	return 0;
}

LRESULT CSettingsOtherDlg::OnBnClickedSelectScriptsFolderButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFolderDialog fldDlg(NULL, m_scripts_fld_dlg_msg, BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS);
	if (fldDlg.DoModal(*this) == IDOK)
	{
		CString folderPath(fldDlg.m_szFolderPath);
		if(!(folderPath.ReverseFind(L'\\') == (folderPath.GetLength() - 1)))
		{
			folderPath.Append(L"\\");
		}

		m_scripts_folder.SetWindowText(folderPath);
	}
	return 0;
}
LRESULT CSettingsOtherDlg::OnBnClickedSettingsAskimage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::EnableWindow(GetDlgItem(IDC_OPTIONS_CLEARIMGS), !IsDlgButtonChecked(IDC_SETTINGS_ASKIMAGE));
	if(IsDlgButtonChecked(IDC_SETTINGS_ASKIMAGE))
		::SendMessage(GetDlgItem(IDC_OPTIONS_CLEARIMGS), BM_SETCHECK, BST_UNCHECKED, 0);
	return 0;
}
