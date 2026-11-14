"""Check the live script targets used by the two bundled formatting combos."""
import hashlib
import json
import unittest
from pathlib import Path

RUNTIME = Path(__file__).resolve().parents[2] / 'runtime'
CASES = [
    ('09_Стихи/39_Расформатировать все стихи от жирности и курсива',
     '16_Поиск форматирования/154_Выделить следующий стих (poem).js'),
    ('37_Эпиграфы/20_Расформатировать все эпиграфы (Комбо)',
     '16_Поиск форматирования/150_Выделить следующий эпиграф.js'),
]


class ComboDependencies(unittest.TestCase):
    def test_live_selector_paths_and_card_hashes(self):
        for name, selector in CASES:
            with self.subTest(script=name):
                script = RUNTIME / 'Scripts' / (name + '.js')
                card = json.loads((RUNTIME / 'ScriptCards' / (name + '.json')).read_text(encoding='utf-8'))
                source = script.read_bytes()
                text = source.decode('utf-8-sig')
                active = '\n'.join(line for line in text.splitlines() if not line.lstrip().startswith('//'))
                self.assertIn(selector.replace('/', '\\\\'), active)
                target = RUNTIME / 'Scripts' / selector
                target_text = target.read_bytes().decode('utf-8-sig')
                self.assertRegex(target_text, r'scriptResult\s*=\s*["\']Found["\']')
                self.assertRegex(target_text, r'return\s+scriptResult')
                dependency = next(d for d in card['dependencies'] if d['path'] == 'Scripts/' + selector)
                self.assertEqual(dependency['sha256'], hashlib.sha256(target.read_bytes()).hexdigest())
                self.assertEqual(card['sourceRevision']['sha256'], hashlib.sha256(source).hexdigest())
                self.assertTrue(card['dependenciesComplete'])

    def test_footnote_dialog_help_is_tracked(self):
        help_path = 'HTML/Поиск некорректных маркеров сносок - справка.htm'
        dialog = RUNTIME / 'HTML/Поиск некорректных маркеров сносок - выбор из вариантов.htm'
        self.assertIn(Path(help_path).name, dialog.read_text(encoding='utf-8-sig'))
        help_hash = hashlib.sha256((RUNTIME / help_path).read_bytes()).hexdigest()
        cards = RUNTIME / 'ScriptCards/32_Маркеры сносок'
        selected = [p for p in cards.glob('*.json') if p.name[:2] in {'01', '02', '03', '04'}]
        self.assertEqual(len(selected), 4)
        for path in selected:
            card = json.loads(path.read_text(encoding='utf-8'))
            dependency = next(d for d in card['dependencies'] if d['path'] == help_path)
            self.assertEqual(dependency['sha256'], help_hash)
            self.assertTrue(dependency['optional'])
            self.assertTrue(card['dependenciesComplete'])

    def test_powershell_scripts_use_private_temp_names(self):
        groups = [('05_Иллюстрации', ('48_', '49_', '54_')),
                  ('95_Для программистов', ('08_', '14_', '15_'))]
        for category, prefixes in groups:
            for prefix in prefixes:
                script, = (RUNTIME / 'Scripts' / category).glob(prefix + '*.js')
                with self.subTest(script=script.name):
                    source = script.read_bytes()
                    text = source.decode('utf-8-sig')
                    self.assertNotIn('C:\\\\__fbe_', text)
                    self.assertIn('GetSpecialFolder(2)', text)
                    self.assertIn('GetTempName()', text)
                    self.assertIn('ExpandEnvironmentStrings("%ComSpec%")', text)
                    self.assertIn('[Console]::OutputEncoding=[System.Text.Encoding]::Unicode;', text)
                    if category == '05_Иллюстрации' and prefix == '54_':
                        self.assertEqual(text.count('filePath.replace(/\'/g, "\'\'")'), 3)
                    card = json.loads((RUNTIME / 'ScriptCards' / category / (script.stem + '.json')).read_text(encoding='utf-8'))
                    self.assertEqual(card['sourceRevision']['sha256'], hashlib.sha256(source).hexdigest())
                    self.assertTrue(card['dependenciesComplete'])

    def test_dependency_review_matches_cards_and_sources(self):
        report = json.loads((RUNTIME.parent / 'docs/script-card-dependency-review-20261115.json').read_text(encoding='utf-8'))
        self.assertEqual(len(report['scripts']), 69)
        self.assertEqual(len({item['card'] for item in report['scripts']}), 69)
        for item in report['scripts']:
            with self.subTest(card=item['card']):
                relative = Path(item['card'])
                card = json.loads((RUNTIME / 'ScriptCards' / relative).read_text(encoding='utf-8'))
                script = RUNTIME / 'Scripts' / relative.with_suffix('.js')
                self.assertTrue(card['dependenciesComplete'])
                self.assertEqual(card['sourceRevision']['sha256'], item['codeSha256'])
                self.assertEqual(hashlib.sha256(script.read_bytes()).hexdigest(), item['codeSha256'])
                self.assertEqual(card['dependencies'], item['dependencies'])
                for dependency in item['dependencies']:
                    data = (RUNTIME / dependency['path']).read_bytes()
                    self.assertEqual(hashlib.sha256(data).hexdigest(), dependency['sha256'])


if __name__ == '__main__':
    unittest.main()
