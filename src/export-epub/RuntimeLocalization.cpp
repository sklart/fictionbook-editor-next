#include "StdAfx.h"
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
    { IDS_EXPORT_ANNOTATION_TITLE, L"export_epub.content.annotation_title" },
    { IDS_EXPORT_BODY_COMMENTS_TITLE, L"export_epub.content.body_comments_title" },
    { IDS_EXPORT_BODY_NOTES_TITLE, L"export_epub.content.body_notes_title" },
    { IDS_EXPORT_BODY_TEXT_PREFIX, L"export_epub.content.body_text_prefix" },
    { IDS_EXPORT_CHAPTER_PREFIX, L"export_epub.content.chapter_prefix" },
    { IDS_EXPORT_COVER_TITLE, L"export_epub.content.cover_title" },
    { IDS_EXPORT_LANDMARK_ANNOTATION, L"export_epub.content.landmark_annotation" },
    { IDS_EXPORT_LANDMARK_BODY_START, L"export_epub.content.landmark_body_start" },
    { IDS_EXPORT_LANDMARK_NOTES, L"export_epub.content.landmark_notes" },
    { IDS_EXPORT_NAVIGATION_TITLE, L"export_epub.content.navigation_title" },
    { IDS_EXPORT_SUMMARY_ACTIONS, L"export_epub.summary.actions" },
    { IDS_EXPORT_SUMMARY_CHAPTERS, L"export_epub.summary.chapters" },
    { IDS_EXPORT_SUMMARY_FILE, L"export_epub.summary.file" },
    { IDS_EXPORT_SUMMARY_FORMAT, L"export_epub.summary.format" },
    { IDS_EXPORT_SUMMARY_IMAGES, L"export_epub.summary.images" },
    { IDS_EXPORT_SUMMARY_RESOURCES, L"export_epub.summary.resources" },
    { IDS_EXPORT_SUMMARY_SAVED, L"export_epub.summary.saved" },
    { IDS_EXPORT_SUMMARY_WARNINGS, L"export_epub.summary.warnings" },
    { IDS_EXPORT_TITLEPAGE_DATE, L"export_epub.content.titlepage_date" },
    { IDS_EXPORT_TITLEPAGE_PUBLISHER, L"export_epub.content.titlepage_publisher" },
    { IDS_EXPORT_TITLEPAGE_SERIES, L"export_epub.content.titlepage_series" },
    { IDS_OPTIONS_BUTTON_CANCEL, L"export_epub.options.button_cancel" },
    { IDS_OPTIONS_BUTTON_PRESET_COMPAT, L"export_epub.options.button_preset_compat" },
    { IDS_OPTIONS_BUTTON_PRESET_DEFAULT, L"export_epub.options.button_preset_default" },
    { IDS_OPTIONS_BUTTON_PRESET_RICH, L"export_epub.options.button_preset_rich" },
    { IDS_OPTIONS_CHECK_ANNOTATION_PAGE, L"export_epub.options.check_annotation_page" },
    { IDS_OPTIONS_CHECK_COVER_FIRST_IMAGE, L"export_epub.options.check_cover_first_image" },
    { IDS_OPTIONS_CHECK_COVER_PAGE, L"export_epub.options.check_cover_page" },
    { IDS_OPTIONS_CHECK_CSS_FIRST_INDENT, L"export_epub.options.check_css_first_indent" },
    { IDS_OPTIONS_CHECK_CSS_HYPHENATE, L"export_epub.options.check_css_hyphenate" },
    { IDS_OPTIONS_CHECK_CSS_JUSTIFY, L"export_epub.options.check_css_justify" },
    { IDS_OPTIONS_CHECK_NCX_FALLBACK, L"export_epub.options.check_ncx_fallback" },
    { IDS_OPTIONS_CHECK_NOTE_BACKLINKS, L"export_epub.options.check_note_backlinks" },
    { IDS_OPTIONS_CHECK_OPEN_FILE_AFTER_EXPORT, L"export_epub.options.check_open_file" },
    { IDS_OPTIONS_CHECK_OPEN_FOLDER_AFTER_EXPORT, L"export_epub.options.check_open_folder" },
    { IDS_OPTIONS_CHECK_REMOVE_UNUSED_IMAGES, L"export_epub.options.check_remove_unused_images" },
    { IDS_OPTIONS_CHECK_SHOW_PREFLIGHT, L"export_epub.options.check_show_preflight" },
    { IDS_OPTIONS_CHECK_SHOW_SUMMARY, L"export_epub.options.check_show_summary" },
    { IDS_OPTIONS_CHECK_TITLE_PAGE, L"export_epub.options.check_title_page" },
    { IDS_OPTIONS_CHECK_TOC_ANNOTATION, L"export_epub.options.check_toc_annotation" },
    { IDS_OPTIONS_CHECK_TOC_COVER, L"export_epub.options.check_toc_cover" },
    { IDS_OPTIONS_CHECK_TOC_NOTES, L"export_epub.options.check_toc_notes" },
    { IDS_OPTIONS_CHECK_WRITE_LOG, L"export_epub.options.check_write_log" },
    { IDS_OPTIONS_DIALOG_TITLE, L"export_epub.options.dialog_title" },
    { IDS_OPTIONS_LABEL_MAX_TOC_DEPTH, L"export_epub.options.label_max_toc_depth" },
    { IDS_OPTIONS_LABEL_MAX_XHTML_KB, L"export_epub.options.label_max_xhtml_kb" },
    { IDS_OPTIONS_SECTION_CSS, L"export_epub.options.section_css" },
    { IDS_OPTIONS_SECTION_GENERAL, L"export_epub.options.section_general" },
    { IDS_OPTIONS_SECTION_IMAGES, L"export_epub.options.section_images" },
    { IDS_OPTIONS_SECTION_NAVIGATION, L"export_epub.options.section_navigation" },
    { IDS_OPTIONS_SECTION_PRESETS, L"export_epub.options.section_presets" },
    { IDS_PREFLIGHT_COPY_PROMPT, L"export_epub.preflight.copy_prompt" },
    { IDS_PREFLIGHT_FULL_COUNT, L"export_epub.preflight.full_count" },
    { IDS_PREFLIGHT_FULL_HEADER, L"export_epub.preflight.full_header" },
    { IDS_PREFLIGHT_SUMMARY_HEADER, L"export_epub.preflight.summary_header" },
    { IDS_PREFLIGHT_SUMMARY_MORE, L"export_epub.preflight.summary_more" },
    { IDS_PREFLIGHT_WARNING_DUP_MANIFEST_ID, L"export_epub.preflight.warning_dup_manifest_id" },
    { IDS_PREFLIGHT_WARNING_DUP_XHTML_ID, L"export_epub.preflight.warning_dup_xhtml_id" },
    { IDS_PREFLIGHT_WARNING_EMPTY_RESOURCE, L"export_epub.preflight.warning_empty_resource" },
    { IDS_PREFLIGHT_WARNING_LARGE_XHTML, L"export_epub.preflight.warning_large_xhtml" },
    { IDS_PREFLIGHT_WARNING_MEDIA_TYPE_MISMATCH, L"export_epub.preflight.warning_media_type_mismatch" },
    { IDS_PREFLIGHT_WARNING_MISSING_FRAGMENT, L"export_epub.preflight.warning_missing_fragment" },
    { IDS_PREFLIGHT_WARNING_MISSING_IMAGE, L"export_epub.preflight.warning_missing_image" },
    { IDS_SAVE_DIALOG_BUTTON_EXPORT_OPTIONS, L"export_epub.save_dialog.button_export_options" },
    { IDS_TOOLTIP_ANNOTATION_PAGE, L"export_epub.tooltip.annotation_page" },
    { IDS_TOOLTIP_COVER_FIRST_IMAGE, L"export_epub.tooltip.cover_first_image" },
    { IDS_TOOLTIP_COVER_PAGE, L"export_epub.tooltip.cover_page" },
    { IDS_TOOLTIP_CSS_FIRST_INDENT, L"export_epub.tooltip.css_first_indent" },
    { IDS_TOOLTIP_CSS_HYPHENATE, L"export_epub.tooltip.css_hyphenate" },
    { IDS_TOOLTIP_CSS_JUSTIFY, L"export_epub.tooltip.css_justify" },
    { IDS_TOOLTIP_MAX_TOC_DEPTH, L"export_epub.tooltip.max_toc_depth" },
    { IDS_TOOLTIP_MAX_XHTML_KB, L"export_epub.tooltip.max_xhtml_kb" },
    { IDS_TOOLTIP_NCX_FALLBACK, L"export_epub.tooltip.ncx_fallback" },
    { IDS_TOOLTIP_NOTE_BACKLINKS, L"export_epub.tooltip.note_backlinks" },
    { IDS_TOOLTIP_OPEN_FILE_AFTER_EXPORT, L"export_epub.tooltip.open_file_after_export" },
    { IDS_TOOLTIP_OPEN_FOLDER_AFTER_EXPORT, L"export_epub.tooltip.open_folder_after_export" },
    { IDS_TOOLTIP_PRESET_COMPAT, L"export_epub.tooltip.preset_compat" },
    { IDS_TOOLTIP_PRESET_DEFAULT, L"export_epub.tooltip.preset_default" },
    { IDS_TOOLTIP_PRESET_RICH, L"export_epub.tooltip.preset_rich" },
    { IDS_TOOLTIP_REMOVE_UNUSED_IMAGES, L"export_epub.tooltip.remove_unused_images" },
    { IDS_TOOLTIP_SHOW_PREFLIGHT, L"export_epub.tooltip.show_preflight" },
    { IDS_TOOLTIP_SHOW_SUMMARY, L"export_epub.tooltip.show_summary" },
    { IDS_TOOLTIP_TITLE_PAGE, L"export_epub.tooltip.title_page" },
    { IDS_TOOLTIP_TOC_ANNOTATION, L"export_epub.tooltip.toc_annotation" },
    { IDS_TOOLTIP_TOC_COVER, L"export_epub.tooltip.toc_cover" },
    { IDS_TOOLTIP_TOC_NOTES, L"export_epub.tooltip.toc_notes" },
    { IDS_TOOLTIP_WRITE_LOG, L"export_epub.tooltip.write_log" },
};

std::map<UINT, CStringW> g_runtimeStrings;
HINSTANCE g_runtimeInstance = nullptr;

} // namespace

void InitExportEpubRuntimeStrings(HINSTANCE instance)
{
    g_runtimeInstance = instance;
    g_runtimeStrings.clear();
    FbeRuntimeLocalization::LoadRuntimeStringFiles(g_runtimeInstance, L"export-epub.json", g_runtimeStringBindings, _countof(g_runtimeStringBindings), g_runtimeStrings);
}

CStringW LoadExportEpubString(UINT stringId, LPCWSTR fallback)
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
