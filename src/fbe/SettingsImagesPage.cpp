#include "stdafx.h"
#include "SettingsImagesPage.h"
#include "Settings.h"
#include "RuntimeLocalization.h"

extern CSettings _Settings;

LRESULT CSettingsImagesPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_SETTINGS_IMAGES);
	m_askImage = GetDlgItem(IDC_SETTINGS_ASKIMAGE);
	m_clearImages = GetDlgItem(IDC_OPTIONS_CLEARIMGS);
	m_imageType = GetDlgItem(IDC_IMAGETYPE);
	m_jpegQuality = GetDlgItem(IDC_JPEGQUALITY);
	m_jpegSpin = GetDlgItem(IDC_JPEGSPIN);
	m_imageImportFormat = GetDlgItem(IDC_IMAGE_IMPORT_FORMAT);
	m_imageImportJpegQuality = GetDlgItem(IDC_IMAGE_IMPORT_JPEG_QUALITY);
	m_imageImportJpegSpin = GetDlgItem(IDC_IMAGE_IMPORT_JPEG_SPIN);
	m_imageImportKeepSupported = GetDlgItem(IDC_IMAGE_IMPORT_KEEP_SUPPORTED);

	m_askImage.SetCheck(_Settings.GetInsImageAsking() ? BST_CHECKED : BST_UNCHECKED);
	m_clearImages.SetCheck(_Settings.GetIsInsClearImage() ? BST_CHECKED : BST_UNCHECKED);
	m_clearImages.EnableWindow(m_askImage.GetCheck() != BST_CHECKED);
	m_imageType.AddString(L"PNG");
	m_imageType.AddString(L"JPEG");
	m_imageType.SetCurSel(_Settings.GetImageType() <= 1 ? _Settings.GetImageType() : 1);
	CString quality;
	quality.Format(L"%d", static_cast<int>(_Settings.GetJpegQuality()));
	m_jpegQuality.SetWindowText(quality);
	m_jpegSpin.SetRange(20, 100);
	m_imageImportFormat.AddString(FbeLoadRuntimeStringByKey(L"fbe.image_import.output_auto", L"Auto"));
	m_imageImportFormat.AddString(FbeLoadRuntimeStringByKey(L"fbe.image_import.output_jpeg", L"JPEG"));
	m_imageImportFormat.AddString(FbeLoadRuntimeStringByKey(L"fbe.image_import.output_png", L"PNG"));
	m_imageImportFormat.SetCurSel(_Settings.GetImageImportFormat());
	quality.Format(L"%d", static_cast<int>(_Settings.GetImageImportJpegQuality()));
	m_imageImportJpegQuality.SetWindowText(quality);
	m_imageImportJpegSpin.SetRange(1, 100);
	m_imageImportKeepSupported.SetCheck(_Settings.GetImageImportKeepSupported() ? BST_CHECKED : BST_UNCHECKED);
	UpdatePasteDependencies();
	UpdateImportDependencies();
	return 1;
}

LRESULT CSettingsImagesPage::OnClickedOK(WORD, WORD, HWND, BOOL&)
{
	if(!Validate()) return 0;
	Commit();
	return 0;
}

bool CSettingsImagesPage::Validate()
{
	const int pasteQuality = m_jpegSpin.GetPos();
	const int importQuality = m_imageImportJpegSpin.GetPos();
	if(pasteQuality < 20 || pasteQuality > 100) { MessageBeep(MB_ICONERROR); m_jpegQuality.SetFocus(); return false; }
	if(importQuality < 1 || importQuality > 100) { MessageBeep(MB_ICONERROR); m_imageImportJpegQuality.SetFocus(); return false; }
	return true;
}

void CSettingsImagesPage::Commit()
{
	_Settings.SetInsImageAsking(m_askImage.GetCheck() == BST_CHECKED);
	_Settings.SetIsInsClearImage(m_clearImages.GetCheck() == BST_CHECKED);
	_Settings.SetImageType(m_imageType.GetCurSel());
	_Settings.SetJpegQuality(m_jpegSpin.GetPos());
	_Settings.SetImageImportFormat(m_imageImportFormat.GetCurSel());
	_Settings.SetImageImportJpegQuality(m_imageImportJpegSpin.GetPos());
	_Settings.SetImageImportKeepSupported(m_imageImportKeepSupported.GetCheck() == BST_CHECKED);
}

LRESULT CSettingsImagesPage::OnClickedCancel(WORD, WORD, HWND, BOOL&) { return 0; }
bool CSettingsImagesPage::CancelChanges() { return true; }

LRESULT CSettingsImagesPage::OnAskImage(WORD, WORD, HWND, BOOL&)
{
	const bool askImage = m_askImage.GetCheck() == BST_CHECKED;
	m_clearImages.EnableWindow(!askImage);
	return 0;
}

void CSettingsImagesPage::UpdatePasteDependencies()
{
	const BOOL jpeg = m_imageType.GetCurSel() == 1;
	m_jpegQuality.EnableWindow(jpeg); m_jpegSpin.EnableWindow(jpeg);
	GetDlgItem(IDC_SETTINGS_OTHER_QUALITY).EnableWindow(jpeg);
}

void CSettingsImagesPage::UpdateImportDependencies()
{
	const BOOL usesJpegQuality = m_imageImportFormat.GetCurSel() != 2;
	m_imageImportJpegQuality.EnableWindow(usesJpegQuality); m_imageImportJpegSpin.EnableWindow(usesJpegQuality);
	GetDlgItem(IDC_SETTINGS_OTHER_IMPORT_QUALITY).EnableWindow(usesJpegQuality);
}

LRESULT CSettingsImagesPage::OnPasteFormatChanged(WORD, WORD, HWND, BOOL&) { UpdatePasteDependencies(); return 0; }
LRESULT CSettingsImagesPage::OnImportFormatChanged(WORD, WORD, HWND, BOOL&) { UpdateImportDependencies(); return 0; }
