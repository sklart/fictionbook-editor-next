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
	m_imageType.SetCurSel(_Settings.GetImageType());
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
	return 1;
}

LRESULT CSettingsImagesPage::OnClickedOK(WORD, WORD, HWND, BOOL&)
{
	_Settings.SetInsImageAsking(m_askImage.GetCheck() == BST_CHECKED);
	_Settings.SetIsInsClearImage(m_clearImages.GetCheck() == BST_CHECKED);
	_Settings.SetImageType(m_imageType.GetCurSel());
	_Settings.SetJpegQuality(m_jpegSpin.GetPos());
	_Settings.SetImageImportFormat(m_imageImportFormat.GetCurSel());
	_Settings.SetImageImportJpegQuality(m_imageImportJpegSpin.GetPos());
	_Settings.SetImageImportKeepSupported(m_imageImportKeepSupported.GetCheck() == BST_CHECKED);
	return 0;
}

LRESULT CSettingsImagesPage::OnClickedCancel(WORD, WORD, HWND, BOOL&) { return 0; }

LRESULT CSettingsImagesPage::OnAskImage(WORD, WORD, HWND, BOOL&)
{
	const bool askImage = m_askImage.GetCheck() == BST_CHECKED;
	m_clearImages.EnableWindow(!askImage);
	if(askImage)
		m_clearImages.SetCheck(BST_UNCHECKED);
	return 0;
}
