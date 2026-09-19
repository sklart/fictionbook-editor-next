import assert from "node:assert/strict";
import { compareJscriptBaseline, jscriptFingerprintKey } from "./jscript-fingerprint.mjs";

const baseline = [{ file: "runtime/dialog.html", scriptBlock: 1, errorNumber: -2146827286, sourceHash: "0abc1234" }];
assert.equal(compareJscriptBaseline(baseline, baseline).unexpected.length, 0, "Точное исключение JScript должно быть разрешено.");
const changedContext = [{ ...baseline[0], sourceHash: "0def5678" }];
const changed = compareJscriptBaseline(changedContext, baseline);
assert.equal(changed.unexpected.length, 1, "Изменённый контекст ошибки в том же блоке должен блокировать CI.");
assert.equal(changed.stale.length, 1, "Старый fingerprint должен требовать обновления baseline.");
const changedError = [{ ...baseline[0], errorNumber: -2146827260 }];
assert.notEqual(jscriptFingerprintKey(changedError[0]), jscriptFingerprintKey(baseline[0]), "Код ошибки входит в fingerprint.");
assert.equal(compareJscriptBaseline(changedError, baseline).unexpected.length, 1, "Новая ошибка в исключённом блоке должна блокировать CI.");
console.log("Microsoft JScript fingerprint regression passed.");
