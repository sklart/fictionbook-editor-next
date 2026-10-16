import assert from "node:assert/strict";
import { ESLint } from "eslint";
import path from "node:path";
import { isNoteCreationPath, normalizeComIndexedProperty } from "./com-indexed-property.mjs";

const filePath = "runtime/Scripts/02_Примечания/02_Добавить примечание, введя его текст в окошке.js";
const statement = 'if (addSnoska && strConst3=="") window.external.inflateBlock(newSection.lastChild)=true;';
const source = `function Run() {\n  ${statement}\n}`;
const normalized = normalizeComIndexedProperty(filePath, source);
assert.ok(isNoteCreationPath(filePath));
assert.equal(normalized, `function Run() {\n  if (addSnoska && strConst3=="") window.external.inflateBlock(newSection.lastChild);\n}`);
assert.equal(normalizeComIndexedProperty("runtime/Scripts/05_Иллюстрации/example.js", source), source,
  "Other script families retain their original lint diagnostics.");

const eslint = new ESLint();
const absolutePath = path.resolve(filePath);
const lint = async (text) => (await eslint.lintText(normalizeComIndexedProperty(filePath, text), { filePath: absolutePath }))[0];
assert.equal((await lint(source)).fatalErrorCount, 0, "The confirmed COM statement must parse in the lint copy.");
for (const invalid of [
  statement.replace("newSection.lastChild", "other.lastChild"),
  statement.replace("=true", "=false"),
  statement.replace("inflateBlock", "inflateBlocks"),
  `${statement} invalid syntax`
]) {
  const candidate = `function Run() {\n  ${invalid}\n}`;
  assert.equal(normalizeComIndexedProperty(filePath, candidate), candidate,
    "A similar or malformed statement must not be normalized.");
  assert.ok((await lint(candidate)).fatalErrorCount > 0, "ESLint must still reject the invalid statement.");
}
console.log("COM indexed-property lint normalization regression passed.");
