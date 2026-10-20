# Structural commands in Body mode

The Body editor exposes four commands through the same applicability checks
used by the menu and command handlers. A check must not modify the DOM.

- **Clone** inserts a new empty container of the same type after the current
  section, poem, stanza, citation, or epigraph. It never copies the source ID
  or MSHTML attributes. Supported `xml:lang` values are inherited.
- **Split** remains in `BodyStructuralEditor`. The existing selection and ID
  transfer rules are retained. Its native MSHTML undo behavior and the reason
  it is not forced into one Ctrl+Z are documented in
  [split-undo-mshtml-probe.md](../archive/undo/split-undo-mshtml-probe.md).
- **Merge** accepts adjacent, same-type sections, citations, or stanzas only
  after checking the layout. Text sections merge with text sections, nested
  sections with nested sections. The second outer ID or other attributes,
  second-section metadata, mixed section content, and citation author lines
  before further citation text cause a safe refusal. Inner nodes are moved
  intact; their IDs, links, formatting, and order are retained.
- **Remove Outer** accepts only an ID-free section whose direct content is
  nested sections. It refuses `body`, any metadata or other content, and an
  outer ID even if no link currently targets it. Children move to the parent
  in their original order.

Failed preflight leaves the DOM unchanged and reports a localized reason for
an invoked command. Execution wraps native DOM changes in an undo unit and
restores the original node order if a DOM operation throws. If restoration
cannot be verified, it reports that failure explicitly. Because MSHTML can record
individual DOM moves independently, the commands do not promise that every
successful operation is undone with a single Ctrl+Z.

`tools/tests/test-fbe-structural-commands.ps1` exercises the decision and
rollback paths quickly. The production MSHTML suite
`test-fbe-structural-commands-runtime.ps1` uses real FB2 fixtures, checks
Undo/Redo, saves, validates against `FictionBook.xsd`, and reopens successful
results. The existing Split production suite covers its own selection and
fault scenarios.
