#pragma once

#include "HtmlExportSettings.h"
#include "HtmlExportGeneralPage.h"
#include "HtmlExportAppearancePage.h"
#include "HtmlExportImagesPage.h"
#include "HtmlExportNotesMetadataPage.h"
#include "TemplateResolver.h"
#include "resource.h"
#include "..\\common\\ModernFileDialog.h"
#include <wtl/atlctrls.h>
#include <vector>

inline void BuildHtmlModernFileTypes(const CString& value, std::vector<CString>& labels, std::vector<CString>& patterns, std::vector<COMDLG_FILTERSPEC>& filters)
{
    labels.clear(); patterns.clear(); filters.clear(); int position=0;
    while(position>=0) { int first=value.Find(L'|',position); if(first<0) break; CString label=value.Mid(position,first-position); position=first+1; int second=value.Find(L'|',position); if(second<0) break; CString pattern=value.Mid(position,second-position); position=second+1; if(label.IsEmpty()||pattern.IsEmpty()) break; labels.push_back(label); patterns.push_back(pattern); }
    for(size_t i=0;i<labels.size();++i) filters.push_back({labels[i],patterns[i]});
}

enum class ExportHtmlFormatMode : UINT
{
    Standalone = 1,
    ExternalImages = 2,
    MHT = 3,
    HtmlOnly = 4
};

inline ExportHtmlFormatMode ExportHtmlFormatModeFromFilterIndex(UINT filterIndex)
{
    switch (filterIndex) {
    case static_cast<UINT>(ExportHtmlFormatMode::ExternalImages): return ExportHtmlFormatMode::ExternalImages;
    case static_cast<UINT>(ExportHtmlFormatMode::MHT): return ExportHtmlFormatMode::MHT;
    case static_cast<UINT>(ExportHtmlFormatMode::HtmlOnly): return ExportHtmlFormatMode::HtmlOnly;
    default: return ExportHtmlFormatMode::Standalone;
    }
}

inline bool ExportHtmlFormatSupportsSplit(ExportHtmlFormatMode mode)
{
    return mode == ExportHtmlFormatMode::ExternalImages || mode == ExportHtmlFormatMode::HtmlOnly;
}

class CHtmlExportOptionsDialog : public CDialogImpl<CHtmlExportOptionsDialog>
{
public:
    enum { IDD = IDD_HTML_EXPORT_OPTIONS };
    HtmlExportSettings m_settings;
    HtmlExportSettings m_workingSettings;
    HtmlExportGeneralPage m_generalPage;
    HtmlExportAppearancePage m_appearancePage;
    HtmlExportImagesPage m_imagesPage;
    HtmlExportNotesMetadataPage m_notesPage;
    int m_currentPage = 0;
    bool m_splitSupported = true;
    CRect m_initialTabs;
    CRect m_initialOk;
    CRect m_initialCancel;
    CSize m_initialClient;
    CSize m_minimumWindow;
    UINT m_initialDpi = 0;
    UINT m_layoutDpi = 0;
    struct PageControlLayout { HWND window; CRect initial; int id; };
    struct PageLayout { CSize initialSize = CSize(0, 0); int scrollX = 0; int scrollY = 0; std::vector<PageControlLayout> controls; };
    PageLayout m_pageLayouts[4];
    void SetSplitSupported(bool value) { m_splitSupported = value; m_generalPage.SetSplitSupported(value); }

    void LoadSettings();
    void Persist();

    BEGIN_MSG_MAP(CHtmlExportOptionsDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        NOTIFY_HANDLER(IDC_OPTIONS_TABS, TCN_SELCHANGE, OnTabChanged)
        COMMAND_ID_HANDLER(IDOK, OnOk)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDpiChanged(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnGetMinMaxInfo(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnTabChanged(int, LPNMHDR, BOOL&);
    LRESULT OnOk(WORD, WORD, HWND, BOOL&);
    LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
    void SelectPage(int page);
    void LayoutPages();
    void LayoutChrome();
    void CapturePageLayouts();
    void LayoutPageFields(int index, HWND page);
    void ScrollPage(int index, WPARAM command, bool horizontal);
    void RevealFocusedPageControl(int index);
    static LRESULT CALLBACK PageSubclassProc(HWND page, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR reference);
};

class CHtmlFileDialogEvents : public CComObjectRootEx<CComSingleThreadModel>, public IFileDialogEvents, public IFileDialogControlEvents
{
public:
    static const DWORD SettingsButtonId = 2001;
    BEGIN_COM_MAP(CHtmlFileDialogEvents)
        COM_INTERFACE_ENTRY(IFileDialogEvents)
        COM_INTERFACE_ENTRY(IFileDialogControlEvents)
    END_COM_MAP()
    HWND owner = NULL; CHtmlExportOptionsDialog* options = NULL;
    STDMETHOD(OnFileOk)(IFileDialog*) { return S_OK; } STDMETHOD(OnFolderChanging)(IFileDialog*, IShellItem*) { return S_OK; }
    STDMETHOD(OnFolderChange)(IFileDialog*) { return S_OK; } STDMETHOD(OnSelectionChange)(IFileDialog*) { return S_OK; }
    STDMETHOD(OnShareViolation)(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE* r) { if (r) *r=FDESVR_DEFAULT; return S_OK; }
    STDMETHOD(OnTypeChange)(IFileDialog* dialog) { UINT type = static_cast<UINT>(ExportHtmlFormatMode::Standalone); if (dialog) dialog->GetFileTypeIndex(&type); if (options) options->SetSplitSupported(ExportHtmlFormatSupportsSplit(ExportHtmlFormatModeFromFilterIndex(type))); return S_OK; } STDMETHOD(OnOverwrite)(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE* r) { if (r) *r=FDEOR_DEFAULT; return S_OK; }
    STDMETHOD(OnItemSelected)(IFileDialogCustomize*, DWORD, DWORD) { return S_OK; } STDMETHOD(OnCheckButtonToggled)(IFileDialogCustomize*, DWORD, BOOL) { return S_OK; }
    STDMETHOD(OnControlActivating)(IFileDialogCustomize*, DWORD) { return S_OK; }
    STDMETHOD(OnButtonClicked)(IFileDialogCustomize* customize, DWORD id);
};
