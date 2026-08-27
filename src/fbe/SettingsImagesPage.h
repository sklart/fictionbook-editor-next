#pragma once

#include <atlhost.h>
#include "resource.h"
#include "SettingsPageLifecycle.h"

class CSettingsImagesPage : public CAxDialogImpl<CSettingsImagesPage>, public ISettingsPage
{
	CButton m_askImage, m_clearImages, m_imageImportKeepSupported;
	CComboBox m_imageType, m_imageImportFormat;
	CUpDownCtrl m_jpegSpin, m_imageImportJpegSpin;
	CEdit m_jpegQuality, m_imageImportJpegQuality;
public:
	enum { IDD = IDD_SETTINGS_IMAGES };
BEGIN_MSG_MAP(CSettingsImagesPage)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	COMMAND_HANDLER(IDC_SETTINGS_ASKIMAGE, BN_CLICKED, OnAskImage)
	CHAIN_MSG_MAP(CAxDialogImpl<CSettingsImagesPage>)
END_MSG_MAP()
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnAskImage(WORD, WORD, HWND, BOOL&);
	bool Validate(); void Commit(); bool CancelChanges();
};
