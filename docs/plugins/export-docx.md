# ExportDOCX

`ExportDOCX.dll` exports an FBE FB2 document to Microsoft Word DOCX. `ExportDOCXBatch.exe` supports unattended single-file and corpus conversion. Build through `tools/build/build.ps1`: the plug-in is written to `out/Release/Plugins`, while the batch executable is written to the selected batch-output directory (or `out/Release` by default).

## Capabilities

- FB2 text, metadata, cover and binary images; title pages with author, genre, series, annotation and technical FB2 information.
- Word styles for headings, quotes, epigraphs, poems, code, captions and tables; A4, A5 and Letter page formats, font and body-text settings.
- Word TOC (depth 1–4), section bookmarks, external and internal hyperlinks, page headers and page numbering.
- Footnotes, endnotes or a final notes section. Only actual FB2 notes are converted to Word notes; ordinary internal links remain hyperlinks.
- DOCX package validation and a per-file diagnostic report with counts for sections, images, notes, tables and links plus unresolved-reference warnings.

## Export settings

The export dialog controls image and cover inclusion, image width, title-page content, TOC and bookmarks, links, note mode, page format, typography and validation/report options. Word may require `F9` to update its generated TOC after opening a document.

## Batch conversion and diagnostics

Run `ExportDOCXBatch.exe -Help` for the authoritative option list. It supports recursive traversal, no-overwrite/resume modes, dry runs, flat output, input lists, CSV logs and optional HTML/TXT summaries. `-ValidateDocx` verifies the ZIP container, required XML/rels parts, relationships and embedded media.

Batch statuses distinguish successful output, output with warnings, invalid DOCX, failed conversion and skipped/dry-run items. Batch execution isolates a single conversion failure so the remaining corpus can continue. Put ad-hoc corpora and reports outside `src`, normally below `out/`.
