# Third-party license files in release packages

`tools/build/package-portable.ps1` copies the applicable upstream license
texts into this directory when a portable package is built.  The files are:

| File | Source in this repository |
| --- | --- |
| `Scintilla-Lexilla.txt` | `third_party/scintilla/License.txt` |
| `PCRE2.txt` | `third_party/pcre2/LICENCE.md` |
| `Hunspell.txt` | `third_party/hunspell/license.hunspell` |
| `Hunspell-MySpell.txt` | `third_party/hunspell/license.myspell` |
| `libwebp.txt` | `third_party/libwebp/COPYING` |
| `OpenJPEG.txt` | `third_party/openjpeg/LICENSE` |
| `libheif.txt` | `third_party/libheif/COPYING` |
| `libde265.txt` | `third_party/libde265/COPYING` |
| `libaom.txt` | `third_party/aom/LICENSE` |
| `libaom-PATENTS.txt` | `third_party/aom/PATENTS` |
| `LunaSVG.txt` | `src/import-epub/thirdparty/lunasvg/LICENSE` |
| `PlutoVG.txt` | `src/import-epub/thirdparty/lunasvg/plutovg/LICENSE` |
| `Theme-palettes-MIT.txt` | adapted XML palette attribution and MIT text |
| `UAC.txt` | `third_party/uac/License.txt` |
| `WTL-MS-PL.txt` | license identifier and canonical source for WTL |

PCRE2's `deps/sljit/LICENSE` must additionally be included when the PCRE2 JIT
is enabled in a release build.  The current FBE build does not enable it.
