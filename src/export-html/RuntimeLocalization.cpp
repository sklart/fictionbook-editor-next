#include "stdafx.h"
#include "resource.h"
#include "RuntimeLocalization.h"
#include "..\common\RuntimeLocalizationCommon.h"

#include <map>
#include <string>
#include <vector>

struct RuntimeStringBinding {
    UINT id;
    const wchar_t* key;
};

static const RuntimeStringBinding g_runtimeStringBindings[] = {
    { IDR_EXPORTHTML, L"export_html.runtime.menu_name" },
    { IDS_ERROR_OPEN_FILE, L"export_html.runtime.error_open_file" },
    { IDS_ERROR_CREATE_DIRECTORY, L"export_html.runtime.error_create_directory" },
    { IDS_ERROR_WRITE_FILE, L"export_html.runtime.error_write_file" },
    { IDS_ERROR_WRITE_FILE2, L"export_html.runtime.error_write_file_short" },
    { IDS_WARNING_FILE_ALREADY_EXISTS, L"export_html.runtime.warning_file_exists" },
    { IDS_SAVE_FILE_FILTER, L"export_html.runtime.save_file_filter" },
    { IDS_XML_PARSE_ERROR, L"export_html.runtime.xml_parse_error" },
    { IDS_AT_LINE_COLUMN, L"export_html.runtime.at_line_column" },
    { IDS_AT_S_S, L"export_html.runtime.at_source_message" },
    { IDS_ERROR, L"export_html.runtime.error_caption" },
    { IDS_COM_ERROR, L"export_html.runtime.com_error_caption" },
    { IDS_TOOLTIP_TEMPLATE, L"export_html.tooltip.template" },
    { IDS_TOOLTIP_BROWSE_TEMPLATE, L"export_html.tooltip.browse_template" },
    { IDS_TOOLTIP_DOCINFO, L"export_html.tooltip.include_description" },
    { IDS_TOOLTIP_TOC_DEPTH, L"export_html.tooltip.toc_depth" },
};

static std::map<UINT, CStringW> g_runtimeStrings;

void InitExportHtmlRuntimeStrings()
{
    g_runtimeStrings.clear();
    FbeRuntimeLocalization::LoadRuntimeStringFiles(_Module.GetModuleInstance(), L"export-html.json", g_runtimeStringBindings, _countof(g_runtimeStringBindings), g_runtimeStrings);
}

CString LoadExportHtmlString(UINT id)
{
    std::map<UINT, CStringW>::const_iterator it = g_runtimeStrings.find(id);
    if (it != g_runtimeStrings.end())
        return it->second;

    CString text;
    text.LoadString(id);
    return text;
}

CString FormatExportHtmlString(UINT id, ...)
{
    CString format = LoadExportHtmlString(id);
    CString text;
    va_list args;
    va_start(args, id);
    text.FormatV(format, args);
    va_end(args);
    return text;
}

int ShowExportHtmlTaskDialog(HWND owner, UINT titleId, LPCTSTR instruction, LPCTSTR content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR icon)
{
    CString title = LoadExportHtmlString(titleId);
    return AtlTaskDialog(owner, (LPCTSTR)title, instruction, content, buttons, icon);
}
