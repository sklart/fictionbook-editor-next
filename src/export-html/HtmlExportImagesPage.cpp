#include "stdafx.h"
#include "HtmlExportImagesPage.h"
#include "RuntimeLocalization.h"
#include "utils.h"

namespace {
bool IsSafeRelativeImageDirectory(const CString& value)
{
    if (value.IsEmpty()) return true;
    if (!::PathIsRelative(value)) return false;
    int position = 0;
    while (position <= value.GetLength()) {
        const int backslash = value.Find(L'\\', position);
        const int slash = value.Find(L'/', position);
        const int separator = backslash < 0 ? slash : (slash < 0 ? backslash : min(backslash, slash));
        const CString segment = value.Mid(position, separator < 0 ? value.GetLength() - position : separator - position);
        if (segment == L"..") return false;
        if (separator < 0) break;
        position = separator + 1;
    }
    return true;
}
}

LRESULT HtmlExportImagesPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
    SetDlgItemText(IDC_IMAGE_MAX_WIDTH_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_IMAGE_MAX_WIDTH));
    SetDlgItemText(IDC_IMAGE_MAX_HEIGHT_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_IMAGE_MAX_HEIGHT));
    SetDlgItemText(IDC_COVER_MODE_LABEL, LoadExportHtmlString(IDS_OPTIONS_COVER_MODE));
    SetDlgItemText(IDC_IMAGES_FOLDER_LABEL, LoadExportHtmlString(IDS_OPTIONS_IMAGES_FOLDER));
    SetDlgItemText(IDC_IMAGES_FOLDER_NAME_LABEL, LoadExportHtmlString(IDS_OPTIONS_IMAGES_FOLDER_NAME));
    SetDlgItemText(IDC_STANDALONE_WARNING_LABEL, LoadExportHtmlString(IDS_OPTIONS_WARNING_MIB));
    LoadFromSettings();
    InitTooltips();
    return TRUE;
}

void HtmlExportImagesPage::LoadFromSettings()
{
    if (!m_settings) return;
    CComboBox cover(GetDlgItem(IDC_COVER_MODE));
    cover.ResetContent();
    const UINT coverLabels[] = { IDS_OPTIONS_VALUE_NORMAL, IDS_OPTIONS_VALUE_READING, IDS_OPTIONS_VALUE_VIEWPORT };
    for (size_t index = 0; index < _countof(coverLabels); ++index) cover.AddString(LoadExportHtmlString(coverLabels[index]));
    cover.SetCurSel(m_settings->coverMode >= 0 && m_settings->coverMode < 3 ? m_settings->coverMode : 0);
    CComboBox folder(GetDlgItem(IDC_IMAGES_FOLDER));
    folder.ResetContent();
    folder.AddString(LoadExportHtmlString(IDS_OPTIONS_VALUE_AUTOMATIC));
    folder.AddString(LoadExportHtmlString(IDS_OPTIONS_VALUE_CUSTOM_NAME));
    folder.SetCurSel(m_settings->externalImagesFolderMode == 1 ? 1 : 0);
    SetDlgItemInt(IDC_IMAGE_MAX_WIDTH, m_settings->imageMaxWidth, FALSE);
    SetDlgItemInt(IDC_IMAGE_MAX_HEIGHT, m_settings->imageMaxHeight, FALSE);
    SetDlgItemText(IDC_IMAGES_FOLDER_NAME, m_settings->externalImagesFolderName);
    SetDlgItemInt(IDC_STANDALONE_WARNING, m_settings->standaloneWarningMiB, FALSE);
    UpdateEnabledState();
}

bool HtmlExportImagesPage::SaveToSettings(HtmlExportSettings& candidate, CString& error)
{
    BOOL translated = FALSE;
    const int width = static_cast<int>(GetDlgItemInt(IDC_IMAGE_MAX_WIDTH, &translated, FALSE));
    if (!translated || width < 0 || width > 10000) { ::SetFocus(GetDlgItem(IDC_IMAGE_MAX_WIDTH)); error = LoadExportHtmlString(IDS_OPTIONS_ERROR_IMAGE_SIZE); return false; }
    const int height = static_cast<int>(GetDlgItemInt(IDC_IMAGE_MAX_HEIGHT, &translated, FALSE));
    if (!translated || height < 0 || height > 10000) { ::SetFocus(GetDlgItem(IDC_IMAGE_MAX_HEIGHT)); error = LoadExportHtmlString(IDS_OPTIONS_ERROR_IMAGE_SIZE); return false; }
    const int warning = static_cast<int>(GetDlgItemInt(IDC_STANDALONE_WARNING, &translated, FALSE));
    if (!translated || (warning != 0 && warning < 1)) { ::SetFocus(GetDlgItem(IDC_STANDALONE_WARNING)); error = LoadExportHtmlString(IDS_OPTIONS_ERROR_WARNING); return false; }
    const CString directory = U::GetWindowText(GetDlgItem(IDC_IMAGES_FOLDER_NAME));
    if (!IsSafeRelativeImageDirectory(directory)) { ::SetFocus(GetDlgItem(IDC_IMAGES_FOLDER_NAME)); error = LoadExportHtmlString(IDS_OPTIONS_ERROR_IMAGE_DIRECTORY); return false; }
    candidate.imageMaxWidth = width;
    candidate.imageMaxHeight = height;
    candidate.coverMode = max(0, min(2, CComboBox(GetDlgItem(IDC_COVER_MODE)).GetCurSel()));
    candidate.externalImagesFolderMode = CComboBox(GetDlgItem(IDC_IMAGES_FOLDER)).GetCurSel() == 1 ? 1 : 0;
    candidate.externalImagesFolderName = candidate.externalImagesFolderMode == 0 ? CString() : directory;
    candidate.standaloneWarningMiB = warning;
    return true;
}

void HtmlExportImagesPage::UpdateEnabledState()
{
    const BOOL customName = CComboBox(GetDlgItem(IDC_IMAGES_FOLDER)).GetCurSel() == 1;
    ::EnableWindow(GetDlgItem(IDC_IMAGES_FOLDER_NAME), customName);
    ::EnableWindow(GetDlgItem(IDC_IMAGES_FOLDER_NAME_LABEL), customName);
}

LRESULT HtmlExportImagesPage::OnFolderMode(WORD, WORD, HWND, BOOL&)
{
    UpdateEnabledState();
    return 0;
}

void HtmlExportImagesPage::InitTooltips()
{
    m_tooltip = ::CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
    if (!m_tooltip) return;
    const struct { UINT control; UINT text; } controls[] = {
        { IDC_IMAGE_MAX_WIDTH, IDS_TOOLTIP_IMAGE_MAX_WIDTH }, { IDC_IMAGE_MAX_HEIGHT, IDS_TOOLTIP_IMAGE_MAX_HEIGHT },
        { IDC_COVER_MODE, IDS_TOOLTIP_COVER_MODE }, { IDC_IMAGES_FOLDER, IDS_TOOLTIP_IMAGES_FOLDER },
        { IDC_IMAGES_FOLDER_NAME, IDS_TOOLTIP_IMAGES_FOLDER_NAME }, { IDC_STANDALONE_WARNING, IDS_TOOLTIP_WARNING_MIB }
    };
    for (size_t index = 0; index < _countof(controls); ++index) AddTooltip(controls[index].control, controls[index].text);
}

void HtmlExportImagesPage::AddTooltip(UINT id, UINT textId)
{
    HWND control = GetDlgItem(id); if (!control) return;
    m_tooltipTexts.push_back(LoadExportHtmlString(textId)); CString& text = m_tooltipTexts.back(); if (text.IsEmpty()) return;
    TOOLINFO info = {}; info.cbSize = sizeof(info); info.uFlags = TTF_IDISHWND | TTF_SUBCLASS; info.hwnd = m_hWnd; info.uId = reinterpret_cast<UINT_PTR>(control); info.lpszText = const_cast<LPTSTR>(static_cast<LPCTSTR>(text));
    ::SendMessage(m_tooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&info));
}

LRESULT HtmlExportImagesPage::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    if (m_tooltip) { ::DestroyWindow(m_tooltip); m_tooltip = NULL; }
    return 0;
}
