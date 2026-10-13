#pragma once

#include "ScriptCardParser.h"
#include "ScriptDescriptor.h"
#include "../ThemeManager.h"
#include "../resource.h"
#include <atomic>
#include <functional>
#include <memory>

class CScriptCardDlg : public CDialogImpl<CScriptCardDlg>
{
public:
	enum { IDD = IDD_SCRIPT_CARD };
	~CScriptCardDlg();
	bool ShowScript(HWND owner, const ScriptDescriptor& script, const CString& resourceRoot,
		const std::function<void(const CString&)>& runScript);
	void RefreshLocalization();
	bool IsLoading() const { return m_work != nullptr; }
	const CString& CurrentUid() const { return m_script.uid; }
	const CString& BodyText() const { return m_bodyText; }

	BEGIN_MSG_MAP(CScriptCardDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInit)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnMinMax)
		MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
		MESSAGE_HANDLER(WM_FBE_THEMECHANGED, OnThemeChanged)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(IDC_SCRIPT_CARD_RUN, OnRun)
		COMMAND_ID_HANDLER(IDC_SCRIPT_CARD_OPEN, OnOpen)
		COMMAND_ID_HANDLER(IDC_SCRIPT_CARD_COPY, OnCopy)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCommand)
	END_MSG_MAP()

private:
	struct Work {
		CString path;
		CString resourceRoot;
		ScriptCards::Card card;
		std::atomic<bool> done{ false };
	};
	std::shared_ptr<Work> m_work;
	std::function<void(const CString&)> m_runScript;
	ScriptDescriptor m_script = {};
	ScriptCards::Card m_lastCard;
	CString m_bodyText;
	HFONT m_font = NULL;
	HICON m_icon = NULL;
	HMODULE m_richEdit = NULL;
	UINT m_dpi = 96;

	LRESULT OnInit(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnMinMax(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnRun(WORD, WORD, HWND, BOOL&);
	LRESULT OnOpen(WORD, WORD, HWND, BOOL&);
	LRESULT OnCopy(WORD, WORD, HWND, BOOL&);
	LRESULT OnCloseCommand(WORD, WORD, HWND, BOOL&);
	void ApplyFont();
	void ApplyBodyPalette();
	void ApplyScriptIcon();
	void Layout();
	CString PlacementPath() const;
	void RestorePlacement();
	void SavePlacement();
	void ShowCard(const ScriptCards::Card& card);
};
