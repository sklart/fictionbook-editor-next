#pragma once

#include <atlhost.h>
#include "..\\..\\resource.h"
#include "..\\..\\ThemeManager.h"
#include "SettingsPageLifecycle.h"
#include "SettingsTooltips.h"

class CPaletteManagerDialog;
constexpr UINT WM_FBE_APPEARANCE_PREVIEW_LAYOUT = WM_APP + 0x14A;

class CSettingsGeneralPage : public CAxDialogImpl<CSettingsGeneralPage>, public ISettingsPage
{
	friend class CPaletteManagerDialog;
	HWND m_paletteManager = NULL;
	CComboBox m_language;
	CComboBox m_interfaceTheme;
	CComboBox m_interfacePalette;
	CComboBox m_paletteRole;
	std::vector<ThemeManager::CustomPalette> m_customPalettes;
	std::vector<ThemeManager::CustomPalette> m_pendingPalettes;
	std::vector<std::wstring> m_newPaletteIds;
	std::vector<std::wstring> m_deletedPaletteIds;
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
	int m_acceptedThemeIndex = INTERFACE_THEME_AUTOMATIC;
	int m_themeDropdownStart = -1;
	bool m_themePreservedBeforeDropdown = false;
	bool m_reopeningThemeDropdown = false;
	bool m_cancellingThemeDropdown = false;
	int m_paletteDropdownStart = -1;
	bool m_paletteChangedBeforeDropdown = false;
	bool m_reopeningPaletteDropdown = false;
	bool m_applyingPalettePreview = false;
	bool m_cancellingPaletteDropdown = false;
	bool m_closing = false;
	bool m_paletteChanged = false;
	bool m_preserveUnavailableDark = false;
	bool m_darkThemeSupported = true;
	bool m_unavailableListTooltips = false;
	void UpdateThemeNotice();
	void FitLocalizedLayout();
	void UpdatePaletteColorControls();
	void UpdatePaletteContrast();
	bool PaletteItemAvailable(int index) const;
	static LRESULT CALLBACK ThemeComboSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
	static LRESULT CALLBACK ThemeListSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
	static void UpdateThemeComboItemHeight(HWND window);

public:
	explicit CSettingsGeneralPage(bool darkThemeSupported = ThemeManager::IsDarkModeSupported()) : m_darkThemeSupported(darkThemeSupported) {}
	static InterfacePalette PaletteForRow(int row);
	static int RowForPalette(InterfacePalette palette);
	bool GetStagedCustomPalette(ThemeManager::CustomPalette& palette) const {
		const int index = m_acceptedPaletteIndex - INTERFACE_PALETTE_COUNT;
		if(index < 0 || index >= static_cast<int>(m_customPalettes.size())) return false;
		palette = m_customPalettes[index]; return true;
	}
	enum { IDD = IDD_SETTINGS_GENERAL };

	BEGIN_MSG_MAP(CSettingsGeneralPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawThemeItem)
		MESSAGE_HANDLER(WM_FBE_THEMECHANGED, OnPaletteThemeChanged)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER(IDC_INTERFACE_THEME, CBN_SELCHANGE, OnInterfaceThemeChanged)
		COMMAND_HANDLER(IDC_INTERFACE_THEME, CBN_SELENDOK, OnInterfaceThemeChanged)
		COMMAND_HANDLER(IDC_INTERFACE_THEME, CBN_DROPDOWN, OnThemeDropdown)
		COMMAND_HANDLER(IDC_INTERFACE_THEME, CBN_SELENDCANCEL, OnThemeDropdownCancel)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE, CBN_SELCHANGE, OnInterfacePaletteChanged)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE, CBN_SELENDOK, OnPaletteDropdownAccept)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE, CBN_DROPDOWN, OnPaletteDropdown)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE, CBN_SELENDCANCEL, OnPaletteDropdownCancel)
		COMMAND_HANDLER(IDC_INTERFACE_PALETTE_COPY, BN_CLICKED, OnCopyPalette)
		COMMAND_HANDLER(IDC_PALETTE_MANAGE, BN_CLICKED, OnManagePalettes)
		COMMAND_HANDLER(IDC_MAIN_TOOLBAR_ICON_SIZE, CBN_SELCHANGE, OnIconPreviewChanged)
		COMMAND_HANDLER(IDC_SCRIPT_ICON_SIZE, CBN_SELCHANGE, OnIconPreviewChanged)
		COMMAND_HANDLER(IDC_PALETTE_IMPORT, BN_CLICKED, OnImportPalette)
		COMMAND_HANDLER(IDC_PALETTE_EXPORT, BN_CLICKED, OnExportPalette)
		COMMAND_HANDLER(IDC_PALETTE_RENAME, BN_CLICKED, OnRenamePalette)
		COMMAND_HANDLER(IDC_PALETTE_DELETE, BN_CLICKED, OnDeletePalette)
		COMMAND_HANDLER(IDC_PALETTE_CHOOSE_COLOR, BN_CLICKED, OnChoosePaletteColor)
		COMMAND_HANDLER(IDC_PALETTE_RESET_COLOR, BN_CLICKED, OnResetPaletteColor)
		CHAIN_MSG_MAP(CAxDialogImpl<CSettingsGeneralPage>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDrawThemeItem(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnPaletteThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClickedOK(WORD, WORD, HWND, BOOL&);
	LRESULT OnClickedCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnInterfaceThemeChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnThemeDropdown(WORD, WORD, HWND, BOOL&);
	LRESULT OnThemeDropdownCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnInterfacePaletteChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnPaletteDropdown(WORD, WORD, HWND, BOOL&);
	LRESULT OnPaletteDropdownCancel(WORD, WORD, HWND, BOOL&);
	LRESULT OnPaletteDropdownAccept(WORD, WORD, HWND, BOOL&);
	LRESULT OnCopyPalette(WORD, WORD, HWND, BOOL&);
	LRESULT OnManagePalettes(WORD, WORD, HWND, BOOL&);
	LRESULT OnIconPreviewChanged(WORD, WORD, HWND, BOOL&);
	LRESULT OnImportPalette(WORD, WORD, HWND, BOOL&);
	LRESULT OnExportPalette(WORD, WORD, HWND, BOOL&);
	LRESULT OnRenamePalette(WORD, WORD, HWND, BOOL&);
	LRESULT OnDeletePalette(WORD, WORD, HWND, BOOL&);
	bool StagePaletteRename(const CString& name);
	bool StagePaletteDelete();
	bool StagePaletteImport(const CString& path, CString& error);
	LRESULT OnChoosePaletteColor(WORD, WORD, HWND, BOOL&);
	LRESULT OnResetPaletteColor(WORD, WORD, HWND, BOOL&);
	bool StagePaletteCopy(const CString& name);
	CString SuggestedPaletteCopyName() const;
	bool StagePaletteColor(ThemeColorRole role, COLORREF color, bool custom);
	void ConfigureInterfaceTheme(bool darkThemeSupported);
	void MakeAppearanceOnly();
	int DesiredIconPreviewHeight(UINT dpi) const;
	bool Validate(); bool PrepareCommit(SettingsFileTransaction&, CString&); void Commit(); void FinishCommit(); bool CancelChanges();
};
