"""Short EPUB 2/3 content regressions through the production batch/DLL boundary."""
import argparse
import base64
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[2]
NS = {'f': 'http://www.gribuser.ru/xml/fictionbook/2.0'}
HREF = '{http://www.w3.org/1999/xlink}href'
PNG = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII=')


class Fidelity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folders, cls.outputs = [], []

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
        finally:
            for folder in cls.folders:
                folder.cleanup()

    def convert(self, body, extra=None, version='3.0', spine=None, fail=False, svg='keep', raw=None, options=(), manifest_extra=(), outside=None):
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
        if version.startswith('3'):
            files['nav.xhtml'] = ('fixture-nav', 'application/xhtml+xml', '<nav epub:type="toc"><ol><li><a href="Text/chapter.xhtml">Fixture</a></li></ol></nav>')
        else:
            files['toc.ncx'] = ('fixture-ncx', 'application/x-dtbncx+xml', '<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head><meta name="dtb:uid" content="fixture"/></head><docTitle><text>Fixture</text></docTitle><navMap><navPoint id="nav1" playOrder="1"><navLabel><text>Fixture</text></navLabel><content src="Text/chapter.xhtml"/></navPoint></navMap></ncx>')
        def xhtml(content):
            return ('<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml" '
                    'xmlns:epub="http://www.idpf.org/2007/ops" xmlns:xlink="http://www.w3.org/1999/xlink">'
                    '<head><title>Fixture</title></head><body>' + content + '</body></html>')
        with zipfile.ZipFile(epub, 'w') as z:
            z.writestr('mimetype', 'application/epub+zip')
            z.writestr('META-INF/container.xml', '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
            manifest = []
            for path, (ident, mime, content) in files.items():
                properties = ' properties="nav"' if ident == 'fixture-nav' else ''
                manifest.append(f'<item id="{ident}" href="{quote(path, safe="/")}" media-type="{mime}"{properties}/>')
                z.writestr('OEBPS/' + path, xhtml(content) if mime == 'application/xhtml+xml' else content)
            modified = '<meta property="dcterms:modified">2026-10-21T00:00:00Z</meta>' if version.startswith('3') else ''
            toc = '' if version.startswith('3') else ' toc="fixture-ncx"'
            z.writestr('OEBPS/book.opf', f'<package xmlns="http://www.idpf.org/2007/opf" version="{version}" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">fixture</dc:identifier><dc:title>Fixture</dc:title><dc:language>ru</dc:language><dc:creator>Tester</dc:creator>' + modified + '</metadata><manifest>' + ''.join(manifest) + ''.join(manifest_extra) + f'</manifest><spine{toc}>' + ''.join(f'<itemref idref="{ident}"/>' for ident in (spine or ['chapter'])) + '</spine></package>')
            for path, content in (raw or {}).items():
                z.writestr(path, content)
        output = folder / 'case.fb2'
        result = subprocess.run([str(BATCH), str(epub), str(output), '--log', '--stats', '--svg', svg, *options],
                                capture_output=True, encoding='utf-8', errors='replace', timeout=45,
                                env={**os.environ, 'TEMP': str(extraction_temp), 'TMP': str(extraction_temp)})
        logs = '\n'.join(p.read_text(encoding='utf-8-sig') for p in folder.glob('*.log'))
        self.diagnostics = result.stdout + result.stderr + logs
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

    def test_list_notes(self):
        root = self.convert('<p>Text<a href="../notes.xhtml#n">1</a></p>',
                            {'notes.xhtml': ('notes', 'application/xhtml+xml', '<ol><li id="n"><p>First</p><p><em>Second</em></p></li></ol>')})
        note = self.notes(root)[0]
        self.assertEqual(len(note.findall('f:p', NS)), 2)
        self.assertIsNotNone(note.find('.//f:emphasis', NS))

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

    def test_empty_leaf_reports_failure_without_filler(self):
        self.convert('<h1>Empty leaf</h1>', fail=True, options=['--save-on-warning'])
        self.assertIn('FB2 schema validation failed', self.diagnostics)
        recovery = self.last_folder / 'case.failed-import.fb2'
        self.assertTrue(recovery.is_file())
        self.assertNotIn('text was not extracted', recovery.read_text(encoding='utf-8-sig'))
        self.assertNotIn('<empty-line', recovery.read_text(encoding='utf-8-sig'))

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

    def test_svg_modes(self):
        svg = '<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"><rect width="4" height="4" fill="red"/></svg>'
        for mode, expected in [('keep', 'image/svg+xml'), ('png', 'image/png'), ('jpg', 'image/jpeg'), ('skip', None)]:
            with self.subTest(mode=mode):
                root = self.convert('<p>SVG<img src="../a.svg"/></p>', {'a.svg': ('a', 'image/svg+xml', svg)}, svg=mode)
                binary = root.find('f:binary', NS)
                self.assertEqual(None if binary is None else binary.get('content-type'), expected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--batch', type=Path, default=ROOT / 'out/Release/ImportEPUBBatch.exe')
    args, rest = parser.parse_known_args()
    BATCH = args.batch.resolve()
    unittest.main(argv=[__file__, *rest])
