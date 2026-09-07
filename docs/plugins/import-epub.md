# ImportEPUB

`ImportEPUB.dll` imports EPUB into FBE and `ImportEPUBBatch.exe` is the console
companion. Build them using `tools/build/build.ps1`; output belongs under
`out/`, not beside the source.

Optional SVG rasterization is provided by `ImportEPUBLunaSVG.dll`. Its pinned
LunaSVG and PlutoVG sources are in `third_party/lunasvg`, with build products
under `build/lib/lunasvg`. The main importer loads the helper dynamically, so
an absent helper produces the existing fallback behavior.

Manual large-corpus and regression helpers are under
`tools/tests/manual/import-epub`. They require explicit corpus paths and write
all results below `out/manual/import-epub` by default.
