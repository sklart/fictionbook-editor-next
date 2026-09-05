# Project Root

The repository root is the directory containing `FBE.sln`. On this automation
host it is:

D:\Download\FBeditor

Use this directory as the workspace root for this task.
Never switch to C:\Users\Sklyarov\Documents\FBeditor.

Build scripts must resolve the repository root from their own location; a
developer checkout or CI must not depend on this absolute path.

# Structural boundaries

- Keep top-level `src`, `runtime`, `localization`, `packaging`, `third_party`,
  `tools`, `docs` and the public PowerShell entry points.
- `src/contracts/fbe.idl` is the plug-in COM contract. Its MIDL outputs belong
  only in `build/generated/<Platform>/<Configuration>/fbe-api`; do not edit
  interfaces, GUIDs, vtable order, calling conventions or ownership rules in a
  structural change.
- `src/common` is only for code with real consumers outside one product.
  Windows/COM code may remain there when its dependencies are explicit; do not
  present it as portable core.
- Code under `src/fbe` is editor-only. Keep document ownership, HWND/MSHTML,
  selection, undo/redo and recovery semantics in the editor unless a smaller
  operation has an explicit tested boundary.
- Do not import `tools/msbuild/FBE.Common.props` into vendored or generated
  projects. The supported release toolchain remains v143 / VC Tools 14.44.
- `third_party` is pinned vendor content. Do not change paths, versions or
  generate broad formatting changes there without a dedicated task.

# Generated outputs and verification

- `build` and `out` are generated. Do not add tracked configuration there.
- Do not commit generated source artifacts from local MIDL, test or staging
  runs unless an explicit tracked-generated policy and freshness check exists.
- Preserve existing package provenance and independent manifest checks; do not
  replace behavior tests with text searches.
- Before a structural commit, run the affected native/behavioral tests and the
  corresponding FAST contracts. Use `tools/build/verify-release.ps1` as the
  compatible public release gate; `-FullValidation` is the broader GUI and
  production contour.
