import base64
import importlib.util
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('compare_epub_fb2', Path(__file__).with_name('compare-epub-fb2.py'))
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class PairAudit(unittest.TestCase):
    def pair(self, directory, result_text='Текст Ω &amp; слово', image=b'image'):
        epub = directory / 'source.epub'; fb2 = directory / 'result.fb2'
        with zipfile.ZipFile(epub, 'w') as archive:
            archive.writestr('META-INF/container.xml', '<container><rootfile full-path="OPS/book.opf"/></container>')
            archive.writestr('OPS/book.opf', '<package><manifest><item id="one" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="one"/></spine></package>')
            archive.writestr('OPS/chapter.xhtml', '<html><body><p id="target">Текст Ω &amp; слово</p><a href="#target">1</a><img src="image.png"/></body></html>')
            archive.writestr('OPS/image.png', b'image')
        fb2.write_text('<FictionBook xmlns:l="http://www.w3.org/1999/xlink"><body><section><p id="target">' + result_text + '</p><p><a l:href="#target">1</a><image l:href="#image"/></p></section></body><binary id="image">' + base64.b64encode(image).decode() + '</binary></FictionBook>', encoding='utf-8')
        return epub, fb2

    def test_matching_pair_is_still_review(self):
        with tempfile.TemporaryDirectory() as temp:
            report = audit.compare(*self.pair(Path(temp)))
            self.assertEqual(report['status'], 'REVIEW')
            self.assertEqual(report['differences']['missing_word_occurrences'], 0)
            self.assertEqual(report['differences']['missing_original_image_occurrences'], 0)
            self.assertEqual(report['fb2']['unresolved_fragment_links'], 0)
            self.assertNotIn('Текст', str(report))

    def test_missing_text_and_changed_image_are_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            report = audit.compare(*self.pair(Path(temp), 'Текст', b'changed'))
            self.assertEqual(report['differences']['missing_word_occurrences'], 2)
            self.assertEqual(report['differences']['missing_original_image_occurrences'], 1)

    def test_reordered_blocks_are_visible_even_when_words_match(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/chapter.xhtml'] = b'<html><body><p>one</p><p>two</p></body></html>'
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items():
                    archive.writestr(name, data)
            fb2.write_text('<FictionBook><body><section><p>two</p><p>one</p></section></body></FictionBook>', encoding='utf-8')
            report = audit.compare(epub, fb2)
            self.assertEqual(report['differences']['missing_word_occurrences'], 0)
            self.assertFalse(report['differences']['blocks_identical_in_order'])

    def test_broken_result_link_is_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            fb2.write_text(fb2.read_text(encoding='utf-8').replace('l:href="#target"', 'l:href="#missing"'), encoding='utf-8')
            report = audit.compare(epub, fb2)
            self.assertEqual(report['fb2']['unresolved_fragment_links'], 1)
            self.assertEqual(report['epub']['unresolved_fragment_links'], 0)

    def test_report_cannot_overwrite_input(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            before = epub.read_bytes()
            result = subprocess.run([sys.executable, str(Path(__file__).with_name('compare-epub-fb2.py')), '--epub', str(epub), '--fb2', str(fb2), '--report', str(epub)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(epub.read_bytes(), before)

    def test_archive_traversal_refused(self):
        with self.assertRaises(ValueError):
            audit.archive_name('OPS/chapter.xhtml', '../../private.png')


if __name__ == '__main__':
    unittest.main()
