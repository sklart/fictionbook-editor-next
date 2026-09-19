import { fingerprintKey } from "./fingerprint.mjs";

export function compareBaseline(currentEntries, baselineEntries) {
  const baselineKeys = new Set(baselineEntries.map(fingerprintKey));
  const currentKeys = new Set(currentEntries.map(fingerprintKey));
  return {
    unexpected: currentEntries.filter((entry) => !baselineKeys.has(fingerprintKey(entry))),
    stale: baselineEntries.filter((entry) => !currentKeys.has(fingerprintKey(entry)))
  };
}
