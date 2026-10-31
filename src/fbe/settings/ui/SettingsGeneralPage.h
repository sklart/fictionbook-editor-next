#pragma once

#include <atlhost.h>
#include "..\\..\\resource.h"
#include "..\\..\\ThemeManager.h"
#include "SettingsPageLifecycle.h"
#include "SettingsTooltips.h"

class CSettingsGeneralPage : public CAxDialogImpl<CSettingsGeneralPage>, public ISettingsPage
{
	CComboBox m_language;
	CComboBox m_interfaceTheme;
	CComboBox m_interfacePalette;
	CComboBox m_paletteRole;
	std::vector<ThemeManager::CustomPalette> m_customPalettes;
	std::vector<ThemeManager::CustomPalette> m_pendingPalettes;
	std::vector<std::wstring> m_newPaletteIds;
	CComboBox m_scriptIconSize;
	CComboBox m_mainToolbarIconSize;
	CComboBox m_genreCatalog;
	CComboBox m_descriptionLayout;
	CComboBox m_defaultEncoding;
	CButton m_keepEncoding;
	CButton m_restorePosition;
	CComboBox m_updateChannel;
	CSettingsTooltips m_tooltips;
	InterfaceTheme m_originalTheme;
	InterfacePalette m_originalPalette = INTERFACE_PALETTE_CLASSIC;
	ThemeManager::CustomPalette m_originalCustomPalette;
	bool m_originalCustomPaletteActive = false;
	int m_acceptedPaletteIndex = 0;
	int m_paletteDropdownStart = -1;
	bool m_paletteChangedBeforeDropdown = false;
	bool m_reopeningPaletteDropdown = false;
	bool m_cancellingPaletteDropdown = false;
	bool m_paletteChanged = false;
	bool m_preserveUnavailableDark = false;
	bool m_darkThemeSupported = true;
	void UpdateThemeNotice();
	void FitLocalizedLayout();
	void UpdatePaletteColorControls();
	void UpdatePaletteContrast();

public:
	explicit CSettingsGeneralPage(bool darkThemeSupported = ThemeManager::IsDarkModeSupported()) : m_darkThemeSupported(darkThemeSupported) {}
	enum { IDD = IDD_SETTINGS_GENERAL };

	BEGIN_MSG_MAP(CSettingsGeneralPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_FBE_THEMECHANGED, OnPaletteThemeChanged)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER(IDC_INTERFACE_THEME, CBN_SELCHANGE, OnInterfaceThemeChanged)
		COMMAND_HANDLER(IDC_INTERFACE_THEME, CBN_SELENDOK, OnInterfaceThemeChanged)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE, CBN_SELCHANGE, OnInterfacePaletteChanged)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE, CBN_DROPDOWN, OnPaletteDropdown)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE, CBN_SELENDCANCEL, OnPaletteDropdownCancel)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE_COPY, BN_CLICKED, OnCopyPalette)
		COMMAND_HANDLER(IDC_PALETTE_IMPORT, BN_CLICKED, OnImportPalette)
		COMMAND_HANDLER(IDC_PALETTE_EXPORT, BN_CLICKED, OnExportPalette)
		COMMAND_HANDLER(IDC_PALETTE_CHOOSE_COLOR, BN_CLICKED, OnChoosePaletteColor)
		COMMAND_HANDLER(IDC_PALETTE_RESET_COLOR, BN_CLICKED, OnResetPaletteColor)
		CHAIN_MSG_MAP(CAxDialogImpl<CSettingsGeneralPage>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnPaletteThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnInterfaceThemeChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnInterfacePaletteChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnPaletteDropdown(WORD, WORD, HWND, BOOL&);
	LRESULT OnPaletteDropdownCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnCopyPalette(WORD, WORD, HWND, BOOL&);
	LRESULT OnImportPalette(WORD, WORD, HWND, BOOL&);
	LRESULT OnExportPalette(WORD, WORD, HWND, BOOL&);
	bool StagePaletteImport(const CString& path, CString& error);
	LRESULT OnChoosePaletteColor(WORD, WORD, HWND, BOOL&);
	LRESULT OnResetPaletteColor(WORD, WORD, HWND, BOOL&);
	bool StagePaletteCopy(const CString& name);
	bool StagePaletteColor(ThemeColorRole role, COLORREF color, bool custom);
	void ConfigureInterfaceTheme(bool darkThemeSupported);
	bool Validate(); bool PrepareCommit(SettingsFileTransaction&, CString&); void Commit(); void FinishCommit(); bool CancelChanges();
};
