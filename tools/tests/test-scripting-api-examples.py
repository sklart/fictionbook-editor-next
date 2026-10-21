"""Negative controls for the complete, runnable handbook format."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/docs'))
from check_scripting_api import expected_methods
from check_scripting_api_examples import check_examples


class HandbookExamplesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = (ROOT / 'docs/scripting-api.md').read_text(encoding='utf-8-sig')
        cls.methods = expected_methods((ROOT / 'src/contracts/fbe.idl').read_text(encoding='utf-8-sig'),
            (ROOT / 'src/fbe/ExternalHelper.cpp').read_text(encoding='utf-8-sig'))

    def corrupted(self, before, after):
        self.assertIn(before, self.document)
        changed = self.document.replace(before, after, 1)
        self.assertNotEqual(changed, self.document)
        self.assertTrue(check_examples(ROOT, changed, self.methods))

    def test_current_handbook(self):
        self.assertEqual([], check_examples(ROOT, self.document, self.methods))

    def test_missing_purpose(self):
        self.corrupted('**Назначение:**', '**Описание:**')

    def test_wrong_dispid(self):
        self.corrupted('DISPID **100**', 'DISPID **999**')

    def test_wrong_signature(self):
        self.corrupted('### `HasScriptingMethod(name)`', '### `HasScriptingMethod()`')

    def test_wrong_call_arity(self):
        self.corrupted("ext.GetScriptingApiVersion()", "ext.GetScriptingApiVersion('extra')")

    def test_unknown_called_method(self):
        self.corrupted('ext.GetScriptingApiVersion()', 'ext.GetScriptingApiVersoin()')

    def test_wrong_version(self):
        self.corrupted('DISPID **100** · API 1.0.0.', 'DISPID **100** · API 1.5.0.')

    def test_incomplete_example(self):
        # Target a method example, not the separate quick-start snippet.
        start = self.document.index('<a id="api-getscriptingapiversion">')
        changed = self.document[:start] + self.document[start:].replace('function Run()', 'function Example()', 1)
        self.assertNotEqual(changed, self.document)
        self.assertTrue(check_examples(ROOT, changed, self.methods))

    def test_historical_anchor(self):
        self.corrupted('<a id="getuuid"></a>', '')

    def test_duplicate_complex_section(self):
        self.assertTrue(check_examples(ROOT, self.document +
            '\n## Сложные скрипты: сочетание методов API\n', self.methods))


if __name__ == '__main__':
    unittest.main()
