#include "stdafx.h"
#include "resource.h"
#include "RuntimeLocalization.h"
#include "..\common\RuntimeLocalizationCommon.h"

#include <map>
#include <string>
#include <vector>

extern HINSTANCE g_hInstance;

struct RuntimeStringBinding
{
    UINT id;
    const wchar_t* key;
};

static const RuntimeStringBinding g_runtimeStringBindings[] = {
    { IDS_IMPORT_OPTIONS_BUTTON_CANCEL, L"import_epub.options.button_cancel" },
    { IDS_IMPORT_OPTIONS_BUTTON_DEFAULTS, L"import_epub.options.button_defaults" },
    { IDS_IMPORT_OPTIONS_BUTTON_OK, L"import_epub.options.button_ok" },
    { IDS_IMPORT_OPTIONS_GROUP_CLEANUP, L"import_epub.group.cleanup" },
    { IDS_IMPORT_OPTIONS_GROUP_CONTENT, L"import_epub.group.content" },
    { IDS_IMPORT_OPTIONS_GROUP_DIAGNOSTICS, L"import_epub.group.diagnostics" },
    { IDS_IMPORT_OPTIONS_GROUP_IMAGES, L"import_epub.group.images" },
    { IDS_IMPORT_OPTIONS_GROUP_LINKS_NOTES, L"import_epub.group.links_notes" },
    { IDS_IMPORT_OPTIONS_HEADER, L"import_epub.options.header" },
    { IDS_IMPORT_OPTIONS_HINT, L"import_epub.options.hint" },
    { IDS_IMPORT_OPTIONS_OPT_CLEAN_TYPOGRAPHY, L"import_epub.option.clean_typography" },
    { IDS_IMPORT_OPTIONS_OPT_COVER, L"import_epub.option.cover" },
    { IDS_IMPORT_OPTIONS_OPT_CSS_SEMANTICS, L"import_epub.option.css_semantics" },
    { IDS_IMPORT_OPTIONS_OPT_DIAGNOSTIC, L"import_epub.option.diagnostic" },
    { IDS_IMPORT_OPTIONS_OPT_IMAGES, L"import_epub.option.images" },
    { IDS_IMPORT_OPTIONS_OPT_KEEP_TEMP, L"import_epub.option.keep_temp" },
    { IDS_IMPORT_OPTIONS_OPT_LINKS, L"import_epub.option.links" },
    { IDS_IMPORT_OPTIONS_OPT_LISTS, L"import_epub.option.lists" },
    { IDS_IMPORT_OPTIONS_OPT_LOG, L"import_epub.option.log" },
    { IDS_IMPORT_OPTIONS_OPT_LOG_ON_WARNINGS, L"import_epub.option.log_on_warnings" },
    { IDS_IMPORT_OPTIONS_OPT_NAV_TITLES, L"import_epub.option.nav_titles" },
    { IDS_IMPORT_OPTIONS_OPT_NOTES, L"import_epub.option.notes" },
    { IDS_IMPORT_OPTIONS_OPT_PAGE_BREAKS, L"import_epub.option.page_breaks" },
    { IDS_IMPORT_OPTIONS_OPT_POEMS, L"import_epub.option.poems" },
    { IDS_IMPORT_OPTIONS_OPT_REMOVE_BACKLINKS, L"import_epub.option.remove_backlinks" },
    { IDS_IMPORT_OPTIONS_OPT_REMOVE_SERVICE_SECTIONS, L"import_epub.option.remove_service_sections" },
    { IDS_IMPORT_OPTIONS_OPT_REPAIR_ENCODING, L"import_epub.option.repair_encoding" },
    { IDS_IMPORT_OPTIONS_OPT_SAVE_FB2, L"import_epub.option.save_fb2" },
    { IDS_IMPORT_OPTIONS_OPT_SAVE_INTERMEDIATE_ON_ERROR, L"import_epub.option.save_intermediate_on_error" },
    { IDS_IMPORT_OPTIONS_OPT_SKIP_HIDDEN, L"import_epub.option.skip_hidden" },
    { IDS_IMPORT_OPTIONS_OPT_SKIP_SERVICE, L"import_epub.option.skip_service" },
    { IDS_IMPORT_OPTIONS_OPT_SPLIT_HEADINGS, L"import_epub.option.split_headings" },
    { IDS_IMPORT_OPTIONS_OPT_SUBTITLES, L"import_epub.option.subtitles" },
    { IDS_IMPORT_OPTIONS_OPT_TABLES, L"import_epub.option.tables" },
    { IDS_IMPORT_OPTIONS_OPT_VALIDATE, L"import_epub.option.validate" },
    { IDS_IMPORT_OPTIONS_SVG_JPEG, L"import_epub.options.svg_jpeg" },
    { IDS_IMPORT_OPTIONS_SVG_KEEP, L"import_epub.options.svg_keep" },
    { IDS_IMPORT_OPTIONS_SVG_LABEL, L"import_epub.options.svg_label" },
    { IDS_IMPORT_OPTIONS_SVG_PNG, L"import_epub.options.svg_png" },
    { IDS_IMPORT_OPTIONS_SVG_SKIP, L"import_epub.options.svg_skip" },
    { IDS_IMPORT_OPTIONS_TITLE, L"import_epub.options.title" },
    { IDS_IMPORT_OPTIONS_TOOLTIP_DEFAULTS, L"import_epub.tooltip.defaults" },
    { IDS_IMPORT_OPTIONS_TOOLTIP_SVG_COMBO, L"import_epub.tooltip.svg_combo" },
    { IDS_IMPORT_OPTIONS_TOOLTIP_SVG_LABEL, L"import_epub.tooltip.svg_label" },
    { IDS_IMPORT_OPTIONS_TT_CLEAN_TYPOGRAPHY, L"import_epub.tooltip.clean_typography" },
    { IDS_IMPORT_OPTIONS_TT_COVER, L"import_epub.tooltip.cover" },
    { IDS_IMPORT_OPTIONS_TT_CSS_SEMANTICS, L"import_epub.tooltip.css_semantics" },
    { IDS_IMPORT_OPTIONS_TT_DIAGNOSTIC, L"import_epub.tooltip.diagnostic" },
    { IDS_IMPORT_OPTIONS_TT_GROUP_CLEANUP, L"import_epub.tooltip.group_cleanup" },
    { IDS_IMPORT_OPTIONS_TT_GROUP_CONTENT, L"import_epub.tooltip.group_content" },
    { IDS_IMPORT_OPTIONS_TT_GROUP_DIAGNOSTICS, L"import_epub.tooltip.group_diagnostics" },
    { IDS_IMPORT_OPTIONS_TT_GROUP_IMAGES, L"import_epub.tooltip.group_images" },
    { IDS_IMPORT_OPTIONS_TT_GROUP_LINKS_NOTES, L"import_epub.tooltip.group_links_notes" },
    { IDS_IMPORT_OPTIONS_TT_IMAGES, L"import_epub.tooltip.images" },
    { IDS_IMPORT_OPTIONS_TT_KEEP_TEMP, L"import_epub.tooltip.keep_temp" },
    { IDS_IMPORT_OPTIONS_TT_LINKS, L"import_epub.tooltip.links" },
    { IDS_IMPORT_OPTIONS_TT_LISTS, L"import_epub.tooltip.lists" },
    { IDS_IMPORT_OPTIONS_TT_LOG, L"import_epub.tooltip.log" },
    { IDS_IMPORT_OPTIONS_TT_LOG_ON_WARNINGS, L"import_epub.tooltip.log_on_warnings" },
    { IDS_IMPORT_OPTIONS_TT_NAV_TITLES, L"import_epub.tooltip.nav_titles" },
    { IDS_IMPORT_OPTIONS_TT_NOTES, L"import_epub.tooltip.notes" },
    { IDS_IMPORT_OPTIONS_TT_PAGE_BREAKS, L"import_epub.tooltip.page_breaks" },
    { IDS_IMPORT_OPTIONS_TT_POEMS, L"import_epub.tooltip.poems" },
    { IDS_IMPORT_OPTIONS_TT_REMOVE_BACKLINKS, L"import_epub.tooltip.remove_backlinks" },
    { IDS_IMPORT_OPTIONS_TT_REMOVE_SERVICE_SECTIONS, L"import_epub.tooltip.remove_service_sections" },
    { IDS_IMPORT_OPTIONS_TT_REPAIR_ENCODING, L"import_epub.tooltip.repair_encoding" },
    { IDS_IMPORT_OPTIONS_TT_SAVE_FB2, L"import_epub.tooltip.save_fb2" },
    { IDS_IMPORT_OPTIONS_TT_SAVE_INTERMEDIATE_ON_ERROR, L"import_epub.tooltip.save_intermediate_on_error" },
    { IDS_IMPORT_OPTIONS_TT_SKIP_HIDDEN, L"import_epub.tooltip.skip_hidden" },
    { IDS_IMPORT_OPTIONS_TT_SKIP_SERVICE, L"import_epub.tooltip.skip_service" },
    { IDS_IMPORT_OPTIONS_TT_SPLIT_HEADINGS, L"import_epub.tooltip.split_headings" },
    { IDS_IMPORT_OPTIONS_TT_SUBTITLES, L"import_epub.tooltip.subtitles" },
    { IDS_IMPORT_OPTIONS_TT_TABLES, L"import_epub.tooltip.tables" },
    { IDS_IMPORT_OPTIONS_TT_VALIDATE, L"import_epub.tooltip.validate" },
    { IDS_IMPORT_PLUGIN_ERROR_COM, L"import_epub.plugin.error_com" },
    { IDS_IMPORT_PLUGIN_ERROR_MSXML_CREATE, L"import_epub.plugin.error_msxml_create" },
    { IDS_IMPORT_PLUGIN_ERROR_MSXML_LOAD, L"import_epub.plugin.error_msxml_load" },
    { IDS_IMPORT_PLUGIN_ERROR_UNEXPECTED, L"import_epub.plugin.error_unexpected" },
    { IDS_IMPORT_PLUGIN_FILEDLG_FILE_LABEL, L"import_epub.plugin.filedlg_file_label" },
    { IDS_IMPORT_PLUGIN_FILEDLG_IMPORT_BUTTON, L"import_epub.plugin.filedlg_import_button" },
    { IDS_IMPORT_PLUGIN_FILEDLG_SETTINGS_BUTTON, L"import_epub.plugin.filedlg_settings_button" },
    { IDS_IMPORT_PLUGIN_FILEDLG_FILTER_EPUB, L"import_epub.plugin.filedlg_filter_epub" },
    { IDS_IMPORT_PLUGIN_FILEDLG_FILTER_ALL, L"import_epub.plugin.filedlg_filter_all" },
    { IDS_IMPORT_PLUGIN_FILEDLG_TITLE, L"import_epub.plugin.filedlg_title" },
    { IDS_IMPORT_PLUGIN_STAGE_CONVERT, L"import_epub.plugin.stage_convert" },
    { IDS_IMPORT_PLUGIN_STAGE_CREATE_DOM, L"import_epub.plugin.stage_create_dom" },
    { IDS_IMPORT_PLUGIN_STAGE_PREPARE, L"import_epub.plugin.stage_prepare" },
    { IDS_IMPORT_PLUGIN_STAGE_READ_SETTINGS, L"import_epub.plugin.stage_read_settings" },
    { IDS_IMPORT_PLUGIN_STAGE_RETURN_RESULT, L"import_epub.plugin.stage_return_result" },
    { IDS_IMPORT_PLUGIN_STAGE_SELECT_FILE, L"import_epub.plugin.stage_select_file" },
    { IDS_IMPORT_PLUGIN_WARNING_PARTIAL_IMPORT, L"import_epub.plugin.warning_partial_import" },
    { IDS_IMPORT_RUNTIME_CHAPTER_TEXT_NOT_EXTRACTED, L"import_epub.runtime.chapter_text_not_extracted" },
    { IDS_IMPORT_RUNTIME_CONTAINER_MISSING, L"import_epub.runtime.container_missing" },
    { IDS_IMPORT_RUNTIME_CONTAINER_ROOTFILE_MISSING, L"import_epub.runtime.container_rootfile_missing" },
    { IDS_IMPORT_RUNTIME_DEDICATION_TITLE, L"import_epub.runtime.dedication_title" },
    { IDS_IMPORT_RUNTIME_DUPLICATE_FB2_ID, L"import_epub.runtime.duplicate_fb2_id" },
    { IDS_IMPORT_RUNTIME_EMPTY_NOTE_PLACEHOLDER, L"import_epub.runtime.empty_note_placeholder" },
    { IDS_IMPORT_RUNTIME_ENCRYPTION_DETECTED, L"import_epub.runtime.encryption_detected" },
    { IDS_IMPORT_RUNTIME_FINAL_BODY_MISSING, L"import_epub.runtime.final_body_missing" },
    { IDS_IMPORT_RUNTIME_FINAL_MSXML_CREATE_FAILED, L"import_epub.runtime.final_msxml_create_failed" },
    { IDS_IMPORT_RUNTIME_FINAL_XML_FAILED, L"import_epub.runtime.final_xml_failed" },
    { IDS_IMPORT_RUNTIME_GDIPLUS_PLACEHOLDER_FAILED, L"import_epub.runtime.gdiplus_placeholder_failed" },
    { IDS_IMPORT_RUNTIME_GDIPLUS_PNG_TO_JPEG_FAILED, L"import_epub.runtime.gdiplus_png_to_jpeg_failed" },
    { IDS_IMPORT_RUNTIME_HTML_FALLBACK_FAILED, L"import_epub.runtime.html_fallback_failed" },
    { IDS_IMPORT_RUNTIME_IMAGE_BINARY_MISSING, L"import_epub.runtime.image_binary_missing" },
    { IDS_IMPORT_RUNTIME_IMAGE_PLACEHOLDER, L"import_epub.runtime.image_placeholder" },
    { IDS_IMPORT_RUNTIME_IMAGE_PLACEHOLDER_OPEN, L"import_epub.runtime.image_placeholder_open" },
    { IDS_IMPORT_RUNTIME_IMAGE_PREPARE_FAILED, L"import_epub.runtime.image_prepare_failed" },
    { IDS_IMPORT_RUNTIME_IMPORT_WARNINGS_TITLE, L"import_epub.runtime.import_warnings_title" },
    { IDS_IMPORT_RUNTIME_INTERNAL_LINK_MISSING_ID, L"import_epub.runtime.internal_link_missing_id" },
    { IDS_IMPORT_RUNTIME_MIMETYPE_MISSING, L"import_epub.runtime.mimetype_missing" },
    { IDS_IMPORT_RUNTIME_MIMETYPE_UNEXPECTED, L"import_epub.runtime.mimetype_unexpected" },
    { IDS_IMPORT_RUNTIME_MSXML_CREATE_FAILED, L"import_epub.runtime.msxml_create_failed" },
    { IDS_IMPORT_RUNTIME_NAV_HEADINGS_COUNT, L"import_epub.runtime.nav_headings_count" },
    { IDS_IMPORT_RUNTIME_NAV_READ_FAILED, L"import_epub.runtime.nav_read_failed" },
    { IDS_IMPORT_RUNTIME_NCX_NAVPOINT_COUNT, L"import_epub.runtime.ncx_navpoint_count" },
    { IDS_IMPORT_RUNTIME_NCX_READ_FAILED, L"import_epub.runtime.ncx_read_failed" },
    { IDS_IMPORT_RUNTIME_NOTES_BODY_MOVED, L"import_epub.runtime.notes_body_moved" },
    { IDS_IMPORT_RUNTIME_NOTE_SECTION_MISSING, L"import_epub.runtime.note_section_missing" },
    { IDS_IMPORT_RUNTIME_OPF_SPINE_MISSING, L"import_epub.runtime.opf_spine_missing" },
    { IDS_IMPORT_RUNTIME_SELECTED_EPUB_MISSING, L"import_epub.runtime.selected_epub_missing" },
    { IDS_IMPORT_RUNTIME_SPINE_FILE_MISSING, L"import_epub.runtime.spine_file_missing" },
    { IDS_IMPORT_RUNTIME_SPINE_IMAGE_MISSING, L"import_epub.runtime.spine_image_missing" },
    { IDS_IMPORT_RUNTIME_SPINE_NO_SUPPORTED, L"import_epub.runtime.spine_no_supported" },
    { IDS_IMPORT_RUNTIME_SVG_ADAPTER_EXPORT_MISSING, L"import_epub.runtime.svg_adapter_export_missing" },
    { IDS_IMPORT_RUNTIME_SVG_ADAPTER_LOAD_FAILED, L"import_epub.runtime.svg_adapter_load_failed" },
    { IDS_IMPORT_RUNTIME_SVG_ADAPTER_MISSING, L"import_epub.runtime.svg_adapter_missing" },
    { IDS_IMPORT_RUNTIME_SVG_ADAPTER_RENDER_FAILED, L"import_epub.runtime.svg_adapter_render_failed" },
    { IDS_IMPORT_RUNTIME_SVG_CONVERTED_READ_FAILED, L"import_epub.runtime.svg_converted_read_failed" },
    { IDS_IMPORT_RUNTIME_SVG_PLACEHOLDER_CREATE_FAILED, L"import_epub.runtime.svg_placeholder_create_failed" },
    { IDS_IMPORT_RUNTIME_SVG_PLACEHOLDER_REASON, L"import_epub.runtime.svg_placeholder_reason" },
    { IDS_IMPORT_RUNTIME_SVG_PLACEHOLDER_SUFFIX, L"import_epub.runtime.svg_placeholder_suffix" },
    { IDS_IMPORT_RUNTIME_SVG_SAVE_JPEG_FAILED, L"import_epub.runtime.svg_save_jpeg_failed" },
    { IDS_IMPORT_RUNTIME_SVG_SKIPPED_BY_OPTION, L"import_epub.runtime.svg_skipped_by_option" },
    { IDS_IMPORT_RUNTIME_SVG_SPINE_SKIPPED, L"import_epub.runtime.svg_spine_skipped" },
    { IDS_IMPORT_RUNTIME_XHTML_BODY_MISSING, L"import_epub.runtime.xhtml_body_missing" },
    { IDS_IMPORT_RUNTIME_XHTML_NOTES_CHECK_FAILED, L"import_epub.runtime.xhtml_notes_check_failed" },
    { IDS_IMPORT_RUNTIME_XHTML_READ_FAILED, L"import_epub.runtime.xhtml_read_failed" },
    { IDS_IMPORT_RUNTIME_XML_FALLBACK_FAILED, L"import_epub.runtime.xml_fallback_failed" },
    { IDS_IMPORT_RUNTIME_XML_LOAD_FAILED, L"import_epub.runtime.xml_load_failed" },
    { IDS_IMPORT_RUNTIME_XML_UNKNOWN, L"import_epub.runtime.xml_unknown" },
    { IDS_IMPORT_RUNTIME_XML_UNKNOWN_AFTER_HTML, L"import_epub.runtime.xml_unknown_after_html" },
    { IDS_IMPORT_RUNTIME_XML_UNKNOWN_AFTER_XML, L"import_epub.runtime.xml_unknown_after_xml" },
    { IDS_IMPORT_RUNTIME_ZIP_COPY_FAILED, L"import_epub.runtime.zip_copy_failed" },
    { IDS_IMPORT_RUNTIME_ZIP_DEST_FAILED, L"import_epub.runtime.zip_dest_failed" },
    { IDS_IMPORT_RUNTIME_ZIP_EXTRACT_FAILED, L"import_epub.runtime.zip_extract_failed" },
    { IDS_IMPORT_RUNTIME_ZIP_ITEMS_DISPATCH_FAILED, L"import_epub.runtime.zip_items_dispatch_failed" },
    { IDS_IMPORT_RUNTIME_ZIP_ITEMS_FAILED, L"import_epub.runtime.zip_items_failed" },
    { IDS_IMPORT_RUNTIME_ZIP_OPEN_FAILED, L"import_epub.runtime.zip_open_failed" },
    { IDS_IMPORT_RUNTIME_ZIP_REPARSE_POINT, L"import_epub.runtime.zip_reparse_point" },
    { IDS_IMPORT_RUNTIME_ZIP_SHELL_CREATE_FAILED, L"import_epub.runtime.zip_shell_create_failed" },
    { IDS_IMPORT_RUNTIME_ZIP_TIMEOUT_CONTAINER, L"import_epub.runtime.zip_timeout_container" },
    { IDS_IMPORT_RUNTIME_ZIP_UNSAFE_PATH, L"import_epub.runtime.zip_unsafe_path" },
};

static std::map<UINT, CStringW> g_runtimeStrings;

void InitImportEpubRuntimeStrings()
{
    const HINSTANCE instance = g_hInstance != nullptr ? g_hInstance : _AtlBaseModule.GetModuleInstance();
    g_runtimeStrings.clear();
    FbeRuntimeLocalization::LoadRuntimeStringFiles(instance, L"import-epub.json", g_runtimeStringBindings, _countof(g_runtimeStringBindings), g_runtimeStrings);
}

CStringW LoadImportEpubString(UINT stringId, LPCWSTR fallback)
{
    std::map<UINT, CStringW>::const_iterator it = g_runtimeStrings.find(stringId);
    if (it != g_runtimeStrings.end() && !it->second.IsEmpty())
        return it->second;

    CStringW text;
    if (text.LoadString(stringId) && !text.IsEmpty())
        return text;

    return fallback ? fallback : L"";
}
