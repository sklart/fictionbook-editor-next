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
    { IDS_PLUGIN_WINDOW_SAVE_ERROR, L"export_html.settings.window_error" },
    { IDS_PLUGIN_SETTINGS_SAVE_ERROR, L"export_html.settings.save_error" },
    { IDS_PLUGIN_SETTINGS_CONFLICT, L"export_html.settings.conflict" },
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
    { IDS_TOOLTIP_CUSTOM_CSS, L"export_html.tooltip.custom_css" },
    { IDS_TOOLTIP_BROWSE_CSS, L"export_html.tooltip.browse_css" },
    { IDS_TOOLTIP_IMAGE_MAX_WIDTH, L"export_html.tooltip.image_max_width" },
    { IDS_TOOLTIP_IMAGE_MAX_HEIGHT, L"export_html.tooltip.image_max_height" },
    { IDS_CUSTOM_SAVE_TEMPLATE_LABEL, L"export_html.dialog.save.template_label" },
    { IDS_CUSTOM_SAVE_INCLUDE_DESC, L"export_html.dialog.save.include_description" },
    { IDS_CUSTOM_SAVE_TOC_DEPTH, L"export_html.dialog.save.toc_depth" },
    { IDS_OPEN_TEMPLATE_FILTER, L"export_html.dialog.save.template_filter" },
	{ IDS_OPEN_CSS_FILTER, L"export_html.dialog.options.css_filter" },
	{ IDS_CUSTOM_SAVE_CUSTOM_CSS, L"export_html.dialog.save.custom_css" },
	{ IDS_CUSTOM_SAVE_IMAGE_MAX_WIDTH, L"export_html.dialog.save.image_max_width" },
	{ IDS_CUSTOM_SAVE_IMAGE_MAX_HEIGHT, L"export_html.dialog.save.image_max_height" },
	{ IDS_UNKNOWN_ERROR, L"export_html.runtime.unknown_error" },
    { IDS_ERROR_EMBEDDED_IMAGES_TEMPLATE, L"export_html.runtime.error_embedded_images_template" },
    { IDS_HTML_EXPORT_OPTIONS_TITLE, L"export_html.dialog.options.title" },
    { IDS_SAVE_BUTTON, L"export_html.dialog.save.button" },
    { IDS_WARNING_EXTERNAL_RESOURCES, L"export_html.runtime.warning_external_resources" },
    { IDS_WARNING_STANDALONE_SIZE, L"export_html.runtime.warning_standalone_size" },
    { IDS_OPTIONS_IMAGES_FOLDER_NAME, L"export_html.dialog.images_folder_name" }, { IDS_OPTIONS_WARNING_MIB, L"export_html.dialog.warning_mib" },
    { IDS_OPTIONS_CSS_CLEAR, L"export_html.dialog.css_clear" },
    { IDS_TOOLTIP_INCLUDE_TOC, L"export_html.tooltip.include_toc" },
    { IDS_TOOLTIP_STYLE, L"export_html.tooltip.style" }, { IDS_TOOLTIP_FONT, L"export_html.tooltip.font" },
    { IDS_TOOLTIP_FONT_SIZE, L"export_html.tooltip.font_size" }, { IDS_TOOLTIP_LINE_HEIGHT, L"export_html.tooltip.line_height" },
    { IDS_TOOLTIP_CONTENT_WIDTH, L"export_html.tooltip.content_width" }, { IDS_TOOLTIP_MARGINS, L"export_html.tooltip.margins" },
    { IDS_TOOLTIP_TEXT_ALIGNMENT, L"export_html.tooltip.text_alignment" }, { IDS_TOOLTIP_HEADING_ALIGNMENT, L"export_html.tooltip.heading_alignment" },
    { IDS_TOOLTIP_CSS_CLEAR, L"export_html.tooltip.css_clear" }, { IDS_TOOLTIP_COVER_MODE, L"export_html.tooltip.cover_mode" },
    { IDS_TOOLTIP_IMAGES_FOLDER, L"export_html.tooltip.images_folder" }, { IDS_TOOLTIP_IMAGES_FOLDER_NAME, L"export_html.tooltip.images_folder_name" },
    { IDS_TOOLTIP_WARNING_MIB, L"export_html.tooltip.warning_mib" }, { IDS_TOOLTIP_NOTE_PLACEMENT, L"export_html.tooltip.note_placement" },
    { IDS_TOOLTIP_METADATA, L"export_html.tooltip.metadata" }, { IDS_TOOLTIP_METADATA_CHILD, L"export_html.tooltip.metadata_child" }, { IDS_TOOLTIP_DOCUMENT_STRUCTURE, L"export_html.tooltip.document_structure" },
    { IDS_TOOLTIP_DOCUMENT_STRUCTURE_FORMAT, L"export_html.tooltip.document_structure_format" }, { IDS_TOOLTIP_DOCUMENT_STRUCTURE_CUSTOM_TEMPLATE, L"export_html.tooltip.document_structure_custom_template" },
    { IDS_OPTIONS_TAB_GENERAL, L"export_html.dialog.tab.general" }, { IDS_OPTIONS_TAB_APPEARANCE, L"export_html.dialog.tab.appearance" }, { IDS_OPTIONS_TAB_IMAGES, L"export_html.dialog.tab.images" }, { IDS_OPTIONS_TAB_NOTES, L"export_html.dialog.tab.notes" },
    { IDS_OPTIONS_FORMAT, L"export_html.dialog.format" }, { IDS_OPTIONS_ENCODING, L"export_html.dialog.encoding" }, { IDS_OPTIONS_INCLUDE_TOC, L"export_html.dialog.include_toc" },
    { IDS_OPTIONS_STYLE, L"export_html.dialog.style" }, { IDS_OPTIONS_FONT, L"export_html.dialog.font" }, { IDS_OPTIONS_FONT_SIZE, L"export_html.dialog.font_size" }, { IDS_OPTIONS_LINE_HEIGHT, L"export_html.dialog.line_height" }, { IDS_OPTIONS_CONTENT_WIDTH, L"export_html.dialog.content_width" }, { IDS_OPTIONS_MARGINS, L"export_html.dialog.margins" }, { IDS_OPTIONS_TEXT_ALIGNMENT, L"export_html.dialog.text_alignment" }, { IDS_OPTIONS_HEADING_ALIGNMENT, L"export_html.dialog.heading_alignment" },
    { IDS_OPTIONS_COVER_MODE, L"export_html.dialog.cover_mode" }, { IDS_OPTIONS_IMAGES_FOLDER, L"export_html.dialog.images_folder" }, { IDS_OPTIONS_STANDALONE_WARNING, L"export_html.dialog.warning" }, { IDS_OPTIONS_NOTE_PLACEMENT, L"export_html.dialog.note_placement" }, { IDS_OPTIONS_DOCUMENT_STRUCTURE, L"export_html.dialog.document_structure" },
    { IDS_OPTIONS_INCLUDE_METADATA, L"export_html.dialog.metadata" }, { IDS_OPTIONS_METADATA_ANNOTATION, L"export_html.dialog.metadata_annotation" }, { IDS_OPTIONS_METADATA_TITLE, L"export_html.dialog.metadata_title" }, { IDS_OPTIONS_METADATA_DOCUMENT, L"export_html.dialog.metadata_document" }, { IDS_OPTIONS_METADATA_PUBLISH, L"export_html.dialog.metadata_publish" }, { IDS_OPTIONS_METADATA_HISTORY, L"export_html.dialog.metadata_history" }, { IDS_OPTIONS_METADATA_AUTHORS, L"export_html.dialog.metadata_authors" }, { IDS_OPTIONS_METADATA_TRANSLATORS, L"export_html.dialog.metadata_translators" }, { IDS_OPTIONS_METADATA_CUSTOM, L"export_html.dialog.metadata_custom" },
    { IDS_OPTIONS_VALUE_DEFAULT, L"export_html.dialog.value.default" }, { IDS_OPTIONS_VALUE_CLASSIC, L"export_html.dialog.value.classic" }, { IDS_OPTIONS_VALUE_BOOK, L"export_html.dialog.value.book" }, { IDS_OPTIONS_VALUE_MINIMAL, L"export_html.dialog.value.minimal" }, { IDS_OPTIONS_VALUE_SERIF, L"export_html.dialog.value.serif" }, { IDS_OPTIONS_VALUE_SANS, L"export_html.dialog.value.sans" }, { IDS_OPTIONS_VALUE_SYSTEM, L"export_html.dialog.value.system" }, { IDS_OPTIONS_VALUE_CUSTOM, L"export_html.dialog.value.custom" }, { IDS_OPTIONS_VALUE_LEFT, L"export_html.dialog.value.left" }, { IDS_OPTIONS_VALUE_JUSTIFIED, L"export_html.dialog.value.justified" }, { IDS_OPTIONS_VALUE_CENTER, L"export_html.dialog.value.center" }, { IDS_OPTIONS_VALUE_NORMAL, L"export_html.dialog.value.normal" }, { IDS_OPTIONS_VALUE_READING, L"export_html.dialog.value.reading" }, { IDS_OPTIONS_VALUE_VIEWPORT, L"export_html.dialog.value.viewport" }, { IDS_OPTIONS_VALUE_AUTOMATIC, L"export_html.dialog.value.automatic" }, { IDS_OPTIONS_VALUE_CUSTOM_NAME, L"export_html.dialog.value.custom_name" }, { IDS_OPTIONS_VALUE_SOURCE, L"export_html.dialog.value.source" }, { IDS_OPTIONS_VALUE_BOOK_END, L"export_html.dialog.value.book_end" }, { IDS_OPTIONS_VALUE_SECTION_END, L"export_html.dialog.value.section_end" }, { IDS_OPTIONS_VALUE_SINGLE_HTML, L"export_html.dialog.value.single_html" }, { IDS_OPTIONS_VALUE_SPLIT_SECTIONS, L"export_html.dialog.value.split_sections" },
    { IDS_OPTIONS_ERROR_TOC_DEPTH, L"export_html.dialog.error.toc_depth" }, { IDS_OPTIONS_ERROR_IMAGE_SIZE, L"export_html.dialog.error.image_size" }, { IDS_OPTIONS_ERROR_IMAGE_DIRECTORY, L"export_html.dialog.error.image_directory" }, { IDS_OPTIONS_ERROR_WARNING, L"export_html.dialog.error.warning" },
    { IDS_OPTIONS_VALUE_LINE_120, L"export_html.dialog.value.line_120" }, { IDS_OPTIONS_VALUE_LINE_150, L"export_html.dialog.value.line_150" }, { IDS_OPTIONS_VALUE_NARROW, L"export_html.dialog.value.narrow" }, { IDS_OPTIONS_VALUE_WIDE, L"export_html.dialog.value.wide" }, { IDS_OPTIONS_ERROR_FONT_SIZE, L"export_html.dialog.error.font_size" },
    { IDS_OPTIONS_ERROR_TEMPLATE, L"export_html.dialog.error.template" },
};

static std::map<UINT, CStringW> g_runtimeStrings;

static bool g_runtimeInitialized = false;

void InitExportHtmlRuntimeStrings()
{
    g_runtimeInitialized = true;
    g_runtimeStrings.clear();
    FbeRuntimeLocalization::LoadBundledPluginRuntimeStringFiles(_Module.GetModuleInstance(), L"export-html.json", g_runtimeStringBindings, _countof(g_runtimeStringBindings), g_runtimeStrings);
}

CString LoadExportHtmlString(UINT id)
{
    if(!g_runtimeInitialized) InitExportHtmlRuntimeStrings();
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
