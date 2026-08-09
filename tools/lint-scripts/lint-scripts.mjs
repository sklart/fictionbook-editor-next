import { ESLint } from "eslint";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const baselinePath = path.join(root, "tools/lint-scripts/baseline.json");
const writeBaseline = process.argv.includes("--write-baseline");
const eslint = new ESLint({ cwd: root });
const results = await eslint.lintFiles(["runtime/**/*.js"]);

function relative(filePath) {
  return path.relative(root, filePath).split(path.sep).join("/");
}

function entry(filePath, message) {
  return {
    file: relative(filePath),
    line: message.line,
    column: message.column,
    ruleId: message.ruleId,
    message: message.message
  };
}

function key(item) {
  return [item.file, item.line, item.column, item.ruleId, item.message].join("\u0000");
}

const messages = results.flatMap((result) =>
  result.messages.map((message) => ({ result, message, entry: entry(result.filePath, message) }))
);

if (writeBaseline) {
  const baseline = messages.map(({ entry: item }) => item).sort((left, right) => key(left).localeCompare(key(right)));
  writeFileSync(baselinePath, `${JSON.stringify(baseline, null, 2)}\n`, "utf8");
  console.log(`Saved ${baseline.length} existing diagnostics to ${relative(baselinePath)}.`);
  process.exit(0);
}

if (!existsSync(baselinePath)) {
  console.error(`Missing baseline: ${relative(baselinePath)}. Run npm run lint:scripts:baseline intentionally.`);
  process.exit(1);
}

const baseline = JSON.parse(readFileSync(baselinePath, "utf8"));
const allowed = new Set(baseline.map(key));
const unexpected = messages.filter(({ entry: item }) => !allowed.has(key(item)));

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
