import { existsSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { compareJscriptBaseline } from "./jscript-fingerprint.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const parser = path.join(root, "tools/lint-scripts/jscript-compat.js");
const runtime = path.join(root, "runtime");
const baselinePath = path.join(root, "tools/lint-scripts/jscript-baseline.json");
const writeBaseline = process.argv.includes("--write-baseline");
const result = spawnSync("cscript.exe", ["//nologo", parser, runtime, "--list"], { encoding: "utf8" });
if (result.error) throw result.error;
if (result.status !== 0) process.exit(result.status ?? 1);
const current = result.stdout.split(/\r?\n/).filter((line) => line.startsWith("FAIL ")).map((line) => line.slice(5)).sort();
const toEntry = (key) => {
  const [encodedFile, scriptBlock, errorNumber, sourceHash] = key.split("|");
  return { file: decodeURIComponent(encodedFile), scriptBlock: Number(scriptBlock), errorNumber: Number(errorNumber), sourceHash };
};

if (writeBaseline) {
  writeFileSync(baselinePath, `${JSON.stringify(current.map(toEntry), null, 2)}\n`, "utf8");
  console.log(`Saved ${current.length} Microsoft JScript compatibility exception(s).`);
  process.exit(0);
}
if (!existsSync(baselinePath)) throw new Error(`Missing JScript baseline: ${path.relative(root, baselinePath)}.`);
const expected = JSON.parse(readFileSync(baselinePath, "utf8"));
const { unexpected, stale } = compareJscriptBaseline(current.map(toEntry), expected);
if (unexpected.length || stale.length) {
  for (const entry of unexpected) {
    console.error(`Unsupported Microsoft JScript syntax: ${entry.file}, script block ${entry.scriptBlock}, error ${entry.errorNumber}, context ${entry.sourceHash}.`);
  }
  for (const entry of stale) {
    console.error(`Stale Microsoft JScript baseline exception: ${entry.file}, script block ${entry.scriptBlock}, error ${entry.errorNumber}, context ${entry.sourceHash}.`);
  }
  process.exit(1);
}
console.log(`Microsoft JScript compatibility passed; ${current.length} historical exception(s) are tracked.`);
