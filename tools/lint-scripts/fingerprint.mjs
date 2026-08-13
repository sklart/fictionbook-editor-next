function normalize(value) {
  return String(value ?? "").replace(/\s+/g, " ").trim();
}

export function createFingerprintEntries(messages, sourceByFile) {
  const occurrences = new Map();
  return messages.map(({ filePath, message }) => {
      const file = filePath.replace(/\\/g, "/");
      const lines = String(sourceByFile.get(filePath) ?? "").split(/\r?\n/);
      const sourceContext = normalize(lines[Math.max(0, message.line - 1)]);
      const base = [file, message.ruleId ?? "", normalize(message.message), sourceContext].join("\u0000");
      const occurrence = (occurrences.get(base) ?? 0) + 1;
      occurrences.set(base, occurrence);
      return { file, ruleId: message.ruleId, message: normalize(message.message), sourceContext, occurrence };
    });
}

export function fingerprintKey(item) {
  return [item.file, item.ruleId ?? "", normalize(item.message), normalize(item.sourceContext), item.occurrence].join("\u0000");
}
