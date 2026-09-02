# Third-party notices

This register is generated from the source trees and build inputs currently
used by FictionBook Editor Next.  “Bundled” means that the component's code or
binary is included in a normal application release; “build-time” means that it
is needed to build the project but is not shipped as a separate component.

| Component | Version in this tree | License | Use | Upstream |
| --- | --- | --- | --- | --- |
| Scintilla | 5.6.6 | Scintilla License (permissive) | XML source editor; `Scintilla.dll` is bundled | <https://www.scintilla.org/> |
| Lexilla | 5.5.3 | Scintilla License (permissive) | Lexers for Scintilla; `Lexilla.dll` is bundled | <https://www.scintilla.org/Lexilla.html> |
| PCRE2 | 10.48 | BSD-3-Clause with PCRE2 exception | Regular-expression backend, statically linked into FBE | <https://github.com/PCRE2Project/pcre2> |
| Hunspell | 1.7.3 | MPL-1.1 OR GPL-2.0-or-later OR LGPL-2.1-or-later | Spell checking, statically linked into FBE | <https://github.com/hunspell/hunspell> |
| English Speller Database / SCOWL | 2026.02.25 | SCOWL permissive license; BSD license for the affix file | Bundled English (`en_US`) spell-check dictionary | <https://github.com/en-wl/wordlist> |
| igerman98 / frami German Hunspell Dictionary | 20161207+frami20170109 | GPL-2.0 OR GPL-3.0 | Bundled German (`de_DE`) spell-check dictionary | <https://github.com/LibreOffice/dictionaries> |
| Goudron Russian Hunspell Dictionary | 1.0.8 | MPL-2.0 | Bundled Russian (`ru_RU`) spell-check dictionary | <https://github.com/Goudron/ru-spelling-dictionary> |
| VESUM / dict_uk | 6.8.5 | MPL-1.1 (Hunspell distribution) | Bundled Ukrainian (`uk_UA`) spell-check dictionary | <https://github.com/brown-uk/dict_uk> |
| libwebp | 1.6.0 | BSD-3-Clause | Static WebP decoder linked into FBE | <https://chromium.googlesource.com/webm/libwebp> |
| OpenJPEG | 2.5.4 | BSD-2-Clause | JPEG 2000 decoder build input for FBE | <https://github.com/uclouvain/openjpeg> |
| libheif | 1.23.3 | LGPL-2.1-or-later | Static ISO-BMFF/HEIF container reader in FBE; AVIF/HEIC/HEIF decoding only | <https://github.com/strukturag/libheif> |
| libde265 | 1.1.1 | LGPL-2.1-or-later | Static HEVC decoder used by bundled libheif | <https://github.com/strukturag/libde265> |
| libaom | 3.15.0 | BSD-2-Clause and Alliance for Open Media Patent License 1.0 | Static AV1 decoder used by bundled libheif | <https://aomedia.googlesource.com/aom> |
| Windows Template Library (WTL) | 10.01 | MS-PL | UI and Windows shell components, compiled into FBE, FBV, and FBShell | <https://sourceforge.net/projects/wtl/> |
| LunaSVG | 3.5.0 | MIT | EPUB import SVG renderer, statically linked into `ImportEPUBLunaSVG.dll` | <https://github.com/sammycage/lunasvg> |
| PlutoVG | 1.3.3 | MIT | LunaSVG raster backend, statically linked into `ImportEPUBLunaSVG.dll` | <https://github.com/sammycage/plutovg> |
| UAC plugin helper | source snapshot (no version metadata) | zlib/libpng license | Build-time UAC support for the NSIS installer | <https://nsis.sourceforge.io/UAC_plug-in> |

## Bundled XML source-editor palettes

The 21 JSON palettes in `runtime/Themes` adapt the names and base colours of
the following upstream projects to FBE Next's own 23 logical XML-editor roles;
they are not copies of the upstream extensions' source code.  The palette
metadata and its adapted colour values are distributed under MIT terms.  The
complete text is shipped as `THIRD-PARTY-LICENSES/Theme-palettes-MIT.txt`.

| Upstream | Revision used for attribution | Bundled palettes |
| --- | --- | --- |
| Microsoft VS Code (<https://github.com/microsoft/vscode>) | main | Dark+, Light Modern, Quiet Light |
| Solarized (<https://github.com/altercation/solarized>) | master | Solarized Light, Solarized Dark |
| Gruvbox (<https://github.com/morhetz/gruvbox>) | master | Gruvbox Light Medium, Gruvbox Dark Medium |
| Everforest (<https://github.com/sainnhe/everforest>) | master | Everforest Light Medium, Everforest Dark Medium |
| Flexoki (<https://github.com/kepano/flexoki>) | main | Flexoki Light, Flexoki Dark |
| Dracula Theme (<https://github.com/dracula/dracula-theme>) | master | Dracula |
| Nord (<https://github.com/nordtheme/nord>) | develop | Nord |
| Catppuccin for VS Code (<https://github.com/catppuccin/vscode>) | main | Catppuccin Latte, Catppuccin Mocha |
| Tokyo Night for VS Code (<https://github.com/enkia/tokyo-night-vscode-theme>) | master | Tokyo Night Storm |
| Ayu for VS Code (<https://github.com/ayu-theme/ayu-vscode>) | master | Ayu Mirage |
| Rosé Pine for VS Code (<https://github.com/rose-pine/vscode>) | main | Rosé Pine Moon |
| Night Owl (<https://github.com/sdras/night-owl-vscode-theme>) | main | Night Owl |
| GitHub Theme for VS Code (<https://github.com/github/github-vscode-theme>) | main | GitHub Light Default, GitHub Dark Default |

## License texts in source and releases

The repository preserves upstream license material alongside source trees:

- `third_party/scintilla/License.txt` and `third_party/lexilla/License.txt`;
- `third_party/pcre2/LICENCE.md` (and `third_party/pcre2/deps/sljit/LICENSE`
  if JIT support is enabled);
- `third_party/hunspell/license.hunspell` and `third_party/hunspell/license.myspell`;
- `third_party/libwebp/COPYING` and `third_party/openjpeg/LICENSE`;
- `third_party/libheif/COPYING`, `third_party/libde265/COPYING`, and
  `third_party/aom/LICENSE` plus `third_party/aom/PATENTS`;
- `src/import-epub/thirdparty/lunasvg/LICENSE` and
  `src/import-epub/thirdparty/lunasvg/plutovg/LICENSE`;
- `third_party/uac/License.txt`.

`THIRD-PARTY-LICENSES/README.md` documents the release-package layout.  WTL
headers carry the required Microsoft Public License notice; its canonical text
is available at <https://opensource.org/license/ms-pl.html>.

The project is GPL-3.0-or-later.  This register is informational and does not
replace the license terms shipped with any component.
