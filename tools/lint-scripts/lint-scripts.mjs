import { ESLint } from "eslint";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { createFingerprintEntries, fingerprintKey } from "./fingerprint.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const baselinePath = path.join(root, "tools/lint-scripts/baseline.json");
const writeBaseline = process.argv.includes("--write-baseline");
const eslint = new ESLint({ cwd: root });
const results = await eslint.lintFiles(["runtime/**/*.js"]);

function relative(filePath) {
  return path.relative(root, filePath).split(path.sep).join("/");
}

const sourceByFile = new Map(results.map((result) => [relative(result.filePath), readFileSync(result.filePath, "utf8")]));
const rawMessages = results.flatMap((result) => result.messages.map((message) => ({ result, filePath: relative(result.filePath), message })));
const entries = createFingerprintEntries(rawMessages, sourceByFile);
const messages = rawMessages.map((item, index) => ({ ...item, entry: entries[index] }));

if (writeBaseline) {
  const baseline = [...entries].sort((left, right) => fingerprintKey(left).localeCompare(fingerprintKey(right)));
  writeFileSync(baselinePath, `${JSON.stringify(baseline, null, 2)}\n`, "utf8");
  console.log(`Saved ${baseline.length} existing diagnostics to ${relative(baselinePath)}.`);
  process.exit(0);
}

if (!existsSync(baselinePath)) {
  console.error(`Missing baseline: ${relative(baselinePath)}. Run npm run lint:scripts:baseline intentionally.`);
  process.exit(1);
}

const baseline = JSON.parse(readFileSync(baselinePath, "utf8"));
const allowed = new Set(baseline.map(fingerprintKey));
const unexpected = messages.filter(({ entry: item }) => !allowed.has(fingerprintKey(item)));

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
  process.exit(1);
}

console.log(`Embedded-script lint passed; ${messages.length} existing diagnostic(s) are tracked in the baseline.`);
