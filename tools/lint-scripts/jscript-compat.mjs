import { existsSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

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
  const [encodedFile, scriptBlock] = key.split("|");
  return { file: decodeURIComponent(encodedFile), scriptBlock: Number(scriptBlock) };
};
const toKey = (entry) => `${encodeURIComponent(entry.file)}|${entry.scriptBlock}`;

if (writeBaseline) {
  writeFileSync(baselinePath, `${JSON.stringify(current.map(toEntry), null, 2)}\n`, "utf8");
  console.log(`Saved ${current.length} Microsoft JScript compatibility exception(s).`);
  process.exit(0);
}
if (!existsSync(baselinePath)) throw new Error(`Missing JScript baseline: ${path.relative(root, baselinePath)}.`);
const expected = JSON.parse(readFileSync(baselinePath, "utf8"));
const expectedSet = new Set(expected.map(toKey));
const currentSet = new Set(current);
const unexpected = current.filter((item) => !expectedSet.has(item));
const stale = expected.map(toKey).filter((item) => !currentSet.has(item));
if (unexpected.length || stale.length) {
  for (const item of unexpected) console.error(`Unsupported Microsoft JScript syntax: ${decodeURIComponent(item.split("|")[0])}, script block ${item.split("|")[1]}.`);
  for (const item of stale) console.error(`Stale Microsoft JScript baseline exception: ${decodeURIComponent(item.split("|")[0])}, script block ${item.split("|")[1]}.`);
  process.exit(1);
}
console.log(`Microsoft JScript compatibility passed; ${current.length} historical exception(s) are tracked.`);
