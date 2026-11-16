"""Check repeated and concurrent EPUB imports do not replace older logs."""
import argparse
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path

BATCH = None


class ImportLogPreservation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fbe-epub-log-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.epub = self.root / 'book.epub'
        with zipfile.ZipFile(self.epub, 'w') as archive:
            archive.writestr('mimetype', 'application/epub+zip')
            archive.writestr('META-INF/container.xml', '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
            archive.writestr('book.opf', '<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Fixture</dc:title><dc:language>en</dc:language><dc:identifier id="uid">test</dc:identifier></metadata><manifest><item id="c" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="c"/></spine></package>')
            archive.writestr('chapter.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter</title></head><body><p>Fixture.</p></body></html>')

    def run_import(self, name):
        return subprocess.run([str(BATCH), str(self.epub), str(self.root / (name + '.fb2')), '--log'], capture_output=True, timeout=30)

    def test_existing_log_is_preserved(self):
        previous = self.root / 'book.ImportEPUB.log'
        previous.write_bytes(b'Existing user log\x00do not replace')
        numbered = self.root / 'book.1.ImportEPUB.log'
        numbered.write_bytes(b'Another earlier log')
        result = self.run_import('first')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(previous.read_bytes(), b'Existing user log\x00do not replace')
        self.assertEqual(numbered.read_bytes(), b'Another earlier log')
        new = self.root / 'book.2.ImportEPUB.log'
        self.assertIn('Source:', new.read_text(encoding='utf-8-sig'))

    def test_failed_import_preserves_existing_log(self):
        previous = self.root / 'book.ImportEPUB.log'
        previous.write_bytes(b'Prior diagnostic')
        self.epub.write_bytes(b'not an EPUB archive')
        result = self.run_import('failed')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(previous.read_bytes(), b'Prior diagnostic')
        self.assertIn('Source:', (self.root / 'book.1.ImportEPUB.log').read_text(encoding='utf-8-sig'))

    def test_automatic_error_log_preserves_existing_log(self):
        previous = self.root / 'book.ImportEPUB.log'
        previous.write_bytes(b'Prior diagnostic')
        self.epub.write_bytes(b'not an EPUB archive')
        result = subprocess.run([str(BATCH), str(self.epub), str(self.root / 'failed.fb2'), '--no-log'], capture_output=True, timeout=30)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(previous.read_bytes(), b'Prior diagnostic')
        self.assertIn('Source:', (self.root / 'book.1.ImportEPUB.log').read_text(encoding='utf-8-sig'))

    def test_concurrent_imports_keep_both_logs(self):
        processes = [subprocess.Popen([str(BATCH), str(self.epub), str(self.root / (str(i) + '.fb2')), '--log'], stdout=subprocess.PIPE, stderr=subprocess.PIPE) for i in range(2)]
        for process in processes:
            stdout, stderr = process.communicate(timeout=30)
            self.assertEqual(process.returncode, 0, stdout + stderr)
        for name in ('book.ImportEPUB.log', 'book.1.ImportEPUB.log'):
            self.assertIn('Source:', (self.root / name).read_text(encoding='utf-8-sig'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--batch', type=Path, required=True)
    args, rest = parser.parse_known_args()
    BATCH = args.batch.resolve()
    unittest.main(argv=[__file__, *rest])
