# ExportEPUB

`ExportEPUB.dll` is the bundled EPUB export plugin; `ExportEPUBBatch.exe` is
its console companion. The supported build entry point is
`tools/build/build.ps1`. Release artifacts are written below `out/`; no local
build wrappers or corpus launchers belong in `src/export-epub`.

The plugin version is derived from the repository-wide version sources. Do not
add a plugin-local version file.
