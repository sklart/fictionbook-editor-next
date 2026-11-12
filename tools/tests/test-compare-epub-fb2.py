import base64
import importlib.util
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest
import xml.etree.ElementTree as ET
import zipfile

spec = importlib.util.spec_from_file_location('compare_epub_fb2', Path(__file__).with_name('compare-epub-fb2.py'))
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class PairAudit(unittest.TestCase):
    def test_metadata_comparison_counts_matching_values_without_exposing_them(self):
        source = ET.fromstring('<package><metadata><title>Secret Title</title><creator>Jane Doe</creator><language>en</language></metadata></package>')
        result = ET.fromstring('<FictionBook><description><title-info><book-title>Secret Title</book-title><author><first-name>Jane</first-name><last-name>Doe</last-name></author><lang>en</lang></title-info></description></FictionBook>')
        source_hashes = audit.epub_metadata(source)
        result_hashes = audit.fb2_metadata(result)
        for field in ('title', 'author', 'language'):
            self.assertEqual(source_hashes[field], result_hashes[field])
        self.assertNotIn('Secret Title', str(source_hashes))
        self.assertEqual(audit.epub_metadata(ET.fromstring('<package/>'))['title'], [])

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

    def test_invalid_spine_reference_is_counted_and_valid_spine_is_compared(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/book.opf'] = entries['OPS/book.opf'].replace(b'</spine>', b'<itemref idref=""/><itemref idref="missing"/></spine>')
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items(): archive.writestr(name, data)
            report = audit.compare(epub, fb2)
            self.assertEqual(report['epub']['invalid_spine_itemrefs'], 2)
            self.assertEqual(report['epub']['spine_documents'], 1)
            self.assertEqual(report['status'], 'REVIEW')

    def test_metadata_analysis_is_in_pair_report(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/book.opf'] = entries['OPS/book.opf'].replace(b'<manifest>', b'<metadata><title>Example</title></metadata><manifest>')
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items(): archive.writestr(name, data)
            fb2.write_text(fb2.read_text(encoding='utf-8').replace('<body>', '<description><title-info><book-title>Example</book-title></title-info></description><body>'), encoding='utf-8')
            report = audit.compare(epub, fb2)
            self.assertEqual(report['differences']['metadata_analysis']['title']['matching_hash_occurrences'], 1)

    def test_missing_text_and_changed_image_are_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            report = audit.compare(*self.pair(Path(temp), 'Текст', b'changed'))
            self.assertEqual(report['differences']['missing_word_occurrences'], 2)
            self.assertEqual(report['differences']['missing_original_image_occurrences'], 1)
            self.assertEqual(len(report['differences']['missing_distinct_image_sha256']), 1)

    def test_repeated_image_reference_is_not_a_missing_distinct_image(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/chapter.xhtml'] = entries['OPS/chapter.xhtml'].replace(b'<img src="image.png"/>', b'<img src="image.png"/><img src="image.png"/>')
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items(): archive.writestr(name, data)
            report = audit.compare(epub, fb2)
            self.assertEqual(report['differences']['missing_original_image_occurrences'], 1)
            self.assertEqual(report['differences']['missing_distinct_image_sha256'], [])

    def test_cover_image_in_description_counts_as_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            text = fb2.read_text(encoding='utf8')
            text = text.replace('<body>', '<description><title-info><coverpage><image l:href="#image"/></coverpage></title-info></description><body>')
            text = text.replace('<image l:href="#image"/></p>', '</p>')
            fb2.write_text(text, encoding='utf8')
            report = audit.compare(epub, fb2)
            self.assertEqual(report['fb2']['cover_image_references'], 1)
            self.assertEqual(report['differences']['missing_distinct_image_sha256'], [])

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

    def test_image_in_spine_is_counted_without_parsing_it_as_xml(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/book.opf'] = (b'<package><manifest><item id="one" href="chapter.xhtml" '
                                       b'media-type="application/xhtml+xml"/><item id="image" '
                                       b'href="image.png" media-type="image/png"/></manifest>'
                                       b'<spine><itemref idref="one"/><itemref idref="image"/></spine></package>')
            entries['OPS/chapter.xhtml'] = b'<html><body><p>text</p></body></html>'
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items():
                    archive.writestr(name, data)
            result = audit.compare(epub, fb2)
            self.assertEqual(result['epub']['non_xhtml_spine_items'], 1)
            self.assertEqual(result['epub']['image_references'], 1)
            self.assertEqual(result['differences']['missing_original_image_occurrences'], 0)

    def test_missing_source_image_is_distinct_from_missing_imported_image(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist() if name != 'OPS/image.png'}
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items(): archive.writestr(name, data)
            result = audit.compare(epub, fb2)
            self.assertEqual(result['differences']['missing_original_image_occurrences'], 0)
            self.assertEqual(result['differences']['missing_epub_image_resource_occurrences'], 1)

    def test_common_xhtml_named_entities_do_not_fetch_a_dtd_or_create_markup(self):
        root = audit.parse_xhtml(b'<html><body><p>A&nbsp;B &amp; &lt;tag&gt; &mdash; &#x41;</p></body></html>')
        paragraph = audit.elements(root, 'p')[0]
        self.assertEqual(''.join(paragraph.itertext()), 'A\u00a0B & <tag> — A')
        root = audit.parse_xhtml(b'<html><body><p>&nbsp;<![CDATA[&nbsp;]]></p><!-- &nbsp; --></body></html>')
        self.assertEqual(''.join(audit.elements(root, 'p')[0].itertext()), '\u00a0&nbsp;')
        with self.assertRaises(ET.ParseError):
            audit.parse_xhtml(b'<html><body>&notARealEntity;</body></html>')

    def test_sequence_reports_reordering_without_claiming_missing_text(self):
        result = audit.compare_block_sequences(['a', 'b', 'c'], ['a', 'c', 'b'])
        self.assertEqual(result['unmatched_source_occurrences'], 0)
        self.assertEqual(result['unique_anchors_outside_ordered_subsequence'], 1)

    def test_sequence_distinguishes_missing_and_repeated_candidates(self):
        result = audit.compare_block_sequences(['a', 'b', 'c'], ['a', 'a', 'c'])
        self.assertEqual(result['unmatched_source_occurrences'], 1)
        self.assertEqual(result['unmatched_target_occurrences'], 1)
        self.assertEqual(result['extra_repeated_target_occurrences'], 1)
        self.assertEqual(result['unique_common_anchors'], 1)

    def test_sequence_handles_large_input_without_quadratic_matrix(self):
        blocks = [str(i) for i in range(100000)]
        result = audit.compare_block_sequences(blocks, list(reversed(blocks)))
        self.assertEqual(result['unique_anchors_outside_ordered_subsequence'], 99999)

    def test_punctuation_and_nbsp_differences_are_not_hidden_by_word_counts(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp), 'Текст Ω &amp; слово')
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/chapter.xhtml'] = '<html><body><p>Текст Ω &amp; слово,\u00a0да—нет</p></body></html>'.encode('utf-8')
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items():
                    archive.writestr(name, data)
            fb2.write_text('<FictionBook><body><section><p>Текст Ω &amp; слово да-нет</p></section></body></FictionBook>', encoding='utf-8')
            report = audit.compare(epub, fb2)
            self.assertEqual(report['differences']['missing_word_occurrences'], 0)
            self.assertEqual(report['differences']['significant_character_deficits'], {'U+002C': 1, 'U+00A0': 1, 'U+2014': 1})
            self.assertEqual(report['differences']['significant_character_excesses'], {'U+002D': 1})

    def test_source_block_provenance_marks_navigation_and_explicit_hidden(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/book.opf'] = (b'<package><manifest><item id="one" href="chapter.xhtml" '
                                       b'media-type="application/xhtml+xml"/><item id="nav" href="nav.xhtml" '
                                       b'media-type="application/xhtml+xml" properties="nav"/></manifest>'
                                       b'<spine><itemref idref="one"/></spine></package>')
            entries['OPS/chapter.xhtml'] = b'<html><body><div style="display:none"><p id="hidden">Hidden text</p></div><p>Visible</p></body></html>'
            entries['OPS/nav.xhtml'] = b'<html><body><h1>Navigation</h1></body></html>'
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items():
                    archive.writestr(name, data)
            source = audit.compare(epub, fb2)['epub']
            self.assertEqual(source['blocks'][0]['tree_path'], '/body[1]/div[1]/p[1]')
            self.assertTrue(source['blocks'][0]['explicitly_hidden'])
            self.assertFalse(source['blocks'][1]['explicitly_hidden'])
            self.assertEqual(source['blocks'][2]['document_role'], 'navigation')
            self.assertEqual(source['word_occurrences_by_document_role']['navigation'], 1)
            self.assertEqual(audit.compare(epub, fb2)['differences']['missing_spine_word_occurrences'], 3)

    def test_strict_space_and_inline_formatting_on_unique_matching_blocks(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/chapter.xhtml'] = '<html><body><p><em>Some</em>\u00a0text</p></body></html>'.encode('utf-8')
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items():
                    archive.writestr(name, data)
            fb2.write_text('<FictionBook><body><section><p>Some text</p></section></body></FictionBook>', encoding='utf-8')
            details = audit.compare(epub, fb2)['differences']['matched_block_details']
            self.assertEqual(details['unique_normalized_matches'], 1)
            self.assertEqual(details['strict_nfc_mismatches'], 1)
            self.assertEqual(details['inline_markup_mismatches'], 1)
            self.assertEqual(details['direct_inline_semantic_mismatches'], 1)

    def test_direct_inline_css_inheritance_and_overrides(self):
        source = audit.parse_xhtml(b'<p style="font-style: italic; font-weight: bold">A <span style="font-weight:normal">B</span> C</p>')
        same = audit.parse_xhtml(b'<p><strong><em>A</em></strong> <em>B</em> <strong><em>C</em></strong></p>')
        changed = audit.parse_xhtml(b'<p><strong><em>A B C</em></strong></p>')
        self.assertEqual(audit.inline_semantic_signature(source), audit.inline_semantic_signature(same))
        self.assertNotEqual(audit.inline_semantic_signature(source), audit.inline_semantic_signature(changed))
        priority = audit.parse_xhtml(b'<p style="font-style:italic!important;font-style:normal">A</p>')
        expected = audit.parse_xhtml(b'<p><em>A</em></p>')
        self.assertEqual(audit.inline_semantic_signature(priority), audit.inline_semantic_signature(expected))

    def test_nonspace_character_counts_help_triage_unsegmented_languages(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/chapter.xhtml'] = '<html><body><p>日本語文章</p></body></html>'.encode('utf-8')
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items(): archive.writestr(name, data)
            fb2.write_text('<FictionBook><body><section><p>日本語 文章</p></section></body></FictionBook>', encoding='utf-8')
            differences = audit.compare(epub, fb2)['differences']
            self.assertGreater(differences['missing_word_occurrences'], 0)
            self.assertEqual(differences['missing_nonspace_character_occurrences'], 0)

    def test_link_target_text_compared_without_requiring_matching_ids(self):
        src = [{'label_sha256': 'label', 'target_text_sha256': 'text', 'target_exists': True, 'note': True}]
        same = [{'label_sha256': 'label', 'target_text_sha256': 'text', 'target_exists': True, 'note': True}]
        changed = [{'label_sha256': 'label', 'target_text_sha256': 'different', 'target_exists': True, 'note': False}]
        self.assertEqual(audit.compare_link_targets(src, same)['candidate_target_text_differences'], 0)
        comparison = audit.compare_link_targets(src, changed)
        self.assertEqual(comparison['candidate_target_text_differences'], 1)
        self.assertEqual(comparison['candidate_note_classification_differences'], 1)
        self.assertEqual(audit.compare_link_targets(src * 2, same)['unique_labels_with_comparable_targets'], 0)

    def test_link_target_analysis_uses_real_epub_and_fb2_targets(self):
        with tempfile.TemporaryDirectory() as temp:
            epub, fb2 = self.pair(Path(temp))
            with zipfile.ZipFile(epub) as archive:
                entries = {name: archive.read(name) for name in archive.namelist()}
            entries['OPS/chapter.xhtml'] = b'<html><body><p><a href="#old">Go</a></p><p id="old">Target text</p></body></html>'
            with zipfile.ZipFile(epub, 'w') as archive:
                for name, data in entries.items(): archive.writestr(name, data)
            fb2.write_text('<FictionBook xmlns:l="http://www.w3.org/1999/xlink"><body><section><p><a l:href="#new">Go</a></p><p id="new">Target text</p></section></body></FictionBook>', encoding='utf-8')
            links = audit.compare(epub, fb2)['differences']['internal_link_target_analysis']
            self.assertEqual(links['unique_labels_with_comparable_targets'], 1)
            self.assertEqual(links['candidate_target_text_differences'], 0)
            fb2.write_text(fb2.read_text(encoding='utf-8').replace('Target text', 'Other target'), encoding='utf-8')
            links = audit.compare(epub, fb2)['differences']['internal_link_target_analysis']
            self.assertEqual(links['candidate_target_text_differences'], 1)


if __name__ == '__main__':
    unittest.main()
