import assert from "node:assert";
import fs from "node:fs";
import vm from "node:vm";

const source = fs.readFileSync("runtime/main.js", "utf8");
const css = fs.readFileSync("runtime/main.css", "utf8");
const fastCss = fs.readFileSync("runtime/main_fast.css", "utf8");
const begin = source.indexOf("function NotePreviewEvent");
const end = source.indexOf("function ShowFullImage", begin);
assert(begin >= 0 && end > begin, "note preview implementation must be present");

let timeoutId = 0;
let timeoutCalls = 0;
let clearCalls = 0;
const traceStages = [];
global.window = {
  setTimeout: () => { timeoutCalls += 1; return ++timeoutId; },
  clearTimeout: () => { clearCalls += 1; },
  getComputedStyle: () => ({ verticalAlign: "" })
};
global.notePreviewState = { panel: null, link: null, showTimer: null, hideTimer: null };
global.TraceDiagnosticEvent = (code, text) => traceStages.push(`${code}:${text}`);
global.document = {
  location: { href: "file:///book.fb2" },
  documentElement: { clientWidth: 800, clientHeight: 600, scrollLeft: 0, scrollTop: 0 },
  body: { clientWidth: 800, clientHeight: 600, scrollLeft: 0, scrollTop: 0 },
  elementFromPoint: () => null,
  getElementById: () => null,
  createTextNode: text => ({ nodeType: 3, text })
};
vm.runInThisContext(source.substring(begin, end), { filename: "runtime/main.js" });

function link(rawHref, expandedHref, hash, rect, align) {
  return {
    nodeType: 1, tagName: "A", className: "note", href: expandedHref, hash,
    currentStyle: { verticalAlign: align || "" }, parentNode: null,
    getAttribute: (name, flags) => name === "href" && flags === 2 ? rawHref : rawHref,
    getBoundingClientRect: () => rect || { left: 10, top: 10, right: 20, bottom: 20 }
  };
}

assert.strictEqual(GetNotePreviewTargetId(link("#n1", "file:///book.fb2#n1", "#n1")), "n1");
assert.strictEqual(GetNotePreviewTargetId(link(null, "file:///book.fb2#n1", "#n1")), "n1");
assert.strictEqual(GetNotePreviewTargetId(link("", "", "#n1")), "n1");
assert.strictEqual(GetNotePreviewTargetId(link("#", "file:///book.fb2#", "")), "");
assert.strictEqual(GetNotePreviewTargetId(link("fbw-internal:#n1", "", "")), "n1");
assert.strictEqual(GetNotePreviewTargetId(link("https://example.com/page#n1", "https://example.com/page#n1", "#n1")), "", "external URLs must not be previewed");
document.URL = "file:///book.fb2";
document.location.href = "about:blank";
assert.strictEqual(GetNotePreviewTargetId(link("", "file:///book.fb2#n1", "#n1")), "n1", "MSHTML document.URL must resolve an expanded local note URL");
document.URL = "file:///D:/Download/FBeditor/out/Release/main.html";
document.location.href = "about:blank";
assert.strictEqual(GetNotePreviewTargetId(link("", "FILE:///d%3A/Download/FBeditor/out/Release/main.html#n1", "#n1")), "n1",
  "MSHTML file URL spellings for the current document must resolve a note target");
assert.strictEqual(GetNotePreviewTargetId(link("", "file:///D:/Download/other.html#n1", "#n1")), "",
  "a different file document must remain external");
delete document.URL;
document.location.href = "file:///book.fb2";
assert(IsNotePreviewLink(link("file:///book.fb2#n1", "file:///book.fb2#n1", "#n1")));

const body = { nodeType: 1, tagName: "DIV", getElementsByTagName: () => { throw new Error("O(N) link scan"); } };
const superLink = link("#n1", "file:///book.fb2#n1", "#n1", { left: 10, top: 100, right: 20, bottom: 105 }, "super");
superLink.parentNode = body;
document.elementFromPoint = (x, y) => y <= 102 && x >= 6 && x <= 24 ? superLink : body;
assert.strictEqual(FindNotePreviewLinkAt(body, 15, 110, body), superLink, "super hit-area must extend down");
const subLink = link("#n2", "file:///book.fb2#n2", "#n2", { left: 10, top: 100, right: 20, bottom: 105 }, "sub");
subLink.parentNode = body;
document.elementFromPoint = (x, y) => y >= 103 && x >= 6 && x <= 24 ? subLink : body;
assert.strictEqual(FindNotePreviewLinkAt(body, 15, 95, body), subLink, "sub hit-area must extend up");

notePreviewState.hideTimer = null;
ScheduleNotePreviewHide();
ScheduleNotePreviewHide();
assert.strictEqual(timeoutCalls, 1, "hide timer must not restart on every mousemove");
ScheduleNotePreview(superLink);
assert(clearCalls >= 1 && notePreviewState.hideTimer === null, "returning to a note cancels pending hide");

let attachedDocumentMouseMove = null;
document.attachEvent = (name, handler) => { if(name === "onmousemove") attachedDocumentMouseMove = handler; };
InitNotePreview(body);
assert(attachedDocumentMouseMove, "legacy MSHTML must receive a delegated document mousemove fallback");
attachedDocumentMouseMove({ clientX: 15, clientY: 110, srcElement: superLink });
assert.strictEqual(notePreviewState.link, superLink, "document-level fallback must preserve note hover detection");
delete document.attachEvent;

const child = { nodeType: 1, attributes: [
  { nodeName: "id", specified: true }, { nodeName: "onclick", specified: true },
  { nodeName: "onmouseover", specified: false }, { nodeName: "contenteditable", specified: true },
  { nodeName: "class", specified: true }], firstChild: null,
  removeAttribute(name) { this.attributes = this.attributes.filter(attribute => attribute.nodeName !== name); } };
SanitizeNotePreviewNode(child);
assert.deepStrictEqual(child.attributes.map(attribute => attribute.nodeName), ["onmouseover", "class"], "MSHTML nodeName attributes must be sanitized safely");
assert.notStrictEqual(child.contentEditable, true, "read-only ownership belongs to the popup, not every inline clone");
const emphasis = { nodeType: 1, attributes: [], firstChild: null, nextSibling: null, removeAttribute() {} };
SanitizeNotePreviewNode({ nodeType: 1, attributes: [], firstChild: emphasis, removeAttribute() {} });
assert.strictEqual(emphasis.contentEditable, undefined, "nested emphasis must not receive an MSHTML-breaking contentEditable mutation");

const appended = [];
function previewNode(tagName, className) {
  return { nodeType: 1, tagName, className: className || "", nextSibling: null,
    cloneNode() { return { nodeType: 1, attributes: [], firstChild: null, removeAttribute() {} }; } };
}
const title = previewNode("DIV", "title");
const paragraph = previewNode("P");
title.nextSibling = paragraph;
AppendNotePreviewContent({ appendChild(item) { appended.push(item); } }, { firstChild: title });
assert.strictEqual(appended.length, 1, "only an immediate section title may be omitted from preview content");

const longPanel = { style: {}, offsetWidth: 420, offsetHeight: 420, scrollHeight: 1000 };
FitNotePreview(longPanel, superLink);
assert.strictEqual(longPanel.style.fontSize, "12.8px", "font size is reduced before scrolling a long note");
assert.strictEqual(longPanel.style.overflow, "auto", "scrollbar is the final fallback for a long note");

const compactPanel = { style: {}, offsetWidth: 280, offsetHeight: 40, scrollHeight: 40 };
FitNotePreview(compactPanel, superLink);
assert.strictEqual(compactPanel.style.width, "280px", "short notes must use the compact first width");
assert.strictEqual(compactPanel.style.fontSize, "16px", "a short note keeps the normal font size");
assert.strictEqual(compactPanel.style.overflow, "hidden", "a fitting note remains compact without a scrollbar");

const fitAttempts = [];
const expandingPanel = { style: {}, offsetWidth: 500, offsetHeight: 300 };
Object.defineProperty(expandingPanel, "scrollHeight", { get() {
  fitAttempts.push(`${this.style.fontSize}:${this.style.width}`);
  return Number.parseInt(this.style.width, 10) >= 500 ? 300 : 1000;
} });
FitNotePreview(expandingPanel, superLink);
assert.strictEqual(expandingPanel.style.width, "500px");
assert.strictEqual(expandingPanel.style.fontSize, "16px", "popup must widen before reducing its font");
assert(fitAttempts.indexOf("16px:500px") >= 0 && fitAttempts.every(attempt => !attempt.startsWith("15.2px")), "all fitting widths are tried at normal font size first");

const minimumFontPanel = { style: {}, currentStyle: { fontSize: "12px" }, offsetWidth: 600, offsetHeight: 420, scrollHeight: 1000 };
FitNotePreview(minimumFontPanel, superLink);
assert.strictEqual(minimumFontPanel.style.fontSize, "11.4px", "absolute font minimum prevents further shrinking");
assert(Number.parseFloat(minimumFontPanel.style.fontSize) >= 11, "font must not fall below the absolute minimum");
minimumFontPanel.scrollHeight = 40;
FitNotePreview(minimumFontPanel, superLink);
assert.strictEqual(minimumFontPanel.style.fontSize, "12px", "a following short note resets a previous reduced font size");
assert.strictEqual(minimumFontPanel.style.width, "280px");

const roundingPanel = { style: {}, offsetWidth: 600, offsetHeight: 420, scrollHeight: 420 };
FitNotePreview(roundingPanel, superLink);
assert.strictEqual(roundingPanel.style.overflow, "auto", "a near-limit note must scroll rather than clip its final line");
assert.strictEqual(GetNotePreviewNaturalHeight({ scrollHeight: 40, offsetHeight: 40,
  firstChild: { nodeType: 1, offsetTop: 12, offsetHeight: 68, nextSibling: null } }), 80,
  "MSHTML child line boxes must extend the measured preview height");
assert.match(css, /div#fbNotePreview\{[\s\S]*border: 1px solid #808080;/, "preview needs a subtle boundary");
assert.match(css, /div#fbNotePreview p\{[\s\S]*text-align: left;/, "preview paragraphs must not inherit justified book text");
for (const property of ["visibility: hidden", "position: absolute", "z-index: 1000", "box-sizing: border-box", "background: #ffffff", "color: #000", "border: 1px solid #808080", "overflow: hidden", "line-height: normal", "text-align: left"]) {
  assert(fastCss.includes(property), `Fast Mode preview must include ${property}`);
}
assert.match(fastCss, /div#fbNotePreview p\{[\s\S]*text-align: left;/, "Fast Mode preview paragraphs must not inherit justified text");

const originalGetElementById = document.getElementById;
document.getElementById = id => id === "fbw_body" ? { currentStyle: {
  backgroundColor: "#20242a", color: "#e8edf2", fontFamily: "Georgia", fontSize: "18px"
} } : null;
assert.deepStrictEqual(GetNotePreviewColors(), { background: "#20242a", color: "#e8edf2" }, "dark Body colors must be copied to the popup");
const themedPanel = { style: {} };
ApplyNotePreviewBodyStyle(themedPanel);
assert.deepStrictEqual(themedPanel.style, {
  backgroundColor: "#20242a", color: "#e8edf2", fontFamily: "Georgia", fontSize: "18px"
}, "popup must inherit Body background, text color, family and size");
assert.deepStrictEqual(GetNotePreviewFontSizes(themedPanel), [18, 17.1, 16.2, 15.3, 14.4],
  "font fit steps must be resolved from the Body 18px size");
const bodySizedFitAttempts = [];
const bodySizedPanel = { style: {}, currentStyle: { fontSize: "18px" }, offsetWidth: 600, offsetHeight: 420 };
Object.defineProperty(bodySizedPanel, "scrollHeight", { get() {
  bodySizedFitAttempts.push(this.style.fontSize);
  return this.style.fontSize === "17.1px" ? 40 : 1000;
} });
FitNotePreview(bodySizedPanel, superLink);
assert.strictEqual(bodySizedFitAttempts[0], "18px", "first fit must use the exact Body pixel size");
assert.strictEqual(bodySizedPanel.style.fontSize, "17.1px", "95% fit must resolve to 17.1px from Body 18px");
document.getElementById = originalGetElementById;

const previewItems = [];
const previewPanel = { style: {}, offsetWidth: 200, offsetHeight: 80,
  appendChild(item) { previewItems.push(item); } };
Object.defineProperty(previewPanel, "innerHTML", { set: () => { previewItems.length = 0; } });
const previewChild = { nodeType: 1, attributes: [{ nodeName: "id", specified: true }], firstChild: null, nextSibling: null,
  removeAttribute(name) { this.attributes = this.attributes.filter(attribute => attribute.nodeName !== name); },
  cloneNode() { return { nodeType: 1, attributes: [{ nodeName: "id", specified: true }], firstChild: null,
    removeAttribute(name) { this.attributes = this.attributes.filter(attribute => attribute.nodeName !== name); } }; } };
const noteTarget = { firstChild: previewChild };
const bodyStyle = { currentStyle: { backgroundColor: "#20242a", color: "#e8edf2", fontFamily: "Georgia", fontSize: "18px" } };
document.getElementById = id => id === "n1" ? noteTarget : (id === "fbw_body" ? bodyStyle : null);
notePreviewState.panel = previewPanel;
notePreviewState.link = superLink;
const originalFitNotePreview = FitNotePreview;
let fitReceivedBodyStyle = false;
FitNotePreview = panel => {
  fitReceivedBodyStyle = panel.style.fontFamily === "Georgia" && panel.style.fontSize === "18px";
	return originalFitNotePreview(panel, superLink);
};
ShowNotePreview(superLink);
FitNotePreview = originalFitNotePreview;
assert(fitReceivedBodyStyle, "Body typography must be applied before FitNotePreview");
assert.strictEqual(previewItems.length, 1, "existing target must be previewed");
assert.strictEqual(previewItems[0].attributes.length, 0, "preview clone must not retain IDs");
assert.strictEqual(previewPanel.style.overflow, "hidden", "short notes must remain compact without a scrollbar");
const brokenLink = link("#missing", "file:///book.fb2#missing", "#missing");
notePreviewState.link = brokenLink;
ShowNotePreview(brokenLink);
assert.strictEqual(previewItems[0].text, "Примечание не найдено", "broken target must remain non-fatal");
assert(traceStages.some(stage => stage.includes("hover-detected")));
assert(traceStages.some(stage => stage.includes("target-resolved")));
assert(traceStages.some(stage => stage.includes("clone-sanitized")));
assert(traceStages.some(stage => stage.includes("popup-visible")));

console.log("note preview regression tests passed");
