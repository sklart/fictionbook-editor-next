// ESLint parses standard ES3, while Microsoft JScript accepts this COM propput.
// Normalize only the confirmed note/comment statement in the lint input.
const noteCreationPath = /^runtime\/Scripts\/(?:02_Примечания|03_Комментарии)\/0[1-6]_[^/]+\.js$/;
const inflateBlockAssignment = /^([ \t]*if \(addSnoska && strConst3==""\) window\.external\.inflateBlock\(newSection\.lastChild\))=true;(?=\r?$)/gm;

export function isNoteCreationPath(filePath) {
  return noteCreationPath.test(filePath.replace(/\\/g, "/"));
}

export function normalizeComIndexedProperty(filePath, source) {
  if (!isNoteCreationPath(filePath)) return source;
  return source.replace(inflateBlockAssignment, "$1;");
}
