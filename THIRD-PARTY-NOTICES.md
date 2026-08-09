# Third-party notices

This register is generated from the source trees and build inputs currently
used by FictionBook Editor Next.  “Bundled” means that the component's code or
binary is included in a normal application release; “build-time” means that it
is needed to build the project but is not shipped as a separate component.

| Component | Version in this tree | License | Use | Upstream |
| --- | --- | --- | --- | --- |
| Scintilla | 5.6.5 | Scintilla License (permissive) | XML source editor; `Scintilla.dll` is bundled | <https://www.scintilla.org/> |
| Lexilla | 5.5.2 | Scintilla License (permissive) | Lexers for Scintilla; `Lexilla.dll` is bundled | <https://www.scintilla.org/Lexilla.html> |
| PCRE2 | 10.47 | BSD-3-Clause with PCRE2 exception | Regular-expression backend, statically linked into FBE | <https://github.com/PCRE2Project/pcre2> |
| Hunspell | 1.7.3 | MPL-1.1 OR GPL-2.0-or-later OR LGPL-2.1-or-later | Spell checking, statically linked into FBE | <https://github.com/hunspell/hunspell> |
| libwebp | 1.6.0 | BSD-3-Clause | Static WebP decoder linked into FBE | <https://chromium.googlesource.com/webm/libwebp> |
| OpenJPEG | 2.5.4 | BSD-2-Clause | JPEG 2000 decoder build input for FBE | <https://github.com/uclouvain/openjpeg> |
| Windows Template Library (WTL) | 10.01 | MS-PL | UI and Windows shell components, compiled into FBE, FBV, and FBShell | <https://sourceforge.net/projects/wtl/> |
| LunaSVG | 3.5.0 | MIT | EPUB import SVG renderer, statically linked into `ImportEPUBLunaSVG.dll` | <https://github.com/sammycage/lunasvg> |
| PlutoVG | 1.3.1 | MIT | LunaSVG raster backend, statically linked into `ImportEPUBLunaSVG.dll` | <https://github.com/sammycage/plutovg> |
| UAC plugin helper | source snapshot (no version metadata) | zlib/libpng license | Build-time UAC support for the NSIS installer | <https://nsis.sourceforge.io/UAC_plug-in> |

## License texts in source and releases

The repository preserves upstream license material alongside source trees:

- `third_party/scintilla/License.txt` and `third_party/lexilla/License.txt`;
- `third_party/pcre2/LICENCE.md` (and `third_party/pcre2/deps/sljit/LICENSE`
  if JIT support is enabled);
- `third_party/hunspell/license.hunspell` and `third_party/hunspell/license.myspell`;
- `third_party/libwebp/COPYING` and `third_party/openjpeg/LICENSE`;
- `src/import-epub/thirdparty/lunasvg/LICENSE` and
  `src/import-epub/thirdparty/lunasvg/plutovg/LICENSE`;
- `third_party/uac/License.txt`.

`THIRD-PARTY-LICENSES/README.md` documents the release-package layout.  WTL
headers carry the required Microsoft Public License notice; its canonical text
is available at <https://opensource.org/license/ms-pl.html>.

The project is GPL-3.0-or-later.  This register is informational and does not
replace the license terms shipped with any component.
