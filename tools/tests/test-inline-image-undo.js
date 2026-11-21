import fs from 'node:fs';
import vm from 'node:vm';
const text = fs.readFileSync(new URL('../../runtime/main.js', import.meta.url), 'utf8');
const start = text.indexOf('function InsInlineImage(check, id)');
const end = text.indexOf('//-----------------------------------------------', start);
if (start < 0 || end < 0) throw new Error('InsInlineImage not found');
function run(fail) {
  let opened = 0, closed = 0, inserted = 0;
  const range = { compareEndPoints: () => 0, parentElement: () => ({tagName:'DIV', className:'section'}),
    pasteHTML: () => { inserted++; if (fail) throw new Error('injected paste failure'); } };
  const context = {document:{selection:{createRange:()=>range}}, window:{external:{
    BeginUndoUnit:()=>{opened++;}, EndUndoUnit:()=>{closed++;}}}, inlineimgcode:'<span></span>'};
  vm.runInNewContext(text.slice(start, end), context);
  let error = null; try { context.InsInlineImage(false, ''); } catch (e) { error = e; }
  if (opened !== 1 || closed !== 1 || inserted !== 1 || (!!error !== fail))
    throw new Error(`fail=${fail} opened=${opened} closed=${closed} inserted=${inserted} error=${!!error}`);
}
run(false); run(true);
console.log('Inline image Undo cleanup passed.');
