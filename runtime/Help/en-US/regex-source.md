# Regular expression help — Source

:::note
Translation note: Russian words and sentences in control examples are intentionally retained. They are test data: the regular expressions, replacement strings, whitespace characters and expected results are identical to the Russian reference edition. The explanations are translated; the examples are not automatically adapted to English typography.
:::

A complete guide to searching and replacing in a book’s XML source in FictionBook Editor Next.

Revision: October 2, 2026. Design mode is covered separately in regex-design.md. These recipes are for Source mode; do not transfer PCRE2 expressions into it without checking them.

## 1. What Source-mode search is for

Source exposes XML tags, attributes, links, entities, and the book’s text. It is suitable for auditing FB2: finding empty elements, imported HTML tags, placeholder links, unexpected attributes, and technical conversion remnants.

Design is often more convenient for literary proofreading of ordinary words. In XML source, a match may occur not only in the book’s text but also in an attribute, comment, CDATA section, filename, or binary data. Account for this before replacement.

“Find and review only” means that a rule returns candidates to inspect. It does not prove that the XML or text is wrong. Even a simple whitespace replacement can change meaningful content; no replacement over arbitrary XML is universally safe here.

## 2. Quick start

Save a copy of the book, switch to Source, open Find, and enable Regular expression. XML examples below generally enable Match case and disable Whole words only. The pattern itself defines tag and attribute boundaries.

For example, to find a simple empty paragraph:

```regex
<p>[ \t]*</p>
```

This finds `<p></p>` and `<p>   </p>` on one physical source line. It does not find `<p>Текст</p>`. Do not automatically replace the matched element with nothing or with `<empty-line/>`: a paragraph and a blank-line element may serve different structural purposes.

Paste only the pattern into the field. JavaScript `/.../g` delimiters, C++ string quotes, and doubled JSON backslashes are unnecessary.

## 3. The engine: Scintilla, not PCRE2

FBE uses Scintilla with `SCFIND_REGEXP` and `SCFIND_CXX11REGEX`. In the build considered here, this is a C++ regular-expression implementation using the ECMAScript grammar. It is neither PCRE2 nor the full feature set of modern JavaScript. [S1, S2]

Scintilla also has an older basic regex mechanism with different conventions. Do not mix old examples that create groups with escaped parentheses with FBE’s C++11 mode: here `(слово)` captures, while `\(слово\)` requires literal parentheses.

Unicode text in XML is not forbidden, but there is no separate UCP mode or PCRE2 property syntax. Character classes depend on the standard-library implementation; do not assume `\w` means all letters in every language. For predictable ASCII matching, use explicit classes such as `[A-Za-z0-9_]`. For Russian text, `[А-Яа-яЁё]` is possible, but does not cover the entire Cyrillic script.

### 3.1. Main differences from Design

| Property | Design | Source |
| --- | --- | --- |
| Search subject | Textual representation of the book | XML source |
| Engine | PCRE2-16 | Scintilla C++11 |
| UCP and Unicode properties | Available in the PCRE2 profile | Not supported as PCRE2 syntax |
| Lookbehind | Available with PCRE2 restrictions | Not supported |
| First-group replacement | `$1` or `\1` | `\1` |
| Formatting in replacement | FBE commands | XML text only; no FBE commands |
| Search across physical lines | Depends on the search representation; cross-paragraph replacement is restricted | Not supported by the current path |

## 4. Line-by-line matching: the key limitation

In FBE’s Scintilla path, `MatchOnLines` matches an expression separately against the content of each physical line. A line can be very long; wrapping it to the window width does not create a new physical line. [S2]

For example:

```xml
<empty-line/> <empty-line/>
```

and:

```xml
<empty-line/>
<empty-line/>
```

These can describe the same neighboring elements in XML. For FBE’s current regex search, they differ: the first pair is accessible to one match, while the second crosses a line boundary and is not.

Adding `\r\n`, `\n`, `\x0D`, `\x0A`, `\s*`, or an “any character” class does not remove the restriction. The engine is not given both lines as one subject. Changing this requires a change to the search implementation, not merely another expression.

Conversely, line-by-line matching does not prevent a replacement from inserting a newline. That is a separate operation, illustrated in K28. Distinguish “find across a line boundary” from “insert a line boundary after matching text”.

### 4.1. Working with multiline XML

To find an attribute placed on its own line, it is often enough to search for the attribute without the entire opening tag. For relationships spanning several lines, use FBE’s structural tools, a validator, an appropriate script, or a separate XML tool.

Do not delete every newline in a book just to accommodate one regex: that can affect text, comments, CDATA, and source readability. First decide whether the task genuinely requires matching across lines.

## 5. Characters and escaping

An ordinary character matches itself. Escape a metacharacter with a backslash to find it literally.

| Required text | Expression |
| --- | --- |
| Period | `\.` |
| Plus | `\+` |
| Question mark | `\?` |
| Asterisk | `\*` |
| Parenthesis | `\(` or `\)` |
| Number in square brackets | `\[[0-9]+\]` |
| Backslash | `\\` |

The dot `.` means one character in the available line fragment, not a literal period. In a UTF-8 document, actual character handling depends on Scintilla’s adapter and the standard library; do not measure emoji length on the assumption that one dot always equals one visible symbol.

`\r` and `\n` denote CR and LF characters but do not permit matching across lines in this line-by-line path. Use `[ \t]` for ordinary spaces and tabs.

PCRE2’s `\Q...\E` is not a portable way to quote literal text in this mode. Escape the required special characters individually.

## 6. Classes, ranges, and word boundaries

`[abc]` finds one character from the list, `[a-z]` one from a range, and `[^abc]` one outside the list. A repetition may follow a class: `[0-9]+`.

| Class | Practical meaning |
| --- | --- |
| `[0-9]` | ASCII digit |
| `[A-Za-z]` | ASCII Latin letter |
| `[ \t]` | Ordinary space or tab |
| `[^<]` | Any available character except an opening angle bracket |
| `[^"']` | A character that is neither kind of straight quote |
| `\d`, `\D` | The implementation’s digit class and its negation |
| `\s`, `\S` | Whitespace class and its negation |
| `\w`, `\W` | Word class and its negation |

A negated class does not understand XML. `[^<]*` is useful for simple text between tags, but is not a complete parser for entities, comments, or CDATA.

`\b` denotes a word boundary and `\B` a non-boundary. A word boundary is often insufficient for attributes: `id`, for example, can follow a colon in a different name. It is more reliable to test an allowed attribute delimiter explicitly—the start of a line or a space/tab.

## 7. Anchors and repetition

In the current search path, `^` and `$` refer to physical line boundaries. For a line containing horizontal whitespace only:

```regex
^[ \t]+$
```

The matched characters can be deleted, but the physical line itself remains: its newline was not part of the match.

| Quantifier | Repetitions |
| --- | --- |
| `?` | Zero or one |
| `*` | Zero or more |
| `+` | One or more |
| `{3}` | Exactly three |
| `{3,}` | At least three |
| `{2,5}` | Two through five |

The C++11 grammar supports lazy forms such as `*?` and `+?`. For XML attributes, however, excluding the delimiting quotation mark is often clearer:

```regex
"[^"\r\n]*"
```

This example is for double quotes. Single quotes require a corresponding separate branch.

Do not transfer PCRE2 possessive quantifiers, atomic groups, or inline options to this mode.

## 8. Groups, alternation, and lookahead

A capturing group `( ... )` saves a fragment for backreferences and replacement. A noncapturing group `(?: ... )` combines parts that do not need a number.

Selecting several tag names:

```regex
<(?:strong|emphasis)>
```

A numeric backreference in search can connect the opening and closing names in a simple single-line fragment:

```regex
<(strong|emphasis)>[ \t]*</\1>
```

It requires the same name in both positions. This does not validate arbitrary XML nesting.

Positive lookahead `(?=...)` and negative lookahead `(?!...)` check what follows without including it in the match. For example, require a valid boundary after a tag name so that `<a` does not match the beginning of `<author>`.

```regex
<a(?=[ \t>])
```

When left context must be checked, capture it and restore it in the replacement: lookbehind is unavailable here. Do not copy `(?<=...)` expressions from the Design guide.

## 9. Replacement in Source mode

FBE uses `SCI_REPLACETARGETRE`: a match has already been found, and Scintilla expands the replacement string according to its own rules. This is not `std::regex_replace` with its usual `$1`, nor FBE’s Design replacement grammar. [S1, S2]

### 9.1. Capture references

| In the replacement field | Meaning |
| --- | --- |
| `\0` | The whole match |
| `\1` … `\9` | The corresponding captures |
| `$1` | Do not use it as a group reference; it is not this path’s syntax |

Find:

```regex
\(([0-9]+)\)
```

Replace with:

```text
[\1]
```

This changes `(12)` to `[12]`. It only makes editorial sense in selected context: a parenthesized number is not necessarily a note marker.

A literal dollar sign in a Source replacement need not be doubled according to another replacement system’s conventions. A backslash, on the other hand, is a control character.

### 9.2. Control characters in replacement

In the Scintilla code used by the checked FBE snapshot, replacement processes `\t`, `\n`, `\r`, and `\\` as tab, LF, CR, and literal backslash. Do not confuse this with the lack of cross-line searching. [S2]

Use `\r\n` for CRLF and `\n` for LF. Choose according to the document, and do not accidentally create mixed line endings.

PCRE2 sequences such as `\x{00A0}` are not a way to insert a Unicode character in this field. Use the actual NBSP character. Design commands `\U`, `\L`, `\T`, `\S`, `\E`, and `\Q` do not control case or formatting here.

### 9.3. Deletion and XML preservation

An empty replacement deletes the matched range. Deleting an empty tag, attribute, or link can damage a document even when the regex found it successfully. The structural recipes below are predominantly diagnostic.

Reordering or renaming an `id` in only one location may leave references pointing to the old value. For linked objects, use the editor’s ID-renaming command rather than independent bulk text replacements.

## 10. How to read the XML recipes

XML is case-sensitive. `<p>` and `<P>` are different names. Attribute order does not define their meaning, values may use single or double quotes, and spaces are allowed around the equals sign. Examples cover common variants where doing so does not make a rule excessively complex. [S3]

`l:` and `xlink:` are common prefixes for the XLink namespace in FB2. A prefix is not itself the meaning; its `xmlns` declaration determines that. Recipes listing these two forms do not guarantee support for every other prefix. Adapt the rule and check the namespace declaration when another form is present.

Many examples use `[^>]*` as an approximation of an opening tag’s contents. This is convenient for ordinary FB2, but `>` can legally occur inside an attribute value. Comments and CDATA may also contain text resembling markup. These recipes therefore identify candidates for review, not full XML validation or blind structural rewriting.

An “empty element” is not always a schema error. A “numeric entity” is not a damaged character. A placeholder, unusual name, or external link may also have a legitimate purpose.

## 11. Practical FB2/XML recipes

Unless stated otherwise, all participating parts of a sample must lie on one physical line. If a pattern searches an entire opening tag, all inspected attributes must also be on that line. A recipe that searches only an attribute does not require the entire tag to be on the same line.

### K01. Spaces at the end of a physical line

Find trailing ordinary spaces and tabs.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
[ \t]+$
```

Test input:

```text
<p>Текст</p>
```

The same test text with visible symbols:

```text
<p>Текст</p>␠␠␠
```

Here ␠ denotes an ordinary space, [TAB] a tab, [NBSP] U+00A0, [NNBSP] U+202F, and [ZWSP] U+200B. This is an explanatory representation; do not insert these labels into the book.

Replace with: leave the field completely empty. Do not type the word “empty”.

Replacement result:

```text
<p>Текст</p>
```

Negative example that must not match:

```text
<p>Текст</p>
```


Limitations and notes: Does not delete the newline. In mixed XML content, CDATA, and whitespace-sensitive areas, trailing spaces can be part of the text. Do not call cleanup of every line unconditionally safe.

### K02. Clear a whitespace-only line

Remove whitespace from a line with no other content.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
^[ \t]+$
```

Test input:

```text

<p>Text</p>
```

The same test text with visible symbols:

```text
␠␠␠[TAB]
<p>Text</p>
```

Here ␠ denotes an ordinary space, [TAB] a tab, [NBSP] U+00A0, [NNBSP] U+202F, and [ZWSP] U+200B. This is an explanatory representation; do not insert these labels into the book.

Replace with: leave the field completely empty. Do not type the word “empty”.

Replacement result:

```text

<p>Text</p>
```

Negative example that must not match:

```text
<p>Text</p>
```


Limitations and notes: The line remains empty: LF/CRLF was not included in the matched range. This does not delete every empty physical line.

### K03. Space before an empty-element close

Find an ordinary gap immediately before />.

Action: replace after review.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
[ \t]+/>
```

Test input:

```text
<empty-line />
```

Replace with:

```text
/>
```

Replacement result:

```text
<empty-line/>
```

Negative example that must not match:

```text
<empty-line/>
```


Limitations and notes: XML allows both forms. The change is cosmetic, not a required error correction. This combination can also occur in text or a comment, so bulk replacement needs review.

### K04. Simple empty paragraph

Check a p pair without textual content.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<p>[ \t]*</p>
```

Test input:

```text
<p>  </p><p>Text</p>
```

Expected match:

```text
<p>  </p>
```

Negative example that must not match:

```text
<p>Text</p>
```


Limitations and notes: Does not cover paragraphs with attributes, newlines, an NBSP entity, or a nested tag. Do not automatically treat an empty paragraph and empty-line as interchangeable.

### K05. Two empty-line elements on one line

Find adjacent FB2 blank-line elements written on one physical source line.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Test input:

```text
<empty-line/> <empty-line />
```

Expected match:

```text
<empty-line/> <empty-line />
```

Negative example that must not match:

```text
<empty-line/>
<empty-line/>
```


Limitations and notes: Only ordinary spaces and tabs are allowed between them. A newline is intentionally unsupported. Two blank lines can be an author’s scene separator.

### K06. Empty important metadata

Inspect several simple metadata fields with no content.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Test input:

```text
<book-title> </book-title>
```

Expected match:

```text
<book-title> </book-title>
```

Negative example that must not match:

```text
<book-title>Book</book-title>
```


Limitations and notes: Not every listed field is mandatory in every context. Regex does not validate the FB2 schema, the parent element’s structure, or the correctness of a populated value.

### K07. Empty inline formatting

Find empty paired formatting elements.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Test input:

```text
<strong> </strong>
```

Expected match:

```text
<strong> </strong>
```

Negative example that must not match:

```text
<strong>Text</strong>
```


Limitations and notes: NBSP, attributes, or nested elements require a different rule. Before deleting an element, check whether it serves an important structural purpose.

### K08. Identical formatting nested within itself

Distinguish strong/strong repetition from a valid strong/emphasis combination.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Test input:

```text
<strong><strong>Text</strong></strong>
```

Expected match:

```text
<strong><strong
```

Negative example that must not match:

```text
<strong><emphasis>Text</emphasis></strong>
```


Limitations and notes: Unlike an overly broad original example, the second name is not chosen independently: it is a backreference to the first. The match does not include the whole element and is not intended for deletion as a single match.

### K09. Possible HTML tags after import

Find common HTML names that need checking in FB2.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Test input:

```text
<div>Text</div>
```

Expected matches, in sequence:

```text
<div>
</div>
```

Negative example that must not match:

```text
<section><p>Text</p></section>
```


Limitations and notes: Tag-like text in comments or CDATA may match. Do not replace b with strong and i with emphasis using one broad pattern without checking content and attributes.

### K10. Uppercase tag

Find ordinary all-uppercase tag names.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Test input:

```text
<P>Text</P>
```

Expected matches, in sequence:

```text
<P>
</P>
```

Negative example that must not match:

```text
<p>Text</p>
```


Limitations and notes: Match case must be enabled; otherwise ordinary p tags are found too. The pattern does not enumerate every valid Unicode XML name or determine whether a namespace is allowed.

### K11. HTML NBSP entity

Find the literal &nbsp; notation in the source file.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
&nbsp;
```

Test input:

```text
<p>&nbsp;</p>
```

Expected match:

```text
&nbsp;
```

Negative example that must not match:

```text
<p>&#160;</p>
```


Limitations and notes: Without a suitable declaration, &nbsp; is not one of XML’s five predefined entities. Check any DTD and comment context. Do not decode all entities in bulk.

### K12. Numeric character references

Find decimal or hexadecimal character notation.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Test input:

```text
<p>&#160; &#xA0;</p>
```

Expected matches, in sequence:

```text
&#160;
&#xA0;
```

Negative example that must not match:

```text
<p>&amp;</p>
```


Limitations and notes: Both forms may be correct. The pattern checks their shape, not the validity of the code point. Decoding &lt; into a literal < inside text can damage XML.

### K13. Empty id

Find an unfilled ordinary id attribute with either kind of quote.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Test input:

```text
<section id="">
```

Expected match:

```text
 id=""
```

Negative example that must not match:

```text
<section id="s1">
```


Limitations and notes: The preceding space is part of the match. Prefixed names such as xml:id are not covered. Generating a new ID must account for references and uniqueness, which regex does not do.

### K14. List id values

Find populated ordinary ids to audit naming.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Test input:

```text
<section id="s1">
```

Expected match:

```text
 id="s1"
```

Negative example that must not match:

```text
<section id="">
```


Limitations and notes: A match list does not prove that all IDs are unique or valid. Use FBE’s built-in command to rename linked objects.

### K15. External HTTP(S) links

Find an href using a common XLink prefix and an external address.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Test input:

```text
<a l:href="https://example.test/book">Text</a>
```

Expected match:

```text
 l:href="https://example.test/book"
```

Negative example that must not match:

```text
<a l:href="#note1">1</a>
```


Limitations and notes: This searches the attribute, not every URL in the text. It supports l and xlink; other namespace prefixes need separate treatment. A match does not verify that the address is reachable.

### K16. Local file:// links

Find a local-file reference that may not work for a reader.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Test input:

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Expected match:

```text
 xlink:href="file:///C:/Book/image.png"
```

Negative example that must not match:

```text
<a xlink:href="#image1">Text</a>
```


Limitations and notes: Do not automatically open an unknown address. Finding it does not decide whether to embed a file, replace the link, or remove it.

### K17. Empty link target

Find an ordinary empty XLink target.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Test input:

```text
<a l:href="">Text</a>
```

Expected match:

```text
 l:href=""
```

Negative example that must not match:

```text
<a l:href="#note1">Text</a>
```


Limitations and notes: An empty link label and a missing href attribute are different cases that need separate checks.

### K18. The #undefined target

Find a literal placeholder target.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Test input:

```text
<a xlink:href="#undefined">Text</a>
```

Expected match:

```text
 xlink:href="#undefined"
```

Negative example that must not match:

```text
<a xlink:href="#note1">Text</a>
```


Limitations and notes: Check whether the actual object exists. The name undefined is not itself forbidden by XML syntax; in practice it often indicates an unfinished association.

### K19. Links with the bookmark prefix

Find characteristic converter bookmark targets, not all internal links.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Test input:

```text
<a l:href="#bookmark12">Text</a>
```

Expected match:

```text
l:href="#bookmark12"
```

Negative example that must not match:

```text
<a l:href="#note1">Text</a>
```


Limitations and notes: A general href="#..." pattern finds any internal link and does not isolate bookmark artifacts. A bookmark target does not prove the link is unnecessary; review before deleting it.

### K20. Word/FBD return links

Find common _ftnref and _ednref forms in link targets.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Test input:

```text
<a l:href="#_ftnref1">Back</a>
```

Expected match:

```text
l:href="#_ftnref1"
```

Negative example that must not match:

```text
<a l:href="#note1">Text</a>
```


Limitations and notes: A return link may be necessary for navigation. Do not delete it automatically merely because of the origin of its name.

### K21. Note links regardless of attribute order

Find an opening a tag containing type=note and an internal XLink href in either order.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Test input:

```text
<a l:href="#note1" type="note">1</a>
```

Expected match:

```text
<a l:href="#note1" type="note">
```

Negative example that must not match:

```text
<a type="link" l:href="#note1">1</a>
```


Limitations and notes: Also allows type before href, the xlink prefix, and single quotes. All inspected attributes must be on one physical line. Values containing >, comments, and unusual markup require XML parsing. The target note’s existence is not checked.

### K22. Possible numeric note markers

Find a number in square, curly, or round brackets.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Test input:

```text
<p>Text [12], {3}, (4).</p>
```

Expected matches, in sequence:

```text
[12]
{3}
(4)
```

Negative example that must not match:

```text
<p>[note]</p>
```


Limitations and notes: These may be bibliography references, equation numbers, explanations, or ordinary text. The pattern does not create notes or check numbering uniqueness.

### K23. Forgotten Your/Name values

Check typical English name placeholders in metadata.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Test input:

```text
<first-name>Your</first-name>
```

Expected match:

```text
<first-name>Your</first-name>
```

Negative example that must not match:

```text
<first-name>John</first-name>
```


Limitations and notes: Review the author/creator context and actual name. Do not automatically replace these values with information from a different edition.

### K24. Unattached #undefined illustration

Find an image pointing to the typical placeholder.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Test input:

```text
<image l:href="#undefined"/>
```

Expected match:

```text
<image l:href="#undefined"/>
```

Negative example that must not match:

```text
<image l:href="#cover"/>
```


Limitations and notes: FB2 normally uses an XLink href rather than HTML src. Check the matching binary ID separately. A missing image and an incorrect reference are different causes.

### K25. Windows-1251 declaration

Find an old encoding declaration in XML.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Test input:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Expected match:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Negative example that must not match:

```text
<?xml version="1.0" encoding="utf-8"?>
```


Limitations and notes: This is not inherently an error. Replacing windows-1251 with utf-8 as text does not re-encode the file’s bytes. Change encoding through a save/conversion operation consistent with the declaration.

### K26. Ampersand without a recognized standard entity

Find & before a sequence unlike a standard character-reference form.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Test input:

```text
<p>A & B</p>
```

Expected match:

```text
&
```

Negative example that must not match:

```text
<p>A &amp; B</p>
```


Limitations and notes: An ampersand is allowed in CDATA and comments, and a DTD may declare other entities. This is a diagnostic filter, not an XML validator. Automatic & → &amp; without context can double-escape text.

### K27. Ordinary internal links

Find a local #id target without confusing this with bookmark detection.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Test input:

```text
<a l:href="#note1">1</a>
```

Expected match:

```text
l:href="#note1"
```

Negative example that must not match:

```text
<a l:href="https://example.test">Text</a>
```


Limitations and notes: Regex shows the reference notation, but does not prove that exactly one target id exists. Dangling or duplicate references require document-level analysis.

### K28. Split two empty-line elements after a single-line search

Demonstrate the difference between a search limitation and inserting a newline through replacement.

Action: one controlled replacement.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Test input:

```text
<empty-line/> <empty-line/>
```

Replace with:

```text
\1\r\n\2
```

Replacement result:

```text
<empty-line/>
<empty-line/>
```

Negative example that must not match:

```text
<empty-line/>
<empty-line/>
```


Limitations and notes: The replacement contains control sequences that Scintilla expands to CRLF. For an LF document, use \1\n\2. This is not a universal XML formatter and does not enable multiline searching.

### K29. Double spaces in simple XML text

Find a text fragment between tags containing several ordinary spaces.

Action: find and review only.

“Regular expression” — on; “Whole words only” — off. “Match case” — on.

Find:

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Test input:

```text
<p>one  two</p>
```

Expected match:

```text
>one  two<
```

Negative example that must not match:

```text
<p>one two</p>
```


Limitations and notes: The match includes the angle delimiters and the entire simple fragment. Design is usually more convenient for correcting words. Do not replace the whole matched XML fragment with a single space.

## 12. Why regex does not replace XML validation

An expression can show that a piece of text resembles a particular sequence of characters. It does not prove the whole document is correct: proper nesting of every element, FB2 schema compliance, namespace declarations, unique IDs, and the existence of every link target require separate checks.

For example, two identical `id` values on different physical lines cannot reliably be detected with one comparison in the current line-by-line regex path. The same problem arises when a link’s target is missing elsewhere in the book. Use FBE’s validation and document-structure analysis for these tasks.

Do not decode all XML entities in bulk: `&lt;` and `&amp;` often protect text from becoming markup. Do not remove everything resembling HTML from CDATA, comments, or code quotations.

Preserving tag pairing, attributes, and content is the responsibility of the replacement operation. Replacing only an opening `<strong>` leaves its closing tag unchanged. A backreference in an example does not make every subsequent replacement structurally safe.

## 13. Common mistakes

### 13.1. Lowercase tags are found instead of uppercase ones

Enable Match case. For XML this is not a cosmetic option. Using `[A-Z]` case-insensitively defeats the purpose of checking uppercase tags.

### 13.2. A literal $1 appears in the replacement

Use `\1`. FBE’s Source replacement goes through Scintilla, not JavaScript formatting or another API’s standard `$1` substitution.

### 13.3. An expression with \p, \K, or lookbehind does not work

These constructs belong to another profile. For a text task, switch to Design; otherwise rewrite the XML recipe with Source tools: explicit classes, captured context, noncapturing groups, and lookahead.

### 13.4. Two neighboring tags are not found

Check for a physical newline, a space before `/>`, single quotes, extra attributes, or a namespace prefix. Literal adjacency in formatted source is not the same as adjacency in the XML tree.

### 13.5. A note link is missed when attributes change order

A simple “type first, href second” pattern depends on attribute order. K21 checks both properties using independent lookaheads. If the tag spans several lines, the current whole-tag search still cannot handle it; search for an attribute or use document structure.

### 13.6. A nesting check calls strong/emphasis an error

Different valid styles may be nested. Checking repetition of the same name requires a backreference, as in K08, rather than two independent groups with identical alternatives.

### 13.7. “Bookmark” finds every note

An internal link containing `#` is not automatically a bookmark artifact. Separate the general internal-link audit in K27 from the specific bookmark prefix in K19.

### 13.8. Replacement begins damaging visible text

In Source, spaces, marks, and fragments may be inside text, an attribute, a comment, or binary data. Undo the change and narrow the scope or pattern. A built-in scenario named “safe replacement” does not eliminate XML context.

## 14. Limits of the Source profile

Do not use UCP, PCRE2 property classes, lookbehind, modern JavaScript/PCRE2-style named groups, atomic groups, possessive quantifiers, branch reset, subroutines, PCRE2 verbs, `\K`, `\G`, or Design replacement-formatting commands in this mode.

Do not use PCRE2 inline options instead of the Source dialog’s checkboxes. Do not expect features of recent browser JavaScript versions to appear automatically in a C++11 ECMAScript implementation.

Line-by-line processing through MatchOnLines remains in force regardless of the newline sequence written in the pattern. Successful local compilation by a different regex engine is not proof of FBE compatibility.

Use Design for Unicode words and complex book proofreading. Use XML/FB2 facilities for structure rather than trying to compensate for restrictions with an unbounded `.*`.

## 15. Performance and long XML lines

A short expression is not necessarily fast. Several unbounded repetitions and long competing alternatives may be slow on large lines. One enormous line containing the entire document or a large binary element is a particular risk.

Where possible, start with a specific tag or attribute name, and use a restricted class instead of general “any characters”. Do not search for every conceivable mistake at once: separate diagnostic recipes are easier to review and undo.

Do not flatten all XML lines to bypass MatchOnLines. That changes the document and does not guarantee acceptable search performance.

## 16. Checks after replacement

Compare the expected and actual matched range. For capture-based replacements, verify that quotes, prefixes, angle brackets, and closing tags are preserved. Make sure comments, CDATA, and binary content have not been affected.

Run FB2 validation, save the book, and reopen it after significant changes. Check Source ↔ Design switching, notes, illustrations, and visible text. When an identifier is renamed, verify both the object and all related references.

## 17. Sources and applicability

This translates the expanded guide based on the supplied regex-source.md. Its “engine → syntax → replacement → XML examples → limitations” structure is preserved, while imprecise broad recipes have been replaced by narrower ones. Explanations and control scenarios were written anew, and technical differences checked against primary sources.

[S1] Official Scintilla documentation, Searching: C++11 mode, search flags, and SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Scintilla source in FBE Next snapshot d2257405d95b0328649acee64b38829b40a4314b: Document.cxx, Cxx11RegexFindText, MatchOnLines, and BuiltinRegex::SubstituteByPosition. This code distinguishes searching across lines from inserting CR/LF in replacement.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 and Namespaces in XML: names, attributes, entities, and namespaces.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`
