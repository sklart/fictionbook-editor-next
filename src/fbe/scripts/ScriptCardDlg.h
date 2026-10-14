#pragma once

#include "ScriptCardParser.h"
#include "ScriptDescriptor.h"
#include "../ThemeManager.h"
#include "../resource.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

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
	const CString& StatusText() const { return m_statusText; }
	HWND WarningWindow() const { return m_warning; }
	HWND WarningTooltipWindow() const { return m_warningTip; }
	const CString& WarningText() const { return m_warningText; }
	const CString& OriginText() const { return m_originText; }
	const CString& HeaderRightText() const { return m_headerRightText; }
	size_t VisualBlockCount() const { return m_blocks.size(); }
	HWND ViewportWindow() const { return m_viewport; }
	int ScrollPosition() const { return m_scrollY; }
	bool CanScroll() const { RECT bounds = {}; return m_viewport && ::GetClientRect(m_viewport, &bounds) &&
		m_contentHeight > bounds.bottom - bounds.top; }
	int CharacteristicLayoutRows() const;

	BEGIN_MSG_MAP(CScriptCardDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInit)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnViewportWheel)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, OnMinMax)
		MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
		MESSAGE_HANDLER(WM_FBE_THEMECHANGED, OnThemeChanged)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnControlColor)
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
	CString m_headerText;
	CString m_headerRightText;
	CString m_statusText;
	CString m_originText;
	struct VisualBlock {
		CString title, content;
		int layout = 0; // 0: full width, 1: property grid, 2: paired blocks, 3: characteristics.
		bool warning = false;
		bool settings = false;
		struct Range { LONG begin, end; };
		std::vector<Range> titles, names, values;
		HWND heading = NULL, body = NULL;
	};
	std::vector<VisualBlock> m_blocks;
	HWND m_viewport = NULL;
	HWND m_headerRight = NULL;
	HWND m_warning = NULL;
	HWND m_warningTip = NULL;
	CString m_warningText;
	HFONT m_headingFont = NULL;
	int m_scrollY = 0;
	int m_contentHeight = 0;
	struct SectionRange { LONG begin; LONG end; bool warning; };
	std::vector<SectionRange> m_sections;
	HFONT m_font = NULL;
	HICON m_icon = NULL;
	HICON m_windowIconSmall = NULL;
	HICON m_windowIconBig = NULL;
	HMODULE m_richEdit = NULL;
	UINT m_dpi = 96;

	LRESULT OnInit(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnViewportWheel(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnMinMax(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnControlColor(UINT, WPARAM, LPARAM, BOOL&);
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
	void ApplyWindowIcon();
	void ApplyRichFormatting();
	void ApplySettingFormatting();
	void AppendSection(const CString& label, const CString& value, bool warning = false);
	void AddVisualBlock(const CString& label, const CString& value, int layout = 0, bool warning = false);
	void AddSettingsBlock(const std::vector<ScriptCards::Setting>& settings);
	void RebuildVisualBlocks();
	void LayoutVisualBlocks(int width, int height);
	void ScrollTo(int offset);
	void HandleViewportScroll(WPARAM command);
	static LRESULT CALLBACK ViewportSubclass(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
	static LRESULT CALLBACK BlockSubclass(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
	void CopyText(const CString& text);
	void Layout();
	CString PlacementPath() const;
	void RestorePlacement();
	void SavePlacement();
	void ShowCard(const ScriptCards::Card& card);
};
