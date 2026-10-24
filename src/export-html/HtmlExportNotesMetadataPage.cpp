#include "stdafx.h"
#include "HtmlExportNotesMetadataPage.h"
#include "RuntimeLocalization.h"

LRESULT HtmlExportNotesMetadataPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
    SetDlgItemText(IDC_NOTE_PLACEMENT_LABEL, LoadExportHtmlString(IDS_OPTIONS_NOTE_PLACEMENT));
    SetDlgItemText(IDC_METADATA, LoadExportHtmlString(IDS_OPTIONS_INCLUDE_METADATA));
    SetDlgItemText(IDC_METADATA_ANNOTATION, LoadExportHtmlString(IDS_OPTIONS_METADATA_ANNOTATION));
    SetDlgItemText(IDC_METADATA_TITLE, LoadExportHtmlString(IDS_OPTIONS_METADATA_TITLE));
    SetDlgItemText(IDC_METADATA_DOCUMENT, LoadExportHtmlString(IDS_OPTIONS_METADATA_DOCUMENT));
    SetDlgItemText(IDC_METADATA_PUBLISH, LoadExportHtmlString(IDS_OPTIONS_METADATA_PUBLISH));
    SetDlgItemText(IDC_METADATA_HISTORY, LoadExportHtmlString(IDS_OPTIONS_METADATA_HISTORY));
    SetDlgItemText(IDC_METADATA_AUTHORS, LoadExportHtmlString(IDS_OPTIONS_METADATA_AUTHORS));
    SetDlgItemText(IDC_METADATA_TRANSLATORS, LoadExportHtmlString(IDS_OPTIONS_METADATA_TRANSLATORS));
    SetDlgItemText(IDC_METADATA_CUSTOM, LoadExportHtmlString(IDS_OPTIONS_METADATA_CUSTOM));
    LoadFromSettings();
    InitTooltips();
    return TRUE;
}

void HtmlExportNotesMetadataPage::LoadFromSettings()
{
    if (!m_settings) return;
    CComboBox placement(GetDlgItem(IDC_NOTE_PLACEMENT));
    placement.ResetContent();
    const UINT labels[] = { IDS_OPTIONS_VALUE_SOURCE, IDS_OPTIONS_VALUE_BOOK_END, IDS_OPTIONS_VALUE_SECTION_END };
    for (size_t index = 0; index < _countof(labels); ++index) placement.AddString(LoadExportHtmlString(labels[index]));
    placement.SetCurSel(m_settings->notePlacement >= 0 && m_settings->notePlacement < 3 ? m_settings->notePlacement : 0);
    CheckDlgButton(IDC_METADATA, m_settings->includeMetadata ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_METADATA_ANNOTATION, m_settings->includeAnnotation ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_METADATA_TITLE, m_settings->includeTitleInfo ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_METADATA_DOCUMENT, m_settings->includeDocumentInfo ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_METADATA_PUBLISH, m_settings->includePublishInfo ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_METADATA_HISTORY, m_settings->includeHistory ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_METADATA_AUTHORS, m_settings->includeAuthors ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_METADATA_TRANSLATORS, m_settings->includeTranslators ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_METADATA_CUSTOM, m_settings->includeCustomInfo ? BST_CHECKED : BST_UNCHECKED);
    UpdateEnabledState();
}

bool HtmlExportNotesMetadataPage::SaveToSettings(HtmlExportSettings& candidate, CString& error)
{
    const int placement = CComboBox(GetDlgItem(IDC_NOTE_PLACEMENT)).GetCurSel();
    if (placement < 0 || placement > 2) { error = LoadExportHtmlString(IDS_ERROR); return false; }
    candidate.notePlacement = placement;
    candidate.includeMetadata = IsDlgButtonChecked(IDC_METADATA) == BST_CHECKED;
    candidate.includeAnnotation = IsDlgButtonChecked(IDC_METADATA_ANNOTATION) == BST_CHECKED;
    candidate.includeTitleInfo = IsDlgButtonChecked(IDC_METADATA_TITLE) == BST_CHECKED;
    candidate.includeDocumentInfo = IsDlgButtonChecked(IDC_METADATA_DOCUMENT) == BST_CHECKED;
    candidate.includePublishInfo = IsDlgButtonChecked(IDC_METADATA_PUBLISH) == BST_CHECKED;
    candidate.includeHistory = IsDlgButtonChecked(IDC_METADATA_HISTORY) == BST_CHECKED;
    candidate.includeAuthors = IsDlgButtonChecked(IDC_METADATA_AUTHORS) == BST_CHECKED;
    candidate.includeTranslators = IsDlgButtonChecked(IDC_METADATA_TRANSLATORS) == BST_CHECKED;
    candidate.includeCustomInfo = IsDlgButtonChecked(IDC_METADATA_CUSTOM) == BST_CHECKED;
    return true;
}

void HtmlExportNotesMetadataPage::UpdateEnabledState()
{
    const BOOL enabled = IsDlgButtonChecked(IDC_METADATA) == BST_CHECKED;
    const UINT controls[] = { IDC_METADATA_ANNOTATION, IDC_METADATA_TITLE, IDC_METADATA_DOCUMENT, IDC_METADATA_PUBLISH, IDC_METADATA_HISTORY, IDC_METADATA_AUTHORS, IDC_METADATA_TRANSLATORS, IDC_METADATA_CUSTOM };
    for (size_t index = 0; index < _countof(controls); ++index) ::EnableWindow(GetDlgItem(controls[index]), enabled);
}

LRESULT HtmlExportNotesMetadataPage::OnMetadata(WORD, WORD, HWND, BOOL&)
{
    UpdateEnabledState();
    return 0;
}

void HtmlExportNotesMetadataPage::InitTooltips()
{
    m_tooltip = ::CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
    if (!m_tooltip) return;
    const struct { UINT control; UINT text; } controls[] = {
        { IDC_NOTE_PLACEMENT, IDS_TOOLTIP_NOTE_PLACEMENT }, { IDC_METADATA, IDS_TOOLTIP_METADATA },
        { IDC_METADATA_ANNOTATION, IDS_TOOLTIP_METADATA_CHILD }, { IDC_METADATA_TITLE, IDS_TOOLTIP_METADATA_CHILD },
        { IDC_METADATA_DOCUMENT, IDS_TOOLTIP_METADATA_CHILD }, { IDC_METADATA_PUBLISH, IDS_TOOLTIP_METADATA_CHILD },
        { IDC_METADATA_HISTORY, IDS_TOOLTIP_METADATA_CHILD }, { IDC_METADATA_AUTHORS, IDS_TOOLTIP_METADATA_CHILD },
        { IDC_METADATA_TRANSLATORS, IDS_TOOLTIP_METADATA_CHILD }, { IDC_METADATA_CUSTOM, IDS_TOOLTIP_METADATA_CHILD }
    };
    for (size_t index = 0; index < _countof(controls); ++index) AddTooltip(controls[index].control, controls[index].text);
}

void HtmlExportNotesMetadataPage::AddTooltip(UINT id, UINT textId)
{
    HWND control = GetDlgItem(id); if (!control) return;
    m_tooltipTexts.push_back(LoadExportHtmlString(textId)); CString& text = m_tooltipTexts.back(); if (text.IsEmpty()) return;
    TOOLINFO info = {}; info.cbSize = sizeof(info); info.uFlags = TTF_IDISHWND | TTF_SUBCLASS; info.hwnd = m_hWnd; info.uId = reinterpret_cast<UINT_PTR>(control); info.lpszText = const_cast<LPTSTR>(static_cast<LPCTSTR>(text));
    ::SendMessage(m_tooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&info));
}

LRESULT HtmlExportNotesMetadataPage::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    if (m_tooltip) { ::DestroyWindow(m_tooltip); m_tooltip = NULL; }
    return 0;
}
