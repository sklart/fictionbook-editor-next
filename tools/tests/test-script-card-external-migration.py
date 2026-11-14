"""Exercise byte-preserving card migration and user-conflict handling."""
from pathlib import Path
import copy
import json
import runpy
import tempfile
import unittest
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
TOOL = runpy.run_path(str(ROOT / 'tools' / 'script-card-tool.py'))
MIGRATE = runpy.run_path(str(ROOT / 'tools' / 'externalize-script-cards.py'))['migrate']


class ExternalMigration(unittest.TestCase):
    def test_migration_preserves_legacy_encodings_and_bom(self):
        import hashlib
        sample = json.loads(next((ROOT / 'runtime' / 'ScriptCards').rglob('*.json')).read_text(encoding='utf8'))
        for encoding, bom in [('cp1251', b''), ('utf-8', b'\xef\xbb\xbf'),
                              ('utf-16-le', b'\xff\xfe'), ('utf-16-be', b'\xfe\xff')]:
            with self.subTest(encoding=encoding), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); scripts = root / 'Scripts'; scripts.mkdir(); cards = root / 'Cards'
                raw = bom + '// Автор\r\nfunction Run(){}\r\n'.encode(encoding)
                card = copy.deepcopy(sample); card['sourceRevision']['algorithm'] = TOOL['ALGORITHM']
                card['sourceRevision']['sha256'] = hashlib.sha256(raw).hexdigest()
                script = scripts / 'пример.js'; script.write_bytes(TOOL['insert_card'](raw, card, encoding))
                self.assertEqual(MIGRATE(scripts, cards, True)['counts'], {'migrated': 1})
                self.assertEqual(script.read_bytes(), raw)
                self.assertEqual(TOOL['inspect_file'](script, None, None, cards / 'пример.json')[1]['status'], 'current')

    def test_migration_preserves_code_and_nonrevision_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); scripts = root / 'Scripts'; cards = root / 'Cards'
            script = scripts / '07_Символы' / '01_Тест.js'; script.parent.mkdir(parents=True)
            sample = next((ROOT / 'runtime' / 'ScriptCards').rglob('*.json'))
            original = json.loads(sample.read_text(encoding='utf8'))
            raw = b'\xef\xbb\xbf// original\r\nvar value = 1;\r\n'
            embedded = copy.deepcopy(original)
            embedded['sourceRevision']['algorithm'] = TOOL['ALGORITHM']
            import hashlib
            embedded['sourceRevision']['sha256'] = hashlib.sha256(raw).hexdigest()
            script.write_bytes(TOOL['insert_card'](raw, embedded))
            report = MIGRATE(scripts, cards, True)
            self.assertEqual(report['counts'], {'migrated': 1})
            self.assertEqual(script.read_bytes(), raw)
            card = json.loads((cards / '07_Символы' / '01_Тест.json').read_text(encoding='utf8'))
            expected = copy.deepcopy(embedded); expected['sourceRevision']['algorithm'] = 'sha256-raw-js-v1'
            self.assertEqual(card, expected)
            self.assertEqual(MIGRATE(scripts, cards, True)['counts'], {'already-migrated': 1})
            card_file = cards / '07_Символы' / '01_Тест.json'
            edited = dict(card, summary='Ручная правка')
            card_file.write_text(json.dumps(edited, ensure_ascii=False), encoding='utf8')
            edited_bytes = card_file.read_bytes()
            self.assertEqual(MIGRATE(scripts, cards, True)['counts'], {'already-migrated': 1})
            self.assertEqual(card_file.read_bytes(), edited_bytes)
            validate = [sys.executable, str(ROOT / 'tools' / 'script-card-tool.py'), 'validate', str(script),
                        '--scripts-root', str(scripts), '--cards-root', str(cards)]
            self.assertEqual(subprocess.run(validate, capture_output=True).returncode, 0)
            doc, result = TOOL['inspect_file'](script, None, None, cards / '07_Символы' / '01_Тест.json')
            self.assertEqual(result['status'], 'current')
            script.write_bytes(raw + b'// change\r\n')
            self.assertEqual(TOOL['inspect_file'](script, None, None, cards / '07_Символы' / '01_Тест.json')[1]['status'], 'changed')

    def test_does_not_overwrite_user_card(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); scripts = root / 'Scripts'; cards = root / 'Cards'; scripts.mkdir(); cards.mkdir()
            card = json.loads(next((ROOT / 'runtime' / 'ScriptCards').rglob('*.json')).read_text(encoding='utf8'))
            raw = b'// code\n'; import hashlib
            card['sourceRevision']['algorithm'] = TOOL['ALGORITHM']; card['sourceRevision']['sha256'] = hashlib.sha256(raw).hexdigest()
            script = scripts / 'x.js'; initial = TOOL['insert_card'](raw, card); script.write_bytes(initial)
            (cards / 'x.json').write_text('{"user": true}', encoding='utf8')
            self.assertEqual(MIGRATE(scripts, cards, True)['counts'], {'conflict': 1})
            self.assertEqual(script.read_bytes(), initial)
            self.assertEqual((cards / 'x.json').read_text(encoding='utf8'), '{"user": true}')

    def test_external_priority_invalid_and_explicit_stamp(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); script = root / 'x.js'; external = root / 'x.json'
            card = json.loads(next((ROOT / 'runtime' / 'ScriptCards').rglob('*.json')).read_text(encoding='utf8'))
            legacy = copy.deepcopy(card); legacy['sourceRevision']['algorithm'] = TOOL['ALGORITHM']
            raw = b'// script\n'; import hashlib
            legacy['sourceRevision']['sha256'] = hashlib.sha256(raw).hexdigest()
            script.write_bytes(TOOL['insert_card'](raw, legacy)); data = script.read_bytes()
            card['sourceRevision']['sha256'] = hashlib.sha256(data).hexdigest()
            card['summary'] = 'External summary'
            external.write_text(json.dumps(card, ensure_ascii=False), encoding='utf8')
            doc, result = TOOL['inspect_file'](script, None, None, external)
            self.assertEqual((doc.card['summary'], result['status']), ('External summary', 'current'))
            external.write_text('{invalid', encoding='utf8')
            with self.assertRaises(TOOL['CardError']): TOOL['inspect_file'](script, None, None, external)
            external.write_text(json.dumps(card, ensure_ascii=False), encoding='utf8')
            script.write_bytes(data + b'// revised\n'); revised = script.read_bytes()
            command = [sys.executable, str(ROOT / 'tools' / 'script-card-tool.py'), 'stamp', str(script), '--card', str(external)]
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run(command + ['--accept-revision'], capture_output=True).returncode, 0)
            self.assertEqual(script.read_bytes(), revised)
            self.assertEqual(TOOL['inspect_file'](script, None, None, external)[1]['status'], 'current')
            external.unlink()
            self.assertEqual(TOOL['inspect_file'](script, None, None, external)[1]['status'], 'changed')

    def test_audit_edits_external_card_without_touching_js(self):
        audit = runpy.run_path(str(ROOT / 'tools' / 'audit-script-cards.py'))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); scripts = root / 'Scripts'; cards = root / 'Cards'; scripts.mkdir(); cards.mkdir()
            script = scripts / 'x.js'; raw = b'// content\r\n'; script.write_bytes(raw)
            card = json.loads(next((ROOT / 'runtime' / 'ScriptCards').rglob('*.json')).read_text(encoding='utf8'))
            import hashlib
            card['sourceRevision']['sha256'] = hashlib.sha256(raw).hexdigest()
            card_path = cards / 'x.json'; card_path.write_text(json.dumps(card, ensure_ascii=False), encoding='utf8')
            apply = audit['apply_proposals']; apply.__globals__['SCRIPTS'] = scripts; apply.__globals__['CARDS'] = cards
            new_summary = card['summary'] + ' revised'
            proposal = {'path':'x.js', 'codeSha256':hashlib.sha256(raw).hexdigest(), 'evidence':'Explicit test review',
                        'changes':[{'field':'summary','before':card['summary'],'after':new_summary}]}
            self.assertEqual(apply([proposal], [{'path':'x.js','result':'requires_review'}]), 1)
            self.assertEqual(script.read_bytes(), raw)
            self.assertEqual(json.loads(card_path.read_text(encoding='utf8'))['summary'], new_summary)


if __name__ == '__main__':
    unittest.main()
