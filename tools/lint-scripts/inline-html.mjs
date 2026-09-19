import { readFileSync } from "node:fs";

const scriptTag = /<script\b([^>]*)>([\s\S]*?)<\/script\s*>/gi;

function lineAt(text, offset) {
  return text.slice(0, offset).split(/\r?\n/).length;
}

export function extractInlineScriptsFromSource(filePath, source) {
  const scripts = [];
  let match;
  while ((match = scriptTag.exec(source)) !== null) {
    // Inline libraries are intentionally not a supported artifact. External
    // scripts are linted from their own file and are never reprocessed here.
    if (/\bsrc\s*=/i.test(match[1])) continue;
    const code = match[2];
    const startLine = lineAt(source, match.index + match[0].indexOf(code));
    scripts.push({ filePath, code, startLine, paddedCode: `${"\n".repeat(startLine - 1)}${code}` });
  }
  return scripts;
}

export function extractInlineScripts(filePath) {
  return extractInlineScriptsFromSource(filePath, readFileSync(filePath, "utf8"));
}
