import assert from "node:assert/strict";
import fs from "node:fs";

function parseCss(path) {
  const source = fs.readFileSync(path, "utf8");
  const withoutComments = source.replace(/\/\*[^]*?\*\//g, "");
  assert(!/\/\*/.test(withoutComments), `${path}: unterminated comment`);

  const rules = new Map();
  const selectorCounts = new Map();
  const blocks = withoutComments.matchAll(/([^{}]+)\{([^{}]*)\}/g);
  const consumed = withoutComments.replace(/[^{}]+\{[^{}]*\}/g, "");
  assert(!/[{}]/.test(consumed), `${path}: unbalanced or nested rule block`);

  for (const block of blocks) {
    const selector = block[1].trim().replace(/\s+/g, " ");
    assert(selector, `${path}: empty selector`);
    selectorCounts.set(selector, (selectorCounts.get(selector) || 0) + 1);
    const declarations = new Map();
    for (const rawDeclaration of block[2].split(";")) {
      const declaration = rawDeclaration.trim();
      if (!declaration) continue;
      const colon = declaration.indexOf(":");
      assert(colon > 0, `${path}: malformed declaration in ${selector}: ${declaration}`);
      const property = declaration.slice(0, colon).trim().toLowerCase();
      const value = declaration.slice(colon + 1).trim();
      assert(/^-?(?:[a-z]|_)[a-z0-9_-]*$/.test(property), `${path}: invalid property ${property}`);
      assert(value, `${path}: empty value for ${property} in ${selector}`);
      assert(!declarations.has(property), `${path}: duplicate ${property} in ${selector}`);
      if (property === "font-style") {
        assert(/^(?:normal|italic|oblique)(?:\s+[-+]?(?:\d*\.)?\d+(?:deg|grad|rad|turn))?$/.test(value),
          `${path}: invalid font-style value ${value}; use font-weight for bold text`);
      }
      declarations.set(property, value);
    }
    // Repeated selectors are valid CSS; later blocks override earlier blocks.
    const resolvedDeclarations = rules.get(selector) || new Map();
    for (const [property, value] of declarations) resolvedDeclarations.set(property, value);
    rules.set(selector, resolvedDeclarations);
  }
  return { rules, selectorCounts };
}

const normalCss = parseCss("runtime/main.css");
const fastCss = parseCss("runtime/main_fast.css");
const normal = normalCss.rules;
const fast = fastCss.rules;

// These declarations define shared editor/UI behaviour and must not drift.
const sharedDeclarations = [
  ["body", ["font-size", "font-family", "padding", "margin"]],
  ["div#fbw_desc div", ["border", "white-space", "clear"]],
  ["div#fbw_desc div.float", ["visibility", "position", "z-index", "top", "left", "background"]],
  ["div#fbw_desc button", ["font-family", "font-size", "color", "float", "cursor"]],
  ["div#fbw_desc button.popup_btn", ["float"]],
  ["div#fbw_desc input", ["font-family", "font-size", "border", "margin-top", "margin-bottom"]],
  ["div#fbw_desc input.short", ["width"]],
  ["div#fbw_desc input.middle", ["width"]],
  ["div#fbw_desc input.wide", ["width"]],
  ["div#fbw_desc input.url", ["width", "margin-left", "margin-top", "margin-bottom"]],
  ["div#fbw_desc textarea#val", ["width", "font-family", "font-size", "border"]],
  ["div#fbw_desc label", ["width", "text-align", "margin-top", "padding-left", "padding-right", "vertical-align", "color"]],
  ["div#fbw_desc label.hl", ["color", "font-weight"]],
  ["div#fbw_desc label.short", ["width"]],
  ["div#fbw_desc fieldset label", ["width"]],
  ["div#fbw_desc fieldset.kid label", ["width"]],
  ["div#fbw_desc legend.top", ["font-size", "color", "font-weight", "margin-top", "margin-bottom"]],
  ["div#fbw_desc textarea#stylesheetId", ["width", "font-family", "font-size", "border"]],
  ["span.top", ["vertical-align"]],
  ["div#fbw_body table.table", ["border-collapse", "border", "margin", "max-width"]],
  ["div#fbw_body table.table th, div#fbw_body table.table td", ["border", "padding", "vertical-align", "text-indent", "word-wrap"]],
  ["div#fbw_body table.table th", ["background-color", "font-weight"]],
  ["div.epigraph p", ["text-indent", "margin"]],
  ["div.title p", ["text-align", "text-indent"]],
  ["p", ["text-indent", "margin-top", "margin-bottom", "text-align"]],
  ["p.subtitle", ["text-align", "font-weight", "text-indent"]],
  ["div#fbNotePreview", ["visibility", "position", "z-index", "box-sizing", "padding", "border", "background", "color", "overflow", "line-height", "text-align", "white-space", "word-spacing", "letter-spacing"]],
  ["div#fbNotePreview p", ["margin", "text-indent", "text-align", "word-spacing"]],
  ["div#fbNotePreview p:last-child", ["margin-bottom"]],
  ["p.th", ["text-align", "background", "text-indent", "margin-bottom", "color"]],
  ["p.td", ["text-indent", "margin-bottom", "background", "color"]],
  ["div#fbw_body div.image", ["margin", "padding", "border", "overflow", "clear", "text-align"]],
  ["img", ["margin", "padding", "border", "position", "cursor"]],
  ["div#fbw_body div.hider", ["border", "margin", "padding", "width"]],
  ["span.image", ["text-indent", "margin", "padding", "border", "overflow", "clear", "text-align"]]
];

// Fast Mode intentionally removes expensive/redundant document decoration and
// compacts descriptor geometry. Every remaining cross-file difference is
// listed here so a new, unclassified drift fails the test.
const intentionalDifferences = [];
function addIntentionalDifferences(selector, reason, declarations) {
  for (const [property, normalValue, fastValue] of declarations) {
    intentionalDifferences.push({ selector, property, normalValue, fastValue, reason });
  }
}

addIntentionalDifferences("div#fbw_desc", "Fast descriptor pane uses compact padding and fixed height.", [
  ["text-align", "left", undefined], ["padding", "0.4em", undefined],
  ["padding-left", undefined, "0.8em"], ["height", undefined, "100%"]
]);
addIntentionalDifferences("div#fbw_desc fieldset", "Fast descriptor fieldsets omit decorative borders and use compact geometry.", [
  ["width", "61.5em", "60em"], ["padding-left", "0.5em", "0.2em"],
  ["padding-right", "0.5em", "0.2em"], ["padding-bottom", "0.5em", "0.3em"],
  ["padding-top", "0.5em", "0.3em"], ["border", "1px solid #C0C0C0", undefined]
]);
addIntentionalDifferences("div#fbw_desc fieldset.kid", "Nested fast fieldsets fill their row and omit a second decorative border.", [
  ["width", "60em", "100%"], ["border", "solid 1px #B0B0B0", undefined]
]);
addIntentionalDifferences("div#fbw_desc legend", "Fast descriptor legends omit left padding with the surrounding border.", [["padding-left", "0.5em", undefined]]);
addIntentionalDifferences("div#fbw_desc br", "Fast descriptor controls use a fixed compact vertical gap.", [["height", "0.5em", "10px"]]);
addIntentionalDifferences("div#fbw_body", "Fast document layout retains its historic wider left margin.", [["margin", "0.2em 0.2em 0 0.2em", "0.2em 0.2em 0 0.5em"]]);
addIntentionalDifferences("div#fbw_body div", "Fast Mode removes per-div indentation and borders for large documents.", [
  ["padding", "0em 0em 0em 0.4em", undefined], ["border-left", "solid 1px", undefined]
]);
addIntentionalDifferences("div#fbw_body div.body", "The fast body has no inherited container decoration.", [["padding", "0px", undefined], ["border", "none", undefined]]);
addIntentionalDifferences("div.section", "Fast Mode omits section border colouring.", [["border-color", "#008000", undefined]]);
addIntentionalDifferences("div.cite", "Fast Mode uses a single subdued left marker instead of full decoration.", [
  ["color", "#660000", "#B46400"], ["border-color", "#660000", "#B46400"], ["border-left", undefined, "solid 1px"]
]);
addIntentionalDifferences("em", "Fast Mode omits the decorative emphasis colour while retaining italics.", [["color", "#0000E0", undefined]]);
addIntentionalDifferences("div.epigraph", "Fast Mode uses a simple left marker instead of decorative epigraph styling.", [
  ["font-size", "80%", undefined], ["border-color", "#00FFFF", "#0000F0"], ["color", "#FF0066", "#0000F0"],
  ["padding-left", undefined, "0.4em"], ["border-left", undefined, "solid 1px"]
]);
addIntentionalDifferences("div.annotation", "Fast Mode uses a simple annotation marker.", [
  ["border-color", "#00CC99", "#2C6B7B"], ["color", "#6633FF", "#2C6B7B"],
  ["padding-left", undefined, "0.4em"], ["border-left", undefined, "solid 1px"]
]);
for (const selector of ["div.history", "div.poem", "div.stanza", "div.table", "div.tr"]) {
  addIntentionalDifferences(selector, "Fast Mode represents structural containers with a simple left marker.", [
    ["padding-left", undefined, "0.4em"], ["border-left", undefined, "solid 1px"]
  ]);
}
addIntentionalDifferences("div.stanza p", "Fast Mode omits decorative stanza colour.", [["color", "#006600", undefined]]);
addIntentionalDifferences("div.title", "Fast Mode keeps title structure but omits enlarged and decorative title styling.", [
  ["font-size", "130%", undefined], ["border-color", "#008080", "rgb(0,150,0)"],
  ["background", "#008080", "rgb(0,150,0)"], ["padding", "0.3em", "0em 0em 0em 0.3em"],
  ["border-left", undefined, "solid 1px"]
]);
addIntentionalDifferences("p.subtitle", "Fast Mode omits decorative subtitle colour.", [["color", "#003300", undefined]]);
addIntentionalDifferences("p.text-author", "Fast Mode uses its simplified author colour.", [["color", "#9900CC", "rgb(192,64,64)"]]);
addIntentionalDifferences("span.code", "Fast Mode keeps inline code upright inside inherited emphasis.", [["font-style", undefined, "normal"]]);
addIntentionalDifferences("a.note", "Fast Mode keeps note markers at the inherited compact size.", [["font-size", "75%", undefined]]);
addIntentionalDifferences("strong", "Fast Mode relies on the browser's semantic strong styling and omits decorative colour.", [
  ["font-weight", "bold", undefined], ["color", "#660066", undefined]
]);

for (const [selector, properties] of sharedDeclarations) {
  const normalRule = normal.get(selector);
  const fastRule = fast.get(selector);
  assert(normalRule, `main.css: missing shared selector ${selector}`);
  assert(fastRule, `main_fast.css: missing shared selector ${selector}`);
  for (const property of properties) {
    assert(normalRule.has(property), `main.css: missing ${property} in ${selector}`);
    assert(fastRule.has(property), `main_fast.css: missing ${property} in ${selector}`);
    assert.strictEqual(fastRule.get(property), normalRule.get(property),
      `shared declaration drift: ${selector} { ${property} }`);
  }
}

assert.strictEqual(normalCss.selectorCounts.get("em"), 1, "main.css: em must have one rule");
assert.strictEqual(fastCss.selectorCounts.get("em"), 1, "main_fast.css: em must have one rule");

const intentionalByKey = new Map(intentionalDifferences.map(difference => [
  `${difference.selector}\u0000${difference.property}`, difference
]));
const actualDifferences = [];
for (const selector of new Set([...normal.keys(), ...fast.keys()])) {
  const normalRule = normal.get(selector) || new Map();
  const fastRule = fast.get(selector) || new Map();
  for (const property of new Set([...normalRule.keys(), ...fastRule.keys()])) {
    const normalValue = normalRule.get(property);
    const fastValue = fastRule.get(property);
    if (normalValue !== fastValue) actualDifferences.push({ selector, property, normalValue, fastValue });
  }
}

assert.strictEqual(intentionalByKey.size, intentionalDifferences.length, "intentional difference keys must be unique");
assert.strictEqual(actualDifferences.length, intentionalDifferences.length,
  "every CSS difference must be classified as intentional");
for (const difference of actualDifferences) {
  const expected = intentionalByKey.get(`${difference.selector}\u0000${difference.property}`);
  assert(expected, `unclassified CSS drift: ${difference.selector} { ${difference.property} }`);
  assert.strictEqual(difference.normalValue, expected.normalValue,
    `main.css changed intentional difference: ${difference.selector} { ${difference.property} }; ${expected.reason}`);
  assert.strictEqual(difference.fastValue, expected.fastValue,
    `main_fast.css changed intentional difference: ${difference.selector} { ${difference.property} }; ${expected.reason}`);
}

console.log("CSS regression checks passed");
