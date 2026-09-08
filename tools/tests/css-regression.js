import assert from "node:assert/strict";
import fs from "node:fs";

function parseCss(path) {
  const source = fs.readFileSync(path, "utf8");
  const withoutComments = source.replace(/\/\*[^]*?\*\//g, "");
  assert(!/\/\*/.test(withoutComments), `${path}: unterminated comment`);

  const rules = new Map();
  const blocks = withoutComments.matchAll(/([^{}]+)\{([^{}]*)\}/g);
  const consumed = withoutComments.replace(/[^{}]+\{[^{}]*\}/g, "");
  assert(!/[{}]/.test(consumed), `${path}: unbalanced or nested rule block`);

  for (const block of blocks) {
    const selector = block[1].trim().replace(/\s+/g, " ");
    assert(selector, `${path}: empty selector`);
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
  return rules;
}

const normal = parseCss("runtime/main.css");
const fast = parseCss("runtime/main_fast.css");

// These declarations define shared editor/UI behaviour.  Deliberately omitted:
// fast-mode layout, colour, border and typography simplifications.
const sharedDeclarations = [
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
  ["div#fbw_desc textarea#stylesheetId", ["width", "font-family", "border"]],
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

console.log("CSS regression checks passed");
