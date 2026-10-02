# Regular expression help — Design

## Engine

Design search uses **PCRE2-16** with UTF mode always enabled. It searches the text model used by FBE Design; this is not the Scintilla Source engine.

## Unicode and UCP

UTF keeps Unicode text intact. Enable **Unicode (UCP)** when `\w`, `\b` and character properties must understand Unicode letters. Use `\p{L}`, `\p{N}` and `\P{...}` for Unicode properties.

Without UCP, `\b` and `\w` use the engine default word rules. With UCP, `\b(\p{L}+)\b` recognizes Cyrillic and other Unicode words.

## Characters and escaping

Ordinary characters match themselves. Escape metacharacters with `\`: `\\`, `\t`, `\r`, `\n`, `\R`, `\xNN` and `\x{...}`. A dot `.` matches one character (subject to options).

## Character classes

Use `[abc]`, `[^abc]`, ranges such as `[a-z]`, and `\d`, `\s`, `\w` with their uppercase negations. Prefer explicit Unicode properties when a book contains several scripts.

## Anchors and boundaries

`^` and `$` are line anchors; `\A` and `\z` anchor the whole subject. `\b` and `\B` are word-boundary positions, and `\G` continues from the previous match.

## Quantifiers

| Syntax | Meaning |
| --- | --- |
| `.*` | greedy: consumes as much as possible |
| `.*?` | lazy: consumes as little as possible |
| `.*+` | possessive: never backtracks |
| `{n,m}` | from n to m repetitions |

Use a lazy quantifier for the nearest closing delimiter. Use a possessive quantifier only when backtracking cannot produce a useful match.

## Groups and backreferences

Capturing groups use `(…)`, non-capturing groups `(?:…)`, named groups `(?<name>…)`, and atomic groups `(?>…)`. Refer back with `\1` or `\k<name>`.

```regex
(?<word>\p{L}+)\s+\k<word>
```

This finds a possible repeated word when UCP is enabled.

## Alternation and lookaround

Use `a|b` for alternatives. Positive lookahead `(?=…)` and negative lookahead `(?!…)` check following text. Lookbehind `(?<=…)` and `(?<!…)` check preceding text.

```regex
(?<=№)\s+(?=\d)
```

## Inline options

`(?i)`, `(?m)`, `(?s)` and `(?x)` enable case-insensitive, multiline, dotall and extended modes. Scope them as `(?i:word)` when possible.

## Advanced PCRE2

Conditionals, branch reset `(?|…)`, numeric and named subroutines `(?1)` / `(?&name)`, `\K`, and `(*SKIP)(*FAIL)` are available.

```regex
"[^"]*"(*SKIP)(*FAIL)|\bword\b
```

The first alternative skips quoted text before matching `word`.

## Replacement in FBE

This is **FBE replacement grammar**, not `pcre2_substitute`. Use `$0` or `\0` for the whole match, `$1`…`$9` or `\1`…`\9` for groups, and `$+` or `\+` for the last group. `\U`, `\L`, `\T`, `\Q`, `\S` and `\E` change FBE formatting/case behavior.

Before → pattern/replacement → after:

```regex
Find:    (\d+)\s*[-–—]\s*(\d+)
Replace: $1–$2
```

`12 - 15` becomes `12–15`.

## Practical book examples

- Multiple spaces: find `[ \t]{2,}`, replace with one space.
- Space before punctuation: find `[ \t]+([,;:!?])`, replace with `$1`.
- Possible repeated word: `\b(\p{L}+)\s+\1\b` with UCP; review each result.
- Mixed Cyrillic/Latin: `(?=[^\r\n]*[А-Яа-яЁё])(?=[^\r\n]*[A-Za-z])[^\r\n]+` is suspicious and requires review.
- Punctuation inside a word: find `\p{L}[,;:]\p{L}`; this is a possible OCR error.
- Lowercase to uppercase OCR: find `\p{Ll}\p{Lu}` and inspect context.
- Numeric range: `(\d+)\s*[-–—]\s*(\d+)` → `$1–$2`.
- Initials: `\b([A-ZА-ЯЁ])\.\s*([A-ZА-ЯЁ])\.` → `$1. $2.`.
- Straight quotes: find `"([^"]+)"`; choose replacement policy manually.
- Three dots: `\.\.\.` → `…` when editorial policy allows it.
- Hidden Unicode: `[\x{00AD}\x{200B}-\x{200D}]` finds suspicious invisible characters.
- Possible missing period: `\p{Ll}\s*\n\s*\p{Lu}` requires manual review.
- NBSP after number sign: `№\s+(?=\d)` → `№ `.
- NBSP after section sign: `§\s+(?=\d)` → `§ `.
- Suspicious Unicode spaces: `[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]` requires review.

## Frequent mistakes

Escape literal regex punctuation. Do not use a blanket Replace All for ReviewOnly patterns. Test on a copy or preview a few hits before replacing.

## Limitations and Design versus Source

Design uses PCRE2-16, UTF/UCP and advanced PCRE2 constructs. Source uses Scintilla C++11 regex line by line and has different replacement syntax. FBE replacement can address only groups 1…9 and does not replace across paragraphs.
## Minimal pattern
```regex
\bword\b
```
