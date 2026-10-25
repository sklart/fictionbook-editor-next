# Performance Phase C2: BinaryStore foundation

## Backward compatibility contract

`BinaryStore` — внутренняя реализация FBE Next. Существующие скрипты не должны
знать о его наличии. Старые способы чтения и изменения `<binary>`, включая
`binary.base64data`, сохраняют прежнюю наблюдаемую семантику. Existing scripting
API, DOM representation, save format and Undo semantics remain compatible;
scripts require no new API and must not access the store directly.

`FB::Doc` owns a native `FbeBinary::BinaryStore`. The hash catalog holds IDs,
MIME, byte size, image dimensions, generation/state and COM identity of binary
DOM nodes. It does not retain SAFEARRAYs, base64 strings, image decoder objects
or a second document-sized byte buffer. Closing/replacing the document releases
the catalog; loading HTML and `PutBinaries` reset it.

`GetBinaryInfo`, `GenerateUniqueBinaryId` and native `Doc::GetBinary` use the
index. `apiAddBinary` uses a private, document-bound unique-ID lookup, with the
old JavaScript scan as compatibility fallback. The private dispatch hooks are
excluded from the public scripting method catalog and do not change the IDL,
API version, DISPIDs or existing script signatures. A helper carries the owning
document GUID, which is resolved against live document owners; it does not
retain a borrowed document pointer or depend on the currently selected book.

Initial registration reuses size/dimensions already calculated for binary UI
controls. Reconciliation reads only IDs, MIME and DOM identity; size and image
dimensions are then calculated lazily for the requested entry. Successful and
unsuccessful image decode attempts are cached. Repeated queries of a stable
entry do not enumerate `binobj` or retrieve/copy/decode `base64data` again.
Built-in image metadata refresh uses cached `GetBinaryInfo`; MIME control refresh
reuses cached size and already known dimensions.

Binary API edits update the affected entry using the existing edit/compensation
path. Validated replacement dimensions are reused. Rename uses the existing
reference-aware JS algorithm and moves the catalog key without reading bytes.
The existing rename validator still checks the DOM while the ID input is being
edited, because its new value can precede the index update. No second reference
or Undo algorithm is introduced. Undo/Redo invalidate the catalog for safe
reconciliation, including MSHTML-owned history and compensation after failures.

`RunScript` invalidates on entry and on every exit, including script errors.
During an arbitrary script, every native metadata/index request reconciles
conservatively: a script may modify expandos or replace handlers between calls.
Consequently the stable-generation cache benchmark does not claim a cache hit
rate for arbitrary script execution. Correctness takes priority in that scope.
After return, the first native request reconciles, and stable subsequent requests
reuse the new metadata. Reconciliation never trusts legacy-edited size/dimensions
UI controls. Duplicate legacy IDs retain the old first-DOM-entry lookup rule;
removing that entry makes the next duplicate visible after reconciliation.

Production Save still calls `GetBinaries` and reads the actual DOM bytes. A
metadata reconciliation failure leaves that established serialization fallback
available. Store contents are never written over script-modified `base64data`.
Byte ownership, lazy materialization and a store-backed serializer belong to C3.

The existing diagnostic trace emits numeric aggregate `binary_store_entries`,
`binary_store_rebuilds`, `binary_store_reconciliations`, `binary_lookup_count`,
`binary_lookup_misses`, `binary_size_calculations`, `binary_size_cache_hits`,
`binary_dimensions_calculations`, `binary_dimensions_cache_hits` and
`binary_legacy_invalidations`. Calculation counters describe work performed by
the store's lazy metadata path; initial metadata already computed by the UI is
registered without recalculation. Counters are inactive without diagnostic
trace. Privacy filtering accepts these exact numeric fields; IDs, bytes, base64
and user paths are not emitted. `binary_store_bytes` is deliberately absent.

## Targeted verification

Run from the repository root after an incremental Release Win32 build, with the
current `runtime/main.js` staged beside `FBE.exe`:

```powershell
./tools/tests/test-fbe-binary-store-contract.ps1
./tools/tests/test-fbe-binary-store-runtime.ps1
./tools/tests/test-fbe-main-js-binary-behavior.ps1
./tools/tests/test-fbe-binary-serialization.ps1
./tools/tests/test-fbe-binary-production-roundtrip.ps1
./tools/tests/test-fbe-scripting-stage3b.ps1
```

The native unit covers hash lookup, document separation, replace/remove,
rename conflicts/generation, reset and trace/privacy boundaries. The runtime
fixture contains 100 binaries of 64 KiB, including PNG/JPEG. It checks initial
registration before rebuild, DOM/store ID/MIME/size equality, 500 repeated size
queries, 100 queries per image, reference-aware rename, Add/Replace/Remove with
Undo/Redo, direct legacy data/ID/MIME writes, exception cleanup and Save/Reopen.
PowerShell independently compares every persisted binary's decoded bytes.
Reports, binary SHA-256, timing and peak process memory are generated under
`out/tests/binary-store`; elapsed time and memory are diagnostic, not CI limits.

Earlier C2 checkpoint: full release verification was **DEFERRED**. The current
backward-compatibility acceptance explicitly requires FullValidation; results follow below.
Native GUI smoke: **PENDING MANUAL VERIFICATION**.

## Acceptance, 2026-10-25

Performance Historical C2 checkpoint: targeted checks passed; this was not full acceptance.
Incremental Release Win32 with `/W4 /WX`: **PASS**, zero compiler warnings/errors.
All seven targeted checks listed above plus diagnostic privacy: **PASS** (six
commands in the list and `test-diagnostic-trace-privacy.ps1`). Binary API 1.5,
legacy `base64data`, Undo/Redo, and production Save/Reopen bytes: **PASS**.

The measured stable-generation contour issued 100 PNG queries, 100 JPEG queries
and 500 queries of one non-image binary: one reconciliation, three size
calculations, two image dimension calculations, 697 size cache hits and 198
dimension cache hits. Elapsed time was 15 ms; process peak private commit was
292,810,752 bytes and peak working set was 245,481,472 bytes. These are diagnostic
measurements of the complete editor process, not a promised memory reduction or
speedup against a baseline.

Verified `FBE.exe` SHA-256:
`0FFB239B1CCC6DB3A06965406F5404C0532A9902011130BA017F58AA40CD0B5A`.
Verified source/staged `main.js` SHA-256:
`1B3F464A30C687BE26674D96A23916F64ACF31F2D8FDD042615CB8BF1AAF4B53`.

During development, the owner lookup through MSHTML document interfaces proved
unreliable and was replaced by the explicit GUID binding. The new intentional
script-error regression initially waited on the real legacy message box; its
test-only closer now handles that expected dialog. An earlier attempt to read
peak memory after process exit returned null; the final fixture records native
process counters before exit. Final results use the corrected binary and current
staged JavaScript. Earlier failure evidence is retained under
`out/tests/binary-store/failure-evidence` and `targeted-initial-results.json`;
the four generated failure runtimes in TEMP were removed after archiving.

## Backward-compatibility acceptance continuation (2026-10-25)

The earlier checkpoint above is historical. The broad local normal contour completed before the final ABI correction;
the required FullValidation is FAIL because current external OLE evidence is unavailable.
Targeted sanitizer verification is recorded separately below.
At the verification checkpoint, no push was authorized. The user subsequently explicitly authorized committing and pushing these C2 changes; this does not close the outstanding release gates.

`test-binary-store-scripting-compatibility.ps1` exercises 46 production scenarios
through old DOM/JS paths, public API 1.5 and the ordinary document lifecycle. It
never calls a BinaryStore test API. Independent byte comparisons and SHA-256
checks cover PNG/JPEG, arbitrary/small payloads, equal-size/equal-dimension but
different bytes, raw A/B/C/D replacements, a separate immediately following
reader script, existing API edit Undo/Redo, Save/Open with XSD, shared references,
legacy add/delete/rename, duplicate content under independent IDs, Visual/Source
and Source edits, close/new/open with reused IDs, malformed/truncated data,
unknown MIME, zero-length mutation, a large image and missing references.
Read-only observations also compare dirty state and actual OLE undo/redo entries.
Raw expando writes retain their old semantics; they do not acquire new Undo or
reference-update behavior.

A real compatibility defect was corrected: duplicate DOM IDs previously made C2
reject the complete catalog. Lookup now preserves the old first-DOM-entry rule,
and removing the first entry exposes the next one. The suite checks both states.

The six bundled scripts run without source modifications; source/staged hashes
are recorded in `out/tests/binary-store/compatibility/corpus-results.json`:

| Bundled script | Exercised operation |
| --- | --- |
| `10_Статистика/03_Статистика документа.js` | Legacy binary size/statistics |
| `05_Иллюстрации/04_Сохранить вложения на накопитель.js` | Legacy SaveBinary; exported byte hashes |
| `05_Иллюстрации/03_Обновить иллюстрации.js` | Shared image refresh |
| `05_Иллюстрации/20_Обновить иллюстрации (версия Александра Ка).js` | Legacy image dimensions/metadata |
| `05_Иллюстрации/01_Проверить иллюстрации.js` | Image/binary reference inspection |
| `05_Иллюстрации/07_Удалить неиспользуемые вложения....js` | Direct DOM deletion of unused binaries |

Automated dialog responses belong only to the disposable test process. Lifecycle
commands save named modified books and explicitly discard the empty unsaved New
fixture; known corpus confirmations accept Yes and informational dialogs close.
Unexpected J900 script diagnostics fail the suite. A debugger established that
putting the new probes inside the large legacy scenario handler exhausted the
JScript stack budget; separate non-inlined test handlers remove that extra frame.
No production stack-size increase or scripting-error suppression was introduced.

### Existing empty-binary load limitation

An initial empty `<binary>` becomes MSXML VT_NULL, which the old GetBinarySize
call does not accept. `main.js:1567` rethrows the original load exception; it is
not the originating operation. The identical fixture reproduced XH140
`0x80020005` on both the preserved pre-C2 production stage and C2. Evidence:
`out/tests/binary-store/empty-load-baseline/summary.json` and both traces.
Pre-C2 FBE SHA-256 is
`5C30209AE94A7DE39D7E7D4D607A8EDCC2DDB0CEDC2FC965B9A91CE206FD555A`;
the common fixture SHA-256 is
`2194CE948A1B7542243AEEF019C0C2EFA73FB08252998B5AEFD3CB2FE39714E8`.
This is a preserved unsupported load case, not a successful empty-load claim.
The zero-length raw mutation case separately verifies observable empty bytes,
allowed legacy metadata refusal and subsequent recovery to a valid payload.

### Performance and memory evidence

Stable metadata queries: 100 PNG + 100 JPEG requests, two actual decodes and
198 dimension hits. After a raw mutation: 100 requests, exactly one new decode
and 99 hits. Size lookup also covers 500 repeated non-image queries. Native
counters and independent saved bytes are checked by the runtime suite.

There is no content-hash cache: SHA-256 is computed independently from the
observable bytes for verification. Hash-cache invalidation is therefore N/A.
The store holds metadata and DOM identities; it owns no permanent payload copy.
The contract checks the implementation as well as the header and public API
catalog/IDL boundary.

The preserved pre-C2 stage and intermediate C2 binary were observed sequentially
with the identical 100 x 64 KiB fixture (6,553,600 binary bytes). After successful
D113 load completion, ten steady samples followed a three-second settle:

| Metric, bytes | Pre-C2 | C2 |
| --- | ---: | ---: |
| Peak working set | 139526144 | 138526720 |
| Steady working set, mean | 109605683 | 108724224 |
| Sampled peak private bytes | 97533952 | 107237376 |

These are observations without a hard threshold or a promised improvement.
Private bytes increased in this sample. Exact executable/runtime/fixture hashes
are in `out/tests/binary-store/memory-comparison/summary.json`; this measurement
is not substituted for final-binary regression evidence.

### Manual GUI checklist — PENDING MANUAL VERIFICATION

1. Open a real image-heavy book and inspect cover plus several inline images.
2. Switch Description, Visual and Source repeatedly and return to the same image.
3. Run a familiar user script that reads `base64data` and reports binary sizes.
4. Replace an image with different bytes but the same size/dimensions.
5. Replace PNG with JPEG and JPEG with PNG; inspect all shared references.
6. Undo and Redo supported image/API edits several times and inspect dirty state.
7. Add, rename and remove an attachment through the normal UI; inspect references.
8. Save, close and reopen; inspect cover, images and attachment inventory.
9. Edit a binary in Source, apply, then inspect Visual and save/reopen.
10. Export EPUB, DOCX and HTML with images and inspect their rendered contents.
11. Close the book, create/open another book reusing an ID, and inspect its bytes/images.
### Full gate limitation and local completion

The exact required command was invoked with no Skip options:
`tools/build/verify-release.ps1 -Configuration Release -FullValidation`.
Its final status is **FAIL**, not PASS. The mandatory external OLE receipt is for
commit `2bc8783aae7546f4a2d78d38dc1e67f25be8be30`, while HEAD is `a4145d8...`
and C2 changes are still uncommitted. The existing contract requires a disposable
GitHub-hosted logon and successful Build evidence. Earlier CI cannot prove this
dirty C2 binary. No receipt, environment identity or guard was changed to accept it.
The three local required matrices (file-close, scripting14, scripting15) passed.

All remaining local commands were executed from the actual verifier fragment,
without rerunning the successful long prefix. A later missing generated
`Lang/Shell/FBVVerbResources.dll` was prepared by the official
`build-fbv-verb-mui.ps1`, exactly as `create-release.ps1` requires; no source or
verifier fix was needed. The remaining PE/Win7/security/runtime-DLL checks then
completed, and the original final incomplete-OLE guard still rejected FULL.
Derivation and exact binary hash are in `full-resume-provenance.json`; logs are
`verify-release-full.log`, `verify-release-full.remaining-local.log` and
`verify-release-full.remaining-pe.log` under `out/tests/binary-store`.

The test-only OLE-parent Split probe retained its documented MSHTML diagnostic
**FAIL** (`docs/archive/undo/split-undo-mshtml-probe.md`). This was not changed or
reported as a product PASS; production Split Undo/Redo passed 16/16 separately.

Normal MSVC Release `/W4 /WX` passed with zero compiler/linker warnings. ASan built
with the same strict normal toolchain. Adding that optional strict flag to the
clang-cl diagnostic build made legacy `/Gm-` an unused-argument `-Werror` failure;
the supported existing UBSan build invocation without that optional flag passed.
This is a toolchain-option incompatibility, not a runtime UB finding. Diagnostic
build policies passed (440 Address TU/mode checks; 220 Undefined translation
units). Both reject an instrumented PE even without diagnostic marker/runtime
files. Ordinary Release was accepted by the production guard. The single exact
ATL SDK symbol exception remains signed-integer-overflow only and unchanged.
### Confirmed C2 alignment finding and regression

The initial UBSan targeted run produced seven failing FBE scenarios with one
common diagnostic at `BinaryDocumentStore.inl`'s call to `Register`:
`constructor call on misaligned address ... BinaryMetadata ... requires 8 byte alignment`.
This is the first-party C2 metadata API / Win32 calling-convention boundary,
not the ATL signed-overflow exception. The x86 by-value argument was only
four-byte aligned. A standalone native unit using the same clang-cl/UBSan flags
reproduced it without the ATL window path. Evidence is in `abi-negative-control`.

`BinaryStore::Register` now takes `const BinaryMetadata&` and copies metadata
into its own entry, where the new generation is assigned. It retains no caller
reference and introduces no payload ownership or public COM/API change. The
instrumented standalone unit then exited 0 with no diagnostics (`abi-regression`).
The native regression locks this reference-based ABI signature at compile time
and checks that registration leaves the input metadata/generation unchanged.
The full same selected ten UBSan product scenarios passed after the rebuild.
No alignment, undefined or signed-overflow checks were disabled.

Microsoft documents that a calling convention controls the alignment of
by-value parameters and that the x86 stack does not automatically guarantee
alignment beyond four bytes:
[MSVC alignment rules](https://learn.microsoft.com/en-us/cpp/cpp/align-cpp),
[x86 stack alignment notes](https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions#conflicts-with-the-x86-compiler).
This explains the observed boundary; the failing and passing standalone controls
establish the behavior of the actual supported LLVM x86 toolchain.

The old broad local/full evidence is explicitly historical after this ABI fix.
Final current-binary evidence is the rebuilt strict Release affected contour
and rebuilt targeted Address/Undefined contours recorded in `final-acceptance`.
The full release acceptance remains FAIL/incomplete pending current external
OLE CI evidence; no earlier binary's full results are promoted to a final PASS.
### Final current-binary verification

| Check | Result |
| --- | --- |
| Release Win32 `/W4 /WX` | PASS, zero compiler/linker warnings |
| Corrected Release affected contour | PASS 17/17 |
| Legacy compatibility | PASS 46/46; bundled corpus 6/6 |
| Corrected ASan build / infrastructure self-test / targeted | PASS / PASS / PASS 10/10 |
| Corrected UBSan build / infrastructure self-test / targeted | PASS / PASS / PASS 10/10 |
| Current PE / Win7 / security / protected runtime inputs | PASS |
| Production payload guard, including markerless diagnostic PE rejection | PASS |
| Required `verify-release -FullValidation` | FAIL: stale mandatory external OLE evidence |
| Manual real-book GUI smoke | PENDING MANUAL VERIFICATION |

Final FBE SHA-256 values:

- Release: `23877BC1DFCF167EF104F4A9B6093FDD4F379F17E967CB81F0B739D63DDBC56B`.
- Address: `E338A128903C9588F47FA41283703E9903DFF7AF52D8CFC6F80C6B7AE7CBEFD9`.
- Undefined: `EDDEAE3CCC0434A31073D924670B5320279EFBC112760ED18ECCED30BE3FCAB5`.
- Source/staged `main.js`: `1B3F464A30C687BE26674D96A23916F64ACF31F2D8FDD042615CB8BF1AAF4B53`.

The corrected Release contour contains native/ABI and two privacy checks, the
same ten binary/image/EPUB/DOCX/HTML catalog cases, and all four Stage3B
portable/installed x FB2/FBD variants. Both diagnostic contours select those ten
catalog entries; `selectionComplete=false` is intentional. Neither result is a
complete ASan/UBSan FULL claim. There are no remaining findings in those product
runs. One initial C2 alignment diagnostic (seven scenarios, one root cause) was
fixed; the duplicate-ID behavior defect was fixed separately.

The long normal FullValidation attempt and its remaining local commands were
completed before the final ABI correction and remain historical evidence for
that executable. After the correction, the affected Release and both sanitizer
contours were rebuilt/rerun with the exact final hashes above; PE/Win7/security
checks were also repeated. The complete long FULL prefix was not repeated merely
to reach the same unavailable external prerequisite. It is not a final-binary
FULL PASS. C2 release acceptance remains incomplete until
current external OLE evidence and the mandatory corrected-revision FULL gate
are completed. No push or C2 commit had been performed at that verification checkpoint. Publication was subsequently explicitly authorized by the user, with these gates still pending.

Current reports are indexed by `out/tests/binary-store/final-acceptance/summary.json`
and mirrored to `out/tests/binary-store/acceptance-summary.json`. Earlier snapshots
are preserved as historical reports. Failed disposable runtimes created by this
continuation were archived and removed (twelve owned `fr-*` directories); the
explicitly retained verifier probe artifacts remain available.
## C2 automatic closure: current-revision OLE and FullValidation (2026-10-25)

This entry supersedes the earlier automatic-gate status without rewriting the
historical attempts above. The user authorized moving from the original C2
commit `9597db31d5058c6568a2c59edb4cc408f77a0463` to the minimal infrastructure
correction `3e44dc5f621819c5a4d50009ad367e24c8ac4fea`.

```yaml
FullValidation: PASS
OLE receipt: current revision
commit tested: 3e44dc5f621819c5a4d50009ad367e24c8ac4fea
manual GUI smoke: PENDING
```

[GitHub-hosted Build 40178218204](https://github.com/sklart/fictionbook-editor-next/actions/runs/40178218204)
completed successfully, including the real OLE Clipboard test under a disposable
user logon. Its original `required-ci-runtime-3e44dc5f621819c5a4d50009ad367e24c8ac4fea`
artifact was downloaded without editing the receipt. The production
`Assert-CiOleRuntimeEvidence.ps1` authenticated the run, exact commit, command
hash, repository and successful workflow status. Receipt SHA-256:
`9AC39BA987EC942FB96DDF92DEAD1DA09468F1C25B9C203ABC4A4EDDD7295F5A`.
The CI executable SHA-256 is
`E94B7B1EAE3353E05B1AB558601CB6A4C77688891F5FB25712CA1A4FDD1111C7`;
this is external CI evidence, separate from the local Release executable below.

The exact local command was run without Skip parameters or relaxed checks:

```powershell
pwsh ./tools/build/verify-release.ps1 -Configuration Release -FullValidation
```

The completed attempt exited 0 and printed the successful release verification
message for version 3.0.10. `out/tests/required-ci-runtime.json` reports all four
mandatory suites PASS and `complete: true`; OLE execution is explicitly external
CI, not local. Local Release FBE.exe remained SHA-256
`23877BC1DFCF167EF104F4A9B6093FDD4F379F17E967CB81F0B739D63DDBC56B`.
The full log is `out/tests/binary-store/closure-3e44dc5f/full-validation.log`,
SHA-256 `24625F22B75F5A87EA76C7D67128EA8618AE85EAD22BFA8EDCC85B86134CDAF0`.

Two infrastructure issues were classified and resolved without product changes:

- The first Build for `9597db31...` failed before OLE because the SettingsTooltips
  source contract still required NULL bounds after the earlier UBSan correction
  introduced an initialized RECT. Commit `3e44dc5f...` updates only that test to
  require the initialized RECT and its use. The affected contract passed; three
  negative controls rejected uninitialized bounds, NULL bounds and a missing
  tooltip style. No C2 or sanitizer suites were rerun separately for this fix.
- The first local FULL attempt failed at Runtime provenance because shared
  generated `out/editor-runtime` DLLs differed from the recorded Release hashes.
  The displaced DLLs were preserved under `closure-3e44dc5f/displaced-editor-runtime`.
  Exact Scintilla/Lexilla copies from the already verified `out/Release`, each
  matching the existing provenance, restored the generated profile. Neither
  manifest nor tracked dependency DLLs were changed. The complete original FULL
  command was then rerun from the beginning and passed.

No additional C2 product defects were found. The native/ABI, cache/runtime and
46/46 legacy compatibility plus 6/6 bundled-script checks ran as mandatory parts
of FullValidation, not as extra standalone evidence runs. ASan/UBSan were not
rerun for infrastructure-only changes. Existing diagnostic-only direct-MSHTML
and OLE-parent Split probes retained their documented FAIL observations;
production SplitContainer Undo/Redo passed 16/16. Those diagnostic observations
were not reclassified or hidden to obtain FULL PASS.

The two automatic closure criteria are now PASS on the tested revision. The
separate eleven-step real-book manual GUI checklist above remains pending.
This documentation-only follow-up does not change product or test sources and
does not claim an OLE receipt for its own subsequent documentation commit.
