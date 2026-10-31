import hashlib
import json
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
read_document = runpy.run_path(str(ROOT / 'tools/script-card-tool.py'))['read_document']
baseline = json.loads((ROOT / 'docs/script-card-code-baseline.json').read_text(encoding='utf-8'))
expected = {
    '01_Генеральная уборка.js': ('bb53493213735dbd03d53238259e6699ed787a212defaa69e28ee6b4f6ce8729', '2.9', '2.10'),
    '09_Генеральная уборка_(ИНОСТРАНКА).js': ('c9409f9afcef3839ab50d4a0e68c9bfcb35597a6ddda436bb3618505e917508f', '2.10', '2.11'),
}
for name, (old_hash, old_version, version) in expected.items():
    records = [x for x in baseline['scripts'] if x['path'].endswith('/' + name)]
    assert len(records) == 1
    entry = records[0]
    data = (ROOT / 'runtime/Scripts' / entry['path']).read_bytes()
    doc = read_document(data)
    assert doc.sha256 == entry['codeSha256'] and len(doc.code_bytes) == entry['codeBytes']
    assert doc.encoding == entry['encoding'] and doc.bom_size == entry['bomBytes']
    assert entry['lineEnding'] == 'CRLF' and b'\n' not in doc.code_bytes.replace(b'\r\n', b'')
    history = [x for x in baseline['executableChanges'] if x['path'] == entry['path']]
    change = history[-1]
    assert change['previousCodeSha256'] == old_hash and change['codeSha256'] == doc.sha256
    assert (change['previousVersion'], change['version']) == (old_version, version)
    assert change['basisCommit'] == '5c7aeb51'
    assert hashlib.sha256(doc.code_bytes + b'\n').hexdigest() != entry['codeSha256']
print('Rule 67 baseline hashes, byte lengths, encoding, line endings and provenance passed.')
