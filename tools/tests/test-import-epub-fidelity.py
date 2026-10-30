"""Short EPUB 2/3 content regressions through the production batch/DLL boundary."""
import argparse
import base64
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
import xml.etree.ElementTree as ET
import zipfile
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[2]
NS = {'f': 'http://www.gribuser.ru/xml/fictionbook/2.0'}
HREF = '{http://www.w3.org/1999/xlink}href'
PNG = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII=')
JPEG = base64.b64decode('/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAABAAEDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwD50ooor8MP9Uz/2Q==')


class Fidelity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folders, cls.outputs, cls.performance = [], [], []

    @classmethod
    def tearDownClass(cls):
        # Independent .NET XSD reader, separate from the importer's MSXML check.
        script = r'''
$ErrorActionPreference='Stop'
$settings=[Xml.XmlReaderSettings]::new()
$settings.ValidationType=[Xml.ValidationType]::Schema
$settings.Schemas.XmlResolver=[Xml.XmlUrlResolver]::new()
$null=$settings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $env:FBE_SCHEMA_TEST_ROOT 'runtime/FictionBook.xsd'))
$settings.Schemas.Compile()
$settings.DtdProcessing=[Xml.DtdProcessing]::Prohibit
foreach($path in ($env:FBE_SCHEMA_TEST_FILES | ConvertFrom-Json)) {
    $reader=[Xml.XmlReader]::Create($path,$settings)
    try {while($reader.Read()) {}} finally {$reader.Close()}
}
Write-Output 'Independent FB2 XSD validation: PASS'
'''
        try:
            result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', script],
                                    env={**os.environ, 'FBE_SCHEMA_TEST_ROOT': str(ROOT),
                                         'FBE_SCHEMA_TEST_FILES': json.dumps([str(p) for p in cls.outputs])},
                                    capture_output=True, encoding='utf-8', errors='replace', timeout=30)
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)
            print('Independent FB2 XSD validation: PASS')
            if cls.performance:
                (ROOT / 'out/tests/epub-notes-perf-after.json').write_text(json.dumps(cls.performance, indent=2), encoding='utf-8')
        finally:
            for folder in cls.folders:
                folder.cleanup()

    def convert(self, body, extra=None, version='3.0', spine=None, fail=False, svg='keep', raw=None, options=(), manifest_extra=(), outside=None, navigation=None, extra_nav='', guide='', body_ids=False, identifier='fixture', date='', head='', language='ru', chapter_title='Fixture', creators=('Tester',), cover_id=''):
        extra = extra or {}
        temp = tempfile.TemporaryDirectory(prefix='epub-fidelity-')
        self.folders.append(temp)
        folder = Path(temp.name)
        self.last_folder = folder
        extraction_temp = folder / 'temp'
        extraction_temp.mkdir()
        for name, data in (outside or {}).items():
            (extraction_temp / name).write_bytes(data)
        epub = folder / 'case.epub'
        files = {'Text/chapter.xhtml': ('chapter', 'application/xhtml+xml', body), **extra}
        tree = [('Fixture', 'Text/chapter.xhtml', [])] if navigation is None else (navigation or [])
        counter = [0]
        def render_tree(entries, ncx=False):
            result = ''
            for title, href, children in entries:
                counter[0] += 1
                ident = counter[0]
                if ncx:
                    result += (f'<navPoint id="nav{ident}" playOrder="{ident}"><navLabel><text>{title}</text></navLabel>'
                               f'<content src="{href}"/>' + render_tree(children, True) + '</navPoint>')
                else:
                    nested = '<ol>' + render_tree(children) + '</ol>' if children else ''
                    result += f'<li><a href="{href}">{title}</a>{nested}</li>'
            return result
        if navigation is False:
            pass  # Deliberately exercise a package with no usable navigation resource.
        elif version.startswith('3'):
            files['nav.xhtml'] = ('fixture-nav', 'application/xhtml+xml', '<nav epub:type="toc"><ol>' + render_tree(tree) + '</ol></nav>' + extra_nav)
        else:
            files['toc.ncx'] = ('fixture-ncx', 'application/x-dtbncx+xml', '<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head><meta name="dtb:uid" content="fixture"/></head><docTitle><text>Fixture</text></docTitle><navMap>' + render_tree(tree, True) + '</navMap></ncx>')
        def xhtml(content, ident):
            return ('<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml" '
                    'xmlns:epub="http://www.idpf.org/2007/ops" xmlns:xlink="http://www.w3.org/1999/xlink">'
                    '<head><title>' + (chapter_title if ident == 'chapter' else 'Fixture') + '</title>' + (head if ident == 'chapter' else '') + '</head><body' + (f' id="body-{ident}"' if body_ids else '') + '>' + content + '</body></html>')
        with zipfile.ZipFile(epub, 'w') as z:
            z.writestr('mimetype', 'application/epub+zip')
            z.writestr('META-INF/container.xml', '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
            manifest = []
            for path, (ident, mime, content) in files.items():
                properties = ' properties="nav"' if ident == 'fixture-nav' else ''
                manifest.append(f'<item id="{ident}" href="{quote(path, safe="/")}" media-type="{mime}"{properties}/>')
                z.writestr('OEBPS/' + path, xhtml(content, ident) if mime == 'application/xhtml+xml' else content)
            modified = '<meta property="dcterms:modified">2026-10-21T00:00:00Z</meta>' if version.startswith('3') else ''
            if cover_id: modified += f'<meta name="cover" content="{cover_id}"/>'
            toc = '' if version.startswith('3') or navigation is False else ' toc="fixture-ncx"'
            date_xml = f'<dc:date>{date}</dc:date>' if date else ''
            language_xml = f'<dc:language>{language}</dc:language>' if language else ''
            creators_xml = ''.join(f'<dc:creator>{creator}</dc:creator>' for creator in creators)
            z.writestr('OEBPS/book.opf', f'<package xmlns="http://www.idpf.org/2007/opf" version="{version}" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">{identifier}</dc:identifier><dc:title>Fixture</dc:title>' + language_xml + creators_xml + date_xml + modified + '</metadata><manifest>' + ''.join(manifest) + ''.join(manifest_extra) + f'</manifest><spine{toc}>' + ''.join(f'<itemref idref="{ident}"/>' for ident in (spine or ['chapter'])) + '</spine>' + guide + '</package>')
            for path, content in (raw or {}).items():
                z.writestr(path, content)
        output = folder / 'case.fb2'
        result = subprocess.run([str(BATCH), str(epub), str(output), '--log', '--stats', '--svg', svg, *options],
                                capture_output=True, encoding='utf-8', errors='replace', timeout=45,
                                env={**os.environ, 'TEMP': str(extraction_temp), 'TMP': str(extraction_temp)})
        logs = '\n'.join(p.read_text(encoding='utf-8-sig') for p in folder.glob('*.log'))
        self.public_diagnostics = result.stdout + result.stderr
        self.diagnostics = self.public_diagnostics + logs
        if fail:
            self.assertNotEqual(result.returncode, 0, self.diagnostics)
            return None
        self.assertEqual(result.returncode, 0, self.diagnostics)
        root = ET.parse(output).getroot()
        self.outputs.append(output)
        ids = [n.attrib['id'] for n in root.iter() if 'id' in n.attrib]
        self.assertEqual(len(ids), len(set(ids)), self.diagnostics)
        binary = {n.attrib['id']: base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        for node in root.iter():
            href = node.get(HREF, '')
            if href.startswith('#'):
                self.assertIn(href[1:], ids, ET.tostring(node, encoding='unicode'))
                if node.tag.endswith('}image'):
                    self.assertIn(href[1:], binary)
        self.assertNotIn('[Chapter text was not extracted]', output.read_text(encoding='utf-8-sig'))
        self.assertNotIn('[Текст главы не извлечён]', output.read_text(encoding='utf-8-sig'))
        return root

    def notes(self, root):
        return root.findall('f:body[@name="notes"]/f:section', NS)

    def text(self, root):
        return ''.join(root.itertext())

    def test_links_outside_spine_epub2(self):
        root = self.convert('<h1>Main</h1><p>Chapter <a href="../links.xhtml#n">[1]</a> remains.</p>',
                            {'links.xhtml': ('links', 'application/xhtml+xml', '<p id="n"><em>Note one</em></p>')}, version='2.0')
        self.assertEqual(len(self.notes(root)), 1)
        self.assertIn('Note one', self.text(self.notes(root)[0]))
        self.assertIsNotNone(self.notes(root)[0].find('.//f:emphasis', NS))
        self.assertEqual(len(root.findall('.//f:a[@type="note"]', NS)), 1)

    def test_inline_note_preserves_chapter(self):
        root = self.convert('<h1>Chapter</h1><p>Before <a epub:type="noteref" href="#n">1</a></p>'
                            '<aside epub:type="footnote" id="n"><p>First note paragraph</p><p><b>Second paragraph</b></p></aside><p>After note</p>')
        main = root.find('f:body', NS)
        self.assertIn('Before', self.text(main)); self.assertIn('After note', self.text(main))
        self.assertNotIn('First note paragraph', self.text(main))
        self.assertEqual(len(self.notes(root)[0].findall('f:p', NS)), 2)

    def test_removed_backlink_keeps_referenced_anchor(self):
        for backlink in ('<a id="footnote-042-backlink" epub:type="backlink" href="#ref">Return</a>',
                         '<p><a id="footnote-042-backlink" epub:type="backlink" href="#ref">Return</a>Note end</p>',
                         '<p><a id="footnote-042-backlink" epub:type="backlink" href="#ref">Return</a></p>'):
            with self.subTest(backlink=backlink):
                root = self.convert('<p><a id="ref" epub:type="noteref" href="#n">1</a> '
                                    '<a href="#footnote-042-backlink">Go to marker</a></p>'
                                    '<aside epub:type="footnote" id="n"><p>Note text</p>' + backlink + '</aside>')
                note, = self.notes(root)
                self.assertNotIn('Return', self.text(note))
                link = next(a for a in root.findall('.//f:a', NS) if self.text(a) == 'Go to marker')
                self.assertTrue(any('#' + node.get('id', '') == link.get(HREF) for node in note.iter()))

    def test_standalone_backlink_is_preserved_when_requested(self):
        root = self.convert('<p><a id="ref" epub:type="noteref" href="#n">1</a></p>'
                            '<aside epub:type="footnote" id="n"><p>Note text</p>'
                            '<a id="footnote-042-backlink" epub:type="backlink" href="#ref">Return</a></aside>',
                            options=['--keep-footnote-backlinks'])
        note, = self.notes(root)
        backlink, = [a for a in note.findall('.//f:a', NS) if self.text(a) == 'Return']
        self.assertTrue(backlink.get(HREF, '').startswith('#'))
        self.assertTrue(any('#' + node.get('id', '') == backlink.get(HREF) for node in root.find('f:body', NS).iter()))

    def test_removed_backlink_anchor_in_non_spine_notes_document(self):
        root = self.convert('<p><a id="ref" epub:type="noteref" href="../notes.xhtml#n">1</a> '
                            '<a href="../notes.xhtml#back">Marker</a></p>',
                            {'notes.xhtml': ('notes', 'application/xhtml+xml',
                                             '<aside epub:type="footnote" id="n"><p>External note</p>'
                                             '<a id="back" epub:type="backlink" href="Text/chapter.xhtml#ref">'
                                             '<span id="nested">Return</span></a></aside>')})
        note, = self.notes(root)
        link = next(a for a in root.findall('.//f:a', NS) if self.text(a) == 'Marker')
        self.assertNotIn('Return', self.text(note))
        self.assertTrue(any('#' + node.get('id', '') == link.get(HREF) for node in note.iter()))
        self.assertTrue(any(node.get('id', '').endswith('nested') for node in note.iter()))

    def test_list_notes(self):
        root = self.convert('<p>Text<a href="../notes.xhtml#n">1</a></p>',
                            {'notes.xhtml': ('notes', 'application/xhtml+xml', '<ol><li id="n"><p>First</p><p><em>Second</em></p></li></ol>')})
        note = self.notes(root)[0]
        self.assertEqual(len(note.findall('f:p', NS)), 2)
        self.assertIsNotNone(note.find('.//f:emphasis', NS))

    def test_optional_unified_notes_follow_first_reference_and_preserve_links(self):
        body = ('<p id="n_1">Start <a epub:type="noteref" href="../notes.xhtml#b"><strong>42</strong></a> '
                '<a epub:type="noteref" href="../notes.xhtml#a">7</a> '
                '<a epub:type="noteref" href="../notes.xhtml#b">42</a> '
                '<a href="../notes.xhtml#b">Ordinary link</a></p>')
        extra = {'notes.xhtml': ('notes', 'application/xhtml+xml',
                 '<aside epub:type="footnote" id="a"><p>First in file</p></aside>'
                 '<aside epub:type="footnote" id="b"><p>Second in file</p><p><em>More text</em></p></aside>')}
        normal = self.convert(body, extra)
        self.assertEqual([self.text(a) for a in normal.findall('.//f:a[@type="note"]', NS)], ['42', '7', '42'])
        root = self.convert(body, extra, options=['--unify-note-numbers'])
        notes = self.notes(root)
        links = root.findall('f:body//f:a[@type="note"]', NS)
        self.assertEqual([self.text(a) for a in links], ['[1]', '[2]', '[1]'])
        self.assertIsNotNone(links[0].find('f:strong', NS))
        self.assertEqual(links[0].get(HREF), links[2].get(HREF))
        self.assertEqual([n.get('id') for n in notes], [links[0].get(HREF)[1:], links[1].get(HREF)[1:]])
        self.assertTrue(self.text(notes[0].find('f:title', NS)).startswith('[1] '))
        self.assertTrue(self.text(notes[1].find('f:title', NS)).startswith('[2] '))
        self.assertIn('Second in file', self.text(notes[0]))
        self.assertIn('First in file', self.text(notes[1]))
        self.assertIsNotNone(notes[0].find('.//f:emphasis', NS))
        self.assertEqual(next(a for a in root.findall('.//f:a', NS) if self.text(a) == 'Ordinary link').get(HREF), links[0].get(HREF))
        self.assertNotEqual(links[0].get(HREF), '#n_1')

    def test_unified_note_numbers_preserve_meaningful_link_text(self):
        root = self.convert('<p>See <a epub:type="noteref" href="#n"><em>translator note</em></a>.</p>'
                            '<aside epub:type="footnote" id="n"><p>Detailed note</p></aside>',
                            options=['--unify-note-numbers'])
        link, = root.findall('.//f:a[@type="note"]', NS)
        self.assertEqual(self.text(link), 'translator note [1]')
        self.assertIsNotNone(link.find('f:emphasis', NS))

    def test_unified_notes_preserve_unreferenced_notes_and_crosslinks(self):
        body = ('<p><a epub:type="noteref" href="../notes.xhtml#b">9</a> '
                '<a epub:type="noteref" href="../notes.xhtml#a">2</a></p>')
        extra = {'notes.xhtml': ('notes', 'application/xhtml+xml',
                 '<aside epub:type="footnote" id="a"><p>A <a href="#b">Other note</a></p></aside>'
                 '<aside epub:type="footnote" id="b"><p>B</p></aside>'
                 '<aside epub:type="footnote" id="unused"><p>Unreferenced note</p></aside>')}
        root = self.convert(body, extra, options=['--unify-note-numbers'])
        notes = self.notes(root)
        self.assertEqual(len(notes), 3)
        self.assertEqual(notes[0].get('id'), 'n_1')
        self.assertEqual(notes[1].get('id'), 'n_2')
        self.assertIn('Unreferenced note', self.text(notes[2]))
        link = next(a for a in notes[1].findall('.//f:a', NS) if self.text(a) == 'Other note')
        self.assertEqual(link.get(HREF), '#n_1')

    def test_duplicate_ids_and_arbitrary_filenames(self):
        root = self.convert('<p>One<a href="../alpha.xhtml#n">1</a> Two<a href="../beta.xhtml#n">2</a></p>',
                            {p: (p[0], 'application/xhtml+xml', f'<div id="n"><p>{t}</p></div>')
                             for p, t in [('alpha.xhtml', 'Alpha note'), ('beta.xhtml', 'Beta note')]})
        notes = self.notes(root)
        self.assertEqual(len(notes), 2)
        links = root.findall('.//f:a[@type="note"]', NS)
        self.assertNotEqual(links[0].get(HREF), links[1].get(HREF))
        for link, text in zip(links, ['Alpha note', 'Beta note']):
            self.assertIn(text, self.text(next(n for n in notes if '#' + n.get('id') == link.get(HREF))))

    def test_ordinary_links(self):
        root = self.convert('<h1 id="N">Title</h1><p><a href="#N">Section</a></p><p id="n">Other anchor</p>')
        self.assertEqual(self.notes(root), [])
        self.assertEqual(root.findall('.//f:a[@type="note"]', NS), [])

    def test_numeric_link_to_heading_is_not_a_note(self):
        root = self.convert('<p>Index<a href="../appendix.xhtml#n">1</a></p>',
                            {'appendix.xhtml': ('appendix', 'application/xhtml+xml', '<h1 id="n">Ordinary appendix</h1><p>Appendix</p>')})
        self.assertEqual(self.notes(root), [])
        self.assertEqual(root.findall('.//f:a[@type="note"]', NS), [])
        self.assertIn('outside imported content', self.diagnostics)

    def test_superscript_unmarked_inline_note(self):
        root = self.convert('<p>Main<sup><a href="#n">1</a></sup></p><p id="n">Unmarked note</p><p>Tail</p>')
        self.assertEqual(len(self.notes(root)), 1)
        self.assertNotIn('Unmarked note', self.text(root.find('f:body', NS)))
        self.assertIn('Tail', self.text(root.find('f:body', NS)))

    def test_forward_document_and_body_anchor(self):
        root = self.convert('<p><a href="later.xhtml">Next chapter</a><a href="later.xhtml#body-id">Its body</a></p>',
                            {'Text/later.xhtml': ('later', 'application/xhtml+xml', '<section id="body-id"><h1>Later</h1><p>Later content</p></section>')},
                            spine=['chapter', 'later'])
        self.assertTrue(all(a.get(HREF).startswith('#') for a in root.findall('.//f:a', NS)))

    def test_title_only_leaf_preserved_without_filler(self):
        root = self.convert('<h1>Empty leaf</h1>')
        body = root.find('f:body', NS)
        self.assertEqual(self.text(body).count('Empty leaf'), 1)
        self.assertIsNotNone(body.find('.//f:p/f:strong', NS))
        self.assertEqual(body.findall('.//f:empty-line', NS), [])
        self.assertIn('Title-only section preserved', self.diagnostics)

    def test_empty_binary_reports_failure(self):
        self.convert('<p><img src="../empty.png"/></p>', {'empty.png': ('pic', 'image/png', b'')}, fail=True)
        self.assertIn('skipped=1', self.diagnostics)

    def test_encoded_note_path_and_fragment(self):
        root = self.convert('<p>Text<a href="../%D0%BF%D1%80%D0%B8%D0%BC%20one.xhtml#%D0%BD1">1</a></p>',
                            {'прим one.xhtml': ('notes', 'application/xhtml+xml', '<p id="н1">Unicode note</p>')})
        self.assertEqual(len(self.notes(root)), 1)

    def test_links_with_splitting_disabled(self):
        root = self.convert('<h1 id="h">Heading</h1><p><a href="#h">Heading link</a></p>', options=['--no-split-sections'])
        self.assertEqual(len(root.findall('.//f:a', NS)), 1)

    def test_empty_parent_and_heading_levels(self):
        root = self.convert('<h1 id="parent">Parent</h1><h2 id="child">Child</h2><p>Child text</p><h3>Deep</h3><p>Deep text</p><h2>Next</h2><p>Next text</p>')
        parent = root.find('f:body/f:section', NS)
        self.assertIsNone(parent.find('f:p', NS))
        self.assertEqual(len(parent.findall('f:section', NS)), 2)
        self.assertIsNotNone(parent.find('f:section/f:section', NS))

    def test_block_and_inline_images_preserve_context(self):
        root = self.convert('<h1>Images</h1><img src="../a.png"/><p>Before<img src="../a.png"/>After</p><p><img src="../a.png"/></p>',
                            {'a.png': ('a', 'image/png', PNG)})
        body = root.find('f:body', NS)
        self.assertEqual(len(body.findall('.//f:section/f:image', NS)), 2)
        inline = next(p for p in body.findall('.//f:p', NS) if 'Before' in self.text(p))
        self.assertIsNotNone(inline.find('f:image', NS))
        self.assertIn('After', self.text(inline))
        self.assertEqual(base64.b64decode(root.find('f:binary', NS).text), PNG)

    def test_figure_image_and_caption_are_preserved(self):
        root = self.convert('<h1>Illustration</h1><figure><img src="../a.png"/><figcaption>Picture caption</figcaption></figure><p>Following text</p>',
                            {'a.png': ('a', 'image/png', PNG)})
        self.assert_once(root, 'Picture caption', 'Following text')
        self.assertIsNotNone(root.find('.//f:section/f:image', NS))
        self.assertLess(self.text(root).index('Picture caption'), self.text(root).index('Following text'))
        self.assertEqual(base64.b64decode(root.find('f:binary', NS).text), PNG)

    def test_figure_and_caption_anchor_targets_are_preserved(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<h1>Illustration</h1><figure id="figure-anchor"><img id="image-anchor" src="../a.png"/>'
                                    '<figcaption id="caption-anchor">Picture <em id="caption-inner">caption</em></figcaption></figure>'
                                    '<p><a href="#figure-anchor">Figure link</a> '
                                    '<a href="#caption-anchor">Caption link</a> <a href="#caption-inner">Inner link</a> '
                                    '<a href="#image-anchor">Image link</a></p>',
                                    {'a.png': ('a', 'image/png', PNG)}, version=version)
                self.assert_once(root, 'Picture caption', 'Figure link', 'Caption link', 'Inner link', 'Image link')
                ids = {node.get('id') for node in root.iter() if node.get('id')}
                self.assertTrue({'figure-anchor', 'image-anchor', 'caption-anchor', 'caption-inner'} <= ids)
                self.assertIsNotNone(root.find(".//f:section/f:image[@id='image-anchor']", NS))
                links = {a.get(HREF) for a in root.findall('.//f:a', NS)}
                self.assertTrue({'#figure-anchor', '#image-anchor', '#caption-anchor', '#caption-inner'} <= links)
                self.assertEqual(base64.b64decode(root.find('f:binary', NS).text), PNG)

    def test_images_at_paragraph_edges_remain_inline_with_text(self):
        root = self.convert('<h1>Mixed</h1><p><img src="../a.png"/>Leading text</p><p>Trailing text<img src="../a.png"/></p>',
                            {'a.png': ('a', 'image/png', PNG)})
        for phrase in ('Leading text', 'Trailing text'):
            paragraph = next(p for p in root.findall('.//f:p', NS) if phrase in self.text(p))
            self.assertIsNotNone(paragraph.find('f:image', NS))
        self.assertEqual(base64.b64decode(root.find('f:binary', NS).text), PNG)

    def test_optional_spacing_around_block_images_not_inline_images(self):
        body = ('<h1>Pictures</h1><p>Opening</p><img src="../a.png"/>'
                '<p>Text<img src="../a.png"/>continues</p><p id="image-anchor"><img src="../a.png"/></p><p>End</p>')
        images = {'a.png': ('a', 'image/png', PNG)}
        for before, after in ((False, False), (True, False), (False, True), (True, True)):
            with self.subTest(before=before, after=after):
                options = (['--blank-before-images'] if before else []) + (['--blank-after-images'] if after else [])
                root = self.convert(body, images, options=options)
                section = root.find('f:body/f:section', NS)
                children = list(section)
                blocks = [i for i, child in enumerate(children) if child.tag.endswith('}image') or
                          (child.tag.endswith('}p') and child.find('f:image', NS) is not None and not self.text(child).strip())]
                self.assertEqual(len(blocks), 2)
                for index in blocks:
                    self.assertEqual(index > 0 and children[index-1].tag.endswith('}empty-line'), before)
                    self.assertEqual(index+1 < len(children) and children[index+1].tag.endswith('}empty-line'), after)
                inline = next(p for p in section.findall('f:p', NS) if 'Text' in self.text(p))
                self.assertEqual(self.text(inline), 'Textcontinues')
                self.assertIsNotNone(inline.find('f:image', NS))
                self.assertTrue(any(n.get('id') == 'image-anchor' for n in root.iter()))

    def test_image_spacing_at_section_start_remains_valid(self):
        root = self.convert('<h1>Pictures</h1><img src="../a.png"/><p>Text</p>',
                            {'a.png': ('a', 'image/png', PNG)},
                            options=['--blank-before-images', '--blank-after-images'])
        section = root.find('f:body/f:section', NS)
        tags = [node.tag.rsplit('}', 1)[-1] for node in section]
        image = tags.index('image')
        self.assertEqual(tags[image-1:image+2], ['empty-line', 'image', 'empty-line'])

    def test_image_spacing_for_consecutive_images_and_notes(self):
        root = self.convert('<p>Body<a epub:type="noteref" href="#n">1</a></p>'
                            '<img src="../a.png"/><img src="../a.png"/><p>End</p>'
                            '<aside epub:type="footnote" id="n"><p>Note</p><img src="../a.png"/><p>Note end</p></aside>',
                            {'a.png': ('a', 'image/png', PNG)},
                            options=['--blank-before-images', '--blank-after-images'])
        self.assertEqual(len(root.findall('f:body//f:image', NS)), 3)
        for section in root.findall('.//f:section', NS):
            children = list(section)
            for index, child in enumerate(children):
                standalone = child.tag.endswith('}image') or (child.tag.endswith('}p') and
                             child.find('f:image', NS) is not None and not self.text(child).strip())
                if standalone:
                    self.assertTrue(index and children[index-1].tag.endswith('}empty-line'))
                    self.assertTrue(index+1 < len(children) and children[index+1].tag.endswith('}empty-line'))
        self.assert_once(root, 'Body', 'End', 'Note end')

    def test_image_spacing_with_nested_sections_preserves_content(self):
        root = self.convert('<section><h1>Parent</h1><img src="../a.png"/>'
                            '<section><h2>Child</h2><p>Child text</p></section></section>',
                            {'a.png': ('a', 'image/png', PNG)},
                            options=['--blank-before-images', '--blank-after-images'])
        self.assert_once(root, 'Parent', 'Child text')
        self.assertEqual(len(root.findall('f:body//f:image', NS)), 1)

    def test_consecutive_images_before_body_remain_valid(self):
        root = self.convert('<h1>Images</h1><img src="../a.png"/><img src="../b.png"/><p>Following text</p>',
                            {'a.png': ('a', 'image/png', PNG), 'b.png': ('b', 'image/png', PNG)})
        self.assertEqual(len(root.findall('.//f:body//f:image', NS)), 2)
        self.assert_once(root, 'Following text')
        self.assertEqual(len(root.findall('f:binary', NS)), 2)
        self.assertIn('Image-only paragraph retained', self.diagnostics)

    def test_image_paragraph_anchor_remains_resolvable(self):
        root = self.convert('<h1>Images</h1><p id="illustration"><img src="../a.png"/></p>'
                            '<p>See <a href="#illustration">illustration</a>.</p>',
                            {'a.png': ('a', 'image/png', PNG)})
        link = next(a for a in root.findall('.//f:a', NS) if 'illustration' in self.text(a))
        target = link.get(HREF).removeprefix('#')
        self.assertTrue(target)
        self.assertEqual(len([node for node in root.iter() if node.get('id') == target]), 1)
        self.assertEqual(base64.b64decode(root.find('f:binary', NS).text), PNG)

    def test_program_used_identifies_current_build(self):
        root = self.convert('<p>Content</p>')
        header = (ROOT / 'src/version.h').read_text(encoding='utf-8')
        version = re.search(r'#define FBE_VERSION_STRING "([^"]+)"', header).group(1)
        self.assertEqual(root.find('.//f:document-info/f:program-used', NS).text,
                         'FBE Next ImportEPUB plugin, FictionBook Editor Next ' + version)

    def test_section_id_and_intentional_repeated_heading_text(self):
        root = self.convert('<section id="Section0035"><h1>Repeated Heading</h1>'
                            '<p>Repeated Heading</p><p>Following body</p></section>', navigation=False)
        ids = [node.get('id', '') for node in root.iter()]
        self.assertTrue(any('Section0035' in identifier for identifier in ids), ids)
        self.assertEqual(self.text(root).count('Repeated Heading'), 2)
        self.assert_once(root, 'Following body')

    def test_navigation_heading_has_no_generated_duplicate(self):
        root = self.convert('<h1 id="chapter">Chapter Heading</h1><p>Body text</p>',
                            navigation=[('Chapter Heading', 'Text/chapter.xhtml#chapter', [])])
        self.assert_once(root, 'Chapter Heading', 'Body text')

    def test_first_quote_does_not_gain_text_heading(self):
        root = self.convert('<blockquote><p><em>Opening quotation</em></p></blockquote><p>Actual body</p>', navigation=False)
        self.assert_once(root, 'Opening quotation', 'Actual body')
        for title in root.findall('.//f:title', NS):
            self.assertNotEqual(self.text(title).strip(), 'Текст')

    def test_optional_title_block_uses_metadata_and_avoids_existing_equivalent(self):
        plain = self.convert('<p>Opening text</p>', navigation=False, chapter_title='Chapter')
        self.assertNotIn('Tester', self.text(plain.find('f:body', NS)))
        added = self.convert('<p>Opening text</p>', navigation=False, chapter_title='Chapter', options=('--title-block',))
        body = added.find('f:body', NS)
        self.assert_once(body, 'Fixture', 'Tester', 'Opening text')
        existing = self.convert('<p>Fixture</p><p>Tester</p><p>Opening text</p>',
                                navigation=False, chapter_title='Chapter', options=('--title-block',))
        self.assert_once(existing.find('f:body', NS), 'Fixture', 'Tester', 'Opening text')

    def test_title_block_handles_multiple_and_absent_authors(self):
        for authors in [('Ada', 'Leo Tolstoy'), ()]:
            root = self.convert('<p>Opening text</p>', navigation=False, chapter_title='Chapter',
                                creators=authors, options=('--title-block',))
            body = root.find('f:body', NS)
            self.assert_once(body, 'Fixture', 'Opening text', *authors)
            self.assertEqual(len(root.findall('.//f:title-info/f:author', NS)), max(1, len(authors)))

    def test_title_block_suppresses_only_converter_generated_duplicate(self):
        added = self.convert('<p>Opening text</p>', navigation=False, options=('--title-block',))
        self.assert_once(added.find('f:body', NS), 'Fixture', 'Tester', 'Opening text')
        repeated = self.convert('<h1>Fixture</h1><p>Fixture</p><p>Body</p>',
                                navigation=False, options=('--title-block',))
        body = repeated.find('f:body', NS)
        self.assertEqual(self.text(body).count('Fixture'), 3)
        self.assert_once(body, 'Tester', 'Body')

    def test_later_matching_text_is_not_assumed_to_be_a_title_page(self):
        root = self.convert('<p>Opening text</p><p>Fixture</p><p>Tester</p><p>Following</p>',
                            navigation=False, chapter_title='Chapter', options=('--title-block',))
        body = root.find('f:body', NS)
        self.assertEqual(self.text(body).count('Fixture'), 2)
        self.assertEqual(self.text(body).count('Tester'), 2)
        self.assert_once(body, 'Opening text', 'Following')

    def test_asterisk_separators_are_optional_and_do_not_change_other_paragraphs(self):
        body = '<p>Before</p><p>***</p><p>* * *</p><p>2 * 3 = 6</p><p>After</p>'
        default = self.convert(body, navigation=False)
        self.assertEqual([self.text(p) for p in default.findall('.//f:p', NS) if self.text(p) in ('***', '* * *')], ['***', '* * *'])
        self.assertFalse(any(self.text(n) in ('***', '* * *') for n in default.findall('.//f:subtitle', NS)))
        converted = self.convert(body, navigation=False, options=('--separator-subtitles',))
        self.assertEqual([self.text(n) for n in converted.findall('.//f:subtitle', NS) if self.text(n) in ('***', '* * *')], ['***', '* * *'])
        self.assert_once(converted, 'Before', '2 * 3 = 6', 'After')

    def test_separator_conversion_preserves_anchor_and_avoids_quote(self):
        root = self.convert('<p><a href="#divider">Jump</a></p><p id="divider"><em>* * *</em></p>'
                            '<blockquote><p>***</p></blockquote><p>End</p>', navigation=False,
                            options=('--separator-subtitles',))
        subtitles = [n for n in root.findall('.//f:subtitle', NS) if self.text(n) == '* * *']
        self.assertEqual(len(subtitles), 1)
        self.assertIsNotNone(subtitles[0].find('.//f:emphasis', NS))
        self.assertTrue(subtitles[0].get('id'))
        link = next(n for n in root.findall('.//f:a', NS) if self.text(n) == 'Jump')
        self.assertEqual(link.get(HREF), '#' + subtitles[0].get('id'))
        self.assertFalse(any(self.text(n) == '***' for n in root.findall('.//f:subtitle', NS)))
        self.assert_once(root, 'End')

    def test_inline_style_override_does_not_leak_to_next_paragraph(self):
        root = self.convert('<p><em>Italic <span style="font-style: normal">Normal</span> italic again</em></p>'
                            '<p>Plain following paragraph</p>', navigation=False)
        plain = next(p for p in root.findall('.//f:p', NS) if 'Plain following paragraph' in self.text(p))
        self.assertEqual(len(plain.findall('.//f:strong', NS)), 0)
        self.assertEqual(len(plain.findall('.//f:emphasis', NS)), 0)
        self.assert_once(root, 'Plain following paragraph', 'Normal')
        self.assertFalse(any('Normal' in self.text(node) for node in root.findall('.//f:emphasis', NS)))

    def test_inline_css_bold_italic_combination(self):
        root = self.convert('<p><span style="font-style: italic; font-weight: 700">Styled text</span></p><p>Plain next</p>', navigation=False)
        styled = next(p for p in root.findall('.//f:p', NS) if 'Styled text' in self.text(p))
        self.assertTrue(any('Styled text' in self.text(node) for node in styled.findall('.//f:strong', NS)))
        self.assertTrue(any('Styled text' in self.text(node) for node in styled.findall('.//f:emphasis', NS)))
        plain = next(p for p in root.findall('.//f:p', NS) if 'Plain next' in self.text(p))
        self.assertIsNone(plain.find('.//f:strong', NS))
        self.assertIsNone(plain.find('.//f:emphasis', NS))

    def test_numeric_font_weights_and_nested_reset(self):
        root = self.convert('<p><span style="font-weight: 550">Light</span> '
                            '<span style="font-weight: 600">Bold <span style="font-weight: 1">Reset</span></span> '
                            '<span style="font-weight: 1000">Heavy</span></p>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Light' in self.text(p))
        strong = paragraph.findall('.//f:strong', NS)
        for label in ('Bold', 'Heavy'):
            self.assertTrue(any(label in self.text(n) for n in strong), label)
        for label in ('Light', 'Reset'):
            self.assertFalse(any(label in self.text(n) for n in strong), label)
        self.assertNotIn('Unsupported CSS font declaration', self.diagnostics)

    def test_block_styles_inherit_and_nested_normal_resets(self):
        root = self.convert('<div style="font-style: italic; font-weight: 700">'
                            '<p>Inherited <span style="font-style: normal; font-weight: normal">Reset</span> Restored</p>'
                            '<p style="font-style: normal; font-weight: 400">Plain block</p></div>'
                            '<p>Outside block</p>', navigation=False)
        paragraphs = root.findall('.//f:p', NS)
        inherited = next(p for p in paragraphs if 'Inherited' in self.text(p))
        for tag in ('strong', 'emphasis'):
            formatted = inherited.findall('.//f:' + tag, NS)
            self.assertTrue(any('Inherited' in self.text(n) for n in formatted))
            self.assertTrue(any('Restored' in self.text(n) for n in formatted))
            self.assertFalse(any('Reset' in self.text(n) for n in formatted))
        for label in ('Plain block', 'Outside block'):
            paragraph = next(p for p in paragraphs if label in self.text(p))
            self.assertIsNone(paragraph.find('.//f:strong', NS))
            self.assertIsNone(paragraph.find('.//f:emphasis', NS))

    def test_css_inherit_initial_and_unset_use_parent_values(self):
        root = self.convert('<div style="font-style: italic; font-weight: bold"><p>'
                            '<span class="reset" style="font-style: inherit; font-weight: unset">Inherited</span> '
                            '<span style="font-style: initial; font-weight: initial">Initial</span></p></div>'
                            '<p><em style="font-style: inherit">Plain em</em></p>',
                            head='<style>.reset {font-style: normal; font-weight: normal}</style>', navigation=False)
        formatted = root.findall('.//f:emphasis', NS), root.findall('.//f:strong', NS)
        for nodes in formatted:
            self.assertTrue(any('Inherited' in self.text(n) for n in nodes))
            self.assertFalse(any('Initial' in self.text(n) for n in nodes))
        self.assertFalse(any('Plain em' in self.text(n) for n in formatted[0]))
        self.assertNotIn('Unsupported CSS font declaration', self.diagnostics)

    def test_stylesheet_classes_inheritance_cascade_and_inline_override(self):
        root = self.convert('<div class="outer"><p class="both">Inherited '
                            '<span class="reset">Reset</span> Restored</p></div><p>Plain</p>',
                            extra={'styles/book.css': ('sheet', 'text/css',
                                '.outer { font-style: italic } .both { font-weight: 700 } '
                                'span.reset { font-style: normal; font-weight: normal }')},
                            head='<link rel="stylesheet" type="text/css" href="../styles/book.css" />', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Inherited' in self.text(p))
        for tag in ('strong', 'emphasis'):
            formatted = paragraph.findall('.//f:' + tag, NS)
            self.assertTrue(any('Inherited' in self.text(n) for n in formatted))
            self.assertTrue(any('Restored' in self.text(n) for n in formatted))
            self.assertFalse(any('Reset' in self.text(n) for n in formatted))
        plain = next(p for p in root.findall('.//f:p', NS) if 'Plain' in self.text(p))
        self.assertIsNone(plain.find('.//f:strong', NS))
        self.assertIsNone(plain.find('.//f:emphasis', NS))

    def test_external_stylesheet_encoded_unicode_path_with_query(self):
        path = 'styles/Книга style.css'
        href = '../' + quote(path, safe='/') + '?version=2'
        root = self.convert('<p class="styled">Text styled</p>',
                            extra={path: ('sheet', 'text/css', '.styled { font-style: italic }')},
                            head=f'<link rel="stylesheet" href="{href}" />', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Text styled' in self.text(p))
        self.assertIsNotNone(paragraph.find('.//f:emphasis', NS))

    def test_stylesheet_not_in_manifest_is_not_loaded(self):
        root = self.convert('<p class="styled">Text plain</p>',
                            raw={'OEBPS/styles/unlisted.css': '.styled {font-weight: bold}'},
                            head='<link rel="stylesheet" href="../styles/unlisted.css" />', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Text plain' in self.text(p))
        self.assertIsNone(paragraph.find('.//f:strong', NS))
        self.assertIn('Stylesheet could not be loaded from the EPUB manifest', self.diagnostics)

    def test_stylesheet_encoded_traversal_is_not_loaded(self):
        root = self.convert('<p class="styled">Text plain</p>',
                            outside={'secret.css': b'.styled {font-weight: bold}'},
                            head='<link rel="stylesheet" href="../../../%2e%2e/secret.css" />', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Text plain' in self.text(p))
        self.assertIsNone(paragraph.find('.//f:strong', NS))
        self.assertIn('Stylesheet could not be loaded from the EPUB manifest', self.diagnostics)

    def test_embedded_css_specificity_and_source_order(self):
        root = self.convert('<p class="first second">Formatted <span style="font-weight: normal">Normal</span></p>',
                            head='<style type="text/css">p.first { font-weight: bold } '
                                 '.second { font-weight: normal } .first { font-style: italic } '
                                 '.first { font-style: normal } .second { font-style: italic }</style>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Formatted' in self.text(p))
        self.assertTrue(any('Formatted' in self.text(n) for n in paragraph.findall('.//f:strong', NS)))
        self.assertTrue(any('Formatted' in self.text(n) for n in paragraph.findall('.//f:emphasis', NS)))
        self.assertFalse(any('Normal' in self.text(n) for n in paragraph.findall('.//f:strong', NS)))

    def test_compound_class_selector_matches_all_classes_and_specificity(self):
        root = self.convert('<p class="one two">Both classes</p><p class="one">One class</p>'
                            '<p class="One two">Case differs</p>',
                            head='<style>p.one.two {font-weight: bold; font-style: italic} '
                                 '.one {font-weight: normal}</style>', navigation=False)
        paragraphs = root.findall('.//f:p', NS)
        both = next(p for p in paragraphs if 'Both classes' in self.text(p))
        self.assertIsNotNone(both.find('.//f:strong', NS))
        self.assertIsNotNone(both.find('.//f:emphasis', NS))
        for label in ('One class', 'Case differs'):
            paragraph = next(p for p in paragraphs if label in self.text(p))
            self.assertIsNone(paragraph.find('.//f:strong', NS))
            self.assertIsNone(paragraph.find('.//f:emphasis', NS))

    def test_css_classes_apply_to_notes_without_leaking_to_other_documents(self):
        root = self.convert('<p>Chapter<a epub:type="noteref" href="#n">1</a></p>'
                            '<aside epub:type="footnote" id="n"><p class="em">Styled note</p></aside>',
                            extra={'Text/next.xhtml': ('next', 'application/xhtml+xml', '<p class="em">Plain other document</p>')},
                            spine=['chapter', 'next'], head='<style>.em {font-style: italic}</style>', navigation=False)
        note = self.notes(root)[0]
        self.assertTrue(any('Styled note' in self.text(n) for n in note.findall('.//f:emphasis', NS)))
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Plain other document' in self.text(p))
        self.assertIsNone(paragraph.find('.//f:emphasis', NS))

    def test_unsupported_css_font_selector_is_reported(self):
        self.convert('<p class="a"><span class="b">Text survives</span></p>',
                     head='<style>.a > .b {font-style: italic}</style>', navigation=False)
        self.assertIn('Unsupported CSS font selector', self.diagnostics)

    def test_css_descendant_selectors_match_ancestors_and_cascade(self):
        root = self.convert('<div class="outer"><p><span class="target">Nested italic</span></p>'
                            '<p class="target">Ancestor is not self</p></div>'
                            '<p><span class="target">Outside plain</span></p><p class="self target">Self only</p>'
                            '<div class="outer"><p class="target">Specific bold</p></div>',
                            head='<style>.outer .target {font-style: italic} '
                                 'div.outer p.target {font-weight: bold} '
                                 'p.target {font-weight: normal} '
                                 '.outer .target {font-style: normal} '
                                 'div.outer .target {font-style: italic !important} '
                                 '.self .target {font-style: italic}</style>', navigation=False)
        for label, italic, bold in (('Nested italic', True, False), ('Ancestor is not self', True, True),
                                    ('Outside plain', False, False), ('Self only', False, False), ('Specific bold', True, True)):
            paragraph = next(p for p in root.findall('.//f:p', NS) if label in self.text(p))
            self.assertEqual(paragraph.find('.//f:emphasis', NS) is not None, italic, label)
            self.assertEqual(paragraph.find('.//f:strong', NS) is not None, bold, label)
        self.assertNotIn('Unsupported CSS font selector', self.diagnostics)

    def test_unsupported_css_at_rules_warn_without_swallowing_following_rules(self):
        root = self.convert('<p class="hidden-rule">Media text</p><p class="supported">Later rule</p>',
                            head='<style>@import "other.css"; @media print {'
                                 '.hidden-rule {font-style: italic} .supported {font-weight: bold}} '
                                 '.supported {font-style: italic}</style>', navigation=False)
        media = next(p for p in root.findall('.//f:p', NS) if 'Media text' in self.text(p))
        later = next(p for p in root.findall('.//f:p', NS) if 'Later rule' in self.text(p))
        self.assertIsNone(media.find('.//f:emphasis', NS))
        self.assertIsNone(later.find('.//f:strong', NS))
        self.assertIsNotNone(later.find('.//f:emphasis', NS))
        self.assertIn('Unsupported CSS at-rule', self.diagnostics)

    def test_css_media_attribute_is_not_applied_unconditionally(self):
        root = self.convert('<p class="print-only">Print rule</p><p class="ordinary">Normal rule</p>',
                            head='<style media="print">.print-only {font-weight: bold}</style>'
                                 '<style>.ordinary {font-style: italic}</style>', navigation=False)
        print_rule = next(p for p in root.findall('.//f:p', NS) if 'Print rule' in self.text(p))
        ordinary = next(p for p in root.findall('.//f:p', NS) if 'Normal rule' in self.text(p))
        self.assertIsNone(print_rule.find('.//f:strong', NS))
        self.assertIsNotNone(ordinary.find('.//f:emphasis', NS))
        self.assertIn('Unsupported CSS media', self.diagnostics)

    def test_unrelated_important_does_not_discard_supported_font_style(self):
        root = self.convert('<p class="styled">Keep italic</p>',
                            head='<style>.styled {color: red !important; font-style: italic}</style>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Keep italic' in self.text(p))
        self.assertIsNotNone(paragraph.find('.//f:emphasis', NS))

    def test_unsupported_font_declaration_does_not_discard_other_font_property(self):
        root = self.convert('<p class="styled">Keep bold</p>',
                            head='<style>.styled {font-style: oblique 20deg; font-weight: bold}</style>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Keep bold' in self.text(p))
        self.assertIsNotNone(paragraph.find('.//f:strong', NS))
        self.assertIn('Unsupported CSS font declaration', self.diagnostics)

    def test_css_important_cascade_and_inline_priority(self):
        root = self.convert('<p class="a b" style="font-style: normal; font-weight: normal">Sheet priority</p>'
                            '<p class="a b" style="font-style: normal !important; font-weight: normal !important">Inline priority</p>'
                            '<p class="a b" style="font-style: italic !important; font-style: normal">Declaration order</p>'
                            '<p>Plain next</p>',
                            head='<style>.a {font-style: italic !important; font-weight: bold !important} '
                                 'p.b {font-style: normal; font-weight: normal} '
                                 '.b {font-weight: normal !important} .a.b {font-weight: bold !important}</style>',
                            navigation=False)
        paragraphs = root.findall('.//f:p', NS)
        for label, italic, bold in (('Sheet priority', True, True), ('Inline priority', False, False),
                                    ('Declaration order', True, True), ('Plain next', False, False)):
            paragraph = next(p for p in paragraphs if label in self.text(p))
            self.assertEqual(paragraph.find('.//f:emphasis', NS) is not None, italic, label)
            self.assertEqual(paragraph.find('.//f:strong', NS) is not None, bold, label)
        self.assertNotIn('Unsupported CSS font declaration', self.diagnostics)

    def test_css_quoted_brace_does_not_end_rule_or_consume_next_selector(self):
        root = self.convert('<p class="one">First italic</p><p class="two">Second bold</p>',
                            head="<style>.one { content: '}'; font-style: italic } .two { font-weight: bold }</style>",
                            navigation=False)
        first = next(p for p in root.findall('.//f:p', NS) if 'First italic' in self.text(p))
        second = next(p for p in root.findall('.//f:p', NS) if 'Second bold' in self.text(p))
        self.assertIsNotNone(first.find('.//f:emphasis', NS))
        self.assertIsNotNone(second.find('.//f:strong', NS))

    def test_css_font_shorthand_with_spacing_warns(self):
        self.convert('<p class="styled">Content</p>',
                     head='<style>.styled { font : italic 12pt serif }</style>', navigation=False)
        self.assertIn('Unsupported CSS font declaration', self.diagnostics)

    def test_css_comment_delimiter_inside_string_is_not_a_comment(self):
        root = self.convert('<p class="styled">Italic text</p>',
                            head="<style>.styled {content: '/*'; /* real comment */ font-style: italic}</style>",
                            navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Italic text' in self.text(p))
        self.assertIsNotNone(paragraph.find('.//f:emphasis', NS))

    def test_unsupported_inline_font_value_warns_without_losing_supported_property(self):
        root = self.convert('<p><span style="font-style: italic !important; font-weight: 700">'
                            'Text retained</span></p>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Text retained' in self.text(p))
        self.assertIsNotNone(paragraph.find('.//f:strong', NS))
        self.assertIn('Unsupported CSS font declaration', self.diagnostics)

    def test_formatted_inline_link_image_and_reset_preserve_content(self):
        root = self.convert('<p><em>Before <a href="#target">Link <span style="font-style: normal">normal</span></a> '
                            '<img src="../images/pixel.png" alt="pixel" /> After</em></p>'
                            '<p id="target">Target</p>',
                            extra={'images/pixel.png': ('pixel', 'image/png', PNG)}, navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Before' in self.text(p))
        self.assertIsNotNone(paragraph.find('.//f:a', NS))
        self.assertIsNotNone(paragraph.find('.//f:image', NS))
        self.assertFalse(any('normal' in self.text(n) for n in paragraph.findall('.//f:emphasis', NS)))
        self.assertTrue(any('Link' in self.text(n) for n in paragraph.findall('.//f:emphasis', NS)))
        self.assert_once(root, 'Before', 'Link', 'normal', 'After', 'Target')

    def test_link_boundary_spaces_survive_with_and_without_formatting(self):
        root = self.convert('<p>one<a href="#target"> two </a>three</p>'
                            '<p>four<a href="#target"><em> five </em></a>six</p>'
                            '<p id="target">Target</p>', navigation=False)
        paragraphs = root.findall('.//f:p', NS)
        self.assertTrue(any(self.text(p) == 'one two three' for p in paragraphs),
                        [self.text(p) for p in paragraphs])
        self.assertTrue(any(self.text(p) == 'four five six' for p in paragraphs),
                        [self.text(p) for p in paragraphs])

    def test_formatting_does_not_leak_after_structural_blocks(self):
        root = self.convert('<h1><b>Bold heading</b></h1><p>After heading</p>'
                            '<blockquote><p><em>Italic quote</em></p></blockquote><p>After quote</p>'
                            '<aside epub:type="epigraph"><p><b>Bold epigraph</b></p></aside><p>After epigraph</p>'
                            '<figure><img src="../images/pixel.png" alt="pixel" />'
                            '<figcaption><em>Italic caption</em></figcaption></figure><p>After figure</p>',
                            extra={'images/pixel.png': ('pixel', 'image/png', PNG)}, navigation=False)
        for text in ('After heading', 'After quote', 'After epigraph', 'After figure'):
            paragraph = next(p for p in root.findall('.//f:p', NS) if text in self.text(p))
            self.assertIsNone(paragraph.find('.//f:strong', NS), text)
            self.assertIsNone(paragraph.find('.//f:emphasis', NS), text)
            self.assert_once(root, text)
        for text in ('Bold heading', 'Italic quote', 'Bold epigraph', 'Italic caption'):
            self.assert_once(root, text)

    def test_language_fallback_uses_substantial_body_and_preserves_metadata(self):
        russian = ('Это история о том как человек увидел свой дом и понял что он был здесь. ' * 16)
        english = ('This is the story of a person who was in the house and saw that it was their home. ' * 16)
        def lang(root): return root.find('.//f:title-info/f:lang', NS).text
        self.assertEqual(lang(self.convert('<p>' + russian + '</p>', language='', navigation=False)), 'ru')
        self.assertEqual(lang(self.convert('<p>' + english + '</p>', language='', navigation=False)), 'en')
        self.assertEqual(lang(self.convert('<p>' + english + '</p>', language='ru', navigation=False)), 'ru')
        catalog = json.loads((ROOT / 'localization/plugin-ui/catalog.json').read_text(encoding='utf-8-sig'))
        self.assertTrue(any(message in self.diagnostics for message in
                            catalog['strings']['import_epub.runtime.language_conflict']['translations'].values()),
                        self.diagnostics)
        self.assertEqual(lang(self.convert('<p>' + russian + english + '</p>', language='', navigation=False)), 'und')
        self.assertEqual(lang(self.convert('<h1>' + english + '</h1><p>Short.</p>', language='', navigation=False)), 'und')
        french = ('Le livre raconte une histoire dans laquelle la personne a vu sa maison et son jardin. ' * 16)
        self.assertEqual(lang(self.convert('<p>' + french + '</p>', language='', navigation=False)), 'und')

    def test_language_sample_excludes_retained_navigation_document(self):
        russian = 'Это история о том как человек увидел свой дом и понял что он был здесь. ' * 16
        english = 'This is the story of a person who was in the house and saw that it was their home. '
        root = self.convert('<p>' + russian + '</p>', language='',
                            navigation=[(english, 'Text/chapter.xhtml', []) for _ in range(40)],
                            spine=['fixture-nav', 'chapter'], options=['--keep-service-pages'])
        self.assertEqual(root.find('.//f:title-info/f:lang', NS).text, 'ru')
        self.assertIn('This is the story', self.text(root))
        self.assertIn('Это история', self.text(root))
        root = self.convert('<p>Short.</p>', language='',
                            navigation=[(english, 'Text/chapter.xhtml', []) for _ in range(40)],
                            spine=['fixture-nav', 'chapter'], options=['--keep-service-pages'])
        self.assertEqual(root.find('.//f:title-info/f:lang', NS).text, 'und')
        root = self.convert('<p>' + russian + '</p>', language='en',
                            navigation=[(english, 'Text/chapter.xhtml', []) for _ in range(40)],
                            spine=['fixture-nav', 'chapter'], options=['--keep-service-pages'])
        self.assertEqual(root.find('.//f:title-info/f:lang', NS).text, 'en')
        catalog = json.loads((ROOT / 'localization/plugin-ui/catalog.json').read_text(encoding='utf-8-sig'))
        self.assertTrue(any(message in self.diagnostics for message in
                            catalog['strings']['import_epub.runtime.language_conflict']['translations'].values()))

    def test_source_timestamp_and_uuid(self):
        uuid = 'urn:uuid:12345678-1234-4234-9234-123456789abc'
        stamp = '2020-01-02T23:45:00-05:00'
        root = self.convert('<p>Content</p>', identifier=uuid, date=stamp)
        date = root.find('.//f:title-info/f:date', NS)
        self.assertEqual(date.text, stamp)
        self.assertEqual(date.get('value'), '2020-01-02')
        self.assertEqual(root.find('.//f:document-info/f:id', NS).text, uuid)

    def test_invalid_dates_do_not_gain_normalized_value(self):
        for stamp in ('2021-02-29', '2020-01-02T25:00:00Z', '2020-01-02T12:00:00+14:30', '2020-01-02garbage'):
            with self.subTest(stamp=stamp):
                root = self.convert('<p>Content</p>', date=stamp)
                date = root.find('.//f:title-info/f:date', NS)
                self.assertEqual(date.text, stamp)
                self.assertIsNone(date.get('value'))

    def test_publication_year_requires_a_valid_source_date(self):
        for version in ('2.0', '3.0'):
            for stamp, expected in (('2020', '2020'), ('2020-02', '2020'),
                                    ('2020-02-29T23:59:59-05:00', '2020'),
                                    ('circa 1999', None), ('2020garbage', None),
                                    ('2020-13', None), ('2021-02-29', None), ('0000', None)):
                with self.subTest(version=version, stamp=stamp):
                    root = self.convert('<p>Content</p>', date=stamp, version=version)
                    self.assertEqual(root.find('.//f:title-info/f:date', NS).text, stamp)
                    year = root.find('.//f:publish-info/f:year', NS)
                    self.assertEqual(year.text if year is not None else None, expected)

    def test_relative_encoded_images(self):
        root = self.convert('<p>Image<img src="../Images/%D1%80%D0%B8%D1%81%20one.png"/></p>',
                            {'Images/рис one.png': ('pic', 'image/png', PNG)})
        self.assertEqual(base64.b64decode(root.find('f:binary', NS).text), PNG)

    def test_picture_srcset_svg_and_object(self):
        root = self.convert('<h1>Images</h1><picture><source srcset="../Images/missing.png 1x, ../Images/a.png 2x"/></picture>'
                            '<p><img src="missing.png" srcset="../Images/b.png 2x"/></p>'
                            '<svg xmlns="http://www.w3.org/2000/svg"><image xlink:href="../Images/a.png"/></svg>'
                            '<object type="image/png" data="../Images/b.png"/>',
                            {'Images/a.png': ('a', 'image/png', PNG), 'Images/b.png': ('b', 'image/png', PNG)})
        self.assertEqual(len(root.findall('.//f:image', NS)), 4)
        self.assertEqual(len(root.findall('f:binary', NS)), 2)

    def test_binary_id_collisions(self):
        root = self.convert('<p><img src="../a.png"/><img src="../b.png"/></p>',
                            {'a.png': ('а', 'image/png', PNG), 'b.png': ('б', 'image/png', PNG)})
        self.assertEqual(len(root.findall('f:binary', NS)), 2)
        self.assertEqual(len({n.get(HREF) for n in root.findall('.//f:image', NS)}), 2)

    def test_optional_image_names_and_cover_references(self):
        images = {'one/same.png': ('coveritem', 'image/png', PNG),
                  'два/same.png': ('other', 'image/png', PNG)}
        body = '<p><img src="../one/same.png"/><img src="../%D0%B4%D0%B2%D0%B0/same.png"/><img src="../one/same.png"/></p>'
        for options in ([], ['--unify-image-names'], ['--unify-image-names', '--separate-cover-name'], ['--separate-cover-name']):
            with self.subTest(options=options):
                root = self.convert(body, images, options=options, cover_id='coveritem')
                binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
                cover = root.find('.//f:coverpage/f:image', NS).get(HREF)[1:]
                references = [n.get(HREF)[1:] for n in root.findall('f:body//f:image', NS)]
                self.assertEqual(len(binaries), 2)
                self.assertEqual(references[0], references[2])
                self.assertEqual(cover, references[0])
                self.assertTrue(all(binaries[ref] == PNG for ref in references))
                if '--unify-image-names' in options:
                    self.assertTrue(all(name.endswith('.png') for name in binaries))
                    self.assertEqual(cover == 'cover.png', '--separate-cover-name' in options)
                    self.assertTrue(all(name.startswith('i_') or name == 'cover.png' for name in binaries))
                else:
                    self.assertEqual(set(binaries), {'coveritem', 'other'})

    def test_unified_image_names_avoid_anchor_collision_and_cover_all_sources(self):
        svg = '<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"><rect width="4" height="4"/></svg>'
        root = self.convert('<p id="i_001.png">Anchor<img src="../same.png"/><img src="../shape.svg"/>'
                            '<img src="../unlisted.png"/></p>',
                            {'same.png': ('same', 'image/png', PNG), 'shape.svg': ('shape', 'image/svg+xml', svg)},
                            raw={'OEBPS/unlisted.png': PNG}, options=['--unify-image-names'], svg='keep')
        binaries = {n.get('id'): n.get('content-type') for n in root.findall('f:binary', NS)}
        self.assertEqual(set(binaries), {'i_001_2.png', 'i_002.svg', 'i_003.png'})
        self.assertEqual(len(root.findall('f:body//f:image', NS)), 3)
        self.assertIn('image/svg+xml', binaries.values())

    def test_unified_svg_names_follow_converted_format(self):
        svg = '<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"><rect width="4" height="4"/></svg>'
        for mode, suffix in [('keep', '.svg'), ('png', '.png'), ('jpg', '.jpg')]:
            with self.subTest(mode=mode):
                root = self.convert('<p><img src="../shape.svg"/></p>',
                                    {'shape.svg': ('shape', 'image/svg+xml', svg)},
                                    options=['--unify-image-names'], svg=mode)
                binary, = root.findall('f:binary', NS)
                self.assertTrue(binary.get('id').endswith(suffix))

    def test_unified_jpeg_preserves_source_bytes_and_extension(self):
        for extension in ('jpeg', 'JPEG'):
            with self.subTest(extension=extension):
                path = 'photo.' + extension
                root = self.convert('<p><img src="../' + path + '"/></p>',
                                    {path: ('photo', 'image/jpeg', JPEG)}, options=['--unify-image-names'])
                binary, = root.findall('f:binary', NS)
                self.assertEqual(binary.get('id'), 'i_001.' + extension)
                self.assertEqual(binary.get('content-type'), 'image/jpeg')
                self.assertEqual(base64.b64decode(binary.text), JPEG)

    def test_image_in_note_and_unused_resource(self):
        root = self.convert('<p>Text<a epub:type="noteref" href="../notes.xhtml#n">1</a></p>',
                            {'notes.xhtml': ('notes', 'application/xhtml+xml', '<div id="n"><p>Illustrated</p><img src="Images/a.png"/></div>'),
                             'Images/a.png': ('a', 'image/png', PNG), 'Images/unused.png': ('unused', 'image/png', PNG)})
        self.assertEqual(len(root.findall('f:binary', NS)), 1)
        self.assertIsNotNone(self.notes(root)[0].find('.//f:image', NS))

    def test_missing_resources_are_visible_warnings(self):
        root = self.convert('<p>Text<a epub:type="noteref" href="../absent.xhtml#n">[9]</a><img src="../missing.png"/></p>')
        self.assertIn('[9]', self.text(root))
        self.assertIn('Unresolved note reference', self.diagnostics)
        self.assertIn('Unresolved image resource', self.diagnostics)
        self.assertIn('with warnings', self.diagnostics)
        self.assertIn('lost=1', self.diagnostics)

    def test_traversal_resource_forbidden(self):
        root = self.convert('<p>Safe<img src="../../../../outside.png"/></p>', outside={'outside.png': PNG})
        self.assertEqual(root.findall('f:binary', NS), [])
        self.assertIn('Unresolved image resource', self.diagnostics)
        self.assertEqual((self.last_folder / 'temp/outside.png').read_bytes(), PNG)

    def test_encoded_traversal_resource_forbidden(self):
        root = self.convert('<p>Safe<img src="%2e%2e%2f%2e%2e%2f%2e%2e%2f%2e%2e%2foutside.png"/></p>', outside={'outside.png': PNG})
        self.assertEqual(root.findall('f:binary', NS), [])
        self.assertIn('Unresolved image resource', self.diagnostics)

    def test_anchor_inside_paragraph_note(self):
        root = self.convert('<p>Text<a href="../links.xhtml#n">1</a></p>',
                            {'links.xhtml': ('links', 'application/xhtml+xml', '<p><a name="n">1.</a> <em>Entire note text</em></p>')})
        self.assertIn('Entire note text', self.text(self.notes(root)[0]))

    def test_plain_backlinked_note_in_spine_arbitrary_file(self):
        root = self.convert('<p>Text<a id="ref" href="other.xhtml#n">1</a></p>',
                            {'Text/other.xhtml': ('other', 'application/xhtml+xml', '<p id="n">Backlinked note<a href="chapter.xhtml#ref">back</a></p>')},
                            spine=['chapter', 'other'])
        self.assertEqual(len(self.notes(root)), 1)
        self.assertIn('Backlinked note', self.text(self.notes(root)[0]))

    def test_mixed_structural_sections_preserve_order(self):
        root = self.convert('<section id="parent"><h1>Parent</h1><p>Intro</p><section><h2>Child</h2><p>Child text</p></section><p>Tail</p></section>')
        text = self.text(root.find('f:body', NS))
        self.assertLess(text.index('Intro'), text.index('Child text'))
        self.assertLess(text.index('Child text'), text.index('Tail'))
        self.assertGreaterEqual(len(root.findall('.//f:section', NS)), 4)

    def test_figure_preserves_text_caption_and_picture(self):
        root = self.convert('<figure><p>Before picture</p><picture><source srcset="../a.png 2x"/></picture><figcaption>Caption</figcaption><p>After caption</p></figure>',
                            {'a.png': ('a', 'image/png', PNG)})
        text = self.text(root)
        self.assertLess(text.index('Before picture'), text.index('Caption'))
        self.assertLess(text.index('Caption'), text.index('After caption'))
        self.assertEqual(len(root.findall('.//f:image', NS)), 1)

    def test_referenced_non_manifest_image(self):
        root = self.convert('<p><img src="../Images/a.png"/></p>', raw={'OEBPS/Images/a.png': PNG})
        self.assertEqual(len(root.findall('f:binary', NS)), 1)

    def test_notes_option_keeps_inline_content(self):
        root = self.convert('<p>Text<a href="#n">1</a></p><aside id="n" epub:type="footnote"><p>Inline content</p></aside>', options=['--no-notes'])
        self.assertEqual(self.notes(root), [])
        self.assertIn('Inline content', self.text(root.find('f:body', NS)))

    def test_case_and_normalization_collisions_in_one_file(self):
        root = self.convert('<h1 id="N">Upper</h1><p><a href="#n">Lower link</a><a href="#a:b">Colon</a><a href="#a_b">Underscore</a></p><p id="n">Lower</p><p id="a:b">Colon target</p><p id="a_b">Underscore target</p>')
        links = root.findall('.//f:a', NS)
        self.assertEqual(len({a.get(HREF) for a in links}), 3)

    def test_unsafe_manifest_forbidden(self):
        self.convert('<p>Safe</p>', manifest_extra=['<item id="bad" href="../../outside.png" media-type="image/png"/>'], fail=True)
        self.assertIn('Unsafe EPUB manifest resource', self.diagnostics)

    def test_anchor_in_multiparagraph_list_note(self):
        root = self.convert('<p>Body<a href="../notes.xhtml#n">1</a></p>',
                            {'notes.xhtml': ('notes', 'application/xhtml+xml',
                             '<ol><li><p><a id="n"/>First paragraph</p><p><em>Second paragraph</em></p><p>Third paragraph</p></li></ol>')})
        note, = self.notes(root)
        self.assertEqual([self.text(p) for p in note.findall('f:p', NS)],
                         ['First paragraph', 'Second paragraph', 'Third paragraph'])
        self.assertIsNotNone(note.find('.//f:emphasis', NS))

    def test_neighboring_multiblock_notes_with_image(self):
        root = self.convert('<p>Body<a href="../notes.xhtml#n">1</a><a href="../notes.xhtml#m">2</a></p>',
                            {'notes.xhtml': ('notes', 'application/xhtml+xml',
                             '<ol><li><p><a id="n"/>First A</p><p>Second A</p><img src="a.png"/></li>'
                             '<li><p><a id="m"/>First B</p><p><b>Second B</b></p></li></ol>'),
                             'a.png': ('a', 'image/png', PNG)})
        a, b = self.notes(root)
        self.assertIn('Second A', self.text(a)); self.assertNotIn('First B', self.text(a))
        self.assertIn('Second B', self.text(b)); self.assertNotIn('First A', self.text(b))
        self.assertIsNotNone(a.find('.//f:image', NS))
        self.assertEqual(base64.b64decode(root.find('f:binary', NS).text), PNG)
        for phrase in ['First A', 'Second A', 'First B', 'Second B']:
            self.assertEqual(self.text(root).count(phrase), 1)

    def test_inline_multiblock_note_keeps_neighbors(self):
        root = self.convert('<h1>Chapter</h1><p>Before<a epub:type="noteref" href="#n">1</a></p>'
                            '<aside epub:type="footnote"><p><a id="n"/>Note first</p><p>Note last</p></aside><p>After</p>')
        main = self.text(root.find('f:body', NS))
        self.assertIn('Before', main); self.assertIn('After', main)
        self.assertNotIn('Note first', main); self.assertNotIn('Note last', main)
        self.assertIn('Note last', self.text(self.notes(root)[0]))

    def test_semantic_note_containers_and_backlink_option(self):
        for tag in ['div', 'section', 'aside']:
            for option, kept in [([], False), (['--keep-footnote-backlinks'], True)]:
                with self.subTest(tag=tag, option=option):
                    root = self.convert('<p>Text<a id="ref" epub:type="noteref" href="#n">1</a></p>'
                                        f'<{tag} epub:type="footnote"><p><a id="n"/>First</p><p>Last '
                                        f'<a epub:type="backlink" href="#ref">ReturnMarker</a></p></{tag}><p>Tail</p>', options=option)
                    note, = self.notes(root)
                    self.assertIn('Last', self.text(note))
                    self.assertEqual('ReturnMarker' in self.text(note), kept)

    def test_skipped_guide_toc_preserves_chapter_and_links(self):
        toc = '<h1>Contents</h1><p id="ch1">Entry</p>'
        chapter = '<h1><a href="part0002.html#ch1">Chapter <strong>One</strong></a></h1><p>Body <a href="next.xhtml#target">Forward</a></p>'
        extra = {'Text/part0002.html': ('service', 'application/xhtml+xml', toc),
                 'Text/next.xhtml': ('next', 'application/xhtml+xml', '<h1 id="target">Next</h1><p>Last</p>')}
        guide = '<guide><reference type="toc" title="Contents" href="Text/part0002.html"/></guide>'
        root = self.convert(chapter, extra, version='2.0', spine=['service', 'chapter', 'next'],
                            guide=guide, options=['--skip-service-pages', '--remove-service-sections'])
        self.assertIn('Chapter One', self.text(root))
        self.assertIn('Body', self.text(root))
        self.assertNotIn('Entry', self.text(root))
        links = [n.get(HREF) for n in root.findall('.//f:a', NS)]
        self.assertTrue(any(link and link.startswith('#') for link in links))

    def test_service_filter_option_matrix(self):
        toc = '<h1>Contents</h1><p id="ch1">Entry</p>'
        chapter = '<h1><a href="part0002.html#ch1">Chapter <strong>One</strong></a></h1><p>Text</p>'
        guide = '<guide><reference type="toc" href="Text/part0002.html"/></guide>'
        extra = {'Text/part0002.html': ('service', 'application/xhtml+xml', toc)}
        for options, skipped in [(['--skip-service-pages', '--remove-service-sections'], True),
                                 (['--skip-service-pages', '--keep-service-sections'], False),
                                 (['--keep-service-pages', '--remove-service-sections'], False),
                                 (['--keep-service-pages', '--keep-service-sections'], False),
                                 (['--skip-service-pages', '--remove-service-sections', '--no-validate'], True),
                                 (['--skip-service-pages', '--remove-service-sections', '--no-links'], True)]:
            with self.subTest(options=options):
                root = self.convert(chapter, extra, version='2.0', spine=['service', 'chapter'], guide=guide, options=options)
                self.assertEqual('Entry' not in self.text(root), skipped)
                self.assertIn('Chapter One', self.text(root))
                if not skipped:
                    self.assertTrue(any((a.get(HREF) or '').startswith('#') for a in root.findall('.//f:a', NS)))

    def test_missing_source_resource_remains_diagnosable(self):
        root = self.convert('<h1>Chapter</h1><p><a href="missing.xhtml#gone">Broken reference</a></p>')
        self.assertIn('Unresolved internal link', self.diagnostics)
        self.assertIn('Links to missing EPUB resources: 1', self.diagnostics)
        links = [a.get(HREF) for a in root.findall('.//f:a', NS)]
        self.assertIn('missing.xhtml#gone', links)

    def test_diagnostic_section_is_bounded_and_excludes_paths(self):
        links = ''.join(f'<a href="missing{i}.xhtml#target">Link{i}</a>' for i in range(20))
        root = self.convert('<h1>Chapter</h1><p>' + links + '</p>', options=['--diagnostic-section'])
        section = root.find('f:body/f:section', NS)
        text = self.text(section)
        self.assertIn('missing EPUB resources: 20', text)
        self.assertNotIn('missing0.xhtml', text)
        self.assertNotIn(str(self.last_folder), text)
        self.assertLess(len(text), 500)
        self.assertIn('Links to missing EPUB resources: 20', self.diagnostics)

    def test_validation_failure_summarizes_ui_and_keeps_log(self):
        links = ''.join(f'<a href="#missing{i}">Link{i}</a>' for i in range(20))
        self.convert('<h1>Chapter</h1><p>' + links + '</p>', fail=True, options=['--no-log'])
        self.assertIn('missing generated link targets: 20', self.public_diagnostics)
        self.assertLess(len(self.public_diagnostics), 1000)
        self.assertNotIn(str(self.last_folder), self.public_diagnostics)
        self.assertIn('missing19', self.diagnostics)
        self.assertTrue(list(self.last_folder.glob('*.log')))

    def test_unresolved_local_fragment_fails_even_without_xsd(self):
        for options in ([], ['--no-validate']):
            with self.subTest(options=options):
                self.convert('<h1>Chapter</h1><p><a href="#missing-anchor">Broken</a></p>',
                             fail=True, options=options)
                self.assertIn('missing-anchor', self.diagnostics)
                self.assertIn('Generated FB2 links with missing targets: 1', self.diagnostics)

    def test_section_with_only_epigraphs_is_valid_and_keeps_text(self):
        root = self.convert('<p class="epigraph">First quotation</p><p class="epigraph">Second quotation</p>')
        self.assertIn('First quotation', self.text(root))
        self.assertIn('Second quotation', self.text(root))

    def test_split_body_id_does_not_wrap_preceding_documents(self):
        root = self.convert('<h1>First chapter</h1><p>First text</p>',
                            {'Text/c1.xhtml': ('c1', 'application/xhtml+xml', '<h1>Second chapter</h1><p>Second text</p>')},
                            spine=['chapter', 'c1'], navigation=False, body_ids=True)
        sections = root.find('f:body', NS).findall('f:section', NS)
        self.assertEqual(len(sections), 2)
        self.assertIn('First text', self.text(sections[0]))
        self.assertNotIn('First text', self.text(sections[1]))
        self.assertIn('Second text', self.text(sections[1]))

    def test_ncx_hierarchy_across_split_continuation(self):
        extra = {'Text/part_split_000.xhtml': ('part', 'application/xhtml+xml', '<p>Part introduction</p>'),
                 'Text/part_split_001.xhtml': ('continued', 'application/xhtml+xml', '<h1>Part heading</h1><p>Part continuation</p>'),
                 'Text/c1.xhtml': ('c1', 'application/xhtml+xml', '<h1>Chapter</h1><p>Chapter text</p>')}
        root = self.convert('<p>Opening</p>', extra, version='2.0', body_ids=True,
                            spine=['chapter', 'part', 'continued', 'c1'],
                            navigation=[('Part', 'Text/part_split_000.xhtml#body-part', [('Chapter', 'Text/c1.xhtml#body-c1', [])])])
        body = root.find('f:body', NS)
        sections = body.findall('f:section', NS)
        self.assertEqual(len(sections), 2)
        part = sections[1]
        self.assertIn('Part introduction', self.text(part))
        self.assertIn('Part continuation', self.text(part))
        self.assertIn('Chapter text', self.text(part))
        self.assertLess(self.text(part).index('Part continuation'), self.text(part).index('Chapter text'))

    def test_split_continuation_with_independent_navigation_stays_flat(self):
        extra = {'Text/part_split_000.xhtml': ('part', 'application/xhtml+xml', '<p>Part introduction</p>'),
                 'Text/part_split_001.xhtml': ('continued', 'application/xhtml+xml', '<h1>Independent heading</h1><p>Independent text</p>'),
                 'Text/c1.xhtml': ('c1', 'application/xhtml+xml', '<h1>Chapter</h1><p>Chapter text</p>')}
        nav = [('Part', 'Text/part_split_000.xhtml#body-part', [('Chapter', 'Text/c1.xhtml#body-c1', [])]),
               ('Independent', 'Text/part_split_001.xhtml#body-continued', [])]
        root = self.convert('<p>Opening</p>', extra, version='2.0', body_ids=True,
                            spine=['chapter', 'part', 'continued', 'c1'], navigation=nav)
        sections = root.find('f:body', NS).findall('f:section', NS)
        self.assertEqual(len(sections), 4)
        self.assertIn('Navigation hierarchy not applied', self.diagnostics)
        text = self.text(root.find('f:body', NS))
        self.assertLess(text.index('Part introduction'), text.index('Independent text'))
        self.assertLess(text.index('Independent text'), text.index('Chapter text'))

    def hierarchy_fixture(self, version='3.0', intro=False, navigation=None, options=()):
        tree = [('Part one', 'Text/chapter.xhtml', [('Chapter one', 'Text/c1.xhtml', []), ('Chapter two', 'Text/c2.xhtml', [])]),
                ('Part two', 'Text/p2.xhtml', [('Chapter three', 'Text/c3.xhtml', [])])]
        return self.convert('<h1 id="part">Part one</h1>' + ('<p>Introduction</p>' if intro else ''),
                            {path: (ident, 'application/xhtml+xml', text) for path, ident, text in [
                                ('Text/c1.xhtml', 'c1', '<h1 id="h1">Chapter one</h1><p>Text one<a href="c3.xhtml#h3">Jump</a></p>'),
                                ('Text/c2.xhtml', 'c2', '<h1>Chapter two</h1><p>Text two</p>'),
                                ('Text/p2.xhtml', 'p2', '<h1>Part two</h1>'),
                                ('Text/c3.xhtml', 'c3', '<h1 id="h3">Chapter three</h1><p>Text three</p>')]},
                            spine=['chapter', 'c1', 'c2', 'p2', 'c3'], version=version,
                            navigation=tree if navigation is None else navigation, options=options)

    def assert_hierarchy(self, root, intro=False):
        body = root.find('f:body', NS)
        first, second = body.findall('f:section', NS)
        self.assertEqual(self.text(first.find('f:title', NS)), 'Part one')
        self.assertEqual(self.text(second.find('f:title', NS)), 'Part two')
        self.assertEqual(len(first.findall('f:section', NS)), 3 if intro else 2)
        self.assertEqual(len(second.findall('f:section', NS)), 1)
        text = self.text(body)
        phrases = ['Part one', *(['Introduction'] if intro else []), 'Chapter one', 'Text one', 'Chapter two', 'Text two', 'Part two', 'Chapter three', 'Text three']
        self.assertEqual([text.index(p) for p in phrases], sorted(text.index(p) for p in phrases))
        for phrase in phrases:
            self.assertEqual(text.count(phrase), 1)
        self.assertEqual(body.findall('.//f:empty-line', NS), [])

    def test_cross_document_ncx_hierarchy(self):
        self.assert_hierarchy(self.hierarchy_fixture(version='2.0'))

    def test_cross_document_nav_hierarchy(self):
        self.assert_hierarchy(self.hierarchy_fixture())

    def test_cross_document_parent_with_text(self):
        self.assert_hierarchy(self.hierarchy_fixture(intro=True), intro=True)

    def test_absent_incomplete_or_conflicting_navigation(self):
        for tree in [False, [], [('Only part', 'Text/chapter.xhtml', [])],
                     [('Part', 'Text/p2.xhtml', [('Earlier', 'Text/c1.xhtml', [])])]]:
            with self.subTest(tree=tree):
                root = self.hierarchy_fixture(navigation=tree)
                body = root.find('f:body', NS)
                self.assertEqual(len(body.findall('f:section', NS)), 5)
                text = self.text(body)
                for phrase in ['Part one', 'Text one', 'Text two', 'Part two', 'Text three']:
                    self.assertEqual(text.count(phrase), 1)
                self.assertLess(text.index('Text one'), text.index('Part two'))

    def test_navigation_fragments_no_duplication_and_page_list_ignored(self):
        tree = [('Part', 'Text/chapter.xhtml#part', [('One', 'Text/c1.xhtml#one', []), ('Two', 'Text/c1.xhtml#two', []), ('Repeated', 'Text/c1.xhtml#two', [])])]
        root = self.convert('<h1 id="part">Part</h1>',
                            {'Text/c1.xhtml': ('c1', 'application/xhtml+xml', '<h1 id="one">One</h1><p>Alpha</p><h1 id="two">Two</h1><p>Beta</p>')},
                            spine=['chapter', 'c1'], navigation=tree,
                            extra_nav='<nav epub:type="page-list"><ol><li><a href="Text/c1.xhtml#two">Page</a><ol><li><a href="Text/chapter.xhtml#part">Other</a></li></ol></li></ol></nav>')
        parent, = root.find('f:body', NS).findall('f:section', NS)
        self.assertEqual(len(parent.findall('f:section', NS)), 2)
        for phrase in ['Part', 'Alpha', 'Beta']:
            self.assertEqual(self.text(root.find('f:body', NS)).count(phrase), 1)
        self.assertNotIn('conflicts with spine', self.diagnostics)

    def test_symbol_and_letter_note_markers(self):
        for marker in ['*', '**', '***', 'a', 'Ж', '†', '‡']:
            with self.subTest(marker=marker):
                root = self.convert(f'<p>Text<a href="../notes.xhtml#n">{marker}</a></p>',
                                    {'notes.xhtml': ('notes', 'application/xhtml+xml', '<li><p><a id="n"/>First</p><p>Last</p></li>')})
                self.assertEqual(len(self.notes(root)), 1)
                self.assertIn('Last', self.text(self.notes(root)[0]))

    def test_ordinary_symbol_and_letter_links(self):
        for marker in ['*', 'a', 'Ж', '†', '‡']:
            with self.subTest(marker=marker):
                root = self.convert(f'<p><a href="#target">{marker}</a></p><p id="target">Ordinary text</p>')
                self.assertEqual(self.notes(root), [])
                self.assertEqual(root.findall('.//f:a[@type="note"]', NS), [])
                self.assertIn('Ordinary text', self.text(root.find('f:body', NS)))

    def test_ambiguous_note_boundary_warns_without_absorbing_neighbors(self):
        root = self.convert('<p>Main<a href="../links.xhtml#n">1</a></p>',
                            {'links.xhtml': ('notes', 'application/xhtml+xml',
                             '<p><a id="n"/>Certain note</p><p>Ambiguous continuation</p>')}, spine=['chapter', 'notes'])
        self.assertIn('Note boundary recognition limited', self.diagnostics)
        note, = self.notes(root)
        self.assertIn('Certain note', self.text(note)); self.assertNotIn('Ambiguous continuation', self.text(note))
        self.assertIn('Ambiguous continuation', self.text(root.find('f:body', NS)))

    def test_three_level_cross_document_navigation(self):
        root = self.convert('<h1>Part</h1>',
                            {'Text/c1.xhtml': ('c1', 'application/xhtml+xml', '<h1>Chapter</h1><p>Introduction</p>'),
                             'Text/c2.xhtml': ('c2', 'application/xhtml+xml', '<h1>Detail</h1><p>Detail text</p>')},
                            spine=['chapter', 'c1', 'c2'],
                            navigation=[('Part', 'Text/chapter.xhtml', [('Chapter', 'Text/c1.xhtml', [('Detail', 'Text/c2.xhtml', [])])])])
        parent, = root.find('f:body', NS).findall('f:section', NS)
        chapter, = parent.findall('f:section', NS)
        detail = chapter.findall('f:section', NS)[-1]
        self.assertIn('Detail text', self.text(detail))
        self.assertEqual(self.text(parent).count('Introduction'), 1)

    def test_multi_paragraph_duplicate_document_ids(self):
        root = self.convert('<p><a href="../notes.xhtml#n">1</a><a href="../links.xhtml#n">2</a></p>',
                            {path: (ident, 'application/xhtml+xml', f'<ol><li><p><a id="n"/>{word} first</p><p>{word} last</p></li></ol>')
                             for path, ident, word in [('notes.xhtml', 'notes', 'Alpha'), ('links.xhtml', 'links', 'Beta')]})
        self.assertEqual(len(self.notes(root)), 2)
        for word in ['Alpha', 'Beta']:
            self.assertEqual(sum(word + ' last' in self.text(n) for n in self.notes(root)), 1)

    def test_hierarchy_and_inline_notes_options_disabled(self):
        root = self.hierarchy_fixture(options=['--no-split-sections'])
        self.assertEqual(len(root.find('f:body', NS).findall('f:section', NS)), 5)
        root = self.convert('<h1>Title</h1><p>Body<a epub:type="noteref" href="#n">1</a></p>'
                            '<aside epub:type="footnote"><p><a id="n"/>First</p><p>Last</p></aside><p>Tail</p>',
                            options=['--no-notes', '--no-split-sections'])
        self.assertEqual(self.notes(root), [])
        for phrase in ['Body', 'First', 'Last', 'Tail']:
            self.assertEqual(self.text(root.find('f:body', NS)).count(phrase), 1)

    def test_svg_modes(self):
        svg = '<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"><rect width="4" height="4" fill="red"/></svg>'
        for mode, expected in [('keep', 'image/svg+xml'), ('png', 'image/png'), ('jpg', 'image/jpeg'), ('skip', None)]:
            with self.subTest(mode=mode):
                root = self.convert('<p>SVG<img src="../a.svg"/></p>', {'a.svg': ('a', 'image/svg+xml', svg)}, svg=mode)
                binary = root.find('f:binary', NS)
                self.assertEqual(None if binary is None else binary.get('content-type'), expected)

    def assert_once(self, root, *phrases):
        for phrase in phrases:
            self.assertEqual(self.text(root).count(phrase), 1, phrase)

    def test_non_spine_ambiguous_note_continuation_recovered(self):
        root = self.convert('<p>Main<a href="../links.xhtml#n">1</a></p>',
                            {'links.xhtml': ('notes', 'application/xhtml+xml',
                             '<p><a id="n"/>Certain note</p><p>Ambiguous continuation</p>')})
        note, = self.notes(root)
        self.assertIn('Certain note', self.text(note)); self.assertNotIn('Ambiguous continuation', self.text(note))
        main = root.find('f:body', NS)
        self.assertIn('Ambiguous continuation', self.text(main)); self.assertNotIn('Certain note', self.text(main))
        self.assertIn('links.xhtml', self.text(main))
        self.assertIn('Preserved ambiguous residual note content', self.diagnostics)
        self.assertIn('found=1 recovered=1 unaccounted=0', self.diagnostics)
        self.assert_once(root, 'Certain note', 'Ambiguous continuation')

    def test_non_spine_multiple_notes_and_residual_fragments(self):
        root = self.convert('<p>Main<a href="../links.xhtml#n">1</a><a href="../links.xhtml#m">2</a></p>',
                            {'links.xhtml': ('notes', 'application/xhtml+xml',
                             '<p id="n">Note alpha</p><p>Residual alpha</p><p id="m">Note beta</p><p>Residual beta</p>'),
                             'unused.xhtml': ('unused', 'application/xhtml+xml', '<p>Unrelated non-spine document</p>')})
        self.assertEqual(len(self.notes(root)), 2)
        self.assert_once(root, 'Note alpha', 'Residual alpha', 'Note beta', 'Residual beta')
        self.assertNotIn('Unrelated non-spine document', self.text(root))
        self.assertIn('Residual alpha', self.text(root.find('f:body', NS)))
        self.assertNotIn('Residual beta', self.text(self.notes(root)[0]))

    def test_residual_links_images_and_cross_references(self):
        root = self.convert('<h1 id="main">Main</h1><p>Body<a href="../links.xhtml#n">1</a>'
                            '<a href="../links.xhtml#tail">Residual link</a></p>',
                            {'links.xhtml': ('notes', 'application/xhtml+xml',
                             '<p><a id="n"/>Selected note <a href="#tail">Tail link</a></p>'
                             '<div id="tail"><p>Residual text <em>with emphasis</em><a href="Text/chapter.xhtml#main">Back to main</a></p>'
                             '<img src="a.png"/></div>'), 'a.png': ('a', 'image/png', PNG)})
        self.assert_once(root, 'Selected note', 'Residual text', 'with emphasis', 'Back to main')
        main = root.find('f:body', NS)
        self.assertIsNotNone(main.find('.//f:emphasis', NS))
        self.assertEqual(len(main.findall('.//f:image', NS)), 1)
        binary, = root.findall('f:binary', NS)
        self.assertEqual(base64.b64decode(binary.text), PNG)
        self.assertNotIn('outside imported content', self.diagnostics)

    def test_spine_remainder_not_duplicated(self):
        root = self.convert('<p>Main<a href="../links.xhtml#n">1</a></p>',
                            {'links.xhtml': ('notes', 'application/xhtml+xml',
                             '<p><a id="n"/>Selected note</p><p>Remaining text</p>')}, spine=['chapter', 'notes'])
        self.assert_once(root, 'Selected note', 'Remaining text')
        self.assertNotIn('Preserved ambiguous residual', self.diagnostics)
        self.assertIn('found=0 recovered=0 unaccounted=0', self.diagnostics)

    def test_unrecoverable_residual_image_is_an_error(self):
        self.convert('<p>Main<a href="../links.xhtml#n">1</a></p>',
                     {'links.xhtml': ('notes', 'application/xhtml+xml',
                      '<p id="n">Selected note</p><p>Residual text<img src="missing.png"/></p>')}, fail=True)
        self.assertIn('Cannot safely preserve unaccounted EPUB note content', self.diagnostics)
        self.assertIn('Residual note image integrity check failed', self.diagnostics)
        self.assertIn('found=2 recovered=0 unaccounted=2', self.diagnostics)

    def test_residual_image_filters_are_intentional(self):
        svg = '<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"><rect width="4" height="4"/></svg>'
        for options, mode in [(['--no-images'], 'keep'), ([], 'skip')]:
            with self.subTest(options=options, mode=mode):
                root = self.convert('<p>Main<a href="../links.xhtml#n">1</a></p>',
                                    {'links.xhtml': ('notes', 'application/xhtml+xml',
                                     '<p id="n">Selected note</p><p>Residual text<img src="a.svg"/></p>'),
                                     'a.svg': ('a', 'image/svg+xml', svg)}, options=options, svg=mode)
                self.assert_once(root, 'Selected note', 'Residual text')
                self.assertEqual(root.findall('.//f:image', NS), [])

    def test_residual_notes_disabled_and_backlinks(self):
        content = '<p id="n">Selected note<a epub:type="backlink" href="Text/chapter.xhtml#ref">ReturnMarker</a></p><p>Remaining text</p>'
        for options, spine, kept, has_recovery in [([], ['chapter'], False, True),
                                                  (['--keep-footnote-backlinks'], ['chapter'], True, True),
                                                  (['--no-notes'], ['chapter', 'notes'], False, False),
                                                  (['--no-notes'], ['chapter'], False, False)]:
            with self.subTest(options=options, spine=spine):
                root = self.convert('<p>Main<a id="ref" href="../links.xhtml#n">1</a></p>',
                                    {'links.xhtml': ('notes', 'application/xhtml+xml', content)}, options=options, spine=spine)
                self.assertEqual('ReturnMarker' in self.text(root), kept)
                self.assertEqual('Preserved ambiguous residual' in self.diagnostics, has_recovery)
                if '--no-notes' not in options or 'notes' in spine:
                    self.assert_once(root, 'Selected note', 'Remaining text')
                else:
                    self.assertNotIn('Remaining text', self.text(root))
                if '--no-notes' in options:
                    self.assertEqual(self.notes(root), [])
                    self.assertIn('Note structure index: elements=0 lookups=0', self.diagnostics)

    def test_residual_nested_containers_and_duplicate_ids(self):
        root = self.convert('<p>Main<a href="../links.xhtml#n">1</a><a href="../notes.xhtml#n">2</a>'
                            '<a href="../links.xhtml#tail">Alpha tail link</a><a href="../notes.xhtml#tail">Beta tail link</a></p>',
                            {path: (ident, 'application/xhtml+xml',
                             f'<div><ol><li><p><a id="n"/>{word} first</p><p>{word} last</p></li></ol>'
                             f'<section id="tail"><p>{word} residual</p></section></div>')
                             for path, ident, word in [('links.xhtml', 'links', 'Alpha'), ('notes.xhtml', 'notes', 'Beta')]})
        self.assertEqual(len(self.notes(root)), 2)
        self.assert_once(root, 'Alpha first', 'Alpha last', 'Beta first', 'Beta last', 'Alpha residual', 'Beta residual')
        self.assertNotIn('Alpha last', self.text(root.find('f:body', NS)))

    def test_residual_whitespace_stable_across_document_passes(self):
        for spacing in ['', '\n  ', '\n\t\n']:
            with self.subTest(spacing=repr(spacing)):
                content = spacing.join(['<div>', '<ol>', '<li>', '<p><a id="n"/>First stable</p>',
                                        '<p>Last stable</p>', '</li>', '</ol>', '<p id="tail">Residual stable</p>', '</div>'])
                root = self.convert('<p>Main<a href="../links.xhtml#n">1</a><a href="../links.xhtml#tail">Tail</a></p>',
                                    {'links.xhtml': ('notes', 'application/xhtml+xml', content)})
                self.assert_once(root, 'First stable', 'Last stable', 'Residual stable')
                self.assertNotIn('Last stable', self.text(root.find('f:body', NS)))

    def performance_samples(self, notes):
        metrics = []
        for count in [1000, 3000]:
            body = ''.join(f'<p>Paragraph {i} remains.</p>' for i in range(count))
            if notes:
                body += ('<p>References <a epub:type="noteref" href="#n1">1</a><a epub:type="noteref" href="#n2">2</a></p>'
                         '<ol><li id="n1"><p>First note</p></li><li id="n2"><p>Second note</p></li></ol>')
            start = time.perf_counter()
            root = self.convert(body)
            elapsed = round(time.perf_counter() - start, 3)
            self.assert_once(root, 'Paragraph 0 remains.', f'Paragraph {count-1} remains.')
            self.assertEqual(len(self.notes(root)), 2 if notes else 0)
            found = re.search(r'Note structure index: elements=(\d+) lookups=(\d+)', self.diagnostics)
            self.assertIsNotNone(found, self.diagnostics)
            indexed, lookups = map(int, found.groups())
            sample = dict(notes=notes, paragraphs=count, seconds=elapsed, indexed_elements=indexed, structural_lookups=lookups)
            self.performance.append(sample); metrics.append(sample)
            print('EPUB performance: ' + json.dumps(sample), flush=True)
            if not notes:
                self.assertEqual((indexed, lookups), (0, 0))
            else:
                self.assertLessEqual(indexed, 4 * (count + 30))
                self.assertLessEqual(lookups, 5 * (count + 30))
        if notes:
            for metric in ['indexed_elements', 'structural_lookups']:
                self.assertLessEqual(metrics[1][metric], metrics[0][metric] * 3.1)

    def test_standalone_attribution_preserves_text_and_valid_structure(self):
        root = self.convert('<h1>Chapter</h1><p>Quotation text.</p><p id="attribution" class="text-author"><em>Author Name</em></p><p>Following text. <a href="#attribution">Attribution link</a></p>')
        self.assert_once(root, 'Quotation text.', 'Author Name', 'Following text.')
        text = self.text(root)
        self.assertLess(text.index('Quotation text.'), text.index('Author Name'))
        self.assertLess(text.index('Author Name'), text.index('Following text.'))
        author = next(p for p in root.findall('.//f:p', NS) if 'Author Name' in self.text(p))
        self.assertIsNotNone(author.find('f:emphasis', NS))
        self.assertTrue(author.get('id'))
        self.assertTrue(any(a.get(HREF) == '#' + author.get('id') for a in root.findall('.//f:a', NS)))
        self.assertEqual(root.findall('.//f:section/f:text-author', NS), [])

    def test_epigraph_and_citation_attributions_survive(self):
        root = self.convert('<section><div class="epigraph"><p>Epigraph line</p><p class="text-author">Epigraph Author</p></div>'
                            '<blockquote><p>Quote line</p><p class="text-author">Quote Author</p></blockquote>'
                            '<section><p>Nested body</p></section></section>')
        self.assert_once(root, 'Epigraph line', 'Epigraph Author', 'Quote line', 'Quote Author', 'Nested body')
        text = self.text(root)
        self.assertLess(text.index('Epigraph line'), text.index('Epigraph Author'))
        self.assertLess(text.index('Quote line'), text.index('Quote Author'))
        self.assertEqual(root.findall('.//f:section/f:text-author', NS), [])

    def test_epigraph_attribution_structure_matrix(self):
        cases = (
            '<section><div class="epigraph"><p>First quotation</p><p class="text-author">First author</p></div></section>',
            '<section><div class="epigraph"><p>First quotation</p><p class="text-author">First author</p></div>'
            '<div class="epigraph"><p>Second quotation</p><p class="text-author">Second author</p></div></section>',
            '<section><section><p>First quotation</p><p class="text-author">First author</p></section>'
            '<section><blockquote><p>Second quotation</p><p class="text-author">Second author</p></blockquote></section></section>',
            '<section><div class="epigraph"><p>First quotation</p><p class="text-author">First author</p></div>'
            '<div class="epigraph"><p>Second quotation</p><p class="text-author">Second author</p></div>'
            '<section><p>Child body</p></section></section>',
        )
        for version in ('2.0', '3.0'):
            for case in cases:
                with self.subTest(version=version, case=case):
                    root = self.convert(case, version=version, navigation=False)
                    expected = ['First quotation', 'First author']
                    if 'Second quotation' in case:
                        expected += ['Second quotation', 'Second author']
                    self.assert_once(root, *expected)
                    text = self.text(root)
                    positions = [text.index(value) for value in expected]
                    self.assertEqual(positions, sorted(positions))
                    self.assertEqual(root.findall('.//f:section/f:text-author', NS), [])

    def test_linear_structure_index_without_notes(self):
        self.performance_samples(False)

    def test_linear_structure_index_with_notes(self):
        self.performance_samples(True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--batch', type=Path, default=ROOT / 'out/Release/ImportEPUBBatch.exe')
    args, rest = parser.parse_known_args()
    BATCH = args.batch.resolve()
    unittest.main(argv=[__file__, *rest])
