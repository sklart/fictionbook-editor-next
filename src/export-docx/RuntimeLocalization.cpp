#include "stdafx.h"
#include "resource.h"
#include "RuntimeLocalization.h"
#include "..\common\RuntimeLocalizationCommon.h"

#include <map>
#include <string>
#include <vector>

namespace {

struct RuntimeStringBinding
{
    UINT id;
    const wchar_t* key;
};

const RuntimeStringBinding g_runtimeStringBindings[] = {
    { IDR_EXPORTDOCX, L"export_docx.runtime.menu" },
    { IDS_AT_LINE_COLUMN, L"export_docx.runtime.at_line_column" },
    { IDS_AT_S_S, L"export_docx.runtime.at_s_s" },
    { IDS_COM_ERROR, L"export_docx.runtime.com_error" },
    { IDS_DOCX_EMPTY_LINE_IGNORE, L"export_docx.runtime.empty_line_ignore" },
    { IDS_DOCX_EMPTY_LINE_PARAGRAPH, L"export_docx.runtime.empty_line_paragraph" },
    { IDS_DOCX_EMPTY_LINE_SEPARATOR, L"export_docx.runtime.empty_line_separator" },
    { IDS_DOCX_EMPTY_LINE_SPACING, L"export_docx.runtime.empty_line_spacing" },
    { IDS_DOCX_LANGUAGE_AUTO, L"export_docx.runtime.language_auto" },
    { IDS_DOCX_LANGUAGE_EN, L"export_docx.runtime.language_en" },
    { IDS_DOCX_LANGUAGE_RU, L"export_docx.runtime.language_ru" },
    { IDS_DOCX_NOTES_ENDNOTES, L"export_docx.runtime.notes_endnotes" },
    { IDS_DOCX_NOTES_FOOTNOTES, L"export_docx.runtime.notes_footnotes" },
    { IDS_DOCX_NOTES_SECTION, L"export_docx.runtime.notes_section" },
    { IDS_DOCX_NOTES_TITLE, L"export_docx.runtime.notes_title" },
    { IDS_DOCX_PROFILE_BOOK, L"export_docx.runtime.profile_book" },
    { IDS_DOCX_PROFILE_COMPACT, L"export_docx.runtime.profile_compact" },
    { IDS_DOCX_PROFILE_MINIMAL, L"export_docx.runtime.profile_minimal" },
    { IDS_DOCX_REPORT_AUTHORS, L"export_docx.runtime.report_authors" },
    { IDS_DOCX_REPORT_COVERS_INSERTED, L"export_docx.runtime.report_covers_inserted" },
    { IDS_DOCX_REPORT_DOCX_PARAGRAPHS, L"export_docx.runtime.report_docx_paragraphs" },
    { IDS_DOCX_REPORT_DUP_BOOKMARKS_SKIPPED, L"export_docx.runtime.report_dup_bookmarks_skipped" },
    { IDS_DOCX_REPORT_EMPTY_LINE, L"export_docx.runtime.report_empty_line" },
    { IDS_DOCX_REPORT_EXTERNAL_LINKS, L"export_docx.runtime.report_external_links" },
    { IDS_DOCX_REPORT_FB2_SECTIONS, L"export_docx.runtime.report_fb2_sections" },
    { IDS_DOCX_REPORT_FB2_STYLESHEETS, L"export_docx.runtime.report_fb2_stylesheets" },
    { IDS_DOCX_REPORT_FILE, L"export_docx.runtime.report_file" },
    { IDS_DOCX_REPORT_HEIGHT_LIMITED_IMAGES, L"export_docx.runtime.report_height_limited_images" },
    { IDS_DOCX_REPORT_IMAGES, L"export_docx.runtime.report_images" },
    { IDS_DOCX_REPORT_IMAGES_EMBEDDED, L"export_docx.runtime.report_images_embedded" },
    { IDS_DOCX_REPORT_IMAGES_MISSING, L"export_docx.runtime.report_images_missing" },
    { IDS_DOCX_REPORT_IMAGES_REFERENCED, L"export_docx.runtime.report_images_referenced" },
    { IDS_DOCX_REPORT_INTERNAL_LINKS_BROKEN, L"export_docx.runtime.report_internal_links_broken" },
    { IDS_DOCX_REPORT_INTERNAL_LINKS_RESOLVED, L"export_docx.runtime.report_internal_links_resolved" },
    { IDS_DOCX_REPORT_INTERNAL_LINKS_TOTAL, L"export_docx.runtime.report_internal_links_total" },
    { IDS_DOCX_REPORT_INTERNAL_TARGETS, L"export_docx.runtime.report_internal_targets" },
    { IDS_DOCX_REPORT_NO, L"export_docx.runtime.report_no" },
    { IDS_DOCX_REPORT_NOTE_LINKS, L"export_docx.runtime.report_note_links" },
    { IDS_DOCX_REPORT_NOTES, L"export_docx.runtime.report_notes" },
    { IDS_DOCX_REPORT_NOTES_CREATED, L"export_docx.runtime.report_notes_created" },
    { IDS_DOCX_REPORT_NOTES_MISSING, L"export_docx.runtime.report_notes_missing" },
    { IDS_DOCX_REPORT_OPEN_AFTER_EXPORT, L"export_docx.runtime.report_open_after_export" },
    { IDS_DOCX_REPORT_PAGE_SIZE, L"export_docx.runtime.report_page_size" },
    { IDS_DOCX_REPORT_SETTINGS_HEADER, L"export_docx.runtime.report_settings_header" },
    { IDS_DOCX_REPORT_SMALL_IMAGES, L"export_docx.runtime.report_small_images" },
    { IDS_DOCX_REPORT_SOURCE_LANGUAGE, L"export_docx.runtime.report_source_language" },
    { IDS_DOCX_REPORT_TABLES, L"export_docx.runtime.report_tables" },
    { IDS_DOCX_REPORT_TITLE, L"export_docx.runtime.report_title" },
    { IDS_DOCX_REPORT_TITLE_PAGE, L"export_docx.runtime.report_title_page" },
    { IDS_DOCX_REPORT_TOC, L"export_docx.runtime.report_toc" },
    { IDS_DOCX_REPORT_TOC_DEPTH, L"export_docx.runtime.report_toc_depth" },
    { IDS_DOCX_REPORT_WARNINGS_HEADER, L"export_docx.runtime.report_warnings_header" },
    { IDS_DOCX_REPORT_WORD_BOOKMARKS, L"export_docx.runtime.report_word_bookmarks" },
    { IDS_DOCX_REPORT_WORD_LANGUAGE, L"export_docx.runtime.report_word_language" },
    { IDS_DOCX_REPORT_YES, L"export_docx.runtime.report_yes" },
    { IDS_DOCX_SAVE_BUTTON, L"export_docx.runtime.save_button" },
    { IDS_DOCX_TAB_ADVANCED, L"export_docx.runtime.tab_advanced" },
    { IDS_DOCX_TAB_FORMAT, L"export_docx.runtime.tab_format" },
    { IDS_DOCX_TAB_MAIN, L"export_docx.runtime.tab_main" },
    { IDS_DOCX_TAB_NOTES, L"export_docx.runtime.tab_notes" },
    { IDS_DOCX_TITLE_ANNOTATION, L"export_docx.runtime.title_annotation" },
    { IDS_DOCX_TITLE_DATE, L"export_docx.runtime.title_date" },
    { IDS_DOCX_TITLE_FB2_VERSION, L"export_docx.runtime.title_fb2_version" },
    { IDS_DOCX_TITLE_GENRES, L"export_docx.runtime.title_genres" },
    { IDS_DOCX_TITLE_HISTORY, L"export_docx.runtime.title_history" },
    { IDS_DOCX_TITLE_LANGUAGE, L"export_docx.runtime.title_language" },
    { IDS_DOCX_TITLE_PROGRAM, L"export_docx.runtime.title_program" },
    { IDS_DOCX_TITLE_PUBLISH_INFO, L"export_docx.runtime.title_publish_info" },
    { IDS_DOCX_TITLE_SERIES, L"export_docx.runtime.title_series" },
    { IDS_DOCX_TITLE_SOURCE_INFO, L"export_docx.runtime.title_source_info" },
    { IDS_DOCX_TITLE_TRANSLATION, L"export_docx.runtime.title_translation" },
    { IDS_DOCX_TOC_PLACEHOLDER, L"export_docx.runtime.toc_placeholder" },
    { IDS_DOCX_TOC_TITLE, L"export_docx.runtime.toc_title" },
    { IDS_DOCX_TOC_UPDATE_HINT, L"export_docx.runtime.toc_update_hint" },
    { IDS_DOCX_WARNING_AUTO_LANGUAGE_EMPTY, L"export_docx.runtime.warning_auto_language_empty" },
    { IDS_DOCX_WARNING_DOCUMENT_NO_BODY, L"export_docx.runtime.warning_document_no_body" },
    { IDS_DOCX_WARNING_DOCX_CREATED_WITH_WARNINGS, L"export_docx.runtime.warning_docx_created_with_warnings" },
    { IDS_DOCX_WARNING_EMPTY_ENDNOTE, L"export_docx.runtime.warning_empty_endnote" },
    { IDS_DOCX_WARNING_EMPTY_FOOTNOTE, L"export_docx.runtime.warning_empty_footnote" },
    { IDS_DOCX_WARNING_EMPTY_IMAGE, L"export_docx.runtime.warning_empty_image" },
    { IDS_DOCX_WARNING_FB2_STYLESHEET, L"export_docx.runtime.warning_fb2_stylesheet" },
    { IDS_DOCX_WARNING_HEADER_TITLE_MISSING, L"export_docx.runtime.warning_header_title_missing" },
    { IDS_DOCX_WARNING_HYPERLINK_REL_MISSING, L"export_docx.runtime.warning_hyperlink_rel_missing" },
    { IDS_DOCX_WARNING_IMAGE_BASE64_FAILED, L"export_docx.runtime.warning_image_base64_failed" },
    { IDS_DOCX_WARNING_IMAGE_NOT_FOUND, L"export_docx.runtime.warning_image_not_found" },
    { IDS_DOCX_WARNING_IMAGE_REL_MISSING, L"export_docx.runtime.warning_image_rel_missing" },
    { IDS_DOCX_WARNING_IMAGE_SIZE_FAILED, L"export_docx.runtime.warning_image_size_failed" },
    { IDS_DOCX_WARNING_IMAGE_UNKNOWN_CONTENT_TYPE, L"export_docx.runtime.warning_image_unknown_content_type" },
    { IDS_DOCX_WARNING_INTERNAL_LINK_BOOKMARKS_DISABLED, L"export_docx.runtime.warning_internal_link_bookmarks_disabled" },
    { IDS_DOCX_WARNING_INTERNAL_LINK_MISSING_TARGET, L"export_docx.runtime.warning_internal_link_missing_target" },
    { IDS_DOCX_WARNING_NOTE_MISSING, L"export_docx.runtime.warning_note_missing" },
    { IDS_DOCX_WARNING_RECURSIVE_NOTE, L"export_docx.runtime.warning_recursive_note" },
    { IDS_DOCX_WARNING_REFERENCED_IMAGES_NOT_EMBEDDED, L"export_docx.runtime.warning_referenced_images_not_embedded" },
    { IDS_DOCX_WARNING_TABLE_NO_ROWS, L"export_docx.runtime.warning_table_no_rows" },
    { IDS_DOCX_WARNING_TABLE_ROWSPAN_COLSPAN, L"export_docx.runtime.warning_table_rowspan_colspan" },
    { IDS_DOCX_WARNING_TABLE_XML_MISSING, L"export_docx.runtime.warning_table_xml_missing" },
    { IDS_DOCX_WARNING_TOC_FIELD_MISSING, L"export_docx.runtime.warning_toc_field_missing" },
    { IDS_ERROR, L"export_docx.runtime.error" },
    { IDS_ERROR_CREATE_DIRECTORY, L"export_docx.runtime.error_create_directory" },
    { IDS_ERROR_OPEN_FILE, L"export_docx.runtime.error_open_file" },
    { IDS_ERROR_WRITE_FILE, L"export_docx.runtime.error_write_file" },
    { IDS_ERROR_WRITE_FILE2, L"export_docx.runtime.error_write_file2" },
    { IDS_SAVE_FILE_FILTER, L"export_docx.runtime.save_file_filter" },
    { IDS_TOOLTIP_ADD_HEADERS, L"export_docx.tooltip.add_headers" },
    { IDS_TOOLTIP_ADD_PAGE_NUMBERS, L"export_docx.tooltip.add_page_numbers" },
    { IDS_TOOLTIP_ADD_TOC, L"export_docx.tooltip.add_toc" },
    { IDS_TOOLTIP_AUTO_HYPHENATION, L"export_docx.tooltip.auto_hyphenation" },
    { IDS_TOOLTIP_CHAPTER_PAGE_BREAK, L"export_docx.tooltip.chapter_page_break" },
    { IDS_TOOLTIP_CREATE_BOOKMARKS, L"export_docx.tooltip.create_bookmarks" },
    { IDS_TOOLTIP_CREATE_REPORT, L"export_docx.tooltip.create_report" },
    { IDS_TOOLTIP_CUSTOM_FONT, L"export_docx.tooltip.custom_font" },
    { IDS_TOOLTIP_DOC_LANGUAGE, L"export_docx.tooltip.doc_language" },
    { IDS_TOOLTIP_EMPTY_LINE_MODE, L"export_docx.tooltip.empty_line_mode" },
    { IDS_TOOLTIP_ENHANCED_FB2_STYLES, L"export_docx.tooltip.enhanced_fb2_styles" },
    { IDS_TOOLTIP_EXPORT_COVER, L"export_docx.tooltip.export_cover" },
    { IDS_TOOLTIP_EXPORT_HYPERLINKS, L"export_docx.tooltip.export_hyperlinks" },
    { IDS_TOOLTIP_EXPORT_IMAGES, L"export_docx.tooltip.export_images" },
    { IDS_TOOLTIP_EXPORT_METADATA, L"export_docx.tooltip.export_metadata" },
    { IDS_TOOLTIP_FIRST_LINE_INDENT, L"export_docx.tooltip.first_line_indent" },
    { IDS_TOOLTIP_FONT_NAME, L"export_docx.tooltip.font_name" },
    { IDS_TOOLTIP_FONT_SIZE, L"export_docx.tooltip.font_size" },
    { IDS_TOOLTIP_IMAGE_MAX_WIDTH_CM, L"export_docx.tooltip.image_max_width_cm" },
    { IDS_TOOLTIP_JUSTIFY_TEXT, L"export_docx.tooltip.justify_text" },
    { IDS_TOOLTIP_LIMIT_IMAGE_WIDTH, L"export_docx.tooltip.limit_image_width" },
    { IDS_TOOLTIP_NO_TITLE_PAGE_NUMBER, L"export_docx.tooltip.no_title_page_number" },
    { IDS_TOOLTIP_NOTES_MODE, L"export_docx.tooltip.notes_mode" },
    { IDS_TOOLTIP_OPEN_AFTER_EXPORT, L"export_docx.tooltip.open_after_export" },
    { IDS_TOOLTIP_PAGE_SIZE, L"export_docx.tooltip.page_size" },
    { IDS_TOOLTIP_PRESET_BOOK, L"export_docx.tooltip.preset_book" },
    { IDS_TOOLTIP_PRESET_EDITORIAL, L"export_docx.tooltip.preset_editorial" },
    { IDS_TOOLTIP_PRESET_MINIMAL, L"export_docx.tooltip.preset_minimal" },
    { IDS_TOOLTIP_RESET_DEFAULTS, L"export_docx.tooltip.reset_defaults" },
    { IDS_TOOLTIP_RESTART_PAGE_NUMBERING, L"export_docx.tooltip.restart_page_numbering" },
    { IDS_TOOLTIP_STYLE_PROFILE, L"export_docx.tooltip.style_profile" },
    { IDS_TOOLTIP_TITLE_INCLUDE_ANNOTATION, L"export_docx.tooltip.title_include_annotation" },
    { IDS_TOOLTIP_TITLE_INCLUDE_FB2_INFO, L"export_docx.tooltip.title_include_fb2_info" },
    { IDS_TOOLTIP_TITLE_INCLUDE_GENRES, L"export_docx.tooltip.title_include_genres" },
    { IDS_TOOLTIP_TITLE_INCLUDE_SERIES, L"export_docx.tooltip.title_include_series" },
    { IDS_TOOLTIP_TITLE_PAGE, L"export_docx.tooltip.title_page" },
    { IDS_TOOLTIP_TOC_DEPTH, L"export_docx.tooltip.toc_depth" },
    { IDS_TOOLTIP_VALIDATE_DOCX, L"export_docx.tooltip.validate_docx" },
    { IDS_WARNING_FILE_ALREADY_EXISTS, L"export_docx.runtime.warning_file_exists" },
    { IDS_XML_PARSE_ERROR, L"export_docx.runtime.xml_parse_error" },
};

std::map<UINT, CStringW> g_runtimeStrings;
std::map<std::wstring, CStringW> g_runtimeKeyStrings;

} // namespace

void InitExportDocxRuntimeStrings()
{
    g_runtimeStrings.clear();
    g_runtimeKeyStrings.clear();
    FbeRuntimeLocalization::LoadRuntimeStringFiles(g_hInstance, L"export-docx.json", g_runtimeStringBindings, _countof(g_runtimeStringBindings), g_runtimeStrings);
    FbeRuntimeLocalization::LoadRuntimeStringFiles(g_hInstance, L"export-docx.json", g_runtimeKeyStrings);
}

CStringW LoadExportDocxString(UINT stringId, LPCWSTR fallback)
{
    const auto it = g_runtimeStrings.find(stringId);
    if (it != g_runtimeStrings.end() && !it->second.IsEmpty()) {
        return it->second;
    }

    CStringW text;
    if (text.LoadStringW(stringId) && !text.IsEmpty()) {
        return text;
    }

    return fallback ? fallback : L"";
}

CStringW LoadExportDocxStringByKey(LPCWSTR key, LPCWSTR fallback)
{
    if (key != nullptr && key[0] != 0) {
        const auto it = g_runtimeKeyStrings.find(std::wstring(key));
        if (it != g_runtimeKeyStrings.end() && !it->second.IsEmpty()) {
            return it->second;
        }
    }

    return fallback ? fallback : L"";
}
