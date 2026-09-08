# ImportEPUB

`ImportEPUB.dll` imports EPUB into the FBE FB2 DOM. `ImportEPUBBatch.exe` provides single-file and corpus EPUB-to-FB2 conversion. Build with `tools/build/build.ps1`: plug-in DLLs are under `out/Release/Plugins`; batch executables use the configured batch-output directory.

## Import capabilities

The importer reads EPUB 2/3 containers, OPF metadata/manifest/spine, `nav.xhtml` and NCX, then imports XHTML in spine order. It transfers document metadata, authors/translators, series, identifiers, cover and images, sections, links/anchors, notes, tables, lists, figures, code and common semantic markup. It also normalizes language and common encoding/typography defects, skips hidden or service pages when selected, and validates the resulting FB2.

The settings dialog controls content/structure, images and SVG, links and notes, cleanup/filtering, validation and logging. Diagnostic output reports missing binaries, unresolved notes, duplicate IDs and suspicious external links.

## SVG

`ImportEPUBLunaSVG.dll` is an optional helper dynamically loaded from the plug-in directory or beside the batch utility. Its pinned LunaSVG/PlutoVG source is in `third_party/lunasvg`; build products go to `build/lib/lunasvg`. SVG can be kept, rasterized to PNG/JPEG, or skipped. If the helper is unavailable or rendering fails, the importer retains its visible fallback behavior.

## Batch utility

Run `ImportEPUBBatch.exe --help` for the exact interface. It supports one EPUB or `--batch` directories, recursive and tree-preserving output, overwrite or resume behavior, CSV/HTML reports, profiles (`full`, `text`, `minimal`), and individual switches for covers, images, notes, tables, lists, links, semantic CSS, service pages and validation. `--svg keep|png|jpg|skip` selects SVG mode.

Manual corpus helpers live in `tools/tests/manual/import-epub`. They accept explicit input paths and write default results below `out/manual/import-epub`. The regression checker verifies failed rows, binary images, SVG conversion, note links, tables and duplicate IDs from a batch CSV report.

## Limits and diagnostics

DRM, fixed-layout EPUB, JavaScript, MathML, audio and video are not imported. Visual CSS is not reproduced wholesale; semantic classes and `epub:type` are used where applicable. ZIP extraction uses the Windows ZIP provider and guards against traversal and reparse-point escapes. MSXML is consumed through the SDK header, so static analysis does not need pre-generated type-library files.
