// SettingsDlg.h : Declaration of the CSettingsDlg

#pragma once

#include <vector>
#include "resource.h"
#include "SettingsAdvancedPage.h"
#include "SettingsImagesPage.h"
#include "SettingsHotkeysDlg.h"
#include "SettingsWordsDlg.h"
#include "SettingsGeneralPage.h"
#include "SettingsEditorPage.h"
#include "SettingsSpellingPage.h"
#include "SettingsSourcePage.h"

enum class SettingsPageId
{
	General,
	Editor,
	Source,
	Images,
	Spelling,
	Keyboard,
	Words,
	Advanced,
	Count
};

// CSettingsDlg

class CSettingsDlg : public CAxDialogImpl<CSettingsDlg>
{
	CListBox m_navigation;
	CSettingsAdvancedPage* m_advancedPage;
	CSettingsGeneralPage* m_generalPage;
	CSettingsEditorPage* m_editorPage;
	CSettingsSpellingPage* m_spellingPage;
	CSettingsImagesPage* m_imagesPage;
	CSettingsSourcePage* m_sourcePage;
	CSettingsHotkeysDlg* m_hotkeysPage;
	CSettingsWordsDlg* m_wordsPage;
	CWindow* m_pages[static_cast<int>(SettingsPageId::Count)];
	ISettingsPage* m_pageLifecycle[static_cast<int>(SettingsPageId::Count)];
	SettingsPageId m_currentPage;
	CRect m_navigationRect;
	CSize m_minimumWindowSize;
	CSize m_preferredWindowSize;
	bool m_automaticSize;
	CRect m_sizeMoveStart;
	UINT m_sizeMoveDpi;
	bool m_sizeMoveDpiChanged;
	bool m_sizingDuringMove;
	UINT m_initialDpi;
	UINT m_layoutDpi;
	int m_pageLeft;
	int m_pageTop;
	int m_pageRightMargin;
	int m_pageBottomMargin;
	int m_buttonWidth;
	int m_buttonHeight;
	int m_buttonGap;
	int m_buttonBottomMargin;
	int m_navigationBottomGap;
	struct PageControlLayout { HWND window; CRect rect; int id; };
	struct PageLayout {
		CSize templateSize = CSize(0, 0);
		CSize contentSize = CSize(0, 0);
		int scrollX = 0;
		int scrollY = 0;
		std::vector<PageControlLayout> controls;
	};
	PageLayout m_pageLayouts[static_cast<int>(SettingsPageId::Count)];

public:
	CSettingsDlg();
	~CSettingsDlg();

	enum { IDD = IDD_TOOLS_SETTINGS };

BEGIN_MSG_MAP(CSettingsDlg)
	MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	MESSAGE_HANDLER(WM_TIMER, OnTimer)
	MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
	COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
	COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
	COMMAND_HANDLER(IDC_SETTINGS_NAV, LBN_SELCHANGE, OnNavigationChanged)
	MESSAGE_HANDLER(WM_SIZE, OnSize)
	MESSAGE_HANDLER(WM_ENTERSIZEMOVE, OnEnterSizeMove)
	MESSAGE_HANDLER(WM_SIZING, OnSizing)
	MESSAGE_HANDLER(WM_EXITSIZEMOVE, OnExitSizeMove)
	MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
	MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSystemAppearanceChanged)
	MESSAGE_HANDLER(WM_SYSCOLORCHANGE, OnSystemAppearanceChanged)
	MESSAGE_HANDLER(WM_THEMECHANGED, OnSystemAppearanceChanged)
	MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
	CHAIN_MSG_MAP(CAxDialogImpl<CSettingsDlg>)
END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnNavigationChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnEnterSizeMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSizing(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnExitSizeMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnGetMinMaxInfo(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDpiChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSystemAppearanceChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

private:
	void SelectPage(SettingsPageId page);
	void LayoutControls(int width, int height);
	void CapturePageLayouts();
	void LayoutPage(int index);
	void ScrollPage(int index, WPARAM command, bool horizontal);
	void RevealFocusedPageControl(int index);
	bool ProbeLayout();
	static LRESULT CALLBACK PageSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
		UINT_PTR subclassId, DWORD_PTR reference);
	void RestoreGeometry();
	void SaveGeometry();
	CRect GetPageRect();
	static int PageIndex(SettingsPageId page);
};
