# ImportEPUB

`ImportEPUB.dll` imports EPUB into the FBE FB2 DOM. `ImportEPUBBatch.exe` provides single-file and corpus EPUB-to-FB2 conversion. Build with `tools/build/build.ps1`: plug-in DLLs are under `out/Release/Plugins`; batch executables use the configured batch-output directory.

## Import capabilities

The importer reads EPUB 2/3 containers, OPF metadata/manifest/spine, `nav.xhtml` and NCX, then imports XHTML in spine order. It transfers document metadata, authors/translators, series, identifiers, cover and images, sections, links/anchors, notes, tables, lists, figures, code and common semantic markup. It also normalizes language and common encoding/typography defects, skips hidden or service pages when selected, and validates the resulting FB2.

The settings dialog controls content/structure, images and SVG, links and notes, cleanup/filtering, validation and logging. Diagnostic output reports missing binaries, unresolved notes, duplicate IDs and suspicious external links.

### Notes, resources and section integrity

Note targets and ordinary anchors are indexed by canonical XHTML path plus the
original, case-sensitive fragment. Manifest binary IDs and source anchors are
reserved in one global namespace before rendering; subsequently discovered
resources use the same allocator. Equal IDs in different documents and IDs which
normalize to the same spelling remain distinct. Forward references are resolved
against that index.

Explicit EPUB/DPUB note semantics are supported on paragraphs, list items and
containers. For unmarked EPUB 2/3 content, a short numeric marker can identify a
note when its target has a suitable paragraph/list/container shape and is in a
separate non-spine document, a `notes`/`footnotes`/`endnotes`/`links` document, or
has a matching backlink or superscript reference. The filename alone is not
sufficient: ordinary heading links remain ordinary links. References lacking
these signals remain ordinary links; ambiguous unmarked markup still requires
verification against the source book.

Markers `*`, `**`, `***`, single Latin/Cyrillic letters, daggers and double
daggers are also recognized when a notes document, semantic note container or
matching backlink supplies additional evidence. A marker alone never moves
ordinary navigation text into notes.

Only collected note subtrees are removed from the main text. A chapter with an
inline note retains its other text; a document is omitted from the main body
only when it contains no remaining substantive content. Notes can contain
multiple paragraphs, inline formatting, lists and images. Unresolved note
references retain a visible marker and produce a warning with the source and
target; there is no fabricated note text.

An anchor inside the first paragraph of a note list item selects the entire
`li`, including subsequent paragraphs and images. The same rule applies to
semantic `div`, `section` and `aside` note containers. Selection is shared by
inventory, extraction and removal from the main body; neighboring notes are
kept separate. Backlinks follow the existing removal option. When only a
paragraph is reliably identified and adjacent unmarked paragraphs could be
continuations, the importer warns about limited boundary recognition and
does not absorb the ambiguous text. Source verification remains necessary.

Image selection follows XHTML-relative paths, including parent segments,
UTF-8 URL escapes and Cyrillic names. `img`, `srcset`, `picture` sources,
`figure`, SVG `image` references and image `object data` are supported. A
missing candidate can fall back to an available supported candidate. Only
referenced resources and the selected cover are embedded; a referenced local
image omitted from the manifest can also be recovered through the same guarded
path resolver. The importer does not scan and embed an entire Images folder.
Media queries, responsive layout and browser viewport selection are not
reproduced: the first available supported candidate is selected.

Heading levels `h1`–`h6` and XHTML section containers preserve nesting and
reading order. Parents containing only child sections have no filler paragraph.
The EPUB 3 `toc` navigation tree (or EPUB 2 NCX `navMap`) can also join parts
and chapters stored in different XHTML files. Fragment targets are mapped to
existing sections; repeated TOC links do not duplicate source content.
`page-list` and other navigation lists do not define chapter nesting. Spine
order takes precedence: a TOC relationship is applied only at a contiguous
reading-order boundary; conflicts produce a warning, rather than moving text.
Absent or incomplete TOC data does not invent parent relationships. Turning
off navigation titles or heading splitting retains their existing behavior
and disables this TOC-based nesting.
When FB2 requires choosing between child sections and paragraph content, real
text runs are wrapped in anonymous child sections in their original order.
After nesting, an independent section containing only its source title is
preserved once as emphasized paragraph text, with a structure-conversion
warning. A title-only parent with children retains its actual title. Neither
case needs filler text, blank paragraphs or disabled schema validation.
Extraction failures are diagnostics, never a "chapter text was not extracted"
sentence inserted into the book.

XML syntax, unique IDs, image binaries and internal FB2 targets are always
checked. The validation option additionally checks the shipped FB2 XSD through
MSXML 6. `FictionBook.xsd` and its included schemas must be present in the
runtime root, one directory above `Plugins/ImportEPUB.dll`; missing schemas and
invalid output cause conversion failure. `--no-validate` disables only XSD
validation, not basic XML/reference integrity checks.

The import log records found/imported/lost note references and image
references/used/imported/skipped resources. A successful partial import returns
warnings through the existing BSTR diagnostic output of the C ABI, the COM v2
host's `import-warning` message, and the legacy dialog. The batch tool includes
these diagnostics in `OK_WITH_WARNINGS` reports. Consumers of the C ABI must
release both returned BSTRs even on success. No interfaces, method signatures,
GUIDs or structure layouts have changed.

### Short content regression

After building Release Win32 `ImportEPUB` and `ImportEPUBBatch` with strict
first-party warnings, run:

```powershell
./tools/tests/test-import-epub-fidelity.ps1
./tools/tests/test-import-epub-v2-runtime.ps1
./tools/tests/test-import-epub-batch-dll-abi.ps1 -DllPath out/Release/Plugins/ImportEPUB.dll
```

The Python test creates small synthetic EPUB 2/3 archives and invokes the real
batch/DLL converter. Assertions cover actual text, note targets, hierarchy,
binary bytes, warning diagnostics, options and all SVG modes. A separate .NET
XML reader validates every successful result against the repository's FB2 XSD.
The test requires Windows, Python 3 and Windows PowerShell; it does not require
third-party Python packages and does not run a book corpus.
Additional cases cover anchors inside multi-paragraph list/semantic notes,
neighboring notes, note images, embedded notes and backlink options; NCX/nav
cross-file and three-level hierarchy, mixed parent text, title-only leaves,
absent/incomplete/conflicting navigation, repeated fragment targets and
excluded page lists; symbolic/letter markers, ordinary lookalike links,
ambiguous note boundaries and disabled import options. Existing image, SVG,
encoding, traversal and ID regressions remain in the same short test contour.

Synthetic results do not establish that particular e-paper source books are
fixed. Those books and Windows 7 SP1 require direct verification in the
corresponding environment.

The dialog has five native tabs: content, images, links and notes, cleanup,
and diagnostics. Defaults, Import and Cancel remain accessible below the pages.
Defaults affect every tab; switching tabs preserves edits. Import confirms and
saves the parameters; Cancel restores the original parameters. The same rules
apply when opening settings from the file picker. A save error keeps the dialog
and its edited values open. Ctrl+Tab and Ctrl+Shift+Tab switch pages; Tab and
Shift+Tab move focus among visible controls. The window position and size are
remembered independently of confirmation; see [settings storage](README.md#где-сохраняются-настройки-dll).

## SVG

`ImportEPUBLunaSVG.dll` is an optional helper dynamically loaded from the plug-in directory or beside the batch utility. Its pinned LunaSVG/PlutoVG source is in `third_party/lunasvg`; build products go to `build/lib/lunasvg`. SVG can be kept, rasterized to PNG/JPEG, or skipped. If the helper is unavailable or rendering fails, the importer retains its visible fallback behavior.

## Batch utility

Run `ImportEPUBBatch.exe --help` for the exact interface. It supports one EPUB or `--batch` directories, recursive and tree-preserving output, overwrite or resume behavior, CSV/HTML reports, profiles (`full`, `text`, `minimal`), and individual switches for covers, images, notes, tables, lists, links, semantic CSS, service pages and validation. `--svg keep|png|jpg|skip` selects SVG mode.

Manual corpus helpers live in `tools/tests/manual/import-epub`. They accept explicit input paths and write default results below `out/manual/import-epub`. The regression checker verifies failed rows, binary images, SVG conversion, note links, tables and duplicate IDs from a batch CSV report.

## Limits and diagnostics

DRM, fixed-layout EPUB, JavaScript, MathML, audio and video are not imported. Visual CSS is not reproduced wholesale; semantic classes and `epub:type` are used where applicable. ZIP extraction uses the Windows ZIP provider and guards against traversal and reparse-point escapes. MSXML is consumed through the SDK header, so static analysis does not need pre-generated type-library files.
