import { ESLint } from "eslint";
import { existsSync, readdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { createFingerprintEntries, fingerprintKey } from "./fingerprint.mjs";
import { compareBaseline } from "./baseline.mjs";
import { extractInlineScripts } from "./inline-html.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const baselinePath = path.join(root, "tools/lint-scripts/baseline.json");
const writeBaseline = process.argv.includes("--write-baseline");
const eslint = new ESLint({ cwd: root });
const jsResults = await eslint.lintFiles(["runtime/**/*.js"]);
const htmlPaths = readdirSync(path.join(root, "runtime"), { recursive: true, withFileTypes: true })
  .filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith(".html"))
  .map((entry) => path.join(entry.parentPath ?? entry.path, entry.name))
  .filter((filePath) => !/[/\\](generated|vendor)[/\\]/i.test(filePath));
const inlineScripts = htmlPaths.flatMap((filePath) => extractInlineScripts(filePath));
const inlineResults = await Promise.all(inlineScripts.map(async (script) => {
  const [result] = await eslint.lintText(script.paddedCode, { filePath: script.filePath });
  return result;
}));
const results = [...jsResults, ...inlineResults];

function relative(filePath) {
  return path.relative(root, filePath).split(path.sep).join("/");
}

const sourceByFile = new Map(results.map((result) => [relative(result.filePath), readFileSync(result.filePath, "utf8")]));
const rawMessages = results.flatMap((result) => result.messages.map((message) => ({ result, filePath: relative(result.filePath), message })));
const entries = createFingerprintEntries(rawMessages, sourceByFile);
const messages = rawMessages.map((item, index) => ({ ...item, entry: entries[index] }));

if (writeBaseline) {
  const baseline = [...entries].sort((left, right) => fingerprintKey(left).localeCompare(fingerprintKey(right)));
  const temporaryBaselinePath = `${baselinePath}.new`;
  writeFileSync(temporaryBaselinePath, `${JSON.stringify(baseline, null, 2)}\n`, "utf8");
  renameSync(temporaryBaselinePath, baselinePath);
  console.log(`Saved ${baseline.length} existing diagnostics to ${relative(baselinePath)}.`);
  process.exit(0);
}

if (!existsSync(baselinePath)) {
  console.error(`Missing baseline: ${relative(baselinePath)}. Run npm run lint:scripts:baseline intentionally.`);
  process.exit(1);
}

const baseline = JSON.parse(readFileSync(baselinePath, "utf8"));
const { unexpected: unexpectedEntries, stale } = compareBaseline(entries, baseline);
const unexpected = messages.filter(({ entry }) => unexpectedEntries.some((item) => fingerprintKey(item) === fingerprintKey(entry)));

if (unexpected.length > 0) {
  const formatter = await eslint.loadFormatter("stylish");
  const affected = new Map();
  for (const { result, message } of unexpected) {
    const copy = affected.get(result.filePath) || { ...result, messages: [] };
    copy.messages.push(message);
    affected.set(result.filePath, copy);
  }
  process.stderr.write(formatter.format([...affected.values()]));
  console.error(`Found ${unexpected.length} diagnostic(s) not present in the baseline.`);
}

if (stale.length > 0) {
  console.error(`Found ${stale.length} stale baseline diagnostic(s); update baseline.json intentionally.`);
  for (const item of stale) {
    console.error(`  ${item.file}: ${item.ruleId ?? "parse"}: ${item.message}`);
  }
}

if (unexpected.length > 0 || stale.length > 0) process.exit(1);

console.log(`Embedded-script lint passed; ${messages.length} diagnostic(s) are tracked in the baseline.`);
