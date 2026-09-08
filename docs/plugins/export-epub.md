# ExportEPUB

`ExportEPUB.dll` exports FBE FB2 documents to EPUB; `ExportEPUBBatch.exe` supports repeatable corpus conversion. Use `tools/build/build.ps1` for builds. The plug-in is emitted to `out/Release/Plugins` and the batch executable to the chosen batch-output directory.

## Formats and capabilities

The exporter creates EPUB 2.0.1 and EPUB 3.3 OCF containers, with `mimetype` stored first and uncompressed, `container.xml`, OPF metadata, EPUB 2 NCX and EPUB 3 navigation documents. It supports configurable chapter splitting and TOC depth, including an NCX fallback for EPUB 3.

It preserves section IDs and valid internal links; converts FB2 notes and comments with forward/back links; exports cover, annotation, metadata, images, tables, poems, quotes, epigraphs and inline markup. EPUB 3 output also carries collection and baseline accessibility metadata. CSS options include alignment, first-line indentation and hyphenation; settings provide compatibility and detailed presets plus preflight validation and a summary dialog.

## Batch use and diagnostics

Run `ExportEPUBBatch.exe -Help` for current options. It is suitable for single-file or recursive corpus processing and produces reports suitable for automation. Keep corpus inputs, logs and packages under user-selected folders or `out/`, never in `src`.

The exporter handles malformed or missing image content types defensively, avoids invalid paragraph nesting around images, and records preflight warnings rather than silently creating broken EPUB links.
