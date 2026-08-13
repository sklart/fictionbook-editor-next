import assert from "node:assert/strict";
import { createFingerprintEntries, fingerprintKey } from "./fingerprint.mjs";

function entries(source, diagnostics) {
  const filePath = "runtime/test.js";
  return createFingerprintEntries(diagnostics.map((message) => ({ filePath, message })), new Map([[filePath, source]]));
}

const original = entries("var old = missing;\n", [{ line: 1, column: 11, ruleId: "no-undef", message: "'missing' is not defined." }]);
const shifted = entries("// harmless line\n// another harmless line\nvar old = missing;\n", [{ line: 3, column: 11, ruleId: "no-undef", message: "'missing' is not defined." }]);
assert.equal(fingerprintKey(original[0]), fingerprintKey(shifted[0]), "Сдвиг строк не должен менять fingerprint старого diagnostic.");

const added = entries("var old = missing;\nvar newValue = unknown;\n", [
  { line: 1, column: 11, ruleId: "no-undef", message: "'missing' is not defined." },
  { line: 2, column: 16, ruleId: "no-undef", message: "'unknown' is not defined." }
]);
const baseline = new Set(original.map(fingerprintKey));
assert.equal(added.filter((item) => !baseline.has(fingerprintKey(item))).length, 1, "Новый diagnostic должен отличаться от baseline.");
console.log("ESLint fingerprint regression passed.");
