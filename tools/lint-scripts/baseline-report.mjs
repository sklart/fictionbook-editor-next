import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const baseline = JSON.parse(readFileSync(path.join(root, "tools/lint-scripts/baseline.json"), "utf8"));

function category(file) {
  if (file.startsWith("runtime/Scripts/")) return "runtime/Scripts/**";
  if (file.startsWith("runtime/HTML/")) return "runtime/HTML/**";
  if (file === "runtime/main.js") return "runtime/main.js";
  return "runtime/other";
}

function extension(file) {
  return file.toLowerCase().endsWith(".html") ? ".html" : ".js";
}

function countBy(items, select) {
  const counts = new Map();
  for (const item of items) {
    const key = select(item);
    counts.set(key, (counts.get(key) ?? 0) + 1);
  }
  return [...counts.entries()].sort(([leftKey, leftCount], [rightKey, rightCount]) =>
    rightCount - leftCount || leftKey.localeCompare(rightKey));
}

function print(title, entries) {
  console.log(`\n${title}`);
  for (const [name, count] of entries) console.log(`${String(count).padStart(6)}  ${name}`);
}

console.log(`ESLint legacy baseline: ${baseline.length} diagnostic(s)`);
print("By runtime area:", countBy(baseline, (entry) => category(entry.file)));
print("By file type:", countBy(baseline, (entry) => extension(entry.file)));
print("By rule:", countBy(baseline, (entry) => entry.ruleId ?? "parse"));
print("Top files:", countBy(baseline, (entry) => entry.file).slice(0, 10));
