# Regular expression help — Design

Translation note: Russian words and sentences in control examples are intentionally retained. They are test data: the regular expressions, replacement strings, whitespace characters and expected results are identical to the Russian reference edition. The explanations are translated; the examples are not automatically adapted to English typography.

A complete guide to searching, replacing, and proofreading books in FictionBook Editor Next.

Revision: October 2, 2026. Source mode has a separate document, regex-source.md. In this guide, “pattern” means a regular expression; a “built-in template” is a saved expression and its settings in the Templates panel.

Translation note: the expressions, replacement strings, and test samples have been preserved from the approved Russian edition. Russian words in code and test examples are intentional, particularly where Cyrillic letters, case, or mixed alphabets are being tested. Translate the explanations, not the samples, when comparing results.

## 1. How to use this guide

A regular expression describes a search rule rather than one exact string. For example, `[0-9]+` finds a sequence of digits of any length, while `[ \t]{2,}` finds two or more ordinary spaces or tabs. The match itself and the text used to replace it are different things.

Section 2 provides a first practical result. Sections 3–15 explain the syntax. Section 16 covers FBE’s special replacement grammar. Section 17 collects ready-to-use book-editing recipes, each with settings, test text, and warnings. Troubleshooting, performance advice, and sources follow at the end.

Copy only the expression into the Find field. The words “Find” and “Replace”, labels such as U+0020, and Markdown backticks are not part of the pattern. Do not add JavaScript `/.../g` delimiters, C++ string-literal quotes, or doubled backslashes copied from JSON.

Checkbox settings matter. A match found with case sensitivity disabled may not be found with it enabled. The settings in a recipe are part of that recipe, not optional formatting.

“Find and review only” identifies a diagnostic recipe. A match may be correct: an intentional repetition, a foreign name, a quotation, a heading, or deliberate typography. “Replace after review” likewise does not mean that the change is safe for every book.

## 2. Your first search and a safe replacement

Save a working copy of the book. Switch to Design, open Find or Replace, and enable Regular expression. For initial experiments, turn off Whole words only: it is better to specify boundaries in the expression itself. Check the search scope and direction.

To find repeated spaces, enter:

```regex
[ \t]{2,}
```

In `Он   пришёл`, this finds the three-space gap. Put one ordinary space in the replacement field. Replace a single occurrence first, check that the result is `Он пришёл`, and only then consider Replace All.

When there are many matches, first inspect the search results and correct one or two representative cases. After a bulk operation, check the text, italics, bold formatting, notes, and paragraph boundaries. If the result is unexpected, undo it before starting another round of edits.

Apply in the Templates panel transfers the expression and its settings to the search dialog. It should not be understood as an instruction to fix every match unconditionally. Use the appropriate dialog commands to perform the search or replacement.

## 3. The engine and the boundaries of Design mode

This mode uses PCRE2-16, passing text and patterns as UTF-16; UTF mode is always enabled. FBE separately builds searchable text from the document and separately performs replacements with regard to the book’s structure. PCRE2 documentation therefore explains matching, but does not define every FBE operation. See technical sources D1–D4.

The search works on a textual representation of the book, not the file’s literal XML markup. Searching for `<strong>` is not a way to find bold text in Design. Search for XML tags in Source mode, and use editor commands or specialized scripts for structural transformations.

A long line wrapping to the window width is not a newline character. Paragraphs, actual breaks, and visual wrapping must not be confused. FBE uses multiline mode for anchors in its searchable representation so that textual line boundaries reflect paragraph boundaries. Matching across paragraphs and replacing across paragraphs are different operations: the implementation considered here rejects cross-paragraph replacement.

The anchors `\A` and `\z` refer to the start and end of the subject passed to the engine. Do not automatically assume that they always mean the start and end of the entire FB2 file: the scope and the search fragment constructed by FBE matter.

## 4. Unicode: UTF, UCP, and case

### 4.1. What UTF does

UTF mode lets the engine process Unicode characters, including Cyrillic. It does not transliterate text, correct OCR, turn `ё` into `е`, or automatically merge canonically equivalent representations of letters.

For example, an accented letter may be stored as one character or as a letter followed by a separate combining mark. The forms look similar, but character-by-character searches may differ. PCRE2 does not perform NFC/NFD normalization for you. Books containing diacritics may require the combining-mark class `\p{M}`.

### 4.2. What UCP does

Unicode (UCP) changes the meaning of shorthand classes, particularly `\w`, `\d`, and `\s`, and the dependent boundaries `\b` and `\B`. It is not the switch that enables Cyrillic text itself.

An important correction to early editions of this guide: explicit Unicode properties such as `\p{L}`, `\p{N}`, and `\P{...}` are available in a Unicode PCRE2 build even without UCP. However, a pattern using both `\p{L}` and `\b` should usually enable UCP so that letters and word boundaries are interpreted consistently.

Compare a search for a Russian word:

```regex
\bмир\b
```

With UCP, boundaries follow the Unicode word class. Without UCP, do not expect `\b` to behave on Cyrillic as it does on Latin ASCII text. A missing match here does not prove that the word is absent.

When you specifically need ASCII digits, use `[0-9]` rather than `\d`: UCP may extend the latter to decimal digits in other writing systems.

### 4.3. Useful properties

| Expression | What it finds |
| --- | --- |
| `\p{L}` | A Unicode letter |
| `\p{Lu}` | An uppercase letter in a case-sensitive search |
| `\p{Ll}` | A lowercase letter in a case-sensitive search |
| `\p{M}` | A combining mark |
| `\p{N}` | A numeric character, a broader concept than a decimal digit |
| `\p{Nd}` | A decimal digit |
| `\p{Latin}` | A character of the Latin script |
| `\p{Cyrillic}` | A character of the Cyrillic script |
| `\P{L}` | A character that is not a letter |

For a sequence of letters together with combining accents:

```regex
[\p{L}\p{M}]+
```

This is still not a universal linguistic definition of a word: hyphens and apostrophes are not included. Add them deliberately when needed.

### 4.4. Case is a separate setting

Enable Match case when looking for anomalies such as `строчнаяПрописная`, initials, or patterns containing `\p{Lu}` and `\p{Ll}`. Case-insensitive matching can undermine the very purpose of such a rule. UCP does not replace this checkbox.

Temporarily enabling case-insensitive matching in an expression:

```regex
(?i)глава
```

Restricting its effect to a group:

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Literal characters and escaping

Letters and most punctuation match themselves. Outside a character class, special characters include the period, brackets, asterisk, plus, question mark, braces, anchors, and backslash.

To search for a metacharacter literally, place a backslash before it:

| Text to find | Expression |
| --- | --- |
| A period | `\.` |
| A question mark | `\?` |
| A plus sign | `\+` |
| An asterisk | `\*` |
| An opening parenthesis | `\(` |
| A closing parenthesis | `\)` |
| Square brackets surrounding a number | `\[[0-9]+\]` |
| A backslash itself | `\\` |

The expression `(123)` finds the digits `123` and captures them as a group; it does not require parentheses in the text. To find the string `(123)`, escape the parentheses.

In a PCRE2 pattern, a long literal fragment can be enclosed between `\Q` and `\E`:

```regex
\QЦена (руб.) + доставка\E
```

Those same sequences have different meanings in FBE’s replacement field. Do not automatically transfer escaping rules from search to replacement.

## 6. Spaces, tabs, and invisible characters

| Expression | Meaning in PCRE2 search |
| --- | --- |
| `[ ]` | Only an ordinary space U+0020 |
| `[ \t]` | An ordinary space or tab |
| `\t` | A tab U+0009 |
| `\x{00A0}` | A non-breaking space |
| `\x{202F}` | A narrow non-breaking space |
| `\h` | A horizontal whitespace character, including several Unicode spaces |
| `\s` | A whitespace character; may include line breaks |
| `\r` | Carriage return, CR |
| `\n` | Line feed, LF |
| `\R` | A Unicode line-break sequence |
| `\x{00AD}` | A soft hyphen |
| `\x{200B}` | A zero-width space |
| `\x{FEFF}` | FEFF occurring within the text |

For ordinary cleanup of gaps between words, choose `[ \t]` rather than `\s`. The latter is broader and may include paragraph boundaries and non-breaking spaces. Similarly, `\h` is useful for diagnosis but too broad for an unspecified typographic replacement.

A non-breaking space keeps parts of a notation together: a number sign and number, initials and surname, or a number and unit. Replacing every such space with an ordinary one may worsen layout. FBE also has a configurable NBSP character; recipes using U+00A0 should be checked against the particular book’s settings and the build in use.

U+200C and U+200D may be necessary in writing systems and composite emoji. Do not indiscriminately delete the whole range of “invisible” characters. Detection does not imply an error.

## 7. Character classes and ranges

A bracketed class consumes one character from its set. `[abc]` means one of three letters, not the word `abc`. Add a quantifier for one or more such letters: `[abc]+`.

The negated class `[^abc]` consumes one character outside the set. It does not test that “abc does not precede the text”. Use lookaround for contextual tests of that kind.

Inside a class, a period and most brackets lose their special meanings. A hyphen can define a range, so place a literal hyphen at the start or end of the class, or escape it. Writing `\]` is a convenient way to include a closing square bracket.

```regex
[А-Яа-яЁё-]+
```

This example permits Russian letters and a hyphen, but does not cover the whole Cyrillic script: Ukrainian, Belarusian, and other letters require a broader set or a Unicode property.

Inside square brackets, `\b` is not a word boundary. Do not try to place word boundaries in an ordinary character set.

## 8. Anchors, boundaries, and zero-length matches

An anchor tests a position without consuming a letter or space. Consequently, `^`, `$`, or `\b` alone can produce a zero-length match, which does not appear in the interface like a normal highlighted text fragment.

| Anchor | Meaning |
| --- | --- |
| `^` | Start of a line with multiline enabled; otherwise start of the subject |
| `$` | End of a line with multiline enabled; treatment of a final newline depends on the mode |
| `\A` | Start of the subject only |
| `\z` | Strict end of the subject |
| `\Z` | End of the subject or the position before its final newline |
| `\b` | Boundary between a word character and a non-word character |
| `\B` | A position that is not a word boundary |
| `\G` | Starting position of the current matching call |

`\G` does not independently remember “the previous match”. It is tied to the start offset supplied by the application. In successive calls that may be the end of the previous match, but in FBE this is not a universal way to traverse a book. Prefer more explicit boundaries for ordinary proofreading. [D1]

To find a word rather than its occurrences inside longer words, use boundaries or tests of adjacent letters. With UCP:

```regex
\bтом\b
```

This does not match the start of `томик`. However, hyphens and apostrophes may divide words differently for the engine than for an editor.

Use zero-length matches especially carefully with Replace All: text is inserted at positions rather than substituted for visible characters. Start with expressions that produce nonempty matches.

## 9. Quantifiers: repetition and backtracking

| Form | Repetitions of the preceding element |
| --- | --- |
| `?` | Zero or one |
| `*` | Zero or more |
| `+` | One or more |
| `{3}` | Exactly three |
| `{3,}` | At least three |
| `{2,5}` | Two through five |

A quantifier applies to the preceding character, class, or group. `аб+` repeats only `б`; `(?:аб)+` repeats the pair of letters.

### 9.1. Greedy matching

Input:

```text
«первый» и «второй»
```

Pattern:

```regex
«.*»
```

The dot and asterisk first take as much text as possible, then the engine backtracks if necessary to satisfy what follows. Here the result spans both pairs of quotation marks.

### 9.2. Lazy matching

```regex
«.*?»
```

This first tries the smallest number of characters but expands the match if needed. On this text, successive searches find `«первый»`, then `«второй»`.

For a simple pair of quotation marks, explicitly restricting the content is often clearer:

```regex
«[^»\r\n]*»
```

This does not parse nested quotations. Nested quotations require separate editorial review.

### 9.3. Possessive quantifiers

```regex
«.*+»
```

This does not mean “an even more correct quotation pattern”. `.*+` consumes the closing quotation mark too and will not give it back; the remaining `»` in the pattern cannot match. There is no result on the sample text.

A version excluding the closing character can be appropriate:

```regex
«[^»\r\n]*+»
```

Use possessive quantifiers and atomic groups to control backtracking, not as a universal way to speed up every expression.

## 10. Groups and backreferences

Parentheses capture the matched fragment. Numbering begins at 1 and follows opening capturing parentheses from left to right. Nesting does not change that order.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

On `02.10.2026`, the groups contain `02`, `10`, and `2026`. This checks the shape of the notation, not the calendar validity of the date.

A noncapturing group `(?:...)` combines elements without consuming a group number. It is useful in complex recipes where replacement references `$1` and `$2` must remain predictable.

Named groups make a pattern easier to read:

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

A backreference finds the same captured text. A subroutine call, described later, repeats a rule and need not find the same text. These are different mechanisms.

For repeated words in a real book, add boundaries, an appropriate case setting, and a suitable whitespace set; a ready-made recipe appears below. A backreference without boundaries can match part of a longer word.

Named groups are allowed in the search expression, but this does not automatically provide named substitutions in FBE’s replacement field. Use the confirmed numeric references for replacement.

## 11. Alternation and atomic groups

The vertical bar selects one of several branches:

```regex
(?:глава|часть)[ \t]+[0-9]+
```

Grouping matters. Without it, a shared suffix may apply only to the final branch. Alternatives are tried in their written order; arrange long and short forms deliberately.

An atomic group prevents backtracking into a group that has already matched successfully:

```regex
(?>а|аб)в
```

On `абв`, the first branch selects `а`; returning to choose `аб` is then forbidden, so no match is found. The non-atomic version can find it. This is a teaching example: do not add atomicity without checking the result.

## 12. Testing surrounding text: lookaround

Lookaround checks context without including it in the whole match. For example, it can select only a number while leaving the sign before it untouched.

```regex
(?<=№ )[0-9]+
```

On `№ 125`, only `125` is found. The space here is exactly one ordinary space.

A test to the right:

```regex
[0-9]+(?=[ \t]+руб\.)
```

On `125 руб.`, the number is found, but not `руб.`.

A negative lookahead:

```regex
\bглава\b(?![ \t]+[0-9])
```

With UCP, this finds the word “глава” when it is not followed by ordinary numbering after a space. It illustrates contextual selection, not a book-correction rule.

Lookbehind is written `(?<=...)`, and negative lookbehind `(?<!...)`. Lookahead is `(?=...)`, and negative lookahead `(?!...)`.

Lookbehind has length restrictions. Modern PCRE2 permits some bounded variable lengths, but not arbitrary unbounded repetition. For portable recipes, use short fixed context or a capture/`\K`; do not rely on `.*` inside lookbehind.

## 13. Inline options

| Option | Effect |
| --- | --- |
| `(?i)` | Ignore case |
| `(?-i)` | Match case |
| `(?m)` | Enable multiline start/end anchors |
| `(?-m)` | Disable multiline anchors |
| `(?s)` | Let the dot match a newline |
| `(?-s)` | Restore the usual dot behavior |
| `(?x)` | Ignore insignificant spaces and comments in the pattern |

`(?m)` does not make the dot cross lines. `(?s)` does not authorize FBE to replace across paragraphs. These are two separate search options and a separate application restriction.

In extended mode, spaces in the expression may cease to be literal. Use `[ ]` for a required space; `#` outside a class can start a comment. Nicely laid-out multiline patterns are useful in documentation, but the recipes below use a single line for the editor’s single-line search field.

## 14. Advanced PCRE2 features

Most corrections do not require this section. It explains constructs you may encounter in someone else’s pattern. Engine support for a construct does not mean that every pattern using it is convenient or safe for a whole book.

### 14.1. Resetting the start of a match

```regex
№[ \t]*\K[0-9]+
```

On `№ 125`, the prefix is checked but the reported match is only `125`. This is useful when the number, rather than the sign, must change. Do not use `\K` inside lookaround without separate verification: PCRE2 restricts that combination.

### 14.2. A condition on group participation

```regex
^(\()?([0-9]+)(?(1)\))$
```

This permits `12` or `(12)`, but not the unclosed `(12`. The condition tests whether group 1 participated. Do not turn this search example into a replacement relying on optional groups without checking FBE’s wrapper behavior.

### 14.3. Shared group numbering across alternatives

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

In both branches the number goes into group 1. This is branch reset. For simple cases, a noncapturing group with one shared capture is usually clearer.

### 14.4. Subroutine calls

```regex
(?<pair>[0-9]{2})-(?&pair)
```

On `12-34`, both parts satisfy the same “two digits” rule, even though their values differ. A backreference `\k<pair>` would require another `12`.

Recursive subroutines can describe some nested structures, but do not replace FBE’s XML parser. Use structural tools to edit `<section>`, `<poem>`, notes, and tables.

### 14.5. Skipping fragments

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

With UCP, this searches for the word outside simple pairs of Russian quotation marks. The first branch marks a quoted fragment to skip; the second searches for the word. It does not handle nested or unclosed quotations. Do not rely on it unconditionally for bulk editing.

## 15. What a regular expression cannot decide

“Uppercase after lowercase”, “no punctuation at the end of a paragraph”, and “repeated word” are formal signals, not editorial decisions. Regex does not know whether `да да` is a typo, an intentional line of dialogue, part of a poem, or a heading.

Do not automatically add periods, join every paragraph starting with lowercase, replace all Latin letters with Cyrillic, or convert every hyphen to a dash. Complex operations need context and sometimes several DOM-based steps. FBE’s cleanup, joined-word, and note scripts address precisely these more complex tasks.

## 16. FBE replacement grammar

PCRE2 performs the search, but FBE parses the replacement string. Replacement examples from JavaScript, Python, .NET, PCRE2 substitute, or another editor must therefore be checked before reuse. The commands below are supported by FBE’s source code and the original guide. [D3, D4]

### 16.1. The match and its groups

| In the replacement field | Meaning |
| --- | --- |
| `$0` or `\0` | The whole match |
| `$1` … `$9` | The corresponding numbered group |
| `\1` … `\9` | An alternative form for a numbered group |
| `$+` or `\+` | The last group returned by the match wrapper |

“FBE replaces only groups 1–9” needs qualification: ordinary text and the whole match can also be inserted. Direct numeric access to groups is restricted; this is not PCRE2’s limit on the number of groups.

Do not use `$10` as a reference to group 10. Named substitutions such as `${name}` are not part of FBE’s confirmed grammar. For predictability, keep the required captures among the first nine groups and make auxiliary groups noncapturing.

Optional and empty groups need a separate trial: FBE has its own SubMatches wrapper. For new bulk replacements, avoid relying on subtle skipped-group behavior or the meaning of “last group”; use explicit mandatory captures.

### 16.2. Rearranging groups

Find:

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Replace with:

```text
$3-$2-$1
```

This changes `02.10.2026` to `2026-10-02`. It demonstrates rearrangement, not a recommendation to reformat every date in a book.

### 16.3. Changing case

| Command | Effect |
| --- | --- |
| `\U` | Convert the inserted fragment to uppercase |
| `\L` | Convert it to lowercase |
| `\T` | Make the fragment’s first letter uppercase and the rest lowercase |
| `\Q` | Reset active case/formatting commands for the following fragment |

Find:

```regex
(иван)
```

Replace with:

```text
\T$1\Q
```

Expected result: `Иван`. `\T` is not sophisticated linguistic title casing of every word: passing `иван иванов` as one fragment does not guarantee `Иван Иванов`.

Do not layer `\U` and `\L` without resetting. Separate segments with `\Q` and check Cyrillic and diacritics. FBE changes the case, not a PCRE2 Unicode normalizer.

### 16.4. Bold and italic formatting

`\S` enables bold for the inserted fragment; `\E` enables italics. `\Q` ends the effect of the active commands on subsequently inserted text.

```text
\S$1\Q
```

This replacement formats group 1. It does not search for existing bold text and is not a way to remove all existing formatting in a book. Test one fragment and Undo first.

### 16.5. Sequences with different search and replacement meanings

In search, `\S` means a non-whitespace character; in FBE replacement it means bold. In search, `\Q...\E` quotes a literal fragment; in replacement, `\Q` resets commands and `\E` enables italics.

Do not enter `\n`, `\t`, `\x{00A0}`, `\$`, or `$$` in a Design replacement expecting another editor’s behavior. They are not part of the literal grammar described above; an unknown control sequence may be discarded. Use an actual NBSP character, and preserve a special character through a captured group or a checked ordinary non-regex replacement.

An empty replacement field deletes the matched text. A field containing one space replaces it with one space. Do not type explanatory labels such as `<пусто>`, `[NBSP]`, or `U+00A0` literally.

## 17. Practical recipes for editing and proofreading

The examples below are independent scenarios, not a promise that their names exactly match the built-in catalog. Try replacement recipes on one match first. Actual spaces and tabs are preserved in the test blocks. Negative examples demonstrate at least one boundary of applicability, but do not replace reviewing the entire book.

### D01. Multiple ordinary spaces into one

Normalize ordinary gaps without touching a single NBSP.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
[ \t]{2,}
```

Test input:

```text
Он   пришёл.
```

Replace with: one ordinary space U+0020. This is one character, not the word “space”.

Replacement result:

```text
Он пришёл.
```

Negative example that must not match:

```text
Он пришёл.
```


Limitations and notes: Several spaces may be intentional in poetry, tables, or simulated indentation. Check the scope; this is not a way to restructure paragraph indentation.

### D02. Spaces at the start of a paragraph

Remove manual indentation before ordinary text.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
^[ \t]+
```

Test input:

```text
   Начало абзаца.
```

The same test text with visible symbols:

```text
␠␠␠Начало␠абзаца.
```

Here ␠ denotes an ordinary space, [TAB] a tab, [NBSP] U+00A0, [NNBSP] U+202F, and [ZWSP] U+200B. This is an explanatory representation; do not insert these labels into the book.

Replace with: leave the field completely empty. Do not type the word “empty”.

Replacement result:

```text
Начало абзаца.
```

Negative example that must not match:

```text
Начало абзаца.
```


Limitations and notes: Manual indentation may be meaningful in poetry or artistic layout. The anchor refers to a textual line, not a visual wrap at the window edge.

### D03. Spaces at the end of a paragraph

Remove a tail of ordinary spaces or tabs.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
[ \t]+$
```

Test input:

```text
Конец абзаца.
```

The same test text with visible symbols:

```text
Конец␠абзаца.␠␠␠
```

Here ␠ denotes an ordinary space, [TAB] a tab, [NBSP] U+00A0, [NNBSP] U+202F, and [ZWSP] U+200B. This is an explanatory representation; do not insert these labels into the book.

Replace with: leave the field completely empty. Do not type the word “empty”.

Replacement result:

```text
Конец абзаца.
```

Negative example that must not match:

```text
Конец абзаца.
```


Limitations and notes: NBSP is deliberately excluded. Use separate diagnostics for unusual spaces.

### D04. Space before a comma or other punctuation

Keep the punctuation mark and remove the preceding gap.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
[ \t]+([,;:!?])
```

Test input:

```text
Слово , другое !
```

Replace with:

```text
$1
```

Replacement result:

```text
Слово, другое!
```

Negative example that must not match:

```text
Слово, другое!
```


Limitations and notes: This rule is for ordinary Russian text. French typography permits a special space before some punctuation; do not apply the recipe to such a book without adapting it.

### D05. Space after an opening mark

Remove ordinary spaces after a bracket or Russian opening quotation mark.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
([(\[«„])[ \t]+
```

Test input:

```text
« слово» ( пример)
```

Replace with:

```text
$1
```

Replacement result:

```text
«слово» (пример)
```

Negative example that must not match:

```text
«слово» (пример)
```


Limitations and notes: It does not affect spaces in formulas or examples unless they immediately follow one of the listed marks. Context still needs review.

### D06. Space before a closing mark

Remove ordinary spaces before a bracket or closing quotation mark.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
[ \t]+([)\]»”])
```

Test input:

```text
«слово » (пример )
```

Replace with:

```text
$1
```

Replacement result:

```text
«слово» (пример)
```

Negative example that must not match:

```text
«слово» (пример)
```


Limitations and notes: This is not a normalizer for every quotation style or mathematical bracket convention.

### D07. Exactly three periods into an ellipsis

Convert three consecutive periods without touching a longer sequence.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
(?<!\.)\.{3}(?!\.)
```

Test input:

```text
Он подумал... и ответил.
```

Replace with:

```text
…
```

Replacement result:

```text
Он подумал… и ответил.
```

Negative example that must not match:

```text
Содержание.....12
```


Limitations and notes: Check the editorial policy. Four periods and table-of-contents leaders are intentionally not changed by this pattern.

### D08. Spaced-out ellipsis

Bring together three periods separated by ordinary spaces.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Test input:

```text
Он подумал. . . и ответил.
```

Replace with:

```text
…
```

Replacement result:

```text
Он подумал… и ответил.
```

Negative example that must not match:

```text
Слово. Другое.
```


Limitations and notes: Also matches three adjacent periods. Do not use it for leader lines or omissions in quotations that follow special conventions.

### D09. Non-breaking space after №

Keep the number sign with the following number.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
№[ \t]+([0-9]+)
```

Test input:

```text
№ 125
```

Replace with:

```text
№ $1
```

Replacement result:

```text
№ 125
```

Negative example that must not match:

```text
№125
```


Limitations and notes: The replacement contains a real U+00A0 between № and $1. Do not replace it with the printed sequence \x{00A0}. The pattern does not insert a missing space.

### D10. Non-breaking space after §

Keep the section sign with its number.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
§[ \t]+([0-9]+)
```

Test input:

```text
§ 12
```

Replace with:

```text
§ $1
```

Replacement result:

```text
§ 12
```

Negative example that must not match:

```text
§12
```


Limitations and notes: The replacement contains an actual NBSP between § and $1. For references with complex numbering, check that the intended fragment was found.

### D11. Possible repeated adjacent word

Find repetition across horizontal spaces without crossing paragraphs.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — off.

Find:

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Test input:

```text
Это это уже было.
```

Expected match:

```text
Это это
```

Negative example that must not match:

```text
Это уже было.
```


Limitations and notes: “Да да” and similar repetitions may be intentional. Do not automatically remove the second instance; first decide whether deletion, a comma, or no change is appropriate. Words with apostrophes or hyphens are not fully covered.

### D12. Latin and Cyrillic within one word

Find OCR mixing inside one uninterrupted word, not any line containing two languages.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Test input:

```text
В слове Тeст латинская e.
```

Expected match:

```text
Тeст
```

Negative example that must not match:

```text
Он прочитал Latin.
```


Limitations and notes: In Тeст, e is Latin. A wholly Latin word beside a Russian word is not treated as mixed. Formulas, product names, and deliberate typographic effects can be valid matches. The rule does not transliterate anything.

### D13. Lowercase immediately followed by uppercase

Find possible joined words or a case error.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
\p{Ll}\p{Lu}
```

Test input:

```text
ОнвышелИздома.
```

Expected match:

```text
лИ
```

Negative example that must not match:

```text
Он вышел из дома.
```


Limitations and notes: Case sensitivity is essential. Compound brands and names such as McDonald may be correct. The match identifies a transition, not an automatically reconstructed word boundary.

### D14. Digit between letters

Find a typical OCR substitution of a digit for a letter.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
\p{L}+[0-9]+\p{L}+
```

Test input:

```text
Это сл0во.
```

Expected match:

```text
сл0во
```

Negative example that must not match:

```text
В главе 10 текст.
```


Limitations and notes: H2O and other formulas may be valid. Do not replace every 0 with о or every 3 with з.

### D15. Punctuation within a letter sequence

Inspect punctuation immediately surrounded by letters on both sides.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
\p{L}+[.,;:!?]\p{L}+
```

Test input:

```text
Он,сказал слово.
```

Expected match:

```text
Он,сказал
```

Negative example that must not match:

```text
Он, сказав слово, ушёл.
```


Limitations and notes: Abbreviations, addresses, and domains may also match. To check commas only, reduce the class to [,]. Do not insert a space into every match in one operation.

### D16. Paragraph starts with lowercase

Find a possible unwanted paragraph break.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
^[ \t]*\p{Ll}
```

Test input:

```text
продолжение предложения.
```

Expected match:

```text
п
```

Negative example that must not match:

```text
Начало предложения.
```


Limitations and notes: Poems, captions, lists, and quotations often legitimately start in lowercase. The pattern does not join paragraphs or understand neighboring context.

### D17. No final punctuation before closing quotation marks

Find a letter or digit at the end followed only by closing marks and spaces.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Test input:

```text
«Он пришёл»
```

Expected match:

```text
л»
```

Negative example that must not match:

```text
«Он пришёл!»
```


Limitations and notes: Headings and captions often do not require a period. A note reference after valid final punctuation may produce a false positive. Unlike a test of the final » alone, this recipe does not flag «Он пришёл!».

### D18. Lowercase after a sentence ending

Find a likely case error after a period, question mark, or exclamation mark.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Test input:

```text
Он пришёл. потом ушёл.
```

Expected match:

```text
. п
```

Negative example that must not match:

```text
Он пришёл. Потом ушёл.
```


Limitations and notes: Periods in abbreviations and an author’s ellipsis often do not end a sentence. This is only a list of candidates to inspect.

### D19. Possible missing period

Find a transition from a lowercase ending to a capitalized word across a space.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Test input:

```text
Он пришёл Потом ушёл.
```

Expected match:

```text
л Потом
```

Negative example that must not match:

```text
Он пришёл потом ушёл.
```


Limitations and notes: Names and titles inside a sentence produce many valid matches. The recipe cannot decide whether a period, comma, or no punctuation belongs there.

### D20. Pairs of straight quotation marks

Find a simple fragment in straight double quotes on one line.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
"([^"\r\n]+)"
```

Test input:

```text
Он сказал "да".
```

Expected match:

```text
"да"
```

Negative example that must not match:

```text
Он сказал «да».
```


Limitations and notes: Check nesting, inch marks, code, and the chosen quotation convention. Replacing with «$1» is appropriate only in selected context; it is not universal typographic processing.

### D21. Hyphen or em dash between numbers

Find a possible range needing an editorial decision.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Test input:

```text
Страницы 12 - 15.
```

Expected match:

```text
12 - 15
```

Negative example that must not match:

```text
Страницы 12–15.
```


Limitations and notes: The date 2026-10-02, a negative number, or subtraction can also match. Do not automatically convert them to ranges. The characters are – en dash, — em dash, and - hyphen.

### D22. Two initials before a surname

Find a simple two-initial form to check its spacing.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Test input:

```text
И.О. Иванов
```

Expected match:

```text
И.О. Иванов
```

Negative example that must not match:

```text
Иванов Иван
```


Limitations and notes: Not all compound surnames or diacritics are covered. After review, a replacement may use $1., NBSP, $2., NBSP, $3; insert actual non-breaking spaces, not their names.

### D23. Surname before two initials

Find the reverse name order.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Test input:

```text
Иванов И.О.
```

Expected match:

```text
Иванов И.О.
```

Negative example that must not match:

```text
Иванов Иван
```


Limitations and notes: This checks a format, not a person’s identity. Compound surnames, particles, and three initials require a separate rule.

### D24. Invisible characters to review

Find a soft hyphen, zero-width space, or FEFF in text.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Test input:

```text
сло​во
```

The same test text with visible symbols:

```text
сло[ZWSP]во
```

Here ␠ denotes an ordinary space, [TAB] a tab, [NBSP] U+00A0, [NNBSP] U+202F, and [ZWSP] U+200B. This is an explanatory representation; do not insert these labels into the book.

Expected match:

```text
​
```

Negative example that must not match:

```text
слово
```


Limitations and notes: The match is invisible in the sample block: U+200B lies between о and в. Insert or delete it only after determining its purpose. A physical file BOM and FEFF within the text are different cases.

### D25. Unusual Unicode spaces

Find narrow, wide, and other special spaces.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Test input:

```text
10 000
```

The same test text with visible symbols:

```text
10[NNBSP]000
```

Here ␠ denotes an ordinary space, [TAB] a tab, [NBSP] U+00A0, [NNBSP] U+202F, and [ZWSP] U+200B. This is an explanatory representation; do not insert these labels into the book.

Expected match:

```text
 
```

Negative example that must not match:

```text
10 000
```


Limitations and notes: A narrow non-breaking space between digit groups may be entirely correct. The recipe helps identify inconsistent usage; it does not declare every such character erroneous.

### D26. Repeated question and exclamation marks

Find expressive punctuation or accidental doubling.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
[!?]{2,}
```

Test input:

```text
Что?! Правда!!!
```

Expected matches, in sequence:

```text
?!
!!!
```

Negative example that must not match:

```text
Что? Правда!
```


Limitations and notes: The combination ?! and authorial repetitions may be intentional. The default action is review, not reducing every sequence to one mark.

### D27. Cyrillic Х beside Roman numerals

Find a Russian Х in a notation otherwise using Latin Roman-numeral characters.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — not required for this expression. “Match case” — on.

Find:

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Test input:

```text
Глава IХ
```

Expected match:

```text
Х
```

Negative example that must not match:

```text
Глава IX
```


Limitations and notes: Х is the Cyrillic letter, while X is Latin. Check case and context; this expression does not validate the whole Roman numeral.

### D28. Several uppercase letters before lowercase

Find a possible OCR case error at the start of a word.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Unicode (UCP)” — on. “Match case” — on.

Find:

```regex
\p{Lu}{2,}\p{Ll}+
```

Test input:

```text
Он сказал ПРИвет.
```

Expected match:

```text
ПРИвет
```

Negative example that must not match:

```text
Он сказал Привет.
```


Limitations and notes: Names, abbreviations with suffixes, and Latin abbreviations may be correct. Do not change case globally without review.

## 18. Common problems and diagnosis

### 18.1. The text is visible, but there is no match

Check Design mode, the regex checkbox, case, scope, direction, and actual characters. Latin `a` and Cyrillic `а` look similar but differ. NBSP is not an ordinary space. Typographic quotes are not straight quotes.

For Russian word boundaries, check UCP. For uppercase/lowercase rules, check case sensitivity. Do not combine Whole words only with complex boundaries of your own unless necessary.

### 18.2. The match is too large

First suspect a greedy dot-star or an overly broad negated class. Replace general “any text” with an explicit permitted set, and restrict lengths and boundaries. Check whether dotall is enabled.

### 18.3. Whitespace matches cross paragraphs

Do not use `\s+` as a synonym for a space. Start with `[ \t]+` for a gap between words and add NBSP separately only when required.

### 18.4. Replacement inserts digits, loses backslashes, or ignores a group name

Check FBE’s replacement grammar in section 16. `${name}`, `$10`, `\n`, and `\x{...}` must not be interpreted according to search rules or another editor. Make sure the required capture exists and is not optional in another branch.

### 18.5. Mixed alphabets are found in an ordinary bilingual sentence

An early example checked for Cyrillic and Latin anywhere in a whole line, also matching `Он прочитал Latin.`. Recipe D12 confines both tests to one word. This is the essential difference between finding an OCR mistake and finding a bilingual line.

### 18.6. “Missing period” flags a correct quotation

Checking only the final character is insufficient: `»` may follow `!`. Recipe D17 accounts for closing quotes and brackets, but headings and notes still require review.

### 18.7. A structure cannot be found

A text regex does not see the DOM as a structural script does. Finding tags, checking nesting, identifying links to missing IDs, and moving notes are separate tasks. The right next tool may be Source mode, the FB2 validator, or a bundled script—not an even more complex expression.

## 19. Performance and large books

Start with a narrow condition: a particular mark, class, word, or paragraph beginning. Avoid repeatedly nested unbounded repetitions and several competing “any text” fragments in succession. They may explore an enormous number of possibilities on a failed search.

Laziness is not a universal cure for a slow pattern: a lazy quantifier also explores alternatives. Explicitly excluding the delimiter, bounding the length, and reducing the search scope are often more effective.

If a search takes a long time, do not launch further replacements on top of it. Simplify the expression and test it on short control text first. A PCRE2 resource-limit error does not mean “no matches”.

For multistep processing involving neighboring paragraphs and tags, a script is often clearer and safer than a single complex regex. Do not combine every proofreading rule into one enormous alternation: separate rules make the cause of each match understandable.

## 20. Checks before saving the result

Inspect the beginning, middle, and end of the processed fragment. Review several matches of each type, especially quotations, ranges, initials, notes, and preserved formatting. Make sure significant NBSP characters have not disappeared and paragraphs have not changed unexpectedly.

After structurally sensitive edits, run FBE’s document validation. Save, reopen when appropriate, and compare the result. A successful regex search does not replace FB2 validation or editorial proofreading.

## 21. Sources and applicability

This is a translation of the expanded Russian guide derived from the supplied regex-design.md, with newly written explanations and test examples. Inaccurate original descriptions of UCP, subject boundaries, mixed alphabets, and replacement grammar were clarified against primary sources. The recipes in section 17 are editorial scenarios, not quotations from the PCRE2 manual.

[D1] Official PCRE2 pattern documentation: anchors, groups, Unicode properties, backtracking, and options.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Official PCRE2 Unicode documentation.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] FBE Next: PCRE2 compatibility and wrapper details. Repository snapshot d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`

[D4] FBE Next: GetReplStr, PrepareRegexReplacementText, and search/replacement operations in FBEview.cpp; SearchPresetCatalog.cpp and search-preset-design-fixtures.cpp in the same snapshot.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/src/fbe/FBEview.cpp`

Engine syntax, interface capabilities, and the correctness of an editorial decision are three different layers. Local tests of sample strings do not guarantee that every FBE build works with every document. The archive README records the verification status of this edition.
