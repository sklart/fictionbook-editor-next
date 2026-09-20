import assert from "node:assert/strict";
import fs from "node:fs";

function parseCss(path) {
  // This deliberately small parser supports only the current flat CSS files:
  // no @media blocks, nested rules, or other advanced CSS constructs.
  const source = fs.readFileSync(path, "utf8");
  const withoutComments = source.replace(/\/\*[^]*?\*\//g, "");
  assert(!/\/\*/.test(withoutComments), `${path}: unterminated comment`);

  const rules = new Map();
  const selectorCounts = new Map();
	const orderedRules = [];
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
		orderedRules.push({ selector, declarations, order: orderedRules.length });
  }
  return { rules, selectorCounts, orderedRules };
}

function selectorSpecificity(selector) {
	const ids = (selector.match(/#[a-z0-9_-]+/gi) || []).length;
	const classes = (selector.match(/\.[a-z0-9_-]+/gi) || []).length;
	const tags = (selector.match(/(?:^|\s)[a-z][a-z0-9_-]*/gi) || []).length;
	return ids * 100 + classes * 10 + tags;
}

function matchesSimpleSelector(token, element) {
	const tag = token.match(/^[a-z][a-z0-9_-]*/i);
	const id = token.match(/#([a-z0-9_-]+)/i);
	const classes = [...token.matchAll(/\.([a-z0-9_-]+)/gi)].map(match => match[1]);
	return (!tag || element.tag === tag[0].toLowerCase()) && (!id || element.id === id[1]) && classes.every(name => element.classes.includes(name));
}

function matchesSelector(selector, ancestors) {
	if (selector.includes(",") || selector.includes(":")) return false;
	const tokens = selector.split(" ");
	let ancestor = ancestors.length - 1;
	for (let index = tokens.length - 1; index >= 0; --index) {
		while (ancestor >= 0 && !matchesSimpleSelector(tokens[index], ancestors[ancestor])) --ancestor;
		if (ancestor < 0) return false;
		--ancestor;
	}
	return true;
}

function paddingValues(value) {
	const values = value.split(/\s+/);
	if (values.length === 1) return [values[0], values[0], values[0], values[0]];
	if (values.length === 2) return [values[0], values[1], values[0], values[1]];
	if (values.length === 3) return [values[0], values[1], values[2], values[1]];
	return values;
}

function normalizeCssValue(value) {
	return /^[-+]?0(?:\.0+)?(?:em|px|pt|%)$/i.test(value) ? "0" : value;
}

function resolveComputedPadding(css, ancestors) {
	const values = { "padding-top": "0", "padding-right": "0", "padding-bottom": "0", "padding-left": "0" };
	const precedence = {};
	for (const rule of css.orderedRules) {
		if (!matchesSelector(rule.selector, ancestors)) continue;
		const specificity = selectorSpecificity(rule.selector);
		for (const [property, value] of rule.declarations) {
			const declarations = property === "padding"
				? [["padding-top", paddingValues(value)[0]], ["padding-right", paddingValues(value)[1]], ["padding-bottom", paddingValues(value)[2]], ["padding-left", paddingValues(value)[3]]]
				: [[property, value]];
			for (const [resolvedProperty, resolvedValue] of declarations) {
				const previous = precedence[resolvedProperty];
				if (!previous || specificity > previous.specificity || (specificity === previous.specificity && rule.order >= previous.order)) {
					values[resolvedProperty] = normalizeCssValue(resolvedValue);
					precedence[resolvedProperty] = { specificity, order: rule.order };
				}
			}
		}
	}
	return values;
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
  ["span.image", ["text-indent", "margin", "padding", "border", "overflow", "clear", "text-align"]],
  ["strong", ["font-weight"]]
];

// Fast Mode removes the base decoration from every document div. Individual
// structural selectors explicitly restore the minimal marker they inherit in
// Normal Mode. Every remaining source-level difference is listed here so a
// new, unclassified drift fails the test.
const intentionalDifferences = [];
function addIntentionalDifferences(selector, reason, declarations) {
  for (const [property, normalValue, fastValue] of declarations) {
    intentionalDifferences.push({ selector, property, normalValue, fastValue, reason });
  }
}

addIntentionalDifferences("div#fbw_body div", "Fast Mode removes per-div indentation and borders for large documents.", [
  ["padding", "0em 0em 0em 0.4em", undefined], ["border-left", "solid 1px", undefined]
]);
for (const selector of ["div.epigraph", "div.annotation", "div.history", "div.poem", "div.stanza", "div.table", "div.tr"]) {
  addIntentionalDifferences(selector, "Restores the structural left marker removed from the fast base div rule.", [
    ["padding-left", undefined, "0.4em"], ["border-left", undefined, "solid 1px"]
  ]);
}
for (const selector of ["div#fbw_body div.cite", "div#fbw_body div.title"]) {
  addIntentionalDifferences(selector, "Restores the structural left marker removed from the fast base div rule.", [["border-left", undefined, "solid 1px"]]);
}
addIntentionalDifferences("strong", "Fast Mode omits only the decorative strong colour.", [["color", "#660066", undefined]]);

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

for (const [className, expected] of [["cite", ["0", "2em", "0", "2em"]], ["title", ["0.3em", "0.3em", "0.3em", "0.3em"]]]) {
	const hierarchy = [{ tag: "div", id: "fbw_body", classes: [] }, { tag: "div", id: "", classes: [className] }];
	const normalPadding = resolveComputedPadding(normalCss, hierarchy);
	const fastPadding = resolveComputedPadding(fastCss, hierarchy);
	const values = ["padding-top", "padding-right", "padding-bottom", "padding-left"];
	assert.deepStrictEqual(values.map(property => normalPadding[property]), expected, `Normal Mode computed ${className} padding`);
	assert.deepStrictEqual(values.map(property => fastPadding[property]), expected, `Fast Mode computed ${className} padding`);
}

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
