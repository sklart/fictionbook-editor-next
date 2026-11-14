const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const directory = path.resolve(__dirname, '../../runtime/Scripts/05_Иллюстрации');
const script = fs.readdirSync(directory).find(name => name.startsWith('54_') && name.endsWith('.js'));
assert(script, 'image quality script not found');
const source = fs.readFileSync(path.join(directory, script), 'utf8');
const match = source.match(/^    function encodePathForPowerShell\(value\) \{[\s\S]*?^    \}/m);
assert(match, 'path encoder missing');
const context = vm.createContext({});
vm.runInContext(match[0], context);
for (const value of ["C:\\Книги\\O'Brien %FBE_DEP_TEST%\\обложка.jpg", 'C:\\Books\\😀.png', '', 'a']) {
  assert.strictEqual(context.encodePathForPowerShell(value), Buffer.from(value, 'utf16le').toString('base64'));
}
console.log('PowerShell path encoding passed.');
