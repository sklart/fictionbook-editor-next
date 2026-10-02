# Regular expression help — Source

## Engine

Source search uses **Scintilla** with `SCFIND_REGEXP` and `SCFIND_CXX11REGEX`. It is not PCRE2.

## Design versus Source

Design uses PCRE2-16, UTF/UCP, advanced PCRE2 and FBE-specific replacement. Source uses Scintilla C++11 regex and `SCI_REPLACETARGETRE`.

## Line-by-line search

Source matching is performed **line by line** through `MatchOnLines`. A regex cannot match through line boundaries. `\r\n`, `\n`, `\x0D` and `\x0A` do not enable a multi-line match in this path.

## Characters and classes

Use literal characters, escaping with `\`, classes `[abc]` and `[^abc]`, ranges `[a-z]`, and `\d`, `\D`, `\s`, `\S`, `\w`, `\W`.

## Anchors, groups and quantifiers

`^` and `$` are line anchors; `\b` and `\B` are word boundaries. Capturing groups `(…)`, alternatives `|`, `*`, `+`, `?`, `{n,m}` and backreference `\1` use the documented C++11 behavior.

## Replacement

Replacement is performed by `SCI_REPLACETARGETRE`. Keep Source replacements simple and verify them on a small selection first.

## Unsupported PCRE2 constructs

Source does not provide UCP, `\p{...}`, lookbehind, `\K`, `\G`, branch reset, PCRE2 verbs, PCRE2 subroutines, or FBE Design replacement formatting.

## FB2 and XML examples

| Goal | Source regex |
| --- | --- |
| Empty paragraph | `<p>[ \t]*</p>` |
| Adjacent empty-line on one Source line | `<empty-line/>[ \t]*<empty-line/>` |
| Empty metadata | `<(book-title|first-name|last-name)>[ \t]*</\1>` |
| Empty formatting | `<(strong|emphasis)>[ \t]*</\1>` |
| Nested formatting | `<(strong|emphasis)>[^<]*<(strong|emphasis)>` |
| Legacy HTML tag | `</?(font|span|div)\b[^>]*>` |
| Uppercase tag | `</?[A-Z][A-Z0-9:-]*\b[^>]*>` |
| Named entity | `&nbsp;` |
| Numeric entity | `&#[0-9]+;|&#x[0-9A-Fa-f]+;` |
| Empty id | `\bid=""` |
| External URL | `(https?|file):[^" ]+` |
| Empty href | `href=""` |
| Undefined reference | `(href|src)="#undefined"` |
| Bookmark | `href="#[^"]+"` |
| Word note marker | `_ftnref|_ednref` |
| Note link | `<a(?=[^>]*type="note")[^>]*>` |
| Possible note marker | `\[[0-9]+\]` |
| Undefined image | `<image[^>]+href="#undefined"` |
| windows-1251 declaration | `encoding="windows-1251"` |

The adjacent empty-line pattern intentionally permits only spaces and tabs; it does not search across Source lines.

## Frequent mistakes

Do not paste PCRE2-only syntax into Source. Do not assume `\s*` crosses lines. Inspect XML matches before replacing, especially ids, links and note markers.

## Limitations

No multi-line Source regex is implemented here. Changing that behavior would affect `^`/`$`, Replace All, performance and existing Scintilla regressions.
## Minimal pattern
```xml
<empty-line/>[ \t]*<empty-line/>
```
