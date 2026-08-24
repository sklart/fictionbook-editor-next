# Third-party license files in release packages

`tools/build/stage-core.ps1` creates the complete license layout in Core;
Portable and Installer consume that Core unchanged. The files are:

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
| `Dictionary-en_US.txt` | English Speller Database / SCOWL 2026.02.25 release notice and license |
| `Dictionary-de_DE.txt` | igerman98 / frami German Hunspell Dictionary 20161207+frami20170109 GPL-2.0 OR GPL-3.0 license |
| `Dictionary-ru_RU.txt` | Goudron Russian Hunspell Dictionary 1.0.8 MPL-2.0 license |
| `Dictionary-uk_UA.txt` | VESUM / dict_uk 6.8.0 Hunspell-distribution MPL-1.1 notice |

PCRE2's `deps/sljit/LICENSE` must additionally be included when the PCRE2 JIT
is enabled in a release build.  The current FBE build does not enable it.
