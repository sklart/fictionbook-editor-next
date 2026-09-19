import assert from "node:assert/strict";
import { ESLint } from "eslint";
import path from "node:path";
import { extractInlineScriptsFromSource } from "./inline-html.mjs";

const source = [
  "<html>",
  "<script src=\"vendor.js\"></script>",
  "<script>",
  "var legacy = missing;",
  "</script>",
  "</html>"
].join("\n");
const scripts = extractInlineScriptsFromSource("runtime/dialog.html", source);
assert.equal(scripts.length, 1, "Внешний script src не должен извлекаться.");
assert.equal(scripts[0].startLine, 3, "Диагностика inline script должна указывать строку HTML.");
assert.equal(scripts[0].paddedCode.split("\n")[3], "var legacy = missing;", "Код должен сохранять исходный номер строки.");
const eslint = new ESLint({
  overrideConfigFile: true,
  overrideConfig: [{
    files: ["runtime/**/*.html"],
    languageOptions: { ecmaVersion: 3, sourceType: "script" },
    rules: { "no-undef": "error" }
  }]
});
const [result] = await eslint.lintText(scripts[0].paddedCode, { filePath: path.resolve(scripts[0].filePath) });
assert.equal(result.messages[0].ruleId, "no-undef", "Inline JavaScript должен проходить ESLint.");
assert.equal(result.messages[0].line, 4, "ESLint должен сообщать строку исходного HTML.");
console.log("Inline HTML script extraction regression passed.");
