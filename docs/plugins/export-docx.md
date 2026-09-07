# ExportDOCX

`ExportDOCX.dll` is the bundled DOCX export plugin; `ExportDOCXBatch.exe` is
its console companion. Build both through `tools/build/build.ps1`, which places
the plugin in `out/Release/Plugins` and the batch utility in its configured
batch-output directory.

Source-layout policy deliberately keeps local build, registration, packaging
and corpus-launcher scripts out of `src/export-docx`. Use the official build
and release-validation scripts under `tools/build`, and put any ad-hoc corpus
work under `tools/tests/manual` with input paths passed as parameters.
