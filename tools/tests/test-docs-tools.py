"""Focused behavioral regressions for documentation checks and audit output."""
from pathlib import Path
import html
import importlib.util
import json
import re
import runpy
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/docs'))
import doc_links as links
import check_scripting_api as api


class LinksTests(unittest.TestCase):
    def test_fences_inline_code_and_comments_are_not_navigation(self):
        text = '```md\n[bad](absent)\n```\n`[bad](absent)`\n<!-- [bad](absent) -->\n[ok](a.md)'
        self.assertEqual([x.target for x in links.links(text)], ['a.md'])

    def test_destinations_references_html_and_balanced_parentheses(self):
        text = '[x](a(b).md "title")\n[id]: <folder/file%20name.md>\n<img src="image.png">'
        self.assertEqual([x.target for x in links.links(text)],
                         ['a(b).md', 'folder/file%20name.md', 'image.png'])

    def test_unicode_duplicate_heading_and_explicit_anchor(self):
        found = links.anchors('# Русский `API`\n# Русский `API`\n<a id="stable"></a>\n```\n# ghost\n```')
        self.assertEqual(found, {'русский-api', 'русский-api-1', 'stable'})

    def test_missing_file_anchor_absolute_and_outside_are_errors(self):
        docs = {'docs/a.md': '# A\n[x](missing.md) [x](#wrong) [x](D:/books/a.md) [x](../../escape.md)'}
        result = links.check_documents(docs, lambda n: n in docs, docs.get)
        self.assertEqual({x['code'] for x in result['errors']},
                         {'missing-file', 'missing-anchor', 'absolute', 'outside'})

    def test_encoded_fragment_relative_file_and_remote(self):
        docs = {'docs/a.md': '[x](b.md#%D1%8F) [web](https://example.invalid/path)', 'docs/b.md': '# Я'}
        result = links.check_documents(docs, lambda n: n in docs, docs.get)
        self.assertEqual(result['status'], 'PASS')
        self.assertEqual(result['external_links_not_fetched'], 1)

    def test_default_covers_root_and_tools_but_skips_generated_and_vendor(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ('README.md', 'docs/a.md', 'tools/README.md', 'build/b.md', 'out/b.md', 'third_party/b.md'):
                p = root / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text('[x](missing.md)', encoding='utf-8')
            result = links.local_check(root)
            self.assertEqual(result['documents'], 3)
            self.assertEqual(len(result['errors']), 3)
            self.assertEqual(links.local_check(root, 'docs')['documents'], 1)
            with self.assertRaises(ValueError):
                links.local_check(root, '..')


class DocumentationStructureTests(unittest.TestCase):
    def test_reserved_redirect_formats_are_rejected(self):
        examples = (
            '# Документ перенесён: тема\nАктуальный вход: [новый](new.md)',
            '## Прежние якоря\n<a id="old"></a>\n[новый](new.md)',
            'Этот файл сохраняет совместимость ранее опубликованного пути.\n[new](new.md)',
            'Объединённый исторический отчёт: archive/settings.md',
            'Исторический документ перенесён: archive/legacy.txt',
            '# Старое название\nЭтот файл сохраняет совместимость ранее опубликованного пути.\n## Прежние якоря\n[new](new.md)',
        )
        for text in examples:
            with self.subTest(text=text):
                self.assertIsNotNone(links.redirect_stub_reason(text))

    def test_research_quotes_and_code_are_not_redirect_pages(self):
        examples = (
            '# Исследование ссылок\n> # Документ перенесён: пример\nСодержательный анализ причины дефекта.',
            '# Техническое решение\n```text\n## Прежние якоря\nЭтот файл сохраняет совместимость ранее опубликованного пути\n```\nВыводы и воспроизводимый тест.',
            '# Исследование\nФормулировка `Исторический документ перенесён:` цитируется для разбора старого формата.',
            '# Работа с якорями\n## Прежние якоря\nИсследование алгоритма формирования slug, условий коллизии и исправления причин.',
        )
        for text in examples:
            with self.subTest(text=text):
                self.assertIsNone(links.redirect_stub_reason(text))

    def test_nested_markdown_and_text_inventory(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'docs/nested').mkdir(parents=True)
            (root / 'docs/nested/old.txt').write_text('Исторический документ перенесён: new.md', encoding='utf-8')
            (root / 'docs/research.md').write_text('# Исследование\n> ## Прежние якоря\nПричины и выводы.', encoding='utf-8')
            self.assertEqual(set(links.redirect_stubs(root)), {'docs/nested/old.txt'})

    def test_repository_contains_no_redirect_documents(self):
        self.assertEqual(links.redirect_stubs(ROOT), {}, 'Remove redirect pages and update direct links')


class ApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.idl = (ROOT / 'src/contracts/fbe.idl').read_text(encoding='utf-8-sig')
        cls.helper = (ROOT / 'src/fbe/ExternalHelper.cpp').read_text(encoding='utf-8-sig')
        cls.doc = (ROOT / 'docs/scripting-api.md').read_text(encoding='utf-8-sig')
        cls.impl = (ROOT / 'src/fbe/scripts/ScriptingApi.inl').read_text(encoding='utf-8-sig')

    def check_doc(self, doc):
        return api.check(self.idl, self.helper, doc, self.impl)

    def test_current_contract_including_binary_methods(self):
        result = self.check_doc(self.doc)
        self.assertEqual(result['status'], 'PASS')
        methods = api.expected_methods(self.idl, self.helper)
        self.assertEqual(methods['AddBinaryFromFile'], 149)
        self.assertEqual(methods['SaveAllBinaries'], 152)
        self.assertNotIn('_EnterScriptContext', methods)
        self.assertEqual(sum(n < 100 for n in methods.values()), 35)

    def test_stale_version_or_count_fails(self):
        cases = (
            (r'Актуальный публичный контракт: API \d+\.\d+\.\d+, (\d+) метод',
             lambda value: str(int(value) + 1), 'Banner method count'),
            (r'^## Оглавление: все (\d+) метод',
             lambda value: str(int(value) + 1), 'TOC method count'),
            (r'Актуальный публичный контракт: API (\d+\.\d+\.\d+),',
             lambda value: '.'.join(value.split('.')[:2] + [str(int(value.split('.')[2]) + 1)]),
             'versions differ'),
        )
        for pattern, corrupt, expected in cases:
            with self.subTest(field=expected):
                match = re.search(pattern, self.doc, re.M)
                self.assertIsNotNone(match, 'Negative fixture must match the current reference')
                start, end = match.span(1)
                doc = self.doc[:start] + corrupt(match[1]) + self.doc[end:]
                self.assertNotEqual(doc, self.doc, 'Negative fixture must actually change')
                result = self.check_doc(doc)
                self.assertEqual(result['status'], 'FAIL')
                self.assertTrue(any(expected in error for error in result['metadata_errors']), result)

    def test_missing_binary_navigation_and_capability_fails(self):
        doc = self.doc.replace('<a id="api-removebinary"></a>', '')
        self.assertIn('removebinary', self.check_doc(doc)['missing_anchors'])
        doc = self.doc.replace('`binaryObjects`', '`undocumented`')
        self.assertTrue(self.check_doc(doc)['metadata_errors'])

    def test_fenced_fake_anchor_is_not_coverage(self):
        doc = self.doc.replace('<a id="api-removebinary"></a>', '')
        doc += '\n```html\n<a id="api-removebinary"></a>\n```\n'
        self.assertIn('removebinary', self.check_doc(doc)['missing_anchors'])

    def test_unknown_and_duplicate_public_anchors_fail(self):
        self.assertIn('future', self.check_doc(self.doc + '\n<a id="api-future"></a>')['unexpected_api_anchors'])
        self.assertIn('removebinary', self.check_doc(self.doc + '\n<a id="api-removebinary"></a>')['duplicate_anchors'])

    def test_dispatcher_changes_do_not_silently_pass(self):
        for helper in (self.helper.replace('L"SaveAllBinaries"', 'nullptr'),
                       self.helper.replace('L"SaveAllBinaries"', 'L"removebinary"'),
                       self.helper.replace('return 100 + static_cast<DISPID>(index)', 'return 200 + static_cast<DISPID>(index)')):
            with self.assertRaises(ValueError):
                api.expected_methods(self.idl, helper)


class AuditOutputTests(unittest.TestCase):
    def setUp(self):
        self.audit = runpy.run_path(str(ROOT / 'tools/audit-script-cards.py'))
        self.globals = self.audit['main'].__globals__
        self.temp = TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'runtime/Scripts').mkdir(parents=True)
        self.context = patch.dict(self.globals, ROOT=self.root, SCRIPTS=self.root / 'runtime/Scripts')
        self.context.start()
        self.addCleanup(self.context.stop)

    def test_default_output_and_git_from_path(self):
        with patch.object(sys, 'argv', ['audit']), patch.object(self.globals['shutil'], 'which', return_value='portable-git'):
            with patch.object(self.globals['subprocess'], 'check_output', return_value='fixture-head\n') as git:
                with patch.dict(self.globals, write_reports=lambda records, output, baseline, reviewed:
                                self.assertEqual(output, self.root / 'out/reports/script-cards')):
                    self.assertEqual(self.audit['main'](), 0)
                self.assertEqual(git.call_args.args[0], ['portable-git', 'rev-parse', 'HEAD'])

    def test_docs_output_rejected_before_scan_or_git(self):
        for suffix in ('docs', 'docs/nested', 'out/../docs'):
            with patch.object(sys, 'argv', ['audit', '--output-dir', str(self.root / suffix)]):
                with patch.object(self.globals['shutil'], 'which', return_value='git'):
                    with patch.object(self.globals['subprocess'], 'check_output') as git:
                        with self.assertRaisesRegex(ValueError, 'inside docs'):
                            self.audit['main']()
                        git.assert_not_called()

    def test_missing_git_is_actionable(self):
        with patch.object(sys, 'argv', ['audit']), patch.object(self.globals['shutil'], 'which', return_value=None):
            with self.assertRaisesRegex(ValueError, 'PATH'):
                self.audit['main']()

    def test_html_source_link_resolves_from_custom_output(self):
        output = self.root / 'custom/reports/nested'
        name = 'раздел/тест & one.js'
        source = self.root / 'runtime/Scripts' / name
        source.parent.mkdir(parents=True)
        source.write_bytes(b'function Run(){}\r\n')
        digest = source.read_bytes()
        proposal = {'path': name, 'changes': [{'field': 'usage', 'before': 'x', 'after': 'y'}], 'evidence': 'fixture'}
        self.audit['write_reports']([], output, 'fixture', [proposal])
        text = (output / 'script-card-content-audit.html').read_text(encoding='utf-8')
        target = html.unescape(unquote(re.search(r"href='([^']+)'", text)[1]))
        self.assertEqual((output / target).resolve(), source.resolve())
        self.assertEqual(source.read_bytes(), digest)
        self.assertEqual(json.loads((output / 'script-card-content-audit.json').read_text(encoding='utf-8'))['baseCommit'], 'fixture')


if __name__ == '__main__':
    unittest.main()
