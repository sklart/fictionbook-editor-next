// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__38D356D4_C28B_47B0_A7AD_8C6B70F7F283__INCLUDED_)
#define AFX_MAINFRM_H__38D356D4_C28B_47B0_A7AD_8C6B70F7F283__INCLUDED_

#include "stdafx.h"
#include <dlgs.h>
#include "resource.h"
#include "res1.h"
#include "RuntimeLocalization.h"
#include "document\\DocumentLocation.h"
#include "document\\DocumentSession.h"
#include "recovery\\RecoveryController.h"
#include "document\\recent\\RecentDocumentsController.h"
#include "scripts\\ScriptUiController.h"
#include "plugins\\PluginUiController.h"
#include "plugins\\PluginExecutionController.h"
#include "diagnostics\\DiagnosticCommandService.h"
#include "toolbars\\ScriptToolbarRuntime.h"
#include "toolbars\\ScriptToolbarManager.h"

#include "atlctrlsext.h"

#include "utils.h"
#include "apputils.h"

#include "FBEView.h"
#include "FBDoc.h"
#include "TreeView.h"
#include "ContainerWnd.h"
#include "Scintilla.h"
#include "SciLexer.h"
#include "EditorEngine.h"
#include "xmlMatchedTagsHighlighter.h"
#include "source\\Fb2SourceAutocomplete.h"
#include "source\\ui\\SourceEditorControl.h"
#include "source\\SourceViewSession.h"
#include "source\\BodySourceSelectionCoordinator.h"
#include "view\\EditorViewState.h"
#include "view\\EditorViewTransition.h"
#include "view\\EditorViewController.h"
#include "view\\EditorSelectionState.h"
#include "FBE.h"
#include "Words.h"
#include "SearchReplace.h"
#include "FindResultsPane.h"
#include "DocumentTree.h"
#include "Speller.h"
#include "StatusBarUnicode.h"
#include "StatusBarText.h"
#include "StatusBarBehavior.h"
#include "StatusBarState.h"
#include "ui/ContextAttributeBars.h"
#include "ThemeManager.h"
#include "view/ui/EditorViewPresentationHost.h"

#if _MSC_VER >= 1000
#pragma once
#pragma warning(disable : 4996)
#endif // _MSC_VER >= 1000

#define MSGFLT_ADD 1
#define MSGFLT_REMOVE 2

// for MessageBox localization
void HookSysDialogs();
void UnhookSysDialogs();

class CThemedSplitterWindow : public WTL::CSplitterWindowImpl<CThemedSplitterWindow>, public IEditorViewSplitter
{
public:
	DECLARE_WND_CLASS_EX2(_T("FBE_ThemedVerticalSplitter"), CThemedSplitterWindow, CS_DBLCLKS, COLOR_WINDOW)

	CThemedSplitterWindow() : WTL::CSplitterWindowImpl<CThemedSplitterWindow>(true) {}
	void SetPresentationSinglePane(int pane) override { SetSinglePaneMode(pane); }
	void DrawSplitterBar(CDCHandle dc)
	{
		RECT rect = {};
		if(!GetSplitterBarRect(&rect)) return;
		::FillRect(dc.m_hDC, &rect, ThemeManager::Brush(THEME_COLOR_SEPARATOR));
		RECT edge = rect;
		edge.right = edge.left + 1;
		::FillRect(dc.m_hDC, &edge, ThemeManager::Brush(THEME_COLOR_BORDER));
		edge = rect;
		edge.left = edge.right - 1;
		::FillRect(dc.m_hDC, &edge, ThemeManager::Brush(THEME_COLOR_BORDER));
	}
};

class CThemedHorSplitterWindow : public WTL::CSplitterWindowImpl<CThemedHorSplitterWindow>
{
public:
	DECLARE_WND_CLASS_EX2(_T("FBE_ThemedHorizontalSplitter"), CThemedHorSplitterWindow, CS_DBLCLKS, COLOR_WINDOW)

	CThemedHorSplitterWindow() : WTL::CSplitterWindowImpl<CThemedHorSplitterWindow>(false) {}
	void DrawSplitterBar(CDCHandle dc)
	{
		RECT rect = {};
		if(!GetSplitterBarRect(&rect)) return;
		::FillRect(dc.m_hDC, &rect, ThemeManager::Brush(THEME_COLOR_SEPARATOR));
		RECT edge = rect;
		edge.bottom = edge.top + 1;
		::FillRect(dc.m_hDC, &edge, ThemeManager::Brush(THEME_COLOR_BORDER));
		edge = rect;
		edge.top = edge.bottom - 1;
		::FillRect(dc.m_hDC, &edge, ThemeManager::Brush(THEME_COLOR_BORDER));
	}
};

class CMainFrame :	public CFrameWindowImpl<CMainFrame>,
					public CCustomizableToolBarCommands<CMainFrame>,
					public CUpdateUI<CMainFrame>,
					public CMessageFilter,
					public CIdleHandler,
					private IEditorSourceExchange
{
public:
	enum UiDirtyFlags
	{
		UiDirtyNone = 0,
		UiDirtySelection = 1 << 0,
		UiDirtyDocument = 1 << 1,
		UiDirtyClipboard = 1 << 2,
		UiDirtySource = 1 << 3,
		UiDirtyView = 1 << 4,
		UiDirtyToolbar = 1 << 5,
		UiDirtyStatus = 1 << 6,
		UiDirtyScroll = 1 << 7,
		UiDirtyAll = UiDirtySelection | UiDirtyDocument | UiDirtyClipboard | UiDirtySource | UiDirtyView | UiDirtyToolbar | UiDirtyStatus | UiDirtyScroll
	};

	enum FILE_OP_STATUS
	{
		FAIL,
		OK,
		CANCELLED
	};

	DECLARE_FRAME_WND_CLASS(_T("FictionBookEditorFrame"), IDR_MAINFRAME)

	CSciFindDlg*	m_sci_find_dlg;
	CSciReplaceDlg*	m_sci_replace_dlg;

	// Child windows
	CThemedSplitterWindow		m_splitter; // doc tree and views
	CThemedHorSplitterWindow	m_editor_results_splitter; // editor and docked Find results
	CContainerWnd		m_view; // document, description and source
	CFindResultsPane	m_find_results_pane;
	//CPaneContainer	m_tree_pane; // left pane with a tree
	//CSplitterWindow		m_dummy_pane; // frame around the tree
	//CTreeView			m_tree; // treeview itself
	CDocumentTree		m_document_tree;

  CMultiPaneStatusBarCtrl m_status; // status bar
  wchar_t strINS[MAX_LOAD_STRING + 1];
  wchar_t strOVR[MAX_LOAD_STRING + 1];

	CCommandBarCtrl	m_MenuBar;			// menu bar
	CToolBarCtrl	m_CmdToolbar;		// commands toolbar
	CImageList		m_commandToolbarImages;	// application-owned command toolbar image list
	int			m_table_toolbar_image_indices[8];
	CToolBarCtrl	m_ScriptsToolbar;	// commands toolbar
	ScriptToolbarRuntimeCollection m_scriptToolbars;
	ScriptToolbarManager m_scriptToolbarManager;
	int			m_scriptsToolbarBaseImageCount;
	CReBarCtrl		m_rebar;			// toolbars
	ContextAttributeBars m_contextAttributeBars;
	SourceEditorControl m_source; // source editor presentation owner
	//bool			m_save_sp_mode;
  FbeRecentDocuments::Controller m_recentDocuments;
  FB::Doc		  *m_doc; // currently open document
  DocumentSession m_document_session;
  DWORD			  m_last_tree_update;
	DWORD			  m_last_external_file_check;
  bool			  m_external_file_check_started;
	bool			  m_clipboard_listener_registered;
	bool			  m_clipboard_has_bitmap;
	DWORD			  m_last_clipboard_fallback_check;
	bool			  m_clipboard_fallback_check_started;
	unsigned int	  m_ui_dirty;
	struct SelectionContext
	{
		MSHTML::IHTMLElementPtr container;
		MSHTML::IHTMLElementPtr structuralContainer;
		MSHTML::IHTMLElementPtr image;
		MSHTML::IHTMLElementPtr section;
		MSHTML::IHTMLElementPtr table;
		MSHTML::IHTMLElementPtr tableCell;
		MSHTML::IHTMLElementPtr anchor;
		bool insideCode;
		bool insideSpan;
		bool valid;
		SelectionContext() : insideCode(false), insideSpan(false), valid(false) {}
		void Invalidate()
		{
			container = nullptr; structuralContainer = nullptr; image = nullptr;
			section = nullptr; table = nullptr; tableCell = nullptr; anchor = nullptr;
			insideCode = false; insideSpan = false;
			valid = false;
		}
	} m_selection_context;
  BOOL			  m_last_sci_ovr:1;
  bool			  m_last_ie_ovr:1;
  bool			  m_doc_changed:1;
  bool			  m_sel_changed:1;
  bool			  m_change_state:1;
  bool			  m_need_title_update:1;
	FbeRecovery::RecoveryController m_recovery;
	ULONGLONG			  m_recovery_generation;
	ULONGLONG			  m_recovery_saved_generation;
	DWORD				  m_recovery_last_edit_tick;
  UINT            m_current_dpi;
  bool            m_status_layout_posted;

  // IDs in combobox
  bool			  m_cb_updated:1;
  bool			  m_cb_last_images:1; // images or plain ids?
  bool			  m_ignore_cb_changes:1;

  int			  m_want_focus; // focus this control when idle

  FBEStatusBar::State m_status_state;

  bool			  m_restore_pos_cmdline;
  
  // incremental search helpers
  CString		  m_is_str;
  CString		  m_is_prev;
  int			  m_incsearch;
  bool			  m_is_fail;

	FbeScripts::UiController m_scripts;
	bool m_testFailNextInitializeScripts;
	int m_testFailAfterCustomToolbarCreates;
	std::vector<CString> m_scriptToolbarMenuIds;
	void ReleaseScriptResources();
	void RestorePortableToolbarLayout(HWND toolbar, bool scriptsToolbar);
	void SavePortableToolbarLayout();
	void DestroyScriptToolbarRuntimeControls();
	bool ApplyScriptToolbarDefinitions(const std::vector<ScriptToolbarDefinition>& previous, const std::vector<ScriptToolbarDefinition>& current);
	bool UpdateScriptToolbarItems(const CString& id, const std::vector<PortableToolbarItem>& items);
	bool InitializeScriptsFromDefinitions(const std::vector<ScriptToolbarDefinition>& definitions, bool hasPersistedMainDefinition);
	void RefreshScriptToolbarViewMenu();
	void InitScriptHotkey(ScriptDescriptor&);

  // contruction/destruction
  CMainFrame() : m_doc(0), m_document_session(), m_last_tree_update(0), m_last_external_file_check(0), m_external_file_check_started(false), m_clipboard_listener_registered(false), m_clipboard_has_bitmap(false), m_last_clipboard_fallback_check(0), m_clipboard_fallback_check_started(false), m_ui_dirty(UiDirtyAll), m_last_sci_ovr(true), m_last_ie_ovr(true),
	 m_doc_changed(false), m_sel_changed(false), m_change_state(false), m_need_title_update(false),
	 m_recovery_generation(0), m_recovery_saved_generation(0), m_recovery_last_edit_tick(0),
	m_current_dpi(96), m_status_layout_posted(false), m_source_view_session(m_source, m_doc, m_editor_selection_state, m_source_selection_coordinator), m_cb_updated(false),
    m_cb_last_images(false), m_ignore_cb_changes(false), m_want_focus(0),
    m_restore_pos_cmdline(false), m_incsearch(0), m_is_fail(false),
    m_sci_find_dlg(0), m_sci_replace_dlg(0),
	 m_scripts(ID_EDIT_INS_SYMBOL + 101, 999), m_testFailNextInitializeScripts(false), m_testFailAfterCustomToolbarCreates(0),
	    m_bad_xml(false), m_selBandID(-1), m_scriptsToolbarBaseImageCount(0)
	// added by SeNS
	{
		strINS[0] = L'\0';
		strOVR[0] = L'\0';
		for(int index = 0; index < 8; ++index)
		{
			m_table_toolbar_image_indices[index] = -1;
		}
		if (_Settings.GetUseSpellChecker())
		{
			m_Speller = new CSpeller(U::GetProgDir()+L"dict\\");
		}
		else
		{
			m_Speller = NULL;
		}
	}
  ~CMainFrame(); 
  // toolbars
  bool	  IsBandVisible(int id);
  
  // browser controls
  void	  AttachDocument(FB::Doc *doc);
  CFBEView& ActiveView() {
/*    return m_doc->m_desc==m_view.GetActiveWnd() ?
	      m_doc->m_desc : m_doc->m_body;*/
	  return m_doc->m_body;
  }
  //bool	  IsSourceActive() { return m_source==m_view.GetActiveWnd(); }
  bool	  IsSourceActive() 
  { 
	  return m_editor_view_state.Current() == EditorView::Source;
  }

  // document structure
  void	  GetDocumentStructure();
  void	  GoTo(MSHTML::IHTMLElement *elem);
  void	  GoTo(int selected_pos);

  // loading/saving support
  CString GetOpenFileName();
  CString GetSaveFileName(CString& encoding);
  bool	  SaveToFile(const CString& filename);
  bool	  DiscardChanges();

	FILE_OP_STATUS	  SaveFile(bool askname);
	void CommitSuccessfulSave();
  FILE_OP_STATUS	  LoadFile(const wchar_t *initfilename=NULL, const DocumentLocation* preferredArchiveLocation=NULL);
  void RunPortableStateTestScenario();
  void TryRestoreRecovery();
  bool SaveRecoveryNow();
	bool TryAutoRecovery();
	void MarkRecoveryDirty();

  // show a specific view
	void	  ShowView(EditorView vt=EditorView::Body);
	EditorView NextEditorView();
	EditorSourceOperationResult PrepareSourceDocument(EditorView previous) override;
  //VIEW_TYPE GetCurView();

  
	EditorViewState m_editor_view_state;
	EditorViewController m_editor_view_controller;
	EditorSelectionState m_editor_selection_state;
	BodySourceSelectionCoordinator m_source_selection_coordinator;
	SourceViewSession m_source_view_session;
	Fb2SourceAutocomplete   m_fb2_autocomplete;

	EditorSourceOperationResult CommitSourceDocument() override;
	void ApplyEditorViewCommandUi(EditorView previous, EditorView target);
	void PresentEditorViewChangeFailure(const EditorViewChangeResult& result);
  void ClearSelection();

	// Plugins support
	PluginUiController m_plugins;
	PluginExecutionController m_plugin_execution;
	FbeDiagnostics::DiagnosticCommandService m_diagnostic_commands;
	void InitializeExtensionUi();
	bool InitializeScripts();
	LRESULT OnViewScriptToolbarToggle(WORD, WORD, HWND, BOOL&);
	void InitializeBundledPlugins();
	void InitializeRecentDocumentsMenu();
	void RegisterPluginHotkey(CString guid, UINT cmd, CString name);

  void AddTbButton(HWND hWnd, const TCHAR *text, const int idCommand = 0, const BYTE bState = 0, const HICON icon = 0);


  // ui updating
  void	  UIUpdateViewCmd(CFBEView& view,WORD wID,OLECMD& oc,const TCHAR *hk);
  void	  UIUpdateViewCmd(CFBEView& view,WORD wID) { UIEnable(wID,view.CheckCommand(wID)); }
  void	  UISetCheckCmd(CFBEView& view, WORD wID)
  {
	  UISetCheck(wID, view.CheckSetCommand(wID));
  }

  void	  StopIncSearch(bool fCancel);
  void	  SetIsText();
	void InvalidateUi(unsigned int flags) { m_ui_dirty |= flags; }
	void InvalidateSelectionContext() { m_selection_context.Invalidate(); }
	void RebuildSelectionContext();
	DWORD BuildBodyCommandState(CFBEView& view);

	// source<->html exchange
	bool SourceToHTML();

	// track changes depending on current view
	bool DocChanged();

	// message handlers
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();
  
	BEGIN_UPDATE_UI_MAP(CMainFrame)
		// ui windows
		UPDATE_ELEMENT(ATL_IDW_BAND_FIRST, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ATL_IDW_BAND_FIRST+1, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ATL_IDW_BAND_FIRST+2, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ATL_IDW_BAND_FIRST+3, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ATL_IDW_BAND_FIRST+4, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ATL_IDW_BAND_FIRST+5, UPDUI_MENUPOPUP)

		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_FASTMODE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_VIEW_TREE, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_DESC, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_VIEW_BODY, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_VIEW_SOURCE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)

		// editing commands
		UPDATE_ELEMENT(ID_EDIT_UNDO, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_REDO, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_CUT, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_COPY, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_PASTE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_BOLD, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_ITALIC, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_STRIK, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_SUP, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_SUB, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_CODE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_FINDNEXT, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_STYLE_NORMAL, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_STYLE_SUBTITLE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_STYLE_TEXTAUTHOR, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TOOLS_SPELLCHECK, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_STYLE_LINK, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_STYLE_NOTE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_STYLE_NOLINK, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_ADD_TITLE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_ADD_BODY, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_ADD_TA, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_CLONE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_INS_IMAGE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_INS_INLINEIMAGE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_ADD_IMAGE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_ADD_EPIGRAPH, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_ADD_ANN, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_SPLIT, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_INS_POEM, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_INS_CITE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_INSERT_TABLE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TABLE_INSERT_ROW_ABOVE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TABLE_INSERT_ROW_BELOW, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TABLE_DELETE_ROW, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TABLE_INSERT_COLUMN_LEFT, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TABLE_INSERT_COLUMN_RIGHT, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TABLE_DELETE_COLUMN, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TABLE_TOGGLE_HEADER_CELL, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_TABLE_MAKE_HEADER_CELLS, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_TABLE_MAKE_NORMAL_CELLS, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_GOTO_FOOTNOTE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_GOTO_REFERENCE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_GOTO_MATCHTAG, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_GOTO_WRONGTAG, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_MERGE, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_REMOVE_OUTER_SECTION, UPDUI_MENUPOPUP|UPDUI_TOOLBAR)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(AU::WM_POSTCREATE, OnPostCreate)
		MESSAGE_HANDLER(AU::WM_SOURCE_MEMORY_BENCHMARK, OnSourceMemoryBenchmark)
		MESSAGE_HANDLER(AU::WM_BODY_SCROLL, OnBodyScroll)
		MESSAGE_HANDLER(AU::WM_DESCRIPTION_FORM_CHANGED, OnDescriptionFormChanged)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_QUERYENDSESSION, OnQueryEndSession)
		MESSAGE_HANDLER(WM_ENDSESSION, OnEndSession)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_CLIPBOARDUPDATE, OnClipboardUpdate)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
		MESSAGE_HANDLER(WM_SYSCOLORCHANGE, OnSettingChange)

		// added by SeNS: toolbar customization menu
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)

		#if _WIN32_WINNT>=0x0501
			MESSAGE_HANDLER(WM_THEMECHANGED, OnSettingChange)
		#endif
		MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
		MESSAGE_HANDLER(WM_FBE_APPLY_XML_SOURCE_THEME, OnApplyXmlSourceTheme)
		MESSAGE_HANDLER(WM_FBE_THEMECHANGED, OnThemeChanged)
		MESSAGE_HANDLER(AU::WM_SETSTATUSTEXT, OnSetStatusText)
		MESSAGE_HANDLER(AU::WM_TRACKPOPUPMENU, OnTrackPopupMenu)

		// incremental search support
		MESSAGE_HANDLER(WM_CHAR, OnChar)
		MESSAGE_HANDLER(WM_COMMAND, OnPreCommand)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_APP + 41, OnDeferredStatusBarLayout)
		MESSAGE_HANDLER(AU::WM_SHOW_FIND_RESULTS_PANE, OnShowFindResultsPane)
		MESSAGE_HANDLER(AU::WM_HIDE_FIND_RESULTS_PANE, OnHideFindResultsPane)
		MESSAGE_HANDLER(AU::WM_REFRESH_FIND_RESULTS_PANE, OnRefreshFindResultsPane)
		MESSAGE_HANDLER(AU::WM_DETACH_FIND_RESULTS_PANE, OnDetachFindResultsPane)

		// tree view notifications
		COMMAND_CODE_HANDLER(IDN_TREE_CLICK, OnTreeClick)
		COMMAND_CODE_HANDLER(IDN_TREE_RETURN, OnTreeReturn)
		COMMAND_CODE_HANDLER(IDN_TREE_MOVE_ELEMENT, OnTreeMoveElement)
		COMMAND_CODE_HANDLER(IDN_TREE_MOVE_ELEMENT_ONE, OnTreeMoveElementOne)
		COMMAND_CODE_HANDLER(IDN_TREE_MOVE_LEFT, OnTreeMoveLeftElement)
		COMMAND_CODE_HANDLER(IDN_TREE_MOVE_ELEMENT_SMART, OnTreeMoveElementSmart)
		COMMAND_CODE_HANDLER(IDN_TREE_VIEW_ELEMENT, OnTreeViewElement)
		COMMAND_CODE_HANDLER(IDN_TREE_VIEW_ELEMENT_SOURCE, OnTreeViewElementSource)
		COMMAND_CODE_HANDLER(IDN_TREE_DELETE_ELEMENT, OnTreeDeleteElement)
		COMMAND_CODE_HANDLER(IDN_TREE_MERGE, OnTreeMerge)
		COMMAND_CODE_HANDLER(IDN_TREE_UPDATE_ME, OnTreeUpdate)
		COMMAND_CODE_HANDLER(IDN_TREE_RESTORE, OnTreeRestore)

		// file menu
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileNew)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnFileSave)
		COMMAND_ID_HANDLER(ID_FILE_SAVE_AS, OnFileSaveAs)
		COMMAND_ID_HANDLER(ID_FILE_VALIDATE, OnFileValidate)

		COMMAND_RANGE_HANDLER(ID_PLUGIN_EXPORT_FIRST, ID_PLUGIN_EXPORT_LAST, OnToolsExport)
		COMMAND_RANGE_HANDLER(ID_PLUGIN_IMPORT_FIRST, ID_PLUGIN_IMPORT_LAST, OnToolsImport)
		COMMAND_ID_HANDLER(ID_LAST_PLUGIN, OnLastPlugin)

		COMMAND_RANGE_HANDLER(ID_FILE_MRU_FIRST,ID_FILE_MRU_LAST,OnFileOpenMRU)
		COMMAND_RANGE_HANDLER(ID_SCI_COLLAPSE1, ID_SCI_COLLAPSE9,OnSciCollapse)
		COMMAND_RANGE_HANDLER(ID_SCI_EXPAND1, ID_SCI_EXPAND9,OnSciExpand)

		// edit menu
		COMMAND_ID_HANDLER(ID_EDIT_INCSEARCH, OnEditIncSearch)
		COMMAND_ID_HANDLER(ID_EDIT_ADDBINARY, OnEditAddBinary)
		COMMAND_ID_HANDLER(ID_EDIT_FIND, OnEditFind)
		COMMAND_ID_HANDLER(ID_EDIT_FINDNEXT, OnEditFind)
		COMMAND_ID_HANDLER(ID_EDIT_REPLACE, OnEditFind)
		COMMAND_RANGE_HANDLER(ID_EDIT_INS_SYMBOL, ID_EDIT_INS_SYMBOL + 100, OnEditInsSymbol)

		// added by SeNS
		// popup menu (speller addons)
		COMMAND_ID_HANDLER(IDC_SPELL_IGNOREALL, OnSpellIgnoreAll)
		COMMAND_ID_HANDLER(IDC_SPELL_ADD2DICT, OnSpellAddToDict)
		COMMAND_RANGE_HANDLER(ID_SPELL_REPLACE_FIRST, ID_SPELL_REPLACE_LAST, OnSpellReplace)

		COMMAND_ID_HANDLER(ID_VER_ADVANCE, OnVersionAdvance)

		// view menu
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST, OnViewToolBar)
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST+1, OnViewToolBar)
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST+2, OnViewToolBar)
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST+3, OnViewToolBar)
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST+4, OnViewToolBar)
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST+5, OnViewToolBar)
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST+6, OnViewToolBar)
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST+7, OnViewToolBar)
		COMMAND_ID_HANDLER(ATL_IDW_BAND_FIRST+8, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_RANGE_HANDLER(ID_STATUS_PANE_POSITION, ID_STATUS_PANE_INSERT_MODE, OnStatusPaneVisibility)
		COMMAND_ID_HANDLER(ID_VIEW_FASTMODE, OnViewFastMode)
		COMMAND_ID_HANDLER(ID_VIEW_SCRIPT_TOOLBARS_MANAGE, OnViewScriptToolbarsManage)
		COMMAND_RANGE_HANDLER(ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_FIRST, ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_LAST, OnViewScriptToolbarToggle)
		COMMAND_ID_HANDLER(ID_VIEW_TREE, OnViewTree)
		COMMAND_ID_HANDLER(ID_VIEW_DESC, OnViewDesc)
		COMMAND_ID_HANDLER(ID_VIEW_BODY, OnViewBody)
		COMMAND_ID_HANDLER(ID_VIEW_SOURCE, OnViewSource)
		COMMAND_ID_HANDLER(ID_VIEW_OPTIONS, OnViewOptions)

		// tools menu
		COMMAND_ID_HANDLER(ID_TOOLS_WORDS, OnToolsWords)
		COMMAND_ID_HANDLER(ID_TOOLS_OPTIONS, OnToolsOptions)
		COMMAND_ID_HANDLER(ID_TOOLS_DIAGNOSTIC_TRACE, OnToolsDiagnosticTrace)
		COMMAND_ID_HANDLER(ID_TOOLS_OPEN_DIAGNOSTIC_LOG, OnToolsOpenDiagnosticLog)
		COMMAND_ID_HANDLER(ID_TOOLS_OPEN_DIAGNOSTIC_FOLDER, OnToolsOpenDiagnosticFolder)
		COMMAND_ID_HANDLER(ID_TOOLS_COPY_DIAGNOSTIC_LOG_PATH, OnToolsCopyDiagnosticLogPath)
		COMMAND_ID_HANDLER(ID_TOOLS_CLEAR_DIAGNOSTIC_LOGS, OnToolsClearDiagnosticLogs)
		COMMAND_ID_HANDLER(ID_TOOLS_CREATE_DIAGNOSTIC_PACKAGE, OnToolsCreateDiagnosticPackage)
		COMMAND_ID_HANDLER(ID_TOOLS_CUSTOMIZE, OnToolCustomize)
		//COMMAND_ID_HANDLER(ID_HIDETOOLBAR, OnHideToolbar)

		COMMAND_RANGE_HANDLER(ID_SCRIPT_BASE, ID_SCRIPT_BASE + FbeScripts::ScriptCommandCount, OnToolsScript)
		COMMAND_ID_HANDLER(ID_LAST_SCRIPT, OnLastScript)

		COMMAND_ID_HANDLER(ID_TOOLS_SPELLCHECK, OnSpellCheck);
		COMMAND_ID_HANDLER(ID_TOOLS_SPELLCHECK_HIGHLIGHT, OnToggleHighlight);

		// help menu
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)

		// navigation commands
		COMMAND_ID_HANDLER(ID_SELECT_TREE, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_ID, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_HREF, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_IMAGE, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_TEXT, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_NEXT_ITEM, OnNextItem)
		COMMAND_ID_HANDLER(ID_SELECT_SECTION, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_IDT, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_STYLET, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_STYLE, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_COLSPAN, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_ROWSPAN, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_ALIGNTR, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_ALIGN, OnSelectCtl)
		COMMAND_ID_HANDLER(ID_SELECT_VALIGN, OnSelectCtl)

		// editor notifications
		COMMAND_CODE_HANDLER(IDN_SEL_CHANGE, OnEdSelChange)
		COMMAND_CODE_HANDLER(IDN_ED_CHANGED, OnEdChange)
		COMMAND_CODE_HANDLER(IDN_ED_TEXT, OnEdStatusText)
		COMMAND_CODE_HANDLER(IDN_WANTFOCUS, OnEdWantFocus)
		COMMAND_CODE_HANDLER(IDN_ED_RETURN, OnEdReturn)
		COMMAND_CODE_HANDLER(IDN_NAVIGATE, OnNavigate)
		COMMAND_CODE_HANDLER(EN_KILLFOCUS, OnEdKillFocus)
		COMMAND_CODE_HANDLER(CBN_EDITCHANGE, OnCbEdChange)
		COMMAND_CODE_HANDLER(CBN_SELENDOK, OnCbSelEndOk)
		COMMAND_CODE_HANDLER(IDN_FAST_MODE_CHANGE, OnFastModeChange)
		COMMAND_HANDLER(IDC_HREF,CBN_SETFOCUS, OnCbSetFocus)

		// source code editor notifications
		NOTIFY_CODE_HANDLER(SCN_MODIFIED, OnSciModified)
		NOTIFY_CODE_HANDLER(SCN_MARGINCLICK, OnSciMarginClick)
		NOTIFY_CODE_HANDLER(SCN_UPDATEUI, OnSciUpdateUI)
		NOTIFY_CODE_HANDLER(SCN_CHARADDED, OnSciCharAdded)
		NOTIFY_CODE_HANDLER(NM_CLICK, OnStatusBarClick)
		NOTIFY_CODE_HANDLER(NM_DBLCLK, OnToolbarDoubleClick)
		NOTIFY_CODE_HANDLER(NM_DBLCLK, OnStatusBarDoubleClick)
		NOTIFY_CODE_HANDLER(NM_RCLICK, OnStatusBarRightClick)
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnCommandToolbarCustomDraw)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFOA, OnRuntimeToolTipTextA)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFOW, OnRuntimeToolTipTextW)

		// tree pane
		COMMAND_ID_HANDLER(ID_PANE_CLOSE, OnViewTree)

		// FBEview calls to process messages without FBEview focused
		COMMAND_ID_HANDLER_EX(ID_GOTO_FOOTNOTE, OnGoToFootnote)
		COMMAND_ID_HANDLER_EX(ID_GOTO_REFERENCE, OnGoToReference)
		COMMAND_ID_HANDLER_EX(ID_GOTO_MATCHTAG, OnGoToMatchTag);
		COMMAND_ID_HANDLER_EX(ID_GOTO_WRONGTAG, OnGoToWrongTag);

		// chain commands to active view
		MESSAGE_HANDLER(WM_COMMAND, OnUnhandledCommand)

		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
		CHAIN_MSG_MAP(CCustomizableToolBarCommands<CMainFrame>)
	END_MSG_MAP()

  LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnQueryEndSession(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnEndSession(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnClipboardUpdate(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnBodyScroll(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDescriptionFormChanged(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnPostCreate(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSourceMemoryBenchmark(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnApplyXmlSourceTheme(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnShowFindResultsPane(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnHideFindResultsPane(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnRefreshFindResultsPane(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDetachFindResultsPane(UINT, WPARAM, LPARAM, BOOL&);
	void ShowFindResultsPane(CFBEView* view);
	void HideFindResultsPane();
	void RefreshFindResultsPane(CFBEView* view);
	void ApplyFindResultsPaneHeight();
	void ConstrainFindResultsPaneSplitter();
  LRESULT OnSettingChange(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnRuntimeToolTipTextA(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnRuntimeToolTipTextW(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnCommandToolbarCustomDraw(int, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnToolbarDoubleClick(int, LPNMHDR pnmh, BOOL& bHandled);
	static LRESULT CALLBACK ScriptsToolbarSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR reference);


  int m_selBandID;

  void ApplyRuntimeToolbarMenuLocalization(HMENU menu)
  {
	if(menu == NULL)
		return;

	const int count = ::GetMenuItemCount(menu);
	for(int i = 0; i < count; ++i)
	{
		const UINT commandId = ::GetMenuItemID(menu, i);
		if(commandId != ID_TOOLS_CUSTOMIZE)
			continue;

		CString text = FbeLoadRuntimeStringByKey(L"fbe.menu.idr_toolbar_menu.customize");
		if(!text.IsEmpty())
			::ModifyMenu(menu, i, MF_BYPOSITION | MF_STRING, commandId, text);
	}
  }

  LRESULT OnContextMenu(UINT, WPARAM, LPARAM lParam, BOOL&) 
  {
	HMENU menu, popup;
	RECT rect;
	CPoint ptMousePos = (CPoint)lParam;
	ScreenToClient(&ptMousePos);
	// find clicked toolbar
	REBARBANDINFO rbi;
	ZeroMemory((void*)&rbi, sizeof(rbi));
	rbi.cbSize = sizeof(REBARBANDINFO);
	rbi.fMask = RBBIM_ID;
	m_selBandID = -1;
	for (unsigned int i=0; i< m_rebar.GetBandCount(); i++)
	{
		m_rebar.GetRect(i, &rect);
		if (PtInRect(&rect,ptMousePos))
		{
			m_rebar.GetBandInfo(i, &rbi);
			m_selBandID = rbi.wID;
			break;
		}
	}
	// display context menu for command & script toolbars only
	if ((m_selBandID == ATL_IDW_BAND_FIRST+1) || (m_selBandID == ATL_IDW_BAND_FIRST+2))
	{
		menu = ::LoadMenu(_Module.GetResourceInstance(), MAKEINTRESOURCEW(IDR_TOOLBAR_MENU));
		popup = ::GetSubMenu(menu, 0);
		ApplyRuntimeToolbarMenuLocalization(popup);
		ClientToScreen(&ptMousePos);
		::TrackPopupMenu(popup, TPM_LEFTALIGN, ptMousePos.x, ptMousePos.y, 0, *this, 0);
	}
	return 0;
  }

  LRESULT OnUnhandledCommand(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
    m_view.SetFocus();
	UpdateViewSizeInfo();
    return 0;
  }
  LRESULT OnDropFiles(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSetStatusText(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
    const CString text((const TCHAR *)lParam);
    if (wParam == AU::StatusTextImmediate) {
      m_status_state.SetTransient(text, ::GetTickCount());
      RefreshStatusMainPane();
      // UpdateWindow delivers only the pending WM_PAINT for the status bar;
      // it cannot pump commands or re-enter serialization.
      if (m_status.IsWindowVisible()) ::UpdateWindow(m_status);
    } else {
      m_status_state.QueueMessage(text);
    }
    return 0;
  }

	LRESULT OnTrackPopupMenu(UINT, WPARAM, LPARAM lParam, BOOL&)
	{
		AU::TRACKPARAMS* tp = (AU::TRACKPARAMS*)lParam;
		// added by SeNS
		if (m_Speller) m_Speller->AppendSpellMenu(tp->hMenu);
		// This message is raised only by the MSHTML BODY context menu.  Let the
		// native popup renderer use the selected app theme; CCommandBarCtrl stays
		// untouched for the regular menu bar.
		const UINT command = ThemeManager::TrackPopupMenu(tp->hMenu, tp->uFlags,
			tp->x, tp->y, m_hWnd);
		if(command != 0)
			::SendMessage(m_hWnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
		return 0;
	}

	LRESULT OnChar(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnPreCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnFileExit(WORD, WORD, HWND, BOOL&)
	{
		// close (possible) opened in script modeless dialogs
		PostMessage(WM_CLOSEDIALOG);
		PostMessage(WM_CLOSE);
		return 0;
	}
	LRESULT OnFileNew(WORD, WORD, HWND, BOOL&);
	LRESULT OnFileOpen(WORD, WORD, HWND, BOOL&);
	LRESULT OnFileOpenMRU(WORD, WORD, HWND, BOOL&);
	LRESULT OnFileSave(WORD, WORD, HWND, BOOL&);
	LRESULT OnFileSaveAs(WORD, WORD, HWND, BOOL&);
	LRESULT OnFileValidate(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolsImport(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolsExport(WORD, WORD, HWND, BOOL&);
	LRESULT OnLastPlugin(WORD, WORD, HWND, BOOL&);

	LRESULT OnEditIncSearch(WORD, WORD, HWND, BOOL&);
	LRESULT OnEditAddBinary(WORD, WORD, HWND, BOOL&);
	LRESULT OnEditFind(WORD, WORD, HWND, BOOL& bHandled)
	{
		if(m_editor_view_state.Current() == EditorView::Description)
			ShowView(BODY);

		bHandled = FALSE;
		return 0;
	}
	LRESULT OnEditInsSymbol(WORD, WORD, HWND, BOOL&);

	// added by SeNS
	LRESULT OnSpellReplace(WORD /* unused: wNotifyCode */, WORD wID, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
	{ 
		if (m_Speller)
		{
			m_doc->m_body.BeginUndoUnit(L"replace word");
			m_Speller->Replace (wID - ID_SPELL_REPLACE_FIRST);
			m_doc->m_body.EndUndoUnit();
		}
		return 0; 
	}
	LRESULT OnSpellIgnoreAll(WORD, WORD, HWND, BOOL&) 
	{ 
		if (m_Speller) m_Speller->IgnoreAll();
		return 0; 
	}
	LRESULT OnSpellAddToDict(WORD, WORD, HWND, BOOL&) 
	{ 
		if (m_Speller) m_Speller->AddToDictionary();
		return 0; 
	}

	LRESULT OnVersionAdvance(WORD delta, WORD, HWND, BOOL&)
	{
		m_doc->AdvanceDocVersion(delta);
		return 0;
	}

  LRESULT OnViewToolBar(WORD, WORD, HWND, BOOL&);
  LRESULT OnViewStatusBar(WORD, WORD, HWND, BOOL&);
  LRESULT OnStatusPaneVisibility(WORD, WORD, HWND, BOOL&);
  LRESULT OnStatusBarClick(int, LPNMHDR, BOOL&);
  LRESULT OnStatusBarDoubleClick(int, LPNMHDR, BOOL&);
  LRESULT OnStatusBarRightClick(int, LPNMHDR, BOOL&);
	LRESULT OnViewFastMode(WORD, WORD, HWND, BOOL&);
	LRESULT OnViewScriptToolbarsManage(WORD, WORD, HWND, BOOL&) { ShowScriptToolbarManagerDialog(); return 0; }
  LRESULT OnViewTree(WORD, WORD, HWND, BOOL&);
  LRESULT OnViewDesc(WORD, WORD, HWND, BOOL&) {
    ShowView(DESC);
    return 0;
  }
  LRESULT OnViewBody(WORD, WORD, HWND, BOOL&) {
    ShowView(BODY);
    return 0;
  }
  LRESULT OnViewSource(WORD, WORD, HWND, BOOL&) {
    ShowView(SOURCE);
    return 0;
  }
  LRESULT OnViewOptions(WORD, WORD, HWND, BOOL&);

  LRESULT OnToolsWords(WORD, WORD, HWND, BOOL&);
  LRESULT OnToolsOptions(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolsDiagnosticTrace(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolsOpenDiagnosticLog(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolsOpenDiagnosticFolder(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolsCopyDiagnosticLogPath(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolsClearDiagnosticLogs(WORD, WORD, HWND, BOOL&);  LRESULT OnToolsScript(WORD, WORD, HWND, BOOL&);
	LRESULT OnToolsCreateDiagnosticPackage(WORD, WORD, HWND, BOOL&);

  LRESULT OnHideToolbar(WORD wNotifyCode, WORD /*wID*/, HWND hWndCtl, BOOL& bHandled)
  {
	  if (m_selBandID < 0 || m_selBandID > 0xffff)
		  return 0;
	  return OnViewToolBar(wNotifyCode, static_cast<WORD>(m_selBandID), hWndCtl, bHandled);
  }

  LRESULT OnToolCustomize(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /* unused: hWndCtl */, BOOL& /*bHandled*/)
  {
	  UnhookSysDialogs();
	  if (m_selBandID == ATL_IDW_BAND_FIRST+1) m_CmdToolbar.Customize(); else
	  if (m_selBandID == ATL_IDW_BAND_FIRST+2) ShowScriptsToolbarCustomizeDialog();
	  HookSysDialogs();
      return 0;
  }

	LRESULT OnLastScript(WORD, WORD, HWND, BOOL&)
	{
		const ScriptDescriptor* lastScript = m_scripts.LastScript();
		if(lastScript != NULL && !IsSourceActive())
		{
			m_doc->RunScript(lastScript->path);
		}
		return 0;
	}

  LRESULT OnAppAbout(WORD, WORD, HWND, BOOL&);

  LRESULT OnSelectCtl(WORD, WORD, HWND, BOOL&);
  LRESULT OnNextItem(WORD, WORD, HWND, BOOL&);

  LRESULT OnEdSelChange(WORD, WORD, HWND /* unused: hWndCtl */, BOOL&) {
    m_sel_changed=true;
	InvalidateSelectionContext();
	InvalidateUi(UiDirtySelection | UiDirtyStatus | UiDirtyToolbar);
    StopIncSearch(true);
	DisplayCharCode();
    return 0;
  }
  LRESULT OnFastModeChange(WORD, WORD mode, HWND /* unused: hWndCtl */, BOOL&)
  {
	  UISetCheck(ID_VIEW_FASTMODE, mode);
	  return 0;
  }
  LRESULT OnEdStatusText(WORD, WORD, HWND hWndCtl, BOOL&) {
    StopIncSearch(true);
    m_status.SetText(ID_DEFAULT_PANE,(const TCHAR *)hWndCtl);
    return 0;
  }
  LRESULT OnEdWantFocus(WORD, WORD wID, HWND, BOOL&) {
    m_want_focus=wID;
    return 0;
  }
  LRESULT OnEdReturn(WORD, WORD, HWND, BOOL&) {
    m_view.SetFocus();
    return 0;
  }
  LRESULT OnNavigate(WORD, WORD, HWND, BOOL&);

	LRESULT OnCbSetFocus(WORD, WORD, HWND, BOOL&)
	{
		if(!m_cb_updated)
		{
			m_ignore_cb_changes = true;

			m_contextAttributeBars.BeginHrefCatalogUpdate();
			m_ignore_cb_changes = false;

			if(m_cb_last_images)
				m_doc->BinIDsToComboBox(m_contextAttributeBars.HrefCatalogForPopulation());
			else
				m_doc->ParaIDsToComboBox(m_contextAttributeBars.HrefCatalogForPopulation());
			m_cb_updated = true;
		}

		return 0;
	}

  void ChangeNBSP(MSHTML::IHTMLElementPtr elem);
  void RemoveLastUndo();

	LRESULT OnEdChange(WORD, WORD, HWND /* unused: hWnd */, BOOL& /* unused: b */) {
    StopIncSearch(true);
		m_doc_changed=true;
		m_need_title_update = true;
		MarkRecoveryDirty();
		InvalidateSelectionContext();
		InvalidateUi(UiDirtyDocument | UiDirtySelection | UiDirtyStatus | UiDirtyToolbar);
    ResetValidationStatus();
    m_cb_updated=false;

	// added by SeNS: update 
	UpdateViewSizeInfo();
	// added by SeNS - process nbsp
	if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
		ChangeNBSP(m_doc->m_body.SelectionContainer());

	// added by SeNS: do spellcheck
	if (m_Speller && m_editor_view_state.Current() == EditorView::Body)
		if (m_Speller->Enabled() && _Settings.GetHighlightMisspells())
			m_Speller->CheckElement(m_doc->m_body.SelectionContainer(), -1);

	return 0;
  }
  LRESULT OnCbEdChange(WORD, WORD, HWND, BOOL&);
  LRESULT OnCbSelEndOk(WORD /* unused: code */, WORD wID, HWND hWnd, BOOL&) {
    PostMessage(WM_COMMAND,MAKELONG(wID,CBN_EDITCHANGE),(LPARAM)hWnd);
    return 0;
  }

  LRESULT OnEdKillFocus(WORD, WORD, HWND, BOOL&) {
    StopIncSearch(true);
    return 0;
  }

  LRESULT OnTreeReturn(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeClick(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeMoveElement(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeMoveElementOne(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeMoveElementSmart(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeMoveLeftElement(WORD, WORD, HWND, BOOL&);
  //LRESULT OnTreeMoveLeftElementOne(WORD, WORD, HWND, BOOL&);
  //LRESULT OnTreeMoveElementWithChildren(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeViewElement(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeViewElementSource(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeDeleteElement(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeMerge(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeUpdate(WORD, WORD, HWND, BOOL&);
  LRESULT OnTreeRestore(WORD, WORD, HWND, BOOL&);

  LRESULT OnGoToFootnote(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */)
  {
	  if (!m_doc->m_body.ReturnToLinkNavigationOrigin() && !m_doc->m_body.GoToFootnote(false))
		m_doc->m_body.GoToReference(false);
	  return 0;
  }

	void ShowScriptsToolbarCustomizeDialog();
	void ShowScriptToolbarManagerDialog();

  LRESULT OnGoToReference(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */)
  {
	  m_doc->m_body.GoToReference(false);
	  return 0;
  }

  LRESULT OnSciModified(int /* unused: id */,NMHDR *hdr,BOOL& bHandled) {
    if (hdr->hwndFrom!=m_source) {
      bHandled=FALSE;
      return 0;
    }
    SciModified(*(SCNotification*)hdr);
	m_need_title_update = true;
	MarkRecoveryDirty();
	InvalidateUi(UiDirtySource | UiDirtyToolbar | UiDirtyStatus);
    return 0;
  }

  LRESULT OnSciMarginClick(int /* unused: id */,NMHDR *hdr,BOOL& bHandled) {
    if (hdr->hwndFrom!=m_source) {
      bHandled=FALSE;
      return 0;
    }
    SciMarginClicked(*(SCNotification*)hdr);
    return 0;
  }

  LRESULT OnSciCharAdded(int /* unused: id */,NMHDR *hdr,BOOL& bHandled) {
    if (hdr->hwndFrom != m_source || m_editor_view_state.Current() != EditorView::Source) {
      bHandled=FALSE;
      return 0;
    }
    ShowFb2Autocomplete(reinterpret_cast<const SCNotification*>(hdr)->ch);
    return 0;
  }

  LRESULT OnSciUpdateUI(int /* unused: id */,NMHDR *hdr,BOOL& /* unused: bHandled */)
  {
    if (hdr->hwndFrom != m_source || m_editor_view_state.Current() != EditorView::Source)
		return 0;

    const SCNotification& scn = *reinterpret_cast<const SCNotification*>(hdr);
    if (scn.updated & SC_UPDATE_LINE_COUNT)
		m_source.UpdateLineNumberMargin(false);
    if (scn.updated & SC_UPDATE_TEXT)
    {
		ClearSourceValidationAnnotations();
        ResetValidationStatus();
    }
	if (scn.updated & (SC_UPDATE_SELECTION | SC_UPDATE_TEXT))
	{
		SciUpdateUI(false);
		InvalidateUi(UiDirtySource | UiDirtyToolbar | UiDirtyStatus);
	}
	return 0;
  }

  LRESULT OnGoToMatchTag(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */)
  {
    if (m_editor_view_state.Current() == EditorView::Source)
		SciUpdateUI(true);
	return 0;
  }

  LRESULT OnGoToWrongTag(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */)
  {
    if (m_editor_view_state.Current() == EditorView::Source)
		SciGotoWrongTag();
	return 0;
  }

	LRESULT OnSciCollapse(WORD cose, WORD wID, HWND, BOOL&);  
	LRESULT OnSciExpand(WORD cose, WORD wID, HWND, BOOL&);  

	void SciModified(const SCNotification& scn);
	void SciMarginClicked(const SCNotification& scn);
	void ShowFb2Autocomplete(int character);
	bool SciUpdateUI(bool gotoTag);
	void SciGotoWrongTag();
	void ClearSourceValidationAnnotations();
	void ShowSourceValidationAnnotation(int line, int column, const CString& message);
	void SciCollapse(int level2Collapse, bool mode);

	void GoToSelectedTreeItem();
	MSHTML::IHTMLDOMNodePtr MoveRightElementWithoutChildren(MSHTML::IHTMLDOMNodePtr node);
	MSHTML::IHTMLDOMNodePtr MoveRightElement(MSHTML::IHTMLDOMNodePtr node);
	MSHTML::IHTMLDOMNodePtr MoveLeftElement(MSHTML::IHTMLDOMNodePtr node);
	MSHTML::IHTMLDOMNodePtr RecoursiveMoveRightElement(CTreeItem item);

	MSHTML::IHTMLDOMNodePtr GetFirstChildSection(MSHTML::IHTMLDOMNodePtr node);
	MSHTML::IHTMLDOMNodePtr GetNextSiblingSection(MSHTML::IHTMLDOMNodePtr node);
	MSHTML::IHTMLDOMNodePtr GetPrevSiblingSection(MSHTML::IHTMLDOMNodePtr node);
	MSHTML::IHTMLDOMNodePtr GetLastChildSection(MSHTML::IHTMLDOMNodePtr node);
	bool IsNodeSection(MSHTML::IHTMLDOMNodePtr node);
	bool IsEmptySection(MSHTML::IHTMLDOMNodePtr section);
	MSHTML::IHTMLDOMNodePtr CreateNestedSection(MSHTML::IHTMLDOMNodePtr section);
	bool IsEmptyText(BSTR text);
	void SourceGoTo(int line, int linePos);
	bool CheckFileTimeStamp();
	bool CheckFileTimeStampIfDue();
	bool RefreshClipboardState();
	void UpdateClipboardCommands();
	void RefreshClipboardStateFallbackIfDue();
	bool ReloadFile();
	void UpdateFileTimeStamp();
	bool ShowSettingsDialog(HWND parent = ::GetActiveWindow());
	void ApplyConfChanges(bool applyDocumentStyles = true);
	void ApplyEditorBackgroundChanges();
	void ApplyXmlSourceEditorChanges(bool saveSettings = true);
	void RestartProgram();
	void FillMenuWithHkeys(HMENU);
	void RefreshLocalizedMainFrameUi();
	void RefreshLocalizedToolbarCaptions();
	void RefreshLocalizedToolbarButtonTexts(CToolBarCtrl& toolbar);

	// added by SeNS
    CSpeller *m_Speller;
	LRESULT OnSpellCheck(WORD, WORD, HWND, BOOL& /* unused: b */)
	{
		if (m_Speller && m_doc && m_editor_view_state.Current() == EditorView::Body && m_Speller->Available())
			m_Speller->StartDocumentCheck(m_doc->m_body.m_mk_srv);
		return S_OK;
	}

	LRESULT OnToggleHighlight(WORD, WORD, HWND, BOOL&)
	{
		if (m_Speller && m_editor_view_state.Current() == EditorView::Body)
		{
			_Settings.SetHighlightMisspells(!_Settings.GetHighlightMisspells());
			m_Speller->SetHighlightMisspells(_Settings.GetHighlightMisspells());
		}
		return S_OK;
	}

	// added by SeNS - paste pictures
	bool BitmapInClipboard()
	{
		return ::IsClipboardFormatAvailable(CF_BITMAP) != FALSE;
	}

    // added by SeNS
	void UpdateViewSizeInfo()
	{
		if (m_doc && m_doc->m_body)
			if (m_doc->m_body.Document())
			{
				MSHTML::IHTMLElement2Ptr m_scrollElement = MSHTML::IHTMLDocument3Ptr(m_doc->m_body.Document())->documentElement;
				if (m_scrollElement)
				{
					_Settings.SetViewWidth (m_scrollElement->clientWidth);
					_Settings.SetViewHeight(m_scrollElement->clientHeight);
					_Settings.SetMainWindow(m_hWnd);
				}
			}
	}

	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL& bHandled)
	{
		UpdateViewSizeInfo();
		ConstrainFindResultsPaneSplitter();
		if (!m_status_layout_posted) { m_status_layout_posted = true; PostMessage(WM_APP + 41); }
		if (_Settings.GetShowFullPathInWindowTitle() && m_doc && m_doc->m_namevalid)
			m_need_title_update = true;
		bHandled = FALSE;
		return 0;
	}
	LRESULT OnDeferredStatusBarLayout(UINT, WPARAM, LPARAM, BOOL&) { m_status_layout_posted = false; UpdateStatusBarLayout(); return 0; }

	// added by SeNS: incorrect XML file flag
	bool m_bad_xml;
	CString m_bad_filename;
	bool LoadToScintilla(CString filename);

	// added by SeNS: issue #127
  void DisplayCharCode();
  void UpdateStatusBar();
  void UpdateStatusBarLayout();
  bool CurrentOverwriteMode() const;
  void RefreshStatusMainPane();
	void SetValidationStatus(FBEStatusBar::ValidationStatus status);
  void ResetValidationStatus();
  void ResetStatusForDocument();
  void SetStatusContext(const CString& text);
  void SetTransientStatus(const CString& text);
  CString GetStatusValidationText() const;
  UINT StatusPaneAt(POINT point) const;
  void ToggleStatusPaneVisibility(UINT command);
};

int	StartScript(CMainFrame* mainframe);
void	StopScript(void);
HRESULT	ScriptLoad(const wchar_t *filename);
HRESULT	ScriptCall(const wchar_t *func,VARIANT *arg, int argnum, VARIANT *ret);
bool	ScriptFindFunc(const wchar_t *func);

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__38D356D4_C28B_47B0_A7AD_8C6B70F7F283__INCLUDED_)
