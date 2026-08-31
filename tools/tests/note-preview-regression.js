import assert from "node:assert";
import fs from "node:fs";
import vm from "node:vm";

const source = fs.readFileSync("runtime/main.js", "utf8");
const begin = source.indexOf("function NotePreviewEvent");
const end = source.indexOf("function ShowFullImage", begin);
assert(begin >= 0 && end > begin, "note preview implementation must be present");

let timeoutId = 0;
let timeoutCalls = 0;
let clearCalls = 0;
global.window = {
  setTimeout: () => { timeoutCalls += 1; return ++timeoutId; },
  clearTimeout: () => { clearCalls += 1; },
  getComputedStyle: () => ({ verticalAlign: "" })
};
global.notePreviewState = { panel: null, link: null, showTimer: null, hideTimer: null };
global.document = {
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

const child = { nodeType: 1, attributes: [{ name: "id" }, { name: "onclick" }, { name: "contenteditable" }, { name: "class" }], firstChild: null,
  removeAttribute(name) { this.attributes = this.attributes.filter(attribute => attribute.name !== name); } };
SanitizeNotePreviewNode(child);
assert.deepStrictEqual(child.attributes.map(attribute => attribute.name), ["class"]);
assert.strictEqual(child.contentEditable, false);

const previewItems = [];
const previewPanel = { style: {}, offsetWidth: 200, offsetHeight: 80,
  appendChild(item) { previewItems.push(item); } };
Object.defineProperty(previewPanel, "innerHTML", { set: () => { previewItems.length = 0; } });
const previewChild = { nodeType: 1, attributes: [{ name: "id" }], firstChild: null, nextSibling: null,
  removeAttribute(name) { this.attributes = this.attributes.filter(attribute => attribute.name !== name); },
  cloneNode() { return { nodeType: 1, attributes: [{ name: "id" }], firstChild: null,
    removeAttribute(name) { this.attributes = this.attributes.filter(attribute => attribute.name !== name); } }; } };
const noteTarget = { firstChild: previewChild };
document.getElementById = id => id === "n1" ? noteTarget : null;
notePreviewState.panel = previewPanel;
notePreviewState.link = superLink;
ShowNotePreview(superLink);
assert.strictEqual(previewItems.length, 1, "existing target must be previewed");
assert.strictEqual(previewItems[0].attributes.length, 0, "preview clone must not retain IDs");
const brokenLink = link("#missing", "file:///book.fb2#missing", "#missing");
notePreviewState.link = brokenLink;
ShowNotePreview(brokenLink);
assert.strictEqual(previewItems[0].text, "Примечание не найдено", "broken target must remain non-fatal");

console.log("note preview regression tests passed");
