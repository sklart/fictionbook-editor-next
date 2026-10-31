// Exercise the actual rule declarations from both bundled cleanup scripts.
import fs from 'node:fs';
import vm from 'node:vm';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const names = ['01_Генеральная уборка.js', '09_Генеральная уборка_(ИНОСТРАНКА).js'];
const negative = ['300 СПАРТАНЦЕВ', '100 СОЛДАТ', '200 СТРАНИЦ', '100 СТРАН', '300 СЛОВ', '120 CATS', '200 °СЛОВ', '20 °Cé'];
const positive = ['20 °C', '20°C', '20 °С', '20 oC', '20 оС', '20\u00a0°C', '20&nbsp;°C', '20°C,', '20°C]', '20°C"'];
let failed = false;
for (const name of names) {
  const source = fs.readFileSync(path.join(root, 'runtime/Scripts/06_Чистка', name), 'utf8');
  const pattern = source.match(/^\s*var re67 = .+;\s*$/m);
  const replacement = source.match(/^\s*var re67_ = .+;\s*$/m);
  if (!pattern || !replacement) throw new Error(`Missing rule 67 in ${name}`);
  const context = { nbspChar: '\u00a0', nbspEntity: '&nbsp;' };
  vm.runInNewContext(`${pattern[0]}\n${replacement[0]}`, context);
  const apply = value => value.replace(context.re67, context.re67_);
  for (const value of negative) {
    const result = apply(value);
    if (result !== value) { console.error(`${name}: false positive ${value} -> ${result}`); failed = true; }
  }
  for (const value of positive) {
    const expected = '20\u00a0°C' + (/[\],"]$/.test(value) ? value.slice(-1) : '');
    const result = apply(value);
    if (result !== expected || apply(result) !== result) { console.error(`${name}: temperature ${value} -> ${result}`); failed = true; }
  }
  // Run the complete bundled script against an isolated minimal DOM fixture.
  // The fixture contains unchanged formatting and links as well as rule 67 cases.
  const values = ['<EM>300 СПАРТАНЦЕВ</EM>', '<STRONG>100 СОЛДАТ</STRONG>', '<A href="#n">200 СТРАНИЦ</A>', '20 °C', '20°C,'];
  const body = { nodeName: 'DIV', className: 'body', firstChild: null, nextSibling: { nodeName: 'DIV' }, parentNode: null };
  const paragraphs = values.map(innerHTML => ({ nodeName: 'P', innerHTML, firstChild: null, nextSibling: null, parentNode: body, getAttribute: () => '', removeAttribute: () => {} }));
  body.firstChild = paragraphs[0];
  for (let i = 0; i + 1 < paragraphs.length; ++i) paragraphs[i].nextSibling = paragraphs[i + 1];
  const messages = [];
  const runtime = { document: { getElementById: () => body }, window: { external: {
    GetNBSP: () => '\u00a0', BeginUndoUnit: () => {}, EndUndoUnit: () => {}
  } }, MsgBox: text => messages.push(text) };
  vm.runInNewContext(source, runtime, { filename: name });
  runtime.Run();
  const first = paragraphs.map(p => p.innerHTML);
  if (first.slice(0, 3).some((value, i) => value !== values[i]) ||
      first[3] !== '20\u00a0°C' || first[4] !== '20\u00a0°C,') {
    console.error(`${name}: full-script fixture changed content or formatting: ${JSON.stringify(first)}`); failed = true;
  }
  runtime.Run();
  if (paragraphs.some((p, i) => p.innerHTML !== first[i])) { console.error(`${name}: full script is not idempotent on fixture`); failed = true; }
  const version = name.startsWith('01_') ? '2.10' : '2.11';
  if (messages.length !== 2 || !messages.every(message => message.includes(`v.${version}`))) { console.error(`${name}: unexpected report version`); failed = true; }
}
// The Alexander Ka variant has an independent rule 741, not rule 67.
const other = fs.readFileSync(path.join(root, 'runtime/Scripts/06_Чистка/02_Генеральная уборка (версия Александра Ка).js'), 'utf8');
const rule741 = other.match(/^\s*var re741 = .+;\s*$/m);
if (!rule741) throw new Error('Missing independent rule 741');
const otherContext = { nbspChar: '\u00a0' };
vm.runInNewContext(rule741[0], otherContext);
for (const value of negative) {
  otherContext.re741.lastIndex = 0;
  if (otherContext.re741.test(value)) { console.error(`Alexander Ka rule 741 matched ${value}`); failed = true; }
}
if (failed) process.exit(1);
console.log('Rule 67: negative, positive and repeat checks passed for both scripts; independent rule 741 negative checks passed.');
