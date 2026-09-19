export function jscriptFingerprintKey(entry) {
  return [entry.file, entry.scriptBlock, entry.errorNumber, entry.sourceHash].join("\u0000");
}

export function compareJscriptBaseline(currentEntries, baselineEntries) {
  const expected = new Set(baselineEntries.map(jscriptFingerprintKey));
  const current = new Set(currentEntries.map(jscriptFingerprintKey));
  return {
    unexpected: currentEntries.filter((entry) => !expected.has(jscriptFingerprintKey(entry))),
    stale: baselineEntries.filter((entry) => !current.has(jscriptFingerprintKey(entry)))
  };
}
