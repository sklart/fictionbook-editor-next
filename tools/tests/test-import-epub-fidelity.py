"""Short EPUB 2/3 content regressions through the production batch/DLL boundary."""
import argparse
import base64
import json
import os
import re
import shutil
import struct
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
import xml.etree.ElementTree as ET
import zipfile
import zlib
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

    def convert(self, body, extra=None, version='3.0', spine=None, fail=False, svg='keep', raw=None, options=(), manifest_extra=(), outside=None, navigation=None, extra_nav='', guide='', body_ids=False, identifier='fixture', date='', head='', language='ru', chapter_title='Fixture', creators=('Tester',), cover_id='', metadata_extra='', temp_padding=0, test_env=None):
        extra = extra or {}
        temp = tempfile.TemporaryDirectory(prefix='epub-fidelity-')
        self.folders.append(temp)
        folder = Path(temp.name)
        self.last_folder = folder
        extraction_temp = folder / ('t' * temp_padding if temp_padding else 'temp')
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
            z.writestr('OEBPS/book.opf', f'<package xmlns="http://www.idpf.org/2007/opf" xmlns:opf="http://www.idpf.org/2007/opf" version="{version}" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">{identifier}</dc:identifier><dc:title>Fixture</dc:title>' + language_xml + creators_xml + date_xml + modified + metadata_extra + '</metadata><manifest>' + ''.join(manifest) + ''.join(manifest_extra) + f'</manifest><spine{toc}>' + ''.join(f'<itemref idref="{ident}"/>' for ident in (spine or ['chapter'])) + '</spine>' + guide + '</package>')
            for path, content in (raw or {}).items():
                z.writestr(path, content)
        output = folder / 'case.fb2'
        result = subprocess.run([str(BATCH), str(epub), str(output), '--log', '--stats', '--svg', svg, *options],
                                capture_output=True, encoding='utf-8', errors='replace', timeout=45,
                                env={**os.environ, 'TEMP': str(extraction_temp), 'TMP': str(extraction_temp), **(test_env or {})})
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

    def test_description_html_paragraphs_and_truncated_last_paragraph(self):
        root = self.convert('<p>Body</p>', metadata_extra=('<dc:description>'
            '&lt;p&gt;&lt;strong&gt;First&lt;/strong&gt; &amp;rsquo; text&lt;/p&gt;'
            '&lt;p&gt;Last paragraph...'
            '</dc:description>'))
        paragraphs = root.findall('f:description/f:title-info/f:annotation/f:p', NS)
        self.assertEqual([''.join(p.itertext()) for p in paragraphs], ['First ’ text', 'Last paragraph...'])
        self.assertEqual(paragraphs[0].find('f:strong', NS).text, 'First')

    def test_description_plain_text_angle_brackets_are_not_markup(self):
        root = self.convert('<p>Body</p>', metadata_extra='<dc:description>value &lt; 5 &amp; more</dc:description>')
        paragraph = root.find('f:description/f:title-info/f:annotation/f:p', NS)
        self.assertEqual(''.join(paragraph.itertext()), 'value &lt; 5 &amp; more'.replace('&lt;', '<').replace('&amp;', '&'))

    def test_description_html_line_breaks_keep_text_and_styles(self):
        markup = '<p>Alpha<br/>Beta<br/><br/>Gamma</p><p><strong>First<br/>Second</strong></p>'
        escaped = markup.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
        root = self.convert('<p>Body</p>', metadata_extra=f'<dc:description>{escaped}</dc:description>')
        annotation = root.find('f:description/f:title-info/f:annotation', NS)
        self.assertEqual([x.tag.rsplit('}', 1)[-1] for x in annotation], ['p', 'p', 'empty-line', 'p', 'p', 'p'])
        self.assertEqual([''.join(x.itertext()) for x in annotation], ['Alpha', 'Beta', '', 'Gamma', 'First', 'Second'])
        self.assertEqual(annotation[-2].find('f:strong', NS).text, 'First')
        self.assertEqual(annotation[-1].find('f:strong', NS).text, 'Second')

    def test_description_html_named_entities_are_decoded_without_losing_unknown(self):
        root = self.convert('<p>Body</p>', metadata_extra=(
            '<dc:description>&lt;p&gt;caf&amp;eacute;&lt;br/&gt;'
            '&amp;Omega; &amp;unknownentity;&lt;/p&gt;</dc:description>'))
        annotation = root.find('f:description/f:title-info/f:annotation', NS)
        self.assertEqual([''.join(x.itertext()) for x in annotation], ['café', 'Ω &unknownentity;'])

    def test_description_feff_is_kept_when_typography_cleanup_disabled(self):
        root = self.convert('<p>Body</p>', metadata_extra='<dc:description>&lt;p&gt;A&#65279;B&lt;/p&gt;</dc:description>',
                            options=('--no-clean-typography',))
        paragraph = root.find('f:description/f:title-info/f:annotation/f:p', NS)
        self.assertEqual(''.join(paragraph.itertext()), 'A\ufeffB')

    def test_referenced_non_spine_document_is_imported_once_and_linked(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                extra = {'Text/author.xhtml': ('author-page', 'application/xhtml+xml',
                         '<h1>About the author</h1><p>Author biography text</p>')}
                root = self.convert('<p>Chapter text</p><p><a href="author.xhtml">Author</a> '
                                    '<a href="author.xhtml">Biography</a></p>',
                                    extra, version=version, spine=['chapter'], navigation=False)
                self.assertEqual(self.text(root).count('Author biography text'), 1)
                links = root.findall('.//f:body/f:section//f:a', NS)
                self.assertEqual(len(links), 2)
                self.assertTrue(all((link.get(HREF) or '').startswith('#') for link in links))
                ids = {node.get('id') for node in root.iter() if node.get('id')}
                self.assertTrue(all(link.get(HREF)[1:] in ids for link in links))

    def test_hidden_or_disabled_links_do_not_import_non_spine_document(self):
        extra = {'Text/author.xhtml': ('author-page', 'application/xhtml+xml', '<p>Supplementary text</p>')}
        for body, options in (('<p hidden="hidden"><a href="author.xhtml">Author</a></p>', ()),
                              ('<p><a href="author.xhtml">Author</a></p>', ('--no-links',))):
            with self.subTest(body=body, options=options):
                root = self.convert(body + '<p>Chapter text</p>', extra, spine=['chapter'],
                                    navigation=False, options=options)
                self.assertNotIn('Supplementary text', self.text(root))

    def test_referenced_unmanifested_local_document_is_preserved(self):
        raw = {'OEBPS/Text/author.xhtml': '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Author</title></head>'
               '<body><p>Unlisted author biography</p></body></html>'}
        root = self.convert('<p><a href="author.xhtml">Author</a></p>', raw=raw, navigation=False)
        self.assertEqual(self.text(root).count('Unlisted author biography'), 1)
        link = root.find('.//f:body/f:section//f:a', NS)
        self.assertIsNotNone(link)
        self.assertTrue((link.get(HREF) or '').startswith('#'))
        self.assertTrue(any(node.get('id') == link.get(HREF)[1:] for node in root.iter()))

    def test_stylesheet_hidden_link_does_not_import_non_spine_document(self):
        extra = {'Text/author.xhtml': ('author-page', 'application/xhtml+xml', '<p>Hidden supplementary text</p>'),
                 'styles/book.css': ('sheet', 'text/css', '.hidden { display: none; }')}
        root = self.convert('<p class="hidden"><a href="author.xhtml">Author</a></p><p>Chapter text</p>',
                            extra, spine=['chapter'], navigation=False,
                            head='<link rel="stylesheet" href="../styles/book.css" />')
        self.assertNotIn('Hidden supplementary text', self.text(root))

    def test_misdeclared_image_is_preserved_by_signature(self):
        root = self.convert('<p>Before <img src="../images/unknown.bin"/> after</p>',
                            extra={'images/unknown.bin': ('photo', 'text/plain', JPEG)}, navigation=False)
        images = root.findall('.//f:body//f:image', NS)
        self.assertEqual(len(images), 1, self.diagnostics)
        target = images[0].get(HREF, '')[1:]
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertEqual(binaries.get(target), JPEG)

    def test_non_image_misdeclared_resource_is_not_embedded(self):
        root = self.convert('<p>Before <img src="../images/unknown.bin"/> after</p>',
                            extra={'images/unknown.bin': ('document', 'text/plain', b'not an image')}, navigation=False)
        self.assertEqual(root.findall('.//f:body//f:image', NS), [])
        self.assertIn('Unresolved image resource', self.diagnostics)

    def test_large_image_assembly_preserves_binary_and_body(self):
        data = JPEG + (b'\x00' * (3 * 1024 * 1024 + 4096))
        root = self.convert('<p>Before</p><p><img src="../images/large.jpg"/></p><p>After</p>',
                            extra={'images/large.jpg': ('large-image', 'image/jpeg', data)}, navigation=False)
        self.assertIn('Before', self.text(root))
        self.assertIn('After', self.text(root))
        images = root.findall('.//f:body//f:image', NS)
        self.assertEqual(len(images), 1, self.diagnostics)
        target = images[0].get(HREF, '')[1:]
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertEqual(binaries.get(target), data)

    def test_mathml_script_indices_keep_structure(self):
        math = ('<math xmlns="http://www.w3.org/1998/Math/MathML"><mrow>'
                '<msub><mi>x</mi><mrow><mi>w</mi><mo>−</mo><mn>1</mn></mrow></msub>'
                '<mo>+</mo><msup><mi>y</mi><mn>2</mn></msup><mo>+</mo>'
                '<msubsup><mi>z</mi><mn>0</mn><mn>3</mn></msubsup>'
                '</mrow></math>')
        root = self.convert('<p>Formula: ' + math + '.</p>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:body//f:p', NS) if 'Formula:' in self.text(p))
        self.assertEqual(self.text(paragraph), 'Formula: xw−1+y2+z03.')
        self.assertEqual([self.text(x) for x in paragraph.findall('f:sub', NS)], ['w−1', '0'])
        self.assertEqual([self.text(x) for x in paragraph.findall('f:sup', NS)], ['2', '3'])

        nested = ('<math xmlns="http://www.w3.org/1998/Math/MathML">'
                  '<msub><mi>x</mi><msup><mi>n</mi><mn>2</mn></msup></msub></math>')
        root = self.convert('<p>' + nested + '</p>', navigation=False)
        sub = root.find('.//f:body//f:sub', NS)
        self.assertIsNotNone(sub)
        self.assertEqual(self.text(sub), 'n2')
        self.assertEqual(self.text(sub.find('f:sup', NS)), '2')

        root = self.convert('<p><msub><span>A</span><span>B</span></msub></p>', navigation=False)
        self.assertEqual(root.findall('.//f:body//f:sub', NS), [])

    def test_mathml_fraction_and_square_root_keep_boundaries(self):
        math = ('<math xmlns="http://www.w3.org/1998/Math/MathML"><mrow>'
                '<mfrac><mrow><mi>a</mi><mo>+</mo><mi>b</mi></mrow>'
                '<mrow><mi>c</mi><mo>−</mo><mi>d</mi></mrow></mfrac>'
                '<mo>+</mo><msqrt><mrow><mi>x</mi><mo>−</mo><mn>1</mn></mrow></msqrt>'
                '</mrow></math>')
        root = self.convert('<p>Formula: ' + math + '.</p>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:body//f:p', NS) if 'Formula:' in self.text(p))
        self.assertEqual(self.text(paragraph), 'Formula: (a+b)/(c−d)+√(x−1).')

    def test_mathml_limits_accents_and_tables_keep_structure(self):
        math = ('<math xmlns="http://www.w3.org/1998/Math/MathML"><mrow>'
                '<munderover><mo>∑</mo><mrow><mi>i</mi><mo>=</mo><mn>0</mn></mrow>'
                '<mrow><mi>w</mi><mo>−</mo><mn>1</mn></mrow></munderover>'
                '<msub><mi>x</mi><mi>i</mi></msub><mo>+</mo>'
                '<mover accent="true"><mn>0100</mn><mo>¯</mo></mover>'
                '</mrow></math>')
        root = self.convert('<p>' + math + '</p>', navigation=False)
        p = next(p for p in root.findall('.//f:body//f:p', NS) if '∑' in self.text(p))
        self.assertEqual([self.text(e) for e in p.findall('f:sub', NS)], ['i=0', 'i'])
        self.assertEqual([self.text(e) for e in p.findall('f:sup', NS)], ['w−1'])
        self.assertIn('overline(0100)', self.text(p))

        math = ('<math xmlns="http://www.w3.org/1998/Math/MathML">'
                '<munder><mrow><mi>x</mi><mo>,</mo><mi>y</mi></mrow>'
                '<mtext>two values</mtext></munder></math>')
        root = self.convert('<p>' + math + '</p>', navigation=False)
        p = next(p for p in root.findall('.//f:body//f:p', NS) if 'underset(' in self.text(p))
        self.assertEqual(self.text(p), 'underset(two values, x,y)')
        self.assertIn('MathML', self.diagnostics)

        math = ('<math xmlns="http://www.w3.org/1998/Math/MathML"><mtable>'
                '<mtr><mtd><mi>x</mi></mtd><mtd><mo>=</mo></mtd><mtd><mn>1</mn></mtd></mtr>'
                '<mtr><mtd><mi>y</mi></mtd><mtd></mtd><mtd><mn>2</mn></mtd></mtr>'
                '</mtable></math>')
        root = self.convert('<p>' + math + '</p>', navigation=False)
        p = next(p for p in root.findall('.//f:body//f:p', NS) if self.text(p).startswith('x |'))
        self.assertEqual(self.text(p), 'x | = | 1; y |  | 2')
        self.assertIn('MathML', self.diagnostics)

    def test_mathml_root_and_semantic_annotation(self):
        math = ('<math xmlns="http://www.w3.org/1998/Math/MathML"><semantics><mroot>'
                '<mrow><mi>x</mi><mo>+</mo><mn>1</mn></mrow><mn>3</mn></mroot>'
                '<annotation encoding="application/x-tex">\\sqrt[3]{x+1}</annotation>'
                '</semantics></math>')
        root = self.convert('<p>' + math + '</p>', navigation=False)
        p = next(p for p in root.findall('.//f:body//f:p', NS) if 'root' in self.text(p))
        self.assertEqual(self.text(p), 'root(3, x+1)')
        self.assertNotIn('sqrt', self.text(p))
        self.assertIn('MathML', self.diagnostics)

        math = ('<math xmlns="http://www.w3.org/1998/Math/MathML">'
                '<mfrac linethickness="0"><mi>n</mi><mi>k</mi></mfrac></math>')
        root = self.convert('<p>' + math + '</p>', navigation=False)
        p = next(p for p in root.findall('.//f:body//f:p', NS) if 'stack(' in self.text(p))
        self.assertEqual(self.text(p), 'stack(n, k)')
        self.assertIn('MathML', self.diagnostics)

    def test_block_mathml_is_one_paragraph(self):
        root = self.convert('<p>Before.</p><div><math xmlns="http://www.w3.org/1998/Math/MathML" '
                            'display="block" id="eq"><mrow><mi>S</mi><mo>=</mo>'
                            '<mfrac><mn>1</mn><mrow><mn>1</mn><mo>−</mo><mi>α</mi>'
                            '</mrow></mfrac></mrow></math></div><p>After.</p>', navigation=False)
        paragraphs = root.findall('.//f:body//f:p', NS)
        equation = next(p for p in paragraphs if p.get('id') == 'eq')
        self.assertEqual(self.text(equation), 'S=(1)/(1−α)')
        text = [self.text(p) for p in paragraphs]
        self.assertLess(text.index('Before.'), text.index('S=(1)/(1−α)'))
        self.assertLess(text.index('S=(1)/(1−α)'), text.index('After.'))

    def test_note_text_div_keeps_inline_superscript(self):
        root = self.convert('<p><a epub:type="noteref" href="#n">1</a></p>'
                            '<aside epub:type="footnote" id="n"><div class="paragraph">'
                            'Area 93 m<sup>2</sup>. <em>Translator</em>. H<sub>2</sub>O.</div></aside>', navigation=False)
        note, = self.notes(root)
        paragraph = next(p for p in note.findall('f:p', NS) if 'Area 93' in ''.join(p.itertext()))
        self.assertEqual(''.join(paragraph.itertext()), 'Area 93 m2. Translator. H2O.')
        self.assertEqual(paragraph.find('f:sup', NS).text, '2')
        self.assertEqual(paragraph.find('f:emphasis', NS).text, 'Translator')
        self.assertEqual(paragraph.find('f:sub', NS).text, '2')

    def test_quote_direct_line_breaks_keep_lines_and_blank_line(self):
        root = self.convert('<blockquote><em>First line</em><br/><em>Second line</em>'
                            '<br/><br/><em>Last line</em><br/></blockquote>', navigation=False)
        cite = root.find('.//f:cite', NS)
        self.assertIsNotNone(cite)
        children = [(e.tag.rsplit('}', 1)[-1], ''.join(e.itertext())) for e in cite]
        self.assertEqual(children, [('p', 'First line'), ('p', 'Second line'),
                                    ('empty-line', ''), ('p', 'Last line')])
        self.assertEqual(len(cite.findall('.//f:emphasis', NS)), 3)

    def test_large_image_preserves_missing_link_text_and_warning(self):
        data = JPEG + (b'\x00' * (3 * 1024 * 1024 + 4096))
        root = self.convert('<p><a href="#missing-anchor">Broken</a></p><p><img src="../images/large.jpg"/></p>',
                     extra={'images/large.jpg': ('large-image', 'image/jpeg', data)},
                     navigation=False)
        self.assertIn('Broken', self.text(root))
        self.assertIn('Unresolved internal fragment', self.diagnostics)
        self.assertFalse(any(a.get(HREF) == '#missing-anchor' for a in root.findall('.//f:a', NS)))

    def test_large_utf8_output_preserves_supplementary_characters(self):
        # Three UTF-16 units per pair ensure that successive 16 Ki-unit
        # boundaries split different positions, including a surrogate pair.
        text = '\U0001f642x' * 12000
        root = self.convert('<p>' + text + '</p>', navigation=False)
        self.assertIn(text, self.text(root))

    def test_large_book_lossless_png_optimization_and_opt_out(self):
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
        pixels = (b'\x00' + b'\x80\x40\x20\xff' * 32) * 32
        png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 32, 32, 8, 6, 0, 0, 0)) +
               chunk(b'tEXt', b'Comment\x00preserve metadata') + chunk(b'IDAT', zlib.compress(pixels, 0)) + chunk(b'IEND', b''))
        extra = {'images/picture.png': ('picture', 'image/png', png)}
        body = '<p><img src="../images/picture.png"/></p>'
        normal = self.convert(body, extra=extra, navigation=False)
        normal_data = base64.b64decode(normal.find('f:binary', NS).text or '')
        self.assertEqual(normal_data, png)
        optimized = self.convert(body, extra=extra, navigation=False,
                                 test_env={'FBE_NEXT_TEST_MODE': '1', 'FBE_NEXT_TEST_EPUB_PNG_OPTIMIZE': '1'})
        output = base64.b64decode(optimized.find('f:binary', NS).text or '')
        self.assertLess(len(output), len(png))
        self.assertIn('Losslessly recompressed', self.diagnostics)
        def parts(data):
            pos, metadata, idats = 8, [], []
            while pos < len(data):
                size = int.from_bytes(data[pos:pos+4], 'big'); kind = data[pos+4:pos+8]
                payload = data[pos+8:pos+8+size]; end = pos+12+size
                self.assertEqual(int.from_bytes(data[pos+8+size:end], 'big'), zlib.crc32(kind+payload) & 0xffffffff)
                (idats if kind == b'IDAT' else metadata).append(payload if kind == b'IDAT' else data[pos:end])
                pos = end
            return metadata, zlib.decompress(b''.join(idats))
        self.assertEqual(parts(png), parts(output))

    def test_png_optimizer_preserves_animated_png_and_invalid_crc(self):
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
        ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', 32, 32, 8, 6, 0, 0, 0))
        pixels = (b'\x00' + b'\x11\x22\x33\xff' * 32) * 32
        image = chunk(b'IDAT', zlib.compress(pixels, 0))
        end = chunk(b'IEND', b'')
        cases = (b'\x89PNG\r\n\x1a\n' + ihdr + chunk(b'acTL', struct.pack('>II', 1, 0)) + image + end,
                 b'\x89PNG\r\n\x1a\n' + ihdr + image[:-1] + bytes([image[-1] ^ 1]) + end,
                 b'\x89PNG\r\n\x1a\n' + ihdr + chunk(b'vpAG', b'unknown unsafe metadata') + image + end)
        for png in cases:
            with self.subTest(sha=__import__('hashlib').sha256(png).hexdigest()):
                root = self.convert('<p><img src="../images/picture.png"/></p>',
                                    extra={'images/picture.png': ('picture', 'image/png', png)}, navigation=False,
                                    test_env={'FBE_NEXT_TEST_MODE': '1', 'FBE_NEXT_TEST_EPUB_PNG_OPTIMIZE': '1'})
                self.assertEqual(base64.b64decode(root.find('f:binary', NS).text or ''), png)

    def test_subscript_with_relative_css_offset_is_preserved(self):
        css = '.offset {vertical-align:baseline; position:relative; top:0.2em}'
        root = self.convert('<p>T<sub class="offset">old</sub></p>',
                            extra={'styles/book.css': ('sheet', 'text/css', css)},
                            head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        self.assertTrue(any('old' in ''.join(n.itertext()) for n in root.findall('.//f:body//f:sub', NS)))

    def test_subscript_relative_offset_respects_cascade_and_baseline(self):
        css = ('.offset {vertical-align:baseline; position:relative; top:0.2em} '
               '.offset.cancel {top:0 !important} .offset.up {top:-0.2em}')
        body = ('<p><sub class="offset">down</sub> <sub class="offset cancel">flat</sub> '
                '<sup class="offset up">up</sup> <span class="offset">ordinary</span></p>')
        root = self.convert(body, extra={'styles/book.css': ('sheet', 'text/css', css)},
                            head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        self.assertEqual([''.join(n.itertext()) for n in root.findall('.//f:body//f:sub', NS)], ['down'])
        self.assertEqual([''.join(n.itertext()) for n in root.findall('.//f:body//f:sup', NS)], ['up'])

    def test_xhtml_named_entities_preserve_accented_text(self):
        root = self.convert('<p>Fran&ccedil;ois &eacute;t&eacute; &acirc;ge &amp; text</p>', navigation=False)
        self.assertIn('François été âge & text', self.text(root))
        self.assertNotIn('&ccedil;', self.text(root))

    def test_xhtml_named_entities_do_not_expand_inside_cdata(self):
        root = self.convert('<p><![CDATA[&ccedil;]]> &eacute;</p>', navigation=False)
        self.assertIn('&ccedil; é', self.text(root))

    def test_xhtml_entities_preserve_literal_escaping_and_unknown_names(self):
        root = self.convert('<p>&amp;ccedil; &unknownEntity; &ccedil;</p>', navigation=False)
        self.assertIn('&ccedil; &unknownEntity; ç', self.text(root))

    def test_xhtml_named_entities_in_links_and_comments(self):
        root = self.convert('<p><!-- &ccedil; -->Fran&ccedil;ois <a href="https://example.invalid/?a=1&amp;b=2">&eacute;t&eacute;</a></p>', navigation=False)
        self.assertIn('François été', self.text(root))
        links = root.findall('.//f:body//f:a', NS)
        self.assertEqual(len(links), 1)
        self.assertEqual(links[0].get(HREF), 'https://example.invalid/?a=1&b=2')

    def test_typography_cleanup_preserves_significant_hyphens_and_dashes(self):
        text = 'hyphen‐word nonbreaking‑word figure‒dash horizontal―bar em—dash'
        for options in ((), ('--no-clean-typography',)):
            with self.subTest(options=options):
                root = self.convert('<p>' + text + '</p>', navigation=False, options=options)
                self.assertIn(text, self.text(root))

    def test_css_list_image_markers_preserve_image_and_cascade(self):
        css = ('.marked {list-style-image: url("../images/marker.png")} '
               '.disabled {list-style-image: none !important}')
        body = ('<ul class="marked"><li>First item</li><li>Second item</li></ul>'
                '<ul class="marked disabled"><li>Fallback item</li></ul>')
        root = self.convert(body, extra={'styles/book.css': ('sheet', 'text/css', css),
                                       'images/marker.png': ('marker', 'image/png', PNG)},
                            head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        images = root.findall('.//f:body//f:image', NS)
        self.assertEqual(len(images), 2, self.diagnostics)
        self.assertEqual(images[0].get(HREF), images[1].get(HREF))
        ident = images[0].get(HREF)[1:]
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertEqual(binaries.get(ident), PNG)
        self.assertIn('• Fallback item', self.text(root))

    def test_css_list_shorthand_cascade_controls_image(self):
        css = ('.marked {list-style-image:url("../images/first.png")} '
               '.reset {list-style:disc} .replace {list-style:square url("../images/second.jpg") inside} '
               '.important {list-style-image:url("../images/first.png") !important; list-style:disc}')
        body = ('<ul class="marked reset"><li>Reset</li></ul>'
                '<ul class="marked replace"><li>Replace</li></ul>'
                '<ul class="marked important"><li>Important</li></ul>')
        extra = {'styles/book.css': ('sheet', 'text/css', css),
                 'images/first.png': ('first', 'image/png', PNG), 'images/second.jpg': ('second', 'image/jpeg', JPEG)}
        root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        images = root.findall('.//f:body//f:image', NS)
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertEqual([binaries.get(n.get(HREF)[1:]) for n in images], [JPEG, PNG])
        self.assertIn('• Reset', self.text(root))

    def test_external_css_list_image_falls_back_with_warning(self):
        css = '.marked {list-style-image: url("https://example.invalid/marker.png")}'
        root = self.convert('<ul class="marked"><li>Item remains</li></ul>',
                            extra={'styles/book.css': ('sheet', 'text/css', css)},
                            head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        self.assertIn('• Item remains', self.text(root))
        self.assertEqual(root.findall('.//f:body//f:image', NS), [])
        self.assertIn('External CSS list marker image was not imported', self.diagnostics)

    def test_css_list_image_inheritance_and_item_override(self):
        css = ('.container {list-style-image:url("../images/first.png")} '
               'li.other {list-style-image:url("../images/second.jpg")}')
        body = ('<div class="container"><ul><li>Inherited</li>'
                '<li style="list-style-image:none">Text fallback</li>'
                '<li class="other">Overridden</li></ul></div>')
        extra = {'styles/book.css': ('sheet', 'text/css', css),
                 'images/first.png': ('first', 'image/png', PNG), 'images/second.jpg': ('second', 'image/jpeg', JPEG)}
        for options in ((), ('--no-images',)):
            with self.subTest(options=options):
                root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../styles/book.css" />',
                                    navigation=False, options=options)
                images = root.findall('.//f:body//f:image', NS)
                self.assertEqual(len(images), 0 if options else 2, self.diagnostics)
                self.assertIn('• Text fallback', self.text(root))
                if images:
                    binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
                    self.assertEqual([binaries.get(n.get(HREF)[1:]) for n in images], [PNG, JPEG])
                else:
                    self.assertIn('• Inherited', self.text(root))
                    self.assertIn('• Overridden', self.text(root))

    def test_css_list_image_inside_nested_list_is_preserved(self):
        css = '.inner {list-style-image:url("../images/marker.png")}'
        body = ('<ul><li>Outer<ul class="inner"><li>Nested one</li><li>Nested two</li></ul>'
                'After nested</li></ul>')
        root = self.convert(body, extra={'styles/book.css': ('sheet', 'text/css', css),
                                       'images/marker.png': ('marker', 'image/png', PNG)},
                            head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        images = root.findall('.//f:body//f:image', NS)
        self.assertEqual(len(images), 2, self.diagnostics)
        text = self.text(root)
        self.assertLess(text.index('Outer'), text.index('Nested one'))
        self.assertLess(text.index('Nested one'), text.index('Nested two'))
        self.assertLess(text.index('Nested two'), text.index('After nested'))

    def test_css_list_images_inside_sidebar_remain_in_order(self):
        css = '.marked {list-style-image:url("../images/marker.png")}'
        body = ('<aside><p>Sidebar start</p><section><ul class="marked" id="list">'
                '<li id="one">First item</li><li id="two">Second item</li>'
                '</ul></section><p>Sidebar end</p></aside>'
                '<p><a href="#one">Go to first</a></p>')
        extra = {'styles/book.css': ('sheet', 'text/css', css), 'images/marker.png': ('marker', 'image/png', PNG)}
        root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        self.assertEqual(len(root.findall('.//f:body//f:image', NS)), 2, self.diagnostics)
        text = self.text(root)
        for first, second in zip(('Sidebar start', 'First item', 'Second item'), ('First item', 'Second item', 'Sidebar end')):
            self.assertLess(text.index(first), text.index(second))
        self.assertTrue({'list', 'one', 'two'}.issubset({n.get('id') for n in root.iter()}))
        self.assertTrue(any(n.get(HREF) == '#one' for n in root.iter()))

    def test_sidebar_list_keeps_multiple_paragraphs_without_css_image(self):
        body = ('<aside><p>Start</p><ul><li><p>First paragraph.</p>'
                '<p>Second paragraph.</p></li></ul><p>End</p></aside>')
        root = self.convert(body, navigation=False)
        paragraphs = [''.join(p.itertext()).strip() for p in root.findall('.//f:cite/f:p', NS)]
        self.assertIn('First paragraph.', paragraphs)
        self.assertIn('Second paragraph.', paragraphs)
        self.assertLess(paragraphs.index('First paragraph.'), paragraphs.index('Second paragraph.'))

    def test_complex_sidebar_list_preserves_image_marker_and_paragraphs(self):
        css = '.marked {list-style-image:url("../images/marker.png")}'
        body = ('<aside><ul class="marked"><li id="first"><p>First paragraph.</p>'
                '<p>Second paragraph.</p></li><li id="second"><p>Third paragraph.</p></li>'
                '</ul></aside><p><a href="#second">Go</a></p>')
        extra = {'styles/book.css': ('sheet', 'text/css', css), 'images/marker.png': ('marker', 'image/png', PNG)}
        root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        paragraphs = [''.join(p.itertext()).strip() for p in root.findall('.//f:cite/f:p', NS)]
        self.assertIn('First paragraph.', paragraphs)
        self.assertIn('Second paragraph.', paragraphs)
        self.assertIn('Third paragraph.', paragraphs)
        self.assertLess(paragraphs.index('First paragraph.'), paragraphs.index('Second paragraph.'))
        self.assertLess(paragraphs.index('Second paragraph.'), paragraphs.index('Third paragraph.'))
        images = root.findall('.//f:cite/f:p/f:image', NS)
        self.assertEqual(len(images), 2, self.diagnostics)
        self.assertTrue({'first', 'second'}.issubset({n.get('id') for n in root.iter()}))
        self.assertTrue(any(n.get(HREF) == '#second' for n in root.iter()))

    def test_complex_sidebar_nested_list_keeps_marker_order(self):
        css = ('.outer {list-style-image:url("../images/first.png")} '
               '.inner {list-style-image:url("../images/second.jpg")}')
        body = ('<aside><ul class="outer"><li> Before <ul class="inner">'
                '<li>Nested</li></ul><p>After</p></li></ul></aside>')
        extra = {'styles/book.css': ('sheet', 'text/css', css),
                 'images/first.png': ('first', 'image/png', PNG), 'images/second.jpg': ('second', 'image/jpeg', JPEG)}
        root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        images = root.findall('.//f:cite//f:image', NS)
        self.assertEqual([binaries.get(n.get(HREF)[1:]) for n in images], [PNG, JPEG])
        text = self.text(root)
        self.assertLess(text.index('Before'), text.index('Nested'))
        self.assertLess(text.index('Nested'), text.index('After'))

    def test_sidebar_list_with_image_only_item_preserves_marker(self):
        css = '.marked {list-style-image:url("../images/first.png")}'
        body = '<aside><ul class="marked"><li><div><img src="../images/second.jpg" alt="" /></div></li></ul></aside>'
        extra = {'styles/book.css': ('sheet', 'text/css', css),
                 'images/first.png': ('first', 'image/png', PNG), 'images/second.jpg': ('second', 'image/jpeg', JPEG)}
        root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        images = root.findall('.//f:cite//f:image', NS)
        self.assertEqual([binaries.get(n.get(HREF)[1:]) for n in images], [PNG, JPEG])

    def test_complex_sidebar_item_marker_starts_new_paragraph(self):
        css = '.marked {list-style-image:url("../images/marker.png")}'
        body = ('<aside>Earlier text<ul class="marked"><li>Item text'
                '<pre><code>code block</code></pre></li></ul></aside>')
        extra = {'styles/book.css': ('sheet', 'text/css', css), 'images/marker.png': ('marker', 'image/png', PNG)}
        root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        paragraphs = root.findall('.//f:cite/f:p', NS)
        marked = [p for p in paragraphs if p.find('f:image', NS) is not None]
        self.assertEqual(len(marked), 1, self.diagnostics)
        self.assertNotIn('Earlier text', ''.join(marked[0].itertext()))
        self.assertIn('Item text', ''.join(marked[0].itertext()))
        self.assertIn('code block', self.text(root))

    def test_complex_sidebar_item_preserves_inline_code_and_emphasis(self):
        css = '.marked {list-style-image:url("../images/marker.png")}'
        body = ('<aside><ul class="marked"><li><code>variable</code> is '
                '<em>important</em>.<pre><code>code block</code></pre></li></ul></aside>')
        extra = {'styles/book.css': ('sheet', 'text/css', css), 'images/marker.png': ('marker', 'image/png', PNG)}
        root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        self.assertTrue(any('variable' in ''.join(n.itertext()) for n in root.findall('.//f:cite/f:p/f:code', NS)))
        self.assertTrue(any('important' in ''.join(n.itertext()) for n in root.findall('.//f:cite/f:p/f:emphasis', NS)))
        self.assertIn('code block', self.text(root))

    def test_hidden_interactive_term_definition_is_preserved(self):
        body = ('<p>Start <span class="keytermentry"><span class="keyterm">Term</span>'
                '<span class="keytermdef"> Interactive definition.</span></span> end.</p>'
                '<p class="ordinary-hidden">Unrelated hidden content.</p>')
        css = '.keytermentry .keytermdef {display:none} .ordinary-hidden {display:none}'
        root = self.convert(body, extra={'styles/book.css': ('sheet', 'text/css', css)},
                            head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        self.assertIn('Interactive definition.', self.text(root))
        self.assertIn('Term (Interactive definition.) end.', self.text(root))
        self.assertNotIn('Unrelated hidden content.', self.text(root))
        self.assertIn('Interactive term definitions were retained inline', self.diagnostics)

    def test_hidden_definition_class_alone_is_not_interactive(self):
        body = '<p><span class="keytermdef">Ordinary hidden text.</span></p>'
        root = self.convert(body, extra={'styles/book.css': ('sheet', 'text/css', '.keytermdef {display:none}')},
                            head='<link rel="stylesheet" href="../styles/book.css" />', navigation=False)
        self.assertNotIn('Ordinary hidden text.', self.text(root))

    def test_html_hidden_attribute_still_hides_interactive_definition(self):
        body = ('<p><span class="keytermentry"><span class="keyterm">Term</span>'
                '<span class="keytermdef" hidden="hidden">Explicitly hidden definition.</span></span></p>')
        root = self.convert(body, navigation=False)
        self.assertNotIn('Explicitly hidden definition.', self.text(root))

    def test_description_unsupported_html_is_preserved(self):
        for markup in ('<p><a href="https://example.org">Link</a></p>',
                       '<p><strong title="detail">Text</strong></p>', '<p>First<p>Second</p></p>'):
            with self.subTest(markup=markup):
                escaped = markup.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                root = self.convert('<p>Body</p>', metadata_extra=f'<dc:description>{escaped}</dc:description>')
                paragraph = root.find('f:description/f:title-info/f:annotation/f:p', NS)
                self.assertEqual(''.join(paragraph.itertext()), markup)

    def test_seven_zip_mislabeled_as_epub_fails_promptly(self):
        archive = base64.b64decode('N3q8ryccAARJ3cU/CwAAAAAAAACiAAAAAAAAANRFoZgBAAZmaXh0dXJlAAEEBgABCQsABwsBAAEhIQEADAcACAoB7kDlBQAABQEZDAAAAAAAAAAAAAAAABFdAGIAdQBpAGwAZAAvAGUAcAB1AGIALQBmAGkAZABlAGwAaQB0AHkALQAyADAAMgA2ADEAMQAxADEALwBmAGkAeAB0AHUAcgBlAC0ANwB6AC8AeAAuAHQAeAB0AAAAFAoBAOJlS9n1cd0BFQYBACAAAAAAAA==')
        with tempfile.TemporaryDirectory(prefix='not-epub-') as folder:
            source, output = Path(folder) / 'input.epub', Path(folder) / 'output.fb2'
            source.write_bytes(archive)
            try:
                result = subprocess.run([str(BATCH), str(source), str(output)], capture_output=True,
                                        timeout=10, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            except subprocess.TimeoutExpired:
                self.fail('7z file disguised as EPUB hung instead of returning an error')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'7z', result.stdout + result.stderr)
            self.assertFalse(output.exists())

    def test_long_temporary_path_with_long_archive_entry(self):
        long_name = 'Text/' + ('chapter_' * 12) + '.xhtml'
        root = self.convert('<p>Control chapter.</p>',
                            extra={long_name: ('long', 'application/xhtml+xml', '<p>Long path text.</p>')},
                            spine=['long'], navigation=False, temp_padding=45)
        self.assertIn('Long path text.', ''.join(root.itertext()))

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

    def test_docutils_smartquotes_and_table_footnotes(self):
        source = (
            '<div class="document text-transform-smartquotes"><div class="section">'
            '<h2>Chapter <a id="ref2" class="footnote-reference" href="#n2"><sup>2</sup></a></h2>'
            '<p>Text <a class="footnote-reference" href="#n1"><sup>1</sup></a>.</p>'
            '<div class="footnotes"><table class="footnote" id="n1"><tr><td><p>First note '
            '<a id="nested-ref" class="footnote-reference smaller" href="#n2"><sup>2</sup></a></p></td></tr></table>'
            '<table class="footnote" id="n2"><tr><td><p><a class="fn-backref" href="#nested-ref">Return</a>Second note</p></td></tr></table></div>'
            '</div></div>')
        root = self.convert(source)
        main = root.find('f:body', NS)
        self.assertIsNone(main.find('.//f:cite', NS))
        self.assertIn('Chapter', self.text(main))
        self.assertNotIn('First note', self.text(main))
        notes = self.notes(root)
        self.assertEqual(len(notes), 2)
        self.assertEqual(sum(self.text(n).count('First note') for n in notes), 1)
        self.assertNotIn('Return', self.text(root))
        links = root.findall('.//f:a[@type="note"]', NS)
        self.assertEqual(len(links), 3)
        targets = {n.get('id') for n in notes}
        self.assertTrue(all(a.get(HREF, '')[1:] in targets for a in links))
        retained = self.convert(source, options=['--keep-footnote-backlinks'])
        back, = [a for a in retained.findall('.//f:a', NS) if self.text(a) == 'Return']
        self.assertTrue(any('#' + e.get('id', '') == back.get(HREF) for e in retained.iter()))
        inline = self.convert(source, options=['--no-notes'])
        self.assertEqual(self.notes(inline), [])
        self.assertEqual(self.text(inline).count('First note'), 1)
        self.assertEqual(inline.findall('.//f:a[@type="note"]', NS), [])
        explicit = self.convert('<div class="text-transform-smartquotes quote"><p>Actual quotation</p></div>')
        self.assertIn('Actual quotation', self.text(explicit.find('.//f:cite', NS)))

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

    def test_html_examples_are_not_fb2_attributes(self):
        example = '&lt;span id="example"&gt;demo&lt;/span&gt;'
        fake_link = '&lt;a l:href="#missing"&gt;demo&lt;/a&gt;'
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<h1 id="real">Heading</h1><p><a href="#real">Real link</a></p>'
                                    f'<pre>{example} {example} {fake_link}</pre>', version=version)
                self.assertEqual(self.text(root).count('<span id="example">demo</span>'), 2)
                self.assertIn('<a l:href="#missing">demo</a>', self.text(root))
                self.assertTrue(any(a.get(HREF) == '#real' for a in root.findall('.//f:a', NS)))

    def test_html_example_does_not_satisfy_real_link(self):
        root = self.convert('<p><a href="#example">Broken link</a></p>'
                            '<pre>&lt;span id="example"&gt;demo&lt;/span&gt;</pre>')
        self.assertIn('Broken link', self.text(root))
        self.assertFalse(any(a.get(HREF) == '#example' for a in root.findall('.//f:a', NS)))
        self.assertIn('Unresolved internal fragment', self.diagnostics)

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

    def test_heading_block_child_does_not_merge_words(self):
        root = self.convert('<h1>Chapter 1: Beggar<span hidden=""> | </span>'
                            '<div><a href="https://example.com/book">Kingdom’s Bloodline</a></div>'
                            'continued</h1><p>Text.</p>', navigation=False)
        title = root.find('.//f:body//f:title/f:p', NS)
        self.assertIsNotNone(title)
        self.assertEqual(self.text(title), 'Chapter 1: Beggar Kingdom’s Bloodline continued')
        self.assertTrue(any(a.get(HREF) == 'https://example.com/book' for a in title.findall('.//f:a', NS)))
        root = self.convert('<h1>Before <div>Middle</div> after</h1><p>Text.</p>', navigation=False)
        title = root.find('.//f:body//f:title/f:p', NS)
        self.assertEqual(self.text(title), 'Before Middle after')

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

    def test_figure_caption_preserves_multiple_paragraphs_and_blank_line(self):
        body = ('<figure><img src="../a.png"/><figcaption id="caption">'
                '<p><em>First caption.</em></p><p/><p>Second caption.</p>'
                '</figcaption></figure><p><a href="#caption">Go</a></p>')
        root = self.convert(body, {'a.png': ('a', 'image/png', PNG)}, navigation=False)
        nodes = [n for n in root.findall('.//f:body//f:section/*', NS) if n.tag.rsplit('}', 1)[-1] in ('p', 'subtitle', 'empty-line')]
        texts = [''.join(n.itertext()).strip() for n in nodes]
        first = texts.index('First caption.')
        second = texts.index('Second caption.')
        self.assertTrue(any(n.tag.endswith('}empty-line') for n in nodes[first + 1:second]))
        self.assertTrue(any(n.get('id') == 'caption' for n in root.iter()))
        self.assertTrue(any(n.get(HREF) == '#caption' for n in root.iter()))

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

    def test_css_media_all_preserves_semantic_font_and_vertical_alignment(self):
        root = self.convert('<p><span class="italic">Emphasized</span> '
                            '<span class="sup">Raised</span> <span class="sub">Lowered</span> '
                            '<a class="sup" href="#target">Link</a> '
                            '<span class="reset" style="vertical-align:baseline">Baseline</span></p>'
                            '<p id="target">Destination</p>',
                            head='<style>@media all {.italic {font-style:italic} .sup {vertical-align:super} '
                                 '.sub {vertical-align:sub} .reset {vertical-align:super}} '
                                 '@media print {.italic {font-weight:bold}}</style>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Emphasized' in self.text(p))
        self.assertTrue(any('Emphasized' in self.text(n) for n in paragraph.findall('.//f:emphasis', NS)))
        self.assertTrue(any('Raised' in self.text(n) for n in paragraph.findall('.//f:sup', NS)))
        self.assertTrue(any('Lowered' in self.text(n) for n in paragraph.findall('.//f:sub', NS)))
        self.assertTrue(any(n.get(HREF) == '#target' and 'Link' in self.text(n) for n in paragraph.findall('.//f:sup/f:a', NS)))
        self.assertFalse(any('Baseline' in self.text(n) for n in paragraph.findall('.//f:sup', NS)))
        self.assertIsNone(paragraph.find('.//f:strong', NS))
        self.assertIn('Unsupported CSS at-rule @media', self.diagnostics)

    def test_css_vertical_alignment_cascade_and_unsupported_value(self):
        root = self.convert('<p><span class="raised" style="vertical-align:baseline">Important raised</span> '
                            '<span class="lowered">Inline lower</span> '
                            '<span style="vertical-align:3px">Unsupported retained</span> '
                            '<sup style="vertical-align:baseline">Native reset</sup> '
                            '<sub style="vertical-align:super">Native override</sub> '
                            '<SUP style="vertical-align:baseline">Upper reset</SUP> '
                            '<SUB style="vertical-align:super">Upper override</SUB></p>',
                            head='<style>.raised {vertical-align:super !important} '
                                 '.lowered {vertical-align:sub}</style>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Important raised' in self.text(p))
        self.assertTrue(any('Important raised' in self.text(n) for n in paragraph.findall('.//f:sup', NS)))
        self.assertTrue(any('Inline lower' in self.text(n) for n in paragraph.findall('.//f:sub', NS)))
        self.assertFalse(any('Native reset' in self.text(n) for n in paragraph.findall('.//f:sup', NS)))
        self.assertFalse(any('Upper reset' in self.text(n) for n in paragraph.findall('.//f:sup', NS)))
        self.assertTrue(any('Native override' in self.text(n) for n in paragraph.findall('.//f:sup', NS)))
        self.assertEqual(sum('Upper override' in self.text(n) for n in paragraph.findall('.//f:sup', NS)), 1)
        self.assertIn('Unsupported retained', self.text(paragraph))
        self.assertIn('Unsupported CSS vertical-align', self.diagnostics)

    def test_css_media_all_nested_rules_and_whitespace_preserved(self):
        root = self.convert('<p>One<span class="empty"> </span>two '
                            '<span class="mark">nested</span> '
                            '<span class="outside">outside</span></p>',
                            head='<style>@media ALL { @media all { .mark {font-weight:bold} } '
                                 '.empty {vertical-align:super} } '
                                 '.outside {font-style:italic}</style>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'nested' in self.text(p))
        self.assertIn('One two', self.text(paragraph))
        self.assertTrue(any('nested' in self.text(n) for n in paragraph.findall('.//f:strong', NS)))
        self.assertTrue(any('outside' in self.text(n) for n in paragraph.findall('.//f:emphasis', NS)))
        self.assertNotIn('Unsupported CSS at-rule @media', self.diagnostics)

    def test_styled_word_boundary_spaces_remain_outside_inline_markup(self):
        root = self.convert('<p>alpha<em> beta </em>gamma<strong> delta </strong>epsilon '
                            'one<span class="raised"> two </span>three</p>',
                            head='<style>.raised {vertical-align:super}</style>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'alpha' in self.text(p))
        self.assertEqual(self.text(paragraph), 'alpha beta gamma delta epsilon one two three')
        emphasis = paragraph.find('f:emphasis', NS)
        strong = paragraph.find('f:strong', NS)
        raised = paragraph.find('f:sup', NS)
        self.assertEqual(paragraph.text, 'alpha ')
        self.assertEqual(emphasis.text, 'beta')
        self.assertEqual(emphasis.tail, ' gamma ')
        self.assertEqual(strong.text, 'delta')
        self.assertEqual(strong.tail, ' epsilon one ')
        self.assertEqual(raised.text, 'two')
        self.assertEqual(raised.tail, ' three')

    def test_styled_boundary_nbsp_survives_without_typography_cleanup(self):
        root = self.convert('<p>alpha<em>&#160;beta&#160;</em>gamma</p>',
                            options=['--no-clean-typography'], navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'beta' in self.text(p))
        emphasis = paragraph.find('f:emphasis', NS)
        self.assertEqual(paragraph.text, 'alpha\u00a0')
        self.assertEqual(emphasis.text, 'beta')
        self.assertEqual(emphasis.tail, '\u00a0gamma')

    def test_page_layout_warning_is_specific_and_deduplicated(self):
        root = self.convert('<p class="styled">Text stays italic</p>',
                            head='<style>@page {margin: 5pt} @page :left {margin: 0} '
                                 '.styled {font-style: italic}</style>'
                                 '<style>@page {padding: 0} @media print {.styled {font-weight:bold}}</style>',
                            navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Text stays italic' in self.text(p))
        self.assertIsNotNone(paragraph.find('.//f:emphasis', NS))
        self.assertIsNone(paragraph.find('.//f:strong', NS))
        log = '\n'.join(p.read_text(encoding='utf-8-sig') for p in self.last_folder.glob('*.log'))
        self.assertEqual(log.count('Unsupported CSS at-rule @page:'), 1)
        self.assertIn('page layout settings cannot be represented in FB2', log)
        self.assertEqual(log.count('Unsupported CSS at-rule @media;'), 1)

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
        root = self.convert('<p><span style="font-style: oblique 20deg; font-weight: 700">'
                            'Text retained</span></p>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Text retained' in self.text(p))
        self.assertIsNotNone(paragraph.find('.//f:strong', NS))
        self.assertIsNone(paragraph.find('.//f:emphasis', NS))
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

    def test_epub2_publication_and_creation_dates_keep_their_meaning(self):
        root = self.convert('<p>Content</p>', version='2.0', navigation=False,
                            metadata_extra='<dc:date opf:event="creation">1999-01-02</dc:date>'
                                           '<dc:date opf:event="publication">2004-03-04</dc:date>')
        self.assertEqual(root.find('.//f:title-info/f:date', NS).text, '2004-03-04')
        self.assertEqual(root.find('.//f:publish-info/f:year', NS).text, '2004')
        dates = [(n.get('info-type'), n.text) for n in root.findall('.//f:custom-info', NS)]
        self.assertIn(('EPUB date creation', '1999-01-02'), dates)
        root = self.convert('<p>Content</p>', version='2.0', navigation=False,
                            metadata_extra='<dc:date opf:event="creation">1999-01-02</dc:date>')
        self.assertEqual(root.find('.//f:title-info/f:date', NS).text, '1999-01-02')
        self.assertIsNone(root.find('.//f:publish-info/f:year', NS))
        root = self.convert('<p>Content</p>', version='2.0', navigation=False, date='2002-01-01',
                            metadata_extra='<dc:date xmlns:alt="http://www.idpf.org/2007/opf" alt:event="publication">2004-03-04</dc:date>'
                                           '<dc:date opf:event="modification">2005-05-06</dc:date>')
        self.assertEqual(root.find('.//f:title-info/f:date', NS).text, '2004-03-04')
        dates = [(n.get('info-type'), n.text) for n in root.findall('.//f:custom-info', NS)]
        self.assertIn(('EPUB date', '2002-01-01'), dates)
        self.assertIn(('EPUB date modification', '2005-05-06'), dates)

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

    def test_svg_relative_raster_image_matches_embedded_image(self):
        template = ('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
                    'width="8" height="8"><image width="8" height="8" xlink:href="%s"/></svg>')
        relative = template % '../images/pixel.png'
        embedded = template % ('data:image/png;base64,' + base64.b64encode(PNG).decode('ascii'))
        empty = '<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"></svg>'
        def render(svg):
            root = self.convert('<p><img src="../art/image.svg"/></p>',
                                extra={'art/image.svg': ('drawing', 'image/svg+xml', svg),
                                       'images/pixel.png': ('pixel', 'image/png', PNG)}, navigation=False, svg='png')
            return base64.b64decode(root.find('f:binary', NS).text or '')
        expected, blank, actual = render(embedded), render(empty), render(relative)
        self.assertNotEqual(expected, blank, 'embedded control image was not rendered')
        self.assertEqual(actual, expected, 'relative raster image was not rendered in SVG: ' + self.diagnostics)

    def test_svg_external_raster_cannot_escape_epub_root(self):
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
               'width="8" height="8"><image width="8" height="8" xlink:href="../../../outside.png"/></svg>')
        root = self.convert('<p><img src="../art/image.svg"/></p>',
                            extra={'art/image.svg': ('drawing', 'image/svg+xml', svg)},
                            outside={'outside.png': PNG}, navigation=False, svg='png')
        self.assertIn('svg raster resource is external, unsupported or outside the epub', self.diagnostics.lower())
        self.assertIn('1 placeholders', self.public_diagnostics.lower())
        self.assertNotEqual(base64.b64decode(root.find('f:binary', NS).text or ''), PNG)

    def test_svg_xml_base_is_reported_instead_of_guessing(self):
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
               'xml:base="../images/" width="8" height="8">'
               '<image width="8" height="8" xlink:href="pixel.png"/></svg>')
        self.convert('<p><img src="../art/image.svg"/></p>',
                     extra={'art/image.svg': ('drawing', 'image/svg+xml', svg),
                            'images/pixel.png': ('pixel', 'image/png', PNG)}, navigation=False, svg='png')
        self.assertIn('svg raster resource with xml:base', self.diagnostics.lower())
        self.assertIn('1 placeholders', self.public_diagnostics.lower())

    def test_svg_large_raster_resource_is_reported(self):
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
               'width="8" height="8"><image width="8" height="8" xlink:href="big.png"/></svg>')
        self.convert('<p><img src="../art/image.svg"/></p>',
                     extra={'art/image.svg': ('drawing', 'image/svg+xml', svg),
                            'art/big.png': ('big', 'image/png', b'\x00' * (16 * 1024 * 1024 + 1))},
                     navigation=False, svg='png')
        self.assertIn('exceeds the embedding limit', self.diagnostics.lower())
        self.assertIn('1 placeholders', self.public_diagnostics.lower())

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

    def test_heading_link_to_backlinked_contents_is_not_a_note(self):
        root = self.convert('<h1><a id="chapter1" href="contents.xhtml#entry1">1</a> Chapter</h1><p>Chapter text</p>',
                            {'Text/contents.xhtml': ('contents', 'application/xhtml+xml',
                             '<section epub:type="frontmatter" title="Contents"><p class="toc_chap"><a id="entry1" href="chapter.xhtml#chapter1">1. Chapter</a></p></section>')},
                            spine=['contents', 'chapter'])
        self.assertEqual(self.notes(root), [])
        self.assertIn('1. Chapter', self.text(root.find('f:body', NS)))

    def test_numeric_contents_link_to_chapter_container_is_not_note(self):
        root = self.convert('<div class="chapter" id="chapter1"><div class="chapterHead"><h2><a href="contents.xhtml#entry1">1</a></h2></div>'
                            '<div class="chapterBody"><p>Essential chapter paragraph</p></div></div>',
                            {'Text/contents.xhtml': ('contents', 'application/xhtml+xml',
                             '<div class="contents"><div id="entry1" class="contentsEntry"><p class="contentsEntryNumber">'
                             '<a href="chapter.xhtml#chapter1">1</a></p></div></div>')}, spine=['contents','chapter'])
        self.assertIn('Essential chapter paragraph', self.text(root.find('f:body', NS)))
        self.assertEqual(self.notes(root), [])

    def test_numeric_link_to_epub_chapter_is_not_note(self):
        root = self.convert('<section epub:type="chapter" id="chapter1"><p>Essential chapter paragraph</p></section>',
                            {'Text/contents.xhtml': ('contents', 'application/xhtml+xml',
                             '<p><a href="chapter.xhtml#chapter1">1</a></p>')}, spine=['contents','chapter'])
        self.assertIn('Essential chapter paragraph', self.text(root.find('f:body', NS)))
        self.assertEqual(self.notes(root), [])

    def test_standalone_quote_attribution_preserves_nested_note_anchor(self):
        root = self.convert('<p class="CHAP-QUOTE-ATTRIBUTION" id="attribution">(Author)<span id="extra"></span><a href="#n9" id="c9"><sup>9</sup></a></p>'
                            '<p class="FOOTNOTE-MID"><a href="#c9" id="n9"><span>9</span></a> Note text</p>')
        targets = {node.get('id') for node in root.iter() if node.get('id')}
        self.assertTrue({'attribution', 'extra', 'c9'}.issubset(targets))
        for link in root.findall('.//f:a', NS):
            href = link.get(HREF, '')
            if href.startswith('#'):
                self.assertIn(href[1:], targets)

    def test_numeric_same_document_note_backlink_keeps_chapter_paragraph(self):
        root = self.convert('<div class="Chapter_section"><p>Ordinary chapter text <sup><a href="#ft1" id="fn1">1</a></sup></p>'
                            '<p class="notes"><a href="#fn1" id="ft1">1</a> Genuine note</p></div>')
        main = self.text(root.find('f:body', NS))
        self.assertIn('Ordinary chapter text', main)
        self.assertNotIn('Ordinary chapter text', self.text(next(b for b in root.findall('f:body',NS) if b.get('name')=='notes')))

    def test_cross_document_link_to_note_reference_keeps_chapter_paragraph(self):
        root = self.convert('<p>Ordinary chapter text <sup><a href="#ft1" id="fn1">1</a></sup></p>'
                            '<p class="notes"><a href="#fn1" id="ft1">1</a> Genuine note</p>',
                            {'Text/other.xhtml': ('other', 'application/xhtml+xml',
                             '<h1>Other chapter<sup><a href="chapter.xhtml#fn1">1</a></sup></h1><p>Other text</p>')},
                            spine=['chapter', 'other'])
        self.assertIn('Ordinary chapter text', self.text(root.find('f:body', NS)))
        self.assertNotIn('Genuine note', self.text(root.find('f:body', NS)))
        notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
        self.assertNotIn('Ordinary chapter text', self.text(notes))
        self.assertIn('Genuine note', self.text(notes))

    def test_cross_link_to_reference_wrapping_superscript(self):
        root = self.convert('<p>Ordinary chapter text <a href="#ft1" id="fn1"><sup>1</sup></a></p>'
                            '<p class="notes"><a href="#fn1" id="ft1">1</a> Genuine note</p>',
                            {'Text/other.xhtml': ('other', 'application/xhtml+xml',
                             '<p>Other text <a href="chapter.xhtml#fn1">1</a></p>')}, spine=['chapter', 'other'])
        self.assertIn('Ordinary chapter text', self.text(root.find('f:body', NS)))
        notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
        self.assertNotIn('Ordinary chapter text', self.text(notes))
        self.assertIn('Genuine note', self.text(notes))

    def test_note_backlink_to_anchor_at_start_of_paragraph(self):
        root = self.convert('<p><a id="ref"></a>Ordinary chapter text <a href="#note"><sup>1</sup></a> continues.</p>'
                            '<p><a id="note"></a><a href="#ref"><sup>1</sup></a> Genuine note.</p>')
        self.assertIn('Ordinary chapter text', self.text(root.find('f:body', NS)))
        notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
        self.assertNotIn('Ordinary chapter text', self.text(notes))
        self.assertIn('Genuine note', self.text(notes))

    def test_symbolic_note_backlink_to_quoted_paragraph(self):
        root = self.convert('<p><a id="ref"></a>“Ordinary dialogue.” <a href="#note"><sup>*</sup></a></p>'
                            '<p><a id="note"></a><a href="#ref"><sup>*</sup></a> Genuine note.</p>')
        self.assertIn('Ordinary dialogue', self.text(root.find('f:body', NS)))
        notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
        self.assertNotIn('Ordinary dialogue', self.text(notes))
        self.assertIn('Genuine note', self.text(notes))

    def test_note_backlink_to_dialogue_with_opening_punctuation(self):
        for opening in ('‘’ey, ordinary dialogue.', '‘…ordinary dialogue.'):
            with self.subTest(opening=opening):
                root = self.convert('<p><a id="ref"></a>' + opening + ' <a href="#note"><sup>1</sup></a></p>'
                                    '<p><a id="note"></a><a href="#ref"><sup>1</sup></a> Genuine note.</p>')
                self.assertIn('ordinary dialogue', self.text(root.find('f:body', NS)).lower())
                notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
                self.assertNotIn('ordinary dialogue', self.text(notes).lower())
                self.assertIn('Genuine note', self.text(notes))

    def test_note_backlink_uses_parent_id_when_link_has_own_id(self):
        root = self.convert('<p>Ordinary chapter text <a href="#note"><sup>1</sup></a><a id="ref"></a> continues.</p>'
                            '<p id="note"><a id="backlink" href="#ref"><sup>1</sup></a> Genuine note.</p>')
        self.assertIn('Ordinary chapter text', self.text(root.find('f:body', NS)))
        notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
        self.assertNotIn('Ordinary chapter text', self.text(notes))
        self.assertIn('Genuine note', self.text(notes))

    def test_note_backlink_uses_outer_container_id(self):
        root = self.convert('<p><a id="ref"></a>Ordinary chapter text <a href="#note"><sup>1</sup></a> continues.</p>'
                            '<div id="note"><div><p><a id="backlink" href="#ref"><sup>1</sup></a> Genuine note.</p></div></div>')
        self.assertIn('Ordinary chapter text', self.text(root.find('f:body', NS)))
        notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
        self.assertNotIn('Ordinary chapter text', self.text(notes))
        self.assertIn('Genuine note', self.text(notes))

    def test_bracket_before_note_backlink_keeps_main_paragraph(self):
        root = self.convert('<p>Ordinary chapter text <a href="#note" id="ref">[1]</a>.</p>'
                            '<p class="footnotes" id="note">[<a href="#ref" id="note-link">1</a>] Genuine note.</p>')
        self.assertIn('Ordinary chapter text', self.text(root.find('f:body', NS)))
        notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
        self.assertNotIn('Ordinary chapter text', self.text(notes))
        self.assertIn('Genuine note', self.text(notes))

    def test_article_note_in_footnotes_aside_keeps_main_and_note(self):
        source = ('<p>Ordinary chapter text <a href="#note" id="ref">[1]</a>.</p>'
                  '<aside id="footnotes"><article id="note"><hr/><p class="footnotes">['
                  '<a href="#ref" id="note-link">1</a>] Genuine note.</p></article></aside>')
        root = self.convert(source)
        self.assertIn('Ordinary chapter text', self.text(root.find('f:body', NS)))
        self.assertNotIn('Genuine note', self.text(root.find('f:body', NS)))
        notes = next(b for b in root.findall('f:body', NS) if b.get('name') == 'notes')
        self.assertNotIn('Ordinary chapter text', self.text(notes))
        self.assertIn('Genuine note', self.text(notes))
        self.assertEqual(len(root.findall('.//f:a[@type="note"]', NS)), 1)
        inline = self.convert(source, options=['--no-notes'])
        self.assertIn('Genuine note', self.text(inline.find('f:body', NS)))

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

    def test_unresolved_links_summarize_warning_and_keep_log(self):
        links = ''.join(f'<a href="#missing{i}">Link{i}</a>' for i in range(20))
        root = self.convert('<h1>Chapter</h1><p>' + links + '</p>')
        self.assertIn('Link19', self.text(root))
        self.assertIn('Links to missing EPUB anchors: 20', self.diagnostics)
        self.assertLess(len(self.public_diagnostics), 1000)
        self.assertNotIn('missing19', self.public_diagnostics)
        self.assertIn('missing19', self.diagnostics)
        self.assertTrue(list(self.last_folder.glob('*.log')))

    def test_unresolved_local_fragment_retains_text_and_warning(self):
        for options in ([], ['--no-validate']):
            with self.subTest(options=options):
                root = self.convert('<h1>Chapter</h1><p>Before <a href="#missing-anchor"><em>Broken</em></a> after</p>',
                                    options=options)
                self.assertIn('Broken', self.text(root))
                self.assertIsNotNone(root.find('.//f:emphasis', NS))
                self.assertFalse(any(a.get(HREF) == '#missing-anchor' for a in root.findall('.//f:a', NS)))
                self.assertIn('missing-anchor', self.diagnostics)
                self.assertIn('Links to missing EPUB anchors: 1', self.diagnostics)

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

    def test_xhtml_load_accounting_reports_distinct_processing_passes(self):
        extra = {'Text/next.xhtml': ('next', 'application/xhtml+xml', '<p>Second chapter</p>')}
        for options, expected, hits in (([], (2, 2, 1), 4), (['--no-notes'], (2, 2, 1), 2)):
            with self.subTest(options=options):
                root = self.convert('<p>First chapter</p>', extra=extra, spine=['chapter', 'next'],
                                    navigation=False, options=options)
                self.assert_once(root, 'First chapter', 'Second chapter')
                found = re.search(r'XHTML load calls: total=(\d+) unique=(\d+) max-per-file=(\d+)', self.diagnostics)
                self.assertIsNotNone(found, self.diagnostics)
                self.assertEqual(tuple(map(int, found.groups())), expected)
                self.assertIn(f'XHTML cache hits: {hits}', self.diagnostics)

    def test_xhtml_cache_document_limit_falls_back_without_content_loss(self):
        extra = {f'Text/chapter-{i}.xhtml': (f'part{i}', 'application/xhtml+xml', f'<p>Unique chapter {i:03d} end.</p>')
                 for i in range(129)}
        root = self.convert('<p>First chapter</p>', extra=extra,
                            spine=['chapter'] + [f'part{i}' for i in range(129)],
                            navigation=False, options=['--no-notes'])
        self.assert_once(root, 'First chapter', 'Unique chapter 128 end.')
        found = re.search(r'XHTML load calls: total=(\d+) unique=(\d+) max-per-file=(\d+)', self.diagnostics)
        self.assertIsNotNone(found, self.diagnostics)
        self.assertEqual(tuple(map(int, found.groups())), (132, 130, 2))
        self.assertIn('XHTML cache hits: 128', self.diagnostics)

    def test_xhtml_cache_byte_limit_falls_back_without_content_loss(self):
        root = self.convert('<p>Large source remains.</p><!--' + ('a' * (512 * 1024)) + '-->',
                            navigation=False, options=['--no-notes'])
        self.assert_once(root, 'Large source remains.')
        self.assertIn('XHTML load calls: total=2 unique=1 max-per-file=2', self.diagnostics)
        self.assertIn('XHTML cache hits: 0', self.diagnostics)

    def test_xhtml_cache_does_not_leak_between_batch_imports(self):
        temp = tempfile.TemporaryDirectory(prefix='epub-cache-batch-')
        self.folders.append(temp)
        inputs = Path(temp.name) / 'input'; inputs.mkdir()
        outputs = Path(temp.name) / 'output'; outputs.mkdir()
        for index, text in enumerate(('First unique body.', 'Second unique body.')):
            self.convert(f'<p>{text}</p>', navigation=False)
            shutil.copyfile(self.last_folder / 'case.epub', inputs / f'book-{index}.epub')
        result = subprocess.run([str(BATCH), '--batch', str(inputs), str(outputs), '--no-report'],
                                capture_output=True, encoding='utf-8', errors='replace', timeout=45)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for index, expected in enumerate(('First unique body.', 'Second unique body.')):
            path = outputs / f'book-{index}.fb2'
            root = ET.parse(path).getroot()
            self.outputs.append(path)
            self.assert_once(root, expected)
            self.assertNotIn('Second unique body.' if index == 0 else 'First unique body.', self.text(root))

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

    def test_epigraph_after_body_preserves_order(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p>Before quotation</p><div class="epigraph"><p>Quoted text</p>'
                                    '<p class="text-author">Attribution</p></div><p>After quotation</p>',
                                    version=version, navigation=False)
                text = self.text(root)
                self.assertLess(text.index('Before quotation'), text.index('Quoted text'))
                self.assertLess(text.index('Quoted text'), text.index('Attribution'))
                self.assertLess(text.index('Attribution'), text.index('After quotation'))

    def test_css_background_in_empty_content_block(self):
        root = self.convert('<div class="illustration"></div><p>Following text.</p>',
                            extra={'Styles/layout.css': ('css', 'text/css', '.illustration { background-image: url(../images/ornament.png); }'),
                                   'images/ornament.png': ('ornament', 'image/png', PNG)},
                            head='<link rel="stylesheet" href="../Styles/layout.css"/>', navigation=False)
        self.assertIn(PNG, {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)})
        self.assertIn('Following text.', self.text(root))

    def test_css_background_cascade_and_inline_override(self):
        css = ('.art { background-image: url(../images/a.png); } '
               'div.art { background-image: url(../images/b.jpg); } '
               '.priority { background-image: url(../images/a.png) !important; }')
        root = self.convert('<div class="art"></div><div class="art" style="background-image:none"></div>'
                            '<div class="art priority" style="background-image:url(../images/b.jpg)"></div>'
                            '<p>After.</p>',
                            extra={'Styles/layout.css': ('css', 'text/css', css),
                                   'images/a.png': ('a', 'image/png', PNG),
                                   'images/b.jpg': ('b', 'image/jpeg', JPEG)},
                            head='<link rel="stylesheet" href="../Styles/layout.css"/>', navigation=False)
        images = root.findall('.//f:body//f:image', NS)
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertEqual([binaries[n.get(HREF)[1:]] for n in images], [JPEG, PNG])

    def test_spine_contents_page_kept_unless_service_removal_requested(self):
        extra = {'Text/contents.xhtml': ('contents', 'application/xhtml+xml',
                 '<p>Visual contents <img src="../images/contents.png"/></p>'),
                 'images/contents.png': ('contents-image', 'image/png', PNG)}
        for options, kept in (([], True), (['--remove-service-sections'], False)):
            with self.subTest(options=options):
                root = self.convert('<p>Chapter text.</p>', extra, spine=['contents', 'chapter'],
                                    options=options, navigation=False)
                self.assertEqual('Visual contents' in self.text(root), kept)
                self.assertEqual(PNG in {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}, kept)

    def test_hidden_block_preserves_referenced_anchor_without_text(self):
        root = self.convert('<p><a href="#hidden-target">Link</a></p>'
                            '<div hidden=""><span id="hidden-target">Invisible text.</span></div>'
                            '<p>Visible text.</p>', navigation=False)
        self.assertNotIn('Invisible text.', self.text(root))
        self.assertIn('Visible text.', self.text(root))
        self.assertIn('hidden-target', {n.get('id') for n in root.iter() if n.get('id')})
        self.assertIn('#hidden-target', [n.get(HREF) for n in root.findall('.//f:a', NS)])

    def test_hidden_boolean_attribute_with_empty_value(self):
        body = ('<p hidden="">Hidden empty.</p><p hidden="false">Hidden false.</p>'
                '<p>Visible text.</p>')
        for options, included in (([], False), (['--include-hidden'], True)):
            with self.subTest(options=options):
                root = self.convert(body, options=options, navigation=False)
                self.assertIn('Visible text.', self.text(root))
                self.assertEqual('Hidden empty.' in self.text(root), included)
                self.assertEqual('Hidden false.' in self.text(root), included)

    def test_stylesheet_visibility_and_explicit_include_hidden(self):
        css = '.hidden {display:none;} .hidden.show {display:block;} .important {display:none !important;}'
        body = ('<p class="hidden">Hidden text.</p><p class="hidden show">Visible override.</p>'
                '<p class="important" style="display:block">Important hidden.</p>')
        extra = {'Styles/layout.css': ('css', 'text/css', css)}
        for options, includes in (([], False), (['--include-hidden'], True)):
            with self.subTest(options=options):
                root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../Styles/layout.css"/>',
                                    options=options, navigation=False)
                self.assertIn('Visible override.', self.text(root))
                self.assertEqual('Hidden text.' in self.text(root), includes)
                self.assertEqual('Important hidden.' in self.text(root), includes)

    def test_hidden_class_is_not_proof_of_invisible_section(self):
        root = self.convert('<section class="hidden"><p>Visible section.</p>'
                            '<img src="../images/map.png"/></section>',
                            extra={'Styles/layout.css': ('css', 'text/css', 'h1.hidden {display:none;}'),
                                   'images/map.png': ('map', 'image/png', PNG)},
                            head='<link rel="stylesheet" href="../Styles/layout.css"/>', navigation=False)
        self.assertIn('Visible section.', self.text(root))
        self.assertIn(PNG, {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)})

    def test_nested_links_keep_text_without_invalid_fb2(self):
        root = self.convert('<p><a href="#first">Outer <a href="#second">Inner</a> tail</a></p>'
                            '<p id="first">First</p><p id="second">Second</p>', navigation=False)
        links = root.findall('.//f:a', NS)
        self.assertEqual([node.get(HREF) for node in links], ['#first', '#second', '#first'])
        self.assertEqual(''.join(''.join(node.itertext()) for node in links), 'Outer Inner tail')
        self.assertNotIn('nested hyperlink was flattened', self.diagnostics.lower())

    def test_nested_link_without_outer_text_reports_omitted_destination(self):
        root = self.convert('<p><a href="#first"><a href="#second">Inner</a></a></p>'
                            '<p id="first">First</p><p id="second">Second</p>', navigation=False)
        links = root.findall('.//f:a', NS)
        self.assertEqual([node.get(HREF) for node in links], ['#second'])
        self.assertIn('outer hyperlink had no separate text', self.diagnostics.lower())

    def test_formatted_nested_link_reports_flattening(self):
        root = self.convert('<p><a href="#first">Outer <strong><a href="#second">Inner</a></strong></a></p>'
                            '<p id="first">First</p><p id="second">Second</p>', navigation=False)
        links = root.findall('.//f:a', NS)
        self.assertEqual([node.get(HREF) for node in links], ['#first'])
        self.assertIn('Inner', self.text(root))
        self.assertIn('nested hyperlink was flattened', self.diagnostics.lower())

    def test_video_poster_preserved_as_image(self):
        root = self.convert('<p>Before</p><video poster="../images/poster.jpg" controls="controls">'
                            '<source src="../media/clip.mp4" type="video/mp4"/></video><p>After</p>',
                            extra={'images/poster.jpg': ('poster', 'image/jpeg', JPEG)}, navigation=False)
        self.assertIn(JPEG, {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)})
        self.assertLess(self.text(root).index('Before'), self.text(root).index('After'))
        self.assertIn('video', self.diagnostics.lower())

    def test_pagebreak_class_preserves_paragraph_content(self):
        for options in (['--pagebreaks'], ['--no-pagebreaks']):
            with self.subTest(options=options):
                root = self.convert('<p class="P_TEXTBODY_CENTERALIGN_PAGEBREAK">'
                                    '<span><img src="../images/gallery.png"/></span>Gallery caption</p>',
                                    extra={'images/gallery.png': ('gallery', 'image/png', PNG)},
                                    options=options, navigation=False)
                self.assertIn('Gallery caption', self.text(root))
                self.assertIn(PNG, {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)})

    def test_preformatted_images_respect_hidden_ancestors_and_order(self):
        root = self.convert('<pre>start<img src="../images/one.png"/><img src="../images/two.jpg"/>'
                            '<span hidden="hidden"><img src="../images/hidden.jpg"/></span>end</pre>',
                            extra={'images/one.png': ('one', 'image/png', PNG),
                                   'images/two.jpg': ('two', 'image/jpeg', JPEG),
                                   'images/hidden.jpg': ('hidden', 'image/jpeg', JPEG)}, navigation=False)
        code = root.find('.//f:code', NS)
        images = code.findall('f:image', NS)
        self.assertEqual(len(images), 2)
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertEqual([binaries[image.get(HREF)[1:]] for image in images], [PNG, JPEG])
        self.assertEqual(code.text, 'start')
        self.assertEqual(images[-1].tail, 'end')

    def test_inline_background_with_text_is_reported(self):
        root = self.convert('<p><span style="background-image:url(../images/cap.png)">A</span>fter</p>',
                            extra={'images/cap.png': ('cap', 'image/png', PNG)}, navigation=False)
        self.assertIn('After', self.text(root))
        self.assertIn('background image on non-empty inline content', self.diagnostics.lower())

    def test_invalid_xhtml_reports_failed_normalization_stage(self):
        self.convert('<p>&nbsp;<b>Unclosed</p>', navigation=False, fail=True)
        self.assertIn('html fallback:', self.diagnostics.lower())

    def test_dedication_paragraph_preserves_inline_font_and_normal_neighbors(self):
        root = self.convert('<p>Before dedication.</p>'
                            '<p class="dedication"><em>Entire italic dedication</em> '
                            '<strong>bold dedication</strong></p>'
                            '<p>After dedication.</p>', navigation=False)
        paragraphs = root.findall('.//f:body//f:p', NS)
        dedication = next(p for p in paragraphs if 'Entire italic dedication' in ''.join(p.itertext()))
        self.assertIn('Entire italic dedication', ''.join(dedication.find('f:emphasis', NS).itertext()))
        self.assertIn('bold dedication', ''.join(dedication.find('f:strong', NS).itertext()))
        after = next(p for p in paragraphs if 'After dedication.' in ''.join(p.itertext()))
        self.assertIsNone(after.find('.//f:emphasis', NS))
        self.assertIsNone(after.find('.//f:strong', NS))

    def test_dedication_paragraph_keeps_pagebreak_labels_and_anchors(self):
        for version in ('2.0', '3.0'):
            for option in ('--pagebreaks', '--no-pagebreaks'):
                with self.subTest(version=version, option=option):
                    root = self.convert('<p><a href="#pg1">First page</a> <a href="#pg2">Second page</a></p>'
                                        '<p class="dedication"><span epub:type="pagebreak" id="pg1" title="PageMarkerOne"/>'
                                        '<em>Italic dedication</em><span epub:type="pagebreak" id="pg2" title="PageMarkerTwo"/>'
                                        '<strong>Bold dedication</strong></p>',
                                        version=version, options=[option], navigation=False)
                    self.assertIn('Italic dedication', ''.join(root.find('.//f:emphasis', NS).itertext()))
                    self.assertIn('Bold dedication', ''.join(root.find('.//f:strong', NS).itertext()))
                    for page in ('pg1', 'pg2'):
                        self.assertTrue(any(n.get('id') == page for n in root.iter()))
                    text = self.text(root)
                    self.assertLess(text.index('Italic dedication'), text.index('Bold dedication'))
                    if option == '--pagebreaks':
                        self.assertLess(text.index('PageMarkerOne'), text.index('Italic dedication'))
                        self.assertLess(text.index('Italic dedication'), text.index('PageMarkerTwo'))
                        self.assertLess(text.index('PageMarkerTwo'), text.index('Bold dedication'))
                    else:
                        self.assertNotIn('PageMarkerOne', text)
                        self.assertNotIn('PageMarkerTwo', text)

    def test_dedication_nested_pagebreak_keeps_label_and_emphasis(self):
        for version, option in (('2.0', '--pagebreaks'), ('2.0', '--no-pagebreaks'),
                                ('3.0', '--pagebreaks'), ('3.0', '--no-pagebreaks')):
            with self.subTest(version=version, option=option):
                root = self.convert('<p><a href="#pg">Page</a></p>'
                                    '<p class="dedication"><em>Before '
                                    '<span epub:type="pagebreak" id="pg" title="NestedMarker"/>'
                                    ' after</em></p>', version=version, options=[option], navigation=False)
                self.assertTrue(any(n.get('id') == 'pg' for n in root.iter()))
                text = self.text(root)
                self.assertLess(text.index('Before'), text.index('after'))
                italic = ''.join(''.join(e.itertext()) for e in root.findall('.//f:body//f:emphasis', NS))
                self.assertIn('Before', italic)
                self.assertIn('after', italic)
                if option == '--pagebreaks':
                    self.assertIn('NestedMarker', text)
                    self.assertLess(text.index('Before'), text.index('NestedMarker'))
                    self.assertLess(text.index('NestedMarker'), text.index('after'))
                    self.assertTrue(any('NestedMarker' in ''.join(e.itertext()) for e in root.findall('.//f:sub', NS)))
                else:
                    self.assertNotIn('NestedMarker', text)

    def test_dedication_nested_breaks_preserve_lines_and_inline_styles(self):
        root = self.convert('<p class="dedication"><em>First line<br/>Second line</em>'
                            '<br/><a href="https://example.org/"><strong>Third line<br/>Fourth line</strong></a></p>',
                            navigation=False)
        lines = [p for p in root.findall('.//f:body//f:p', NS)
                 if any(s in ''.join(p.itertext()) for s in ('First line', 'Second line', 'Third line', 'Fourth line'))]
        self.assertEqual([''.join(p.itertext()).strip() for p in lines],
                         ['First line', 'Second line', 'Third line', 'Fourth line'])
        for p in lines[:2]: self.assertIsNotNone(p.find('.//f:emphasis', NS))
        for p in lines[2:]:
            self.assertIsNotNone(p.find('.//f:strong', NS))
            self.assertTrue(any(n.get('{http://www.w3.org/1999/xlink}href') == 'https://example.org/'
                                for n in p.findall('.//f:a', NS)))

    def test_dedication_breaks_keep_blank_line_and_nested_page_marker(self):
        root = self.convert('<p><a href="#pg">Page</a></p>'
                            '<p class="dedication"><em>One<br/><br/>'
                            '<span epub:type="pagebreak" id="pg" title="PageMark"/>Two</em></p>',
                            options=['--pagebreaks'], navigation=False)
        section = next(s for s in root.findall('.//f:body//f:section', NS)
                       if any('One' in ''.join(p.itertext()) for p in s.findall('./f:p', NS)))
        lines = [p for p in section.findall('./f:p', NS) if ''.join(p.itertext()).strip()]
        self.assertEqual([''.join(p.itertext()).strip() for p in lines], ['One', 'PageMarkTwo'])
        self.assertIsNotNone(section.find('./f:empty-line', NS))
        self.assertTrue(any(n.get('id') == 'pg' for n in root.iter()))
        self.assertIsNotNone(lines[1].find('.//f:sub', NS))
        self.assertIn('Two', ''.join(lines[1].find('.//f:emphasis', NS).itertext()))

    def test_orphan_closing_tags_preserve_text_and_links_with_warning(self):
        root = self.convert('<p>Before</a><a href="#target">Link</a> after</p>'
                            '<!-- </div> --><p id="target">Target</p></div>', navigation=False)
        self.assertIn('BeforeLink after', self.text(root))
        self.assertEqual([node.get(HREF) for node in root.findall('.//f:a', NS)], ['#target'])
        self.assertIn('ignored unmatched closing html tag', self.diagnostics.lower())

    def test_misnested_closing_tags_are_not_repaired(self):
        self.convert('<p><a href="#target"><b>Text</a></b></p><p id="target">Target</p>',
                     navigation=False, fail=True)
        self.assertNotIn('ignored unmatched closing html tag', self.diagnostics.lower())

    def test_stylesheet_background_with_text_is_reported_only_when_effective(self):
        css = '.cap { background-image: url(../images/cap.png); }'
        extra = {'Styles/layout.css': ('css', 'text/css', css),
                 'images/cap.png': ('cap', 'image/png', PNG)}
        head = '<link rel="stylesheet" href="../Styles/layout.css"/>'
        root = self.convert('<p><span class="cap">A</span>fter</p>', extra=extra, head=head, navigation=False)
        self.assertIn('After', self.text(root))
        self.assertIn('background image on non-empty inline content', self.diagnostics.lower())
        self.convert('<p><span class="cap" style="background:none">A</span>fter</p>',
                     extra=extra, head=head, navigation=False)
        self.assertNotIn('background image on non-empty inline content', self.diagnostics.lower())
        for body, options in (('<p><span class="cap" hidden="">A</span>fter</p>', []),
                              ('<p><span class="cap">A</span>fter</p>', ['--no-images'])):
            with self.subTest(body=body, options=options):
                self.convert(body, extra=extra, head=head, options=options, navigation=False)
                self.assertNotIn('background image on non-empty inline content', self.diagnostics.lower())

    def test_standalone_inline_link_retains_target_and_text(self):
        css = 'a.button { background-image: url(../images/logo.png); }'
        body = ('<div><a class="button" href="https://example.org/">Visit <strong>site</strong></a></div>'
                '<div><a href="#destination">Jump</a></div><p id="destination">Destination.</p>')
        extra = {'Styles/layout.css': ('css', 'text/css', css), 'images/logo.png': ('logo', 'image/png', PNG)}
        head = '<link rel="stylesheet" href="../Styles/layout.css"/>'
        root = self.convert(body, extra=extra, head=head, navigation=False)
        links = root.findall('.//f:body//f:a', NS)
        external = next((node for node in links if node.get(HREF) == 'https://example.org/'), None)
        self.assertIsNotNone(external)
        self.assertEqual(''.join(external.itertext()), 'Visit site')
        self.assertIsNotNone(external.find('f:strong', NS))
        self.assertTrue(any((node.get(HREF) or '').startswith('#') and ''.join(node.itertext()) == 'Jump' for node in links))
        self.assertIn('background image on non-empty inline content', self.diagnostics.lower())
        root = self.convert(body, extra=extra, head=head, navigation=False, options=['--no-links'])
        self.assertEqual(root.findall('.//f:body//f:a', NS), [])
        self.assertIn('Visit site', self.text(root))
        root = self.convert('<div><a href="https://example.org/"><div><p>First block.</p>'
                            '<p>Second block.</p></div></a></div>', navigation=False)
        paragraphs = [''.join(node.itertext()) for node in root.findall('.//f:body//f:p', NS)]
        self.assertTrue(any('First block.' in text for text in paragraphs))
        self.assertTrue(any('Second block.' in text for text in paragraphs))
        self.assertIn('block hyperlink could not be preserved', self.diagnostics.lower())

    def test_standalone_link_preserves_descendant_image_anchor(self):
        body = ('<div><a href="https://example.org/"><img id="image-target" src="../images/logo.png"/></a></div>'
                '<p><a href="#image-target">Go to image</a></p>')
        root = self.convert(body, extra={'images/logo.png': ('logo', 'image/png', PNG)}, navigation=False)
        ids = {node.get('id') for node in root.iter() if node.get('id')}
        link = next(node for node in root.findall('.//f:body//f:a', NS) if ''.join(node.itertext()) == 'Go to image')
        self.assertIn(link.get(HREF)[1:], ids)

    def test_block_wrapped_single_image_preserves_link_and_anchors(self):
        body = ('<div><a href="https://example.org/"><div id="wrapper-target">'
                '<img id="image-target" src="../images/logo.png"/></div></a></div>'
                '<p><a href="#wrapper-target">Wrapper</a> <a href="#image-target">Image</a></p>')
        root = self.convert(body, extra={'images/logo.png': ('logo', 'image/png', PNG)}, navigation=False)
        image = root.find('.//f:body//f:a[@l:href="https://example.org/"]/f:image',
                          {'f': NS['f'], 'l': 'http://www.w3.org/1999/xlink'})
        self.assertIsNotNone(image)
        ids = {node.get('id') for node in root.iter() if node.get('id')}
        targets = [node.get(HREF)[1:] for node in root.findall('.//f:body//f:a', NS)
                   if (node.get(HREF) or '').startswith('#')]
        self.assertEqual(len(targets), 2)
        self.assertTrue(all(target in ids for target in targets))

    def test_empty_inline_css_background_preserves_image_in_order(self):
        css = '.ornament { background-image: url(../images/ornament.png); }'
        extra = {'Styles/layout.css': ('css', 'text/css', css),
                 'images/ornament.png': ('ornament', 'image/png', PNG)}
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p>Before <span class="ornament"></span> after.</p>',
                                    extra=extra, head='<link rel="stylesheet" href="../Styles/layout.css"/>',
                                    navigation=False, version=version)
                paragraph = next(p for p in root.findall('.//f:body//f:p', NS) if 'Before' in ''.join(p.itertext()))
                image = paragraph.find('f:image', NS)
                self.assertIsNotNone(image)
                binary = next(n for n in root.findall('f:binary', NS) if n.get('id') == image.get(HREF)[1:])
                self.assertEqual(base64.b64decode(binary.text or ''), PNG)
                self.assertIn('Before', paragraph.text)
                self.assertIn('after.', image.tail)
        for body, options in (('<p>Before <span class="ornament" style="background:none"></span> after.</p>', []),
                              ('<p>Before <span class="ornament" hidden=""></span> after.</p>', []),
                              ('<p>Before <span class="ornament"></span> after.</p>', ['--no-images'])):
            with self.subTest(body=body, options=options):
                root = self.convert(body, extra=extra, head='<link rel="stylesheet" href="../Styles/layout.css"/>',
                                    navigation=False, options=options)
                self.assertEqual(root.findall('.//f:body//f:image', NS), [])
        root = self.convert('<p><a href="https://example.org/"><span class="ornament"></span></a></p>',
                            extra=extra, head='<link rel="stylesheet" href="../Styles/layout.css"/>', navigation=False)
        self.assertIsNotNone(root.find('.//f:body//f:a/f:image', NS))
        root = self.convert('<p>Before <span style="background:url(../images/ornament.png)"></span> after.</p>',
                            extra=extra, navigation=False)
        self.assertIsNotNone(root.find('.//f:body//f:p/f:image', NS))

    def test_empty_link_css_background_preserves_image_and_link(self):
        css = 'a.logo { background-image: url(../images/logo.png); }'
        root = self.convert('<p>Before <a class="logo" href="https://example.org/"></a> after.</p>',
                            extra={'Styles/layout.css': ('css', 'text/css', css),
                                   'images/logo.png': ('logo', 'image/png', PNG)},
                            head='<link rel="stylesheet" href="../Styles/layout.css"/>', navigation=False)
        link = root.find('.//f:body//f:a', NS)
        self.assertIsNotNone(link)
        self.assertEqual(link.get(HREF), 'https://example.org/')
        image = link.find('f:image', NS)
        self.assertIsNotNone(image)
        binary = next(n for n in root.findall('f:binary', NS) if n.get('id') == image.get(HREF)[1:])
        self.assertEqual(base64.b64decode(binary.text or ''), PNG)
        self.assertIn('after.', link.tail)
        root = self.convert('<div><a class="logo" href="https://example.org/"></a></div><p>After.</p>',
                            extra={'Styles/layout.css': ('css', 'text/css', css),
                                   'images/logo.png': ('logo', 'image/png', PNG)},
                            head='<link rel="stylesheet" href="../Styles/layout.css"/>', navigation=False)
        link = root.find('.//f:body//f:a', NS)
        self.assertIsNotNone(link)
        self.assertEqual(link.get(HREF), 'https://example.org/')
        self.assertIsNotNone(link.find('f:image', NS))
        extra = {'Styles/layout.css': ('css', 'text/css', css), 'images/logo.png': ('logo', 'image/png', PNG)}
        for options, image_expected in ((['--no-images'], False), (['--no-links'], True)):
            with self.subTest(options=options):
                root = self.convert('<div><a class="logo" href="https://example.org/"></a></div><p>After.</p>',
                                    extra=extra, head='<link rel="stylesheet" href="../Styles/layout.css"/>',
                                    navigation=False, options=options)
                self.assertEqual(bool(root.findall('.//f:body//f:image', NS)), image_expected)
                self.assertEqual(root.findall('.//f:body//f:a', NS), [])

    def test_aria_hidden_does_not_hide_visual_content(self):
        root = self.convert('<p aria-hidden="true">Visible text for sighted readers.</p>'
                            '<div aria-hidden="true"><img src="../images/ornament.png"/></div>'
                            '<p style="display:none">Actually hidden text.</p>',
                            extra={'images/ornament.png': ('ornament', 'image/png', PNG)}, navigation=False)
        self.assertIn('Visible text for sighted readers.', self.text(root))
        self.assertNotIn('Actually hidden text.', self.text(root))
        self.assertIn(PNG, {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)})

    def test_sidebar_preserves_images_between_paragraphs(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<aside class="sidebar"><div><img src="../images/before.png"/></div>'
                                    '<p>Middle text</p><div><img src="../images/after.jpg"/></div></aside>',
                                    extra={'images/before.png': ('before', 'image/png', PNG),
                                           'images/after.jpg': ('after', 'image/jpeg', JPEG)},
                                    version=version, navigation=False)
                cite = root.find('.//f:cite', NS)
                self.assertIsNotNone(cite)
                order = [('image' if p.find('f:image', NS) is not None else ''.join(p.itertext()))
                         for p in cite.findall('f:p', NS)]
                self.assertEqual(order, ['image', 'Middle text', 'image'])
                binaries = {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
                self.assertTrue({PNG, JPEG}.issubset(binaries))

    def test_sidebar_headings_and_content_survive_without_poem_import(self):
        for version in ('2.0', '3.0'):
            for options in ((), ('--no-poems',)):
                with self.subTest(version=version, options=options):
                    root = self.convert('<p>Before</p><aside epub:type="sidebar">'
                                        '<h2 id="sidebar-heading">Sidebar heading</h2>'
                                        '<p>Sidebar paragraph</p></aside><p>After</p>',
                                        version=version, navigation=False, options=options)
                    self.assert_once(root, 'Before', 'Sidebar heading', 'Sidebar paragraph', 'After')
                    text = self.text(root)
                    positions = [text.index(value) for value in ('Before', 'Sidebar heading', 'Sidebar paragraph', 'After')]
                    self.assertEqual(positions, sorted(positions))
                    self.assertTrue(any(node.get('id') == 'sidebar-heading' for node in root.iter()))

    def test_aside_with_paragraph_divs_keeps_all_text(self):
        for options in ((), ('--no-poems',)):
            with self.subTest(options=options):
                root = self.convert('<aside><div epub:type="appendix"><section><h2>Appendix heading</h2>'
                                    '<p>First paragraph.</p><div>List <strong>introduction</strong> here.<div><p>List content.</p></div></div>'
                                    '<section><div class="Para" id="p2">'
                                    'Nested paragraph <em>with emphasis</em> survives.</div>'
                                    '<div><div class="FixedLine">const example = 42;</div></div>'
                                    '</section></section></div></aside>', navigation=False, options=options)
                self.assert_once(root, 'First paragraph.', 'List introduction here.', 'List content.', 'Nested paragraph', 'const example = 42;')
                self.assertTrue(any('introduction' in self.text(node) for node in root.findall('.//f:strong', NS)))
                self.assertTrue(any(node.get('id') == 'p2' for node in root.iter()))
                text = self.text(root)
                self.assertLess(text.index('First paragraph.'), text.index('Nested paragraph'))
                self.assertLess(text.index('Nested paragraph'), text.index('const example = 42;'))

    def test_fixed_line_preserves_inline_fragments_as_one_paragraph(self):
        body = ('<div class="ProgramCode"><div class="LineGroup">'
                '<div class="FixedLine" id="line"><strong>COUNT_</strong><strong id="nested">STAR</strong>'
                '<strong>: 40485</strong></div></div></div><p><a href="#nested">Target</a></p>')
        root = self.convert(body, navigation=False)
        paragraphs = root.findall('.//f:body//f:p', NS)
        matching = [p for p in paragraphs if ''.join(p.itertext()) == 'COUNT_STAR: 40485']
        self.assertEqual(len(matching), 1, self.diagnostics)
        self.assertEqual(matching[0].get('id'), 'line')
        self.assertEqual(''.join(''.join(n.itertext()) for n in matching[0].findall('.//f:strong', NS)), 'COUNT_STAR: 40485')
        self.assertTrue(any(n.get('id') == 'nested' for n in root.iter()))
        self.assertTrue(any(n.get(HREF) == '#nested' for n in root.iter()))

    def test_fixed_line_keeps_inline_links_and_nested_anchor_targets(self):
        body = ('<div class="ProgramCode"><div class="FixedLine" id="line">'
                'before <a href="#destination"><strong>linked</strong></a> '
                '<span id="inside">after</span></div></div>'
                '<p id="destination">Destination</p><p><a href="#inside">Inside</a></p>')
        root = self.convert(body, navigation=False)
        ids = [n.get('id') for n in root.iter() if n.get('id')]
        self.assertEqual(ids.count('inside'), 1)
        self.assertEqual(ids.count('destination'), 1)
        paragraphs = root.findall('.//f:body//f:p', NS)
        matching = [p for p in paragraphs if 'before linked after' in ''.join(p.itertext())]
        self.assertEqual(len(matching), 1)
        self.assertTrue(any(n.get(HREF) == '#destination' for n in matching[0].iter()))
        self.assertTrue(any(n.get(HREF) == '#inside' for n in root.iter()))

    def test_blank_fixed_line_preserves_code_spacing(self):
        body = ('<div class="ProgramCode"><div class="LineGroup">'
                '<div class="FixedLine">first</div><div class="FixedLine">&#160;</div>'
                '<div class="FixedLine"><span id="blank-anchor"/></div>'
                '<div class="FixedLine">second</div></div></div><p><a href="#blank-anchor">anchor</a></p>')
        root = self.convert(body, navigation=False)
        body_node = root.find('f:body', NS)
        sequence = [(n.tag.rsplit('}', 1)[-1], ''.join(n.itertext()).strip())
                    for n in body_node.iter() if n.tag.rsplit('}', 1)[-1] in ('p', 'empty-line')]
        first = sequence.index(('p', 'first'))
        second = sequence.index(('p', 'second'))
        self.assertTrue(any(tag == 'empty-line' or (tag == 'p' and not text) for tag, text in sequence[first + 1:second]))
        self.assertGreaterEqual(sum(tag == 'empty-line' for tag, _ in sequence[first + 1:second]), 2)
        self.assertTrue(any(n.get('id') == 'blank-anchor' for n in root.iter()))

    def test_fixed_line_image_is_not_mistaken_for_blank_line(self):
        body = ('<div class="ProgramCode"><div class="FixedLine">before</div>'
                '<div class="FixedLine"><img src="../images/marker.png" alt=""/></div>'
                '<div class="FixedLine">after</div></div>')
        root = self.convert(body, extra={'images/marker.png': ('marker', 'image/png', PNG)}, navigation=False)
        images = root.findall('.//f:body//f:image', NS)
        self.assertEqual(len(images), 1, self.diagnostics)
        body_node = root.find('f:body', NS)
        sequence = [('image' if n.tag.rsplit('}', 1)[-1] == 'image' else ''.join(n.itertext()).strip())
                    for n in body_node.iter() if n.tag.rsplit('}', 1)[-1] in ('p', 'image')]
        self.assertLess(sequence.index('before'), sequence.index('image'))
        self.assertLess(sequence.index('image'), sequence.index('after'))

    def test_preformatted_lines_inside_blockquote_survive(self):
        root = self.convert('<blockquote><p>Before code.</p><pre id="example">'
                            'first line\n  second line</pre><p>After code.</p></blockquote>', navigation=False)
        lines = [''.join(node.itertext()) for node in root.findall('.//f:code', NS)]
        self.assertEqual([line.replace('\u00a0', ' ') for line in lines], ['first line', '  second line'])
        self.assertTrue(any(node.get('id') == 'example' for node in root.iter()))
        self.assert_once(root, 'Before code.', 'After code.')

    def test_preformatted_inline_images_preserve_line_order(self):
        for wrapper in ('', 'blockquote'):
            with self.subTest(wrapper=wrapper):
                source = ('<pre id="example">first <img src="../images/callout.png"/> end\n'
                          '<img src="../images/callout.png"/>second\n'
                          '<img src="../images/callout.png"/></pre>')
                if wrapper:
                    source = '<blockquote>' + source + '</blockquote>'
                root = self.convert(source, extra={'images/callout.png': ('callout', 'image/png', PNG)}, navigation=False)
                codes = root.findall('.//f:code', NS)
                self.assertEqual(len(codes), 3)
                self.assertEqual([''.join(code.itertext()).replace('\u00a0', ' ') for code in codes],
                                 ['first  end', 'second', ''])
                self.assertEqual([len(code.findall('f:image', NS)) for code in codes], [1, 1, 1])
                self.assertEqual(codes[0].find('f:image', NS).tail, ' end')
                self.assertEqual(codes[1].find('f:image', NS).tail, 'second')
                self.assertIn(PNG, {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)})

    def test_preformatted_image_offsets_after_encoding_repair(self):
        damaged = 'Привет'.encode('utf8').decode('cp1251')
        root = self.convert('<pre>' + damaged + '<img src="../images/callout.png"/>tail\n'
                            '<span id="second">' + damaged + '</span><img src="../images/callout.png"/>end</pre>',
                            extra={'images/callout.png': ('callout', 'image/png', PNG)}, navigation=False)
        codes = root.findall('.//f:code', NS)
        self.assertEqual(len(codes), 2)
        for code, suffix in zip(codes, ('tail', 'end')):
            self.assertEqual(code.text, 'Привет')
            image = code.find('f:image', NS)
            self.assertIsNotNone(image)
            self.assertEqual(image.tail, suffix)
        self.assertTrue(any(node.get('id') == 'second' for node in root.iter()))

    def test_blockquote_remains_when_poem_import_is_disabled(self):
        root = self.convert('<p>Before</p><blockquote><p>Quotation survives</p></blockquote><p>After</p>',
                            navigation=False, options=['--no-poems'])
        self.assert_once(root, 'Before', 'Quotation survives', 'After')

    def test_nested_list_link_labels_do_not_merge(self):
        root = self.convert('<ul><li>Entries<ul><li><a href="#a">82–83</a></li>'
                            '<li><a href="#b">105–106</a></li></ul></li></ul>'
                            '<p id="a">First target</p><p id="b">Second target</p>', navigation=False)
        paragraphs = [''.join(node.itertext()) for node in root.findall('.//f:p', NS)]
        labels = [next(i for i, text in enumerate(paragraphs) if value in text)
                  for value in ('Entries', '82–83', '105–106')]
        self.assertEqual(labels, sorted(labels))
        self.assertEqual(len(set(labels)), 3)
        links = [a.get(HREF) for a in root.findall('.//f:a', NS)]
        self.assertIn('#a', links)
        self.assertIn('#b', links)

    def test_duplicate_word_note_backlink_does_not_move_main_paragraph(self):
        root = self.convert('<div><div id="_ednref1">Before<a href="#_edn1"><sup>1</sup></a> after.</div>'
                            '<div id="_ednref1"><br/></div><h2>Footnotes</h2>'
                            '<div id="_edn1"><p><a href="#_ednref1"><sup>1</sup></a> Note text.</p></div></div>',
                            navigation=False)
        main = next(b for b in root.findall('f:body', NS) if b.get('name') != 'notes')
        paragraph = next(p for p in main.findall('.//f:p', NS) if 'Before' in ''.join(p.itertext()))
        self.assertEqual(''.join(paragraph.itertext()), 'Before1 after.')
        self.assertEqual(len(self.notes(root)), 1)
        self.assertIn('Note text.', self.text(self.notes(root)[0]))
        self.assertEqual(len(main.findall('.//f:a[@type="note"]', NS)), 1)

        chapter = ('<div><div id="_ednref1">Before<a href="#_edn1"><sup>1</sup></a> after.</div>'
                   '<div id="_ednref1"><br/></div><h2>Footnotes</h2>'
                   '<div id="_edn1"><p><a href="#_ednref1"><sup>1</sup></a> Current note.</p></div></div>')
        previous = ('<h1>Previous</h1><p>Earlier text.</p><div id="_edn1"><p>'
                    '<a href="chapter.xhtml#_ednref1"><sup>1</sup></a> Earlier note.</p></div>')
        root = self.convert(chapter, extra={'Text/previous.xhtml': ('previous', 'application/xhtml+xml', previous)},
                            spine=['previous', 'chapter'], navigation=False)
        main = next(b for b in root.findall('f:body', NS) if b.get('name') != 'notes')
        paragraph = next(p for p in main.findall('.//f:p', NS) if 'Before' in ''.join(p.itertext()))
        self.assertEqual(''.join(paragraph.itertext()), 'Before1 after.')
        self.assertTrue(any('Current note.' in self.text(n) for n in self.notes(root)))

    def test_linked_foot_paragraphs_stay_in_their_notes(self):
        root = self.convert('<p>Text <a href="#en1" id="ref1"><sup>*</sup></a> '
                            '<a href="#en2" id="ref2"><sup>†</sup></a></p>'
                            '<p class="foot"><a href="#ref1" id="en1"><sup>*</sup></a> First heading</p>'
                            '<p class="foot">First body.</p><p class="foot-para">First continuation.</p>'
                            '<p class="foot"><a href="#ref2" id="en2"><sup>†</sup></a> Second heading</p>'
                            '<p class="foot">Second body.</p><p>Following main text.</p>', navigation=False)
        notes = self.notes(root)
        self.assertEqual(len(notes), 2)
        first = next(n for n in notes if n.get('id') == 'en1')
        second = next(n for n in notes if n.get('id') == 'en2')
        self.assertIn('First body.', self.text(first))
        self.assertIn('First continuation.', self.text(first))
        self.assertNotIn('Second body.', self.text(first))
        self.assertIn('Second body.', self.text(second))
        main = next(b for b in root.findall('f:body', NS) if b.get('name') != 'notes')
        self.assertNotIn('First body.', self.text(main))
        self.assertNotIn('Second body.', self.text(main))
        self.assertIn('Following main text.', self.text(main))

    def test_unreferenced_note_callout_stays_in_main_body(self):
        root = self.convert('<p>Before</p><div class="note" id="callout"><p>Note: keep this tip.</p></div>'
                            '<p>After <a epub:type="noteref" href="#real">1</a></p>'
                            '<aside epub:type="footnote" id="real"><p>Real footnote.</p></aside>', navigation=False)
        main = next(b for b in root.findall('f:body', NS) if b.get('name') != 'notes')
        self.assertIn('Note: keep this tip.', self.text(main))
        note, = self.notes(root)
        self.assertIn('Real footnote.', self.text(note))
        self.assertNotIn('keep this tip', self.text(note))

    def test_unlinked_dictionary_note_stays_after_entry(self):
        root = self.convert('<p>First entry.</p><div class="notegroup">'
                            '<p class="note" id="usage">Usage information.</p></div>'
                            '<p>Second entry.</p>', navigation=False)
        main = next(b for b in root.findall('f:body', NS) if b.get('name') != 'notes')
        text = self.text(main)
        self.assertLess(text.index('First entry.'), text.index('Usage information.'))
        self.assertLess(text.index('Usage information.'), text.index('Second entry.'))
        self.assertEqual(self.notes(root), [])

    def test_index_pagebreak_inside_note_is_not_a_note_reference(self):
        chapter = ('<p>Text <a href="notes.xhtml#note9"><sup>9</sup></a>.</p>'
                   '<p>Index page <a href="notes.xhtml#pg255">255</a>.</p>')
        notes = ('<section epub:type="endnotes"><p><span epub:type="pagebreak" id="pg255" '
                 'title="255"/><a id="note9" href="chapter.xhtml#ref9">9</a> Actual note.</p></section>')
        root = self.convert(chapter, extra={'Text/notes.xhtml': ('notes', 'application/xhtml+xml', notes)},
                            spine=['chapter', 'notes'], navigation=False)
        main = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
        links = main.findall('.//f:a', NS)
        note = next(a for a in links if self.text(a) == '9')
        page = next(a for a in links if self.text(a) == '255')
        self.assertEqual(note.get('type'), 'note')
        self.assertIsNone(page.get('type'))
        self.assertEqual(page.get(HREF), '#pg255')
        self.assertTrue(any(e.get('id') == 'pg255' for e in root.iter()))

        chapter = chapter.replace('#pg255', '#pg_255')
        notes = notes.replace('<span epub:type="pagebreak" id="pg255" title="255"/>', '<a id="pg_255"/>')
        root = self.convert(chapter, extra={'Text/notes.xhtml': ('notes', 'application/xhtml+xml', notes)},
                            spine=['chapter', 'notes'], navigation=False)
        main = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
        page = next(a for a in main.findall('.//f:a', NS) if self.text(a) == '255')
        self.assertIsNone(page.get('type'))
        self.assertEqual(page.get(HREF), '#pg_255')

    def test_toc_entries_are_not_promoted_to_notes(self):
        chapter = '<h1>Chapter</h1><p><a epub:type="noteref" href="toc.xhtml#toc1">1</a> Reference.</p>'
        toc = ('<nav epub:type="toc" role="doc-toc" class="toc"><ol><li id="toc1">'
               '<p>Chapter one</p><ol><li><p>Subsection</p></li></ol></li></ol></nav>')
        root = self.convert(chapter, extra={'Text/toc.xhtml': ('contents', 'application/xhtml+xml', toc)},
                            spine=['chapter', 'contents'], navigation=False)
        main = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
        self.assertIn('Chapter one', self.text(main))
        self.assertIn('Subsection', self.text(main))
        self.assertEqual(self.notes(root), [])

        self.assertTrue(any(a.get(HREF) == '#toc1' for a in root.findall('.//f:a', NS)))

        root = self.convert('<p>Content</p><div epub:type="footnotes">'
                            '<nav epub:type="toc" role="doc-toc"><ol><li id="nav-item">'
                            '<p>Visible navigation</p></li></ol></nav></div>', navigation=False)
        self.assertIn('Visible navigation', self.text(next(b for b in root.findall('f:body', NS) if b.get('name') != 'notes')))
        self.assertEqual(self.notes(root), [])

        root = self.convert('<nav epub:type="toc" role="doc-toc"><ol><li id="outer"><p>Chapter</p>'
                            '<ol><li class="footnote" id="toc-footnote"><p>Footnotes</p></li></ol>'
                            '</li></ol></nav><p>Text</p>', navigation=False)
        main = next(b for b in root.findall('f:body', NS) if b.get('name') != 'notes')
        self.assertIn('Chapter', self.text(main))
        self.assertIn('Footnotes', self.text(main))
        self.assertEqual(self.notes(root), [])

    def test_answer_class_is_not_a_footnote(self):
        chapter = ('<ol><li><p><a id="q" href="answers.xhtml#a">3.</a> '
                   'Name a promotion method.</p></li></ol>')
        answers = ('<ol><li class="answer" id="a"><p><a href="chapter.xhtml#q">3.</a> '
                   'Answers will vary.</p><p>A newsletter is one option.</p></li></ol>')
        root = self.convert(chapter, extra={'Text/answers.xhtml': ('answers', 'application/xhtml+xml', answers)},
                            spine=['chapter', 'answers'], navigation=False)
        main = next(b for b in root.findall('f:body', NS) if b.get('name') != 'notes')
        self.assertIn('Name a promotion method.', self.text(main))
        self.assertIn('A newsletter is one option.', self.text(main))
        self.assertEqual(self.notes(root), [])

    def test_quiz_answers_with_bidirectional_numbers_stay_in_spine(self):
        chapter = ('<h1>Quiz</h1><ol><li><p><a id="q1" href="answers.xhtml#a1">1.</a> '
                   'What is SQL?</p></li></ol>')
        answers = ('<h1>Answers</h1><ol><li><p><a id="a1" href="chapter.xhtml#q1">1.</a> '
                   'What is the SQL?</p><p>Answer: a query language.</p></li></ol>')
        root = self.convert(chapter, extra={'Text/answers.xhtml': ('answers', 'application/xhtml+xml', answers)},
                            spine=['chapter', 'answers'], navigation=False)
        main = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
        self.assertIn('What is SQL?', self.text(main))
        self.assertIn('What is the SQL?', self.text(main))
        self.assertIn('Answer: a query language.', self.text(main))
        self.assertEqual(self.notes(root), [])

        root = self.convert('<ol><li><p><a id="q" href="answers.xhtml#a">1.</a> '
                            'Use a query.</p><pre>SELECT 1;\nSELECT 2;</pre></li></ol>',
                            extra={'Text/answers.xhtml': ('answers', 'application/xhtml+xml',
                                   '<ol><li><p><a id="a" href="chapter.xhtml#q">1.</a> '
                                   'Using a query.</p><pre>SELECT 1;\nSELECT 2;</pre></li></ol>')},
                            spine=['chapter', 'answers'], navigation=False)
        main = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
        self.assertIn('Use a query.', self.text(main))
        self.assertIn('Using a query.', self.text(main))
        lines = [''.join(p.itertext()) for p in main.findall('.//f:p', NS)]
        self.assertEqual(lines.count('SELECT 1;'), 2)
        self.assertEqual(lines.count('SELECT 2;'), 2)
        self.assertEqual(self.notes(root), [])

    def test_bibliography_references_remain_in_main_body(self):
        chapter = '<p>Chapter citation [<a id="ref1a" href="refs.xhtml#ref1">1</a>].</p>'
        for css_class in ('ref', 'ref1'):
            with self.subTest(css_class=css_class):
                refs = ('<h2>References</h2><p class="' + css_class + '">'
                        '[<a id="ref1" href="chapter.xhtml#ref1a">1</a>] Bibliography entry.</p>')
                root = self.convert(chapter, extra={'Text/refs.xhtml': ('refs', 'application/xhtml+xml', refs)},
                                    spine=['chapter', 'refs'], navigation=False)
                main = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
                self.assertIn('Bibliography entry.', self.text(main))
                self.assert_once(root, 'Chapter citation', 'Bibliography entry.')

    def test_self_linked_inline_marker_does_not_capture_main_paragraph(self):
        root = self.convert('<p>Main paragraph <span class="Footnote-reference">'
                            '<span id="footnote-001-backlink"><a href="#footnote-001-backlink">1</a>'
                            '</span></span> continues here.</p><p class="Body_Body--Footnote---First">'
                            '<a href="#footnote-001-backlink">1</a> Actual note text.</p>', navigation=False)
        main = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
        self.assertIn('Main paragraph', self.text(main))
        self.assertIn('continues here', self.text(main))
        self.assert_once(root, 'Main paragraph', 'Actual note text.')

    def test_unmarked_numeric_note_backlink_does_not_capture_main_paragraph(self):
        for filename in ('volume_notes.xhtml', 'other.xhtml'):
            with self.subTest(filename=filename):
                chapter = ('<p>Main paragraph before <a id="ref1" href="' + filename + '#note1">1</a> and after.</p>'
                           '<p>Next chapter paragraph.</p>')
                notes = ('<section epub:type="backmatter"><p class="notes-hd"><span>'
                         '<a id="note1" href="chapter.xhtml#ref1">1</a></span> Actual note content.</p></section>')
                root = self.convert(chapter, extra={'Text/' + filename: ('notes', 'application/xhtml+xml', notes)},
                                    spine=['chapter', 'notes'], navigation=False)
                main = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
                notes_body = next(body for body in root.findall('f:body', NS) if body.get('name') == 'notes')
                self.assertIn('Main paragraph before', self.text(main))
                self.assertIn('Next chapter paragraph', self.text(main))
                self.assertNotIn('Main paragraph before', self.text(notes_body))
                self.assertIn('Actual note content', self.text(notes_body))

    def test_preformatted_hyperlinks_survive_across_lines(self):
        root = self.convert('<p id="target">Target</p><pre>  See <a href="#target">Problem\n'
                            '3.46</a> &amp; <a href="https://example.com/">external</a>.</pre>',
                            navigation=False)
        code = root.findall('.//f:body//f:p/f:code', NS)
        self.assertEqual([self.text(p) for p in code], ['  See Problem', '3.46 & external.'])
        links = [a for p in code for a in p.findall('f:a', NS)]
        self.assertEqual([(self.text(a), a.get(HREF)) for a in links],
                         [('Problem', '#target'), ('3.46', '#target'), ('external', 'https://example.com/')])

        body = ('<p id="target">Target</p><pre><a href="#target">A</a>'
                '<a href="https://example.com/">B</a><img src="../a.png"/>C</pre>')
        root = self.convert(body, extra={'a.png': ('a', 'image/png', PNG)}, navigation=False)
        code, = root.findall('.//f:body//f:p/f:code', NS)
        self.assertEqual([(self.text(a), a.get(HREF)) for a in code.findall('f:a', NS)],
                         [('A', '#target'), ('B', 'https://example.com/')])
        self.assertEqual([e.tag.rsplit('}', 1)[-1] for e in code], ['a', 'a', 'image'])
        root = self.convert(body, extra={'a.png': ('a', 'image/png', PNG)},
                            options=['--no-links'], navigation=False)
        code, = root.findall('.//f:body//f:p/f:code', NS)
        self.assertEqual(code.findall('f:a', NS), [])
        self.assertEqual(self.text(code), 'ABC')

    def test_preformatted_nested_anchor_stays_near_its_line(self):
        root = self.convert('<p><a href="#page2">Page link</a></p>'
                            '<pre>First line\n<span epub:type="pagebreak" id="page2"/>Second line\nThird line</pre>',
                            navigation=False)
        paragraphs = root.findall('.//f:p', NS)
        marker = next(index for index, node in enumerate(paragraphs) if node.get('id') == 'page2')
        self.assertLess(marker, len(paragraphs) - 1)
        self.assertIn('Second line', self.text(paragraphs[marker + 1]))
        self.assertTrue(any(link.get(HREF) == '#page2' for link in root.findall('.//f:a', NS)))

    def test_preformatted_multiple_anchors_crlf_and_empty_target(self):
        root = self.convert('<p><a href="#first">A</a><a href="#second">B</a><a href="#empty">C</a></p>'
                            '<pre id="block">\r\nStart\r\n<span id="first"/>Middle\n<span id="second"/>End</pre>'
                            '<pre><span id="empty"/></pre>', navigation=False)
        paragraphs = root.findall('.//f:p', NS)
        ids = {node.get('id'): index for index, node in enumerate(paragraphs) if node.get('id')}
        self.assertTrue({'block', 'first', 'second', 'empty'}.issubset(ids))
        self.assertIn('Middle', self.text(paragraphs[ids['first'] + 1]))
        self.assertIn('End', self.text(paragraphs[ids['second'] + 1]))
        self.assertLess(ids['first'], ids['second'])

    def test_preformatted_br_and_blank_lines_preserve_boundaries(self):
        root = self.convert('<pre>First<br/>Second<br/><br/>Third</pre>', navigation=False)
        code_nodes = root.findall('.//f:p/f:code', NS)
        self.assertEqual([self.text(node) for node in code_nodes], ['First', 'Second', 'Third'])
        self.assertTrue(root.findall('.//f:empty-line', NS))

    def test_zero_width_joiners_survive_text_and_preformatted_blocks(self):
        text = 'می\u200cروم 👩\u200d🔬'
        root = self.convert('<p>' + text + '</p><pre>' + text + '</pre>', navigation=False)
        body = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
        self.assertEqual(self.text(body).count(text), 2)

    def test_disabling_typography_cleanup_preserves_preformatted_nbsp(self):
        text = 'A\u00a0B\u00adC'
        root = self.convert('<p>' + text + '</p><pre>' + text + '</pre>',
                            navigation=False, options=['--no-clean-typography'])
        body = next(body for body in root.findall('f:body', NS) if body.get('name') != 'notes')
        self.assertEqual(self.text(body).count(text), 2)

    def test_sidebar_and_late_epigraph_keep_link_targets(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><a href="#sidebar">Sidebar</a> <a href="#inner">Inner</a> '
                                    '<a href="#quote">Quotation</a></p>'
                                    '<aside id="sidebar"><p>Sidebar <span id="inner">text</span></p></aside>'
                                    '<div class="epigraph" id="quote"><p>Quotation text</p></div>',
                                    version=version, navigation=False)
                ids = {n.get('id') for n in root.iter() if n.get('id')}
                links = [a.get(HREF) for a in root.findall('.//f:a', NS)]
                self.assertTrue({'sidebar', 'inner', 'quote'}.issubset(ids))
                self.assertTrue({'#sidebar', '#inner', '#quote'}.issubset(links))
                self.assertIn('Sidebar text', self.text(root))
                self.assertIn('Quotation text', self.text(root))

    def test_epigraph_only_section_keeps_link_target(self):
        root = self.convert('<p><a href="later.xhtml#quotation">Quotation link</a></p>',
                            {'Text/later.xhtml': ('later', 'application/xhtml+xml',
                             '<div class="epigraph" id="quotation"><p>Quoted text</p></div>')},
                            spine=['chapter', 'later'], navigation=False)
        link = next(a for a in root.findall('.//f:a', NS) if self.text(a) == 'Quotation link')
        self.assertEqual(link.get(HREF), '#quotation')
        self.assertTrue(any(n.get('id') == 'quotation' for n in root.iter()))
        self.assertIn('Quoted text', self.text(root))

    def test_list_item_preserves_nested_paragraph_boundaries(self):
        root = self.convert('<ol><li id="item">First <a id="index"></a>sentence.<p id="note">Note</p>'
                            '<p><em>Explanation.</em></p></li><li>Next</li></ol>'
                            '<p><a href="#note">go</a></p>', navigation=False)
        paragraphs = [''.join(p.itertext()) for p in root.findall('.//f:body//f:p', NS)]
        self.assertIn('1. First sentence.', paragraphs)
        self.assertIn('Note', paragraphs)
        self.assertIn('Explanation.', paragraphs)
        self.assertIn('2. Next', paragraphs)
        self.assertFalse(any('First sentence.Note' in text for text in paragraphs))
        self.assertTrue(any(p.get('id') == 'note' for p in root.findall('.//f:p', NS)))
        self.assertTrue(any(p.get('id') == 'index' for p in root.findall('.//f:p', NS)))

        root = self.convert('<ol><li>Outer<ul><li>Inner:<p>First code</p>'
                            '<p>Second code</p></li></ul></li></ol>', navigation=False)
        paragraphs = [''.join(p.itertext()) for p in root.findall('.//f:body//f:p', NS)]
        self.assertIn('1. Outer', paragraphs)
        self.assertIn('• Inner:', paragraphs)
        self.assertIn('First code', paragraphs)
        self.assertIn('Second code', paragraphs)

    def test_list_item_div_preserves_nested_choices_and_anchors(self):
        root = self.convert('<p><a href="#choice">Choice link</a></p>'
                            '<ol><li><div><p>Question?</p><ol type="a">'
                            '<li><p id="choice">First choice</p></li>'
                            '<li><p>Second choice</p></li></ol></div></li></ol>', navigation=False)
        paragraphs = root.findall('.//f:body//f:p', NS)
        texts = [self.text(p) for p in paragraphs]
        self.assertIn('1. Question?', texts)
        self.assertIn('a. First choice', texts)
        self.assertIn('b. Second choice', texts)
        choice = next(p for p in paragraphs if p.get('id') == 'choice')
        self.assertEqual(self.text(choice), 'a. First choice')
        self.assertLess(texts.index('1. Question?'), texts.index('a. First choice'))
        self.assertLess(texts.index('a. First choice'), texts.index('b. Second choice'))

    def test_list_preserves_non_item_anchors_and_blocks(self):
        root = self.convert('<p><a href="#target">Target</a></p><ol><li>First item</li>'
                            '<a id="target"></a><hr/><div><table><tr><td>Intervening table</td>'
                            '</tr></table></div><li>Last item</li></ol>', navigation=False)
        self.assertTrue(any(n.get('id') == 'target' for n in root.iter()))
        self.assert_once(root, 'First item', 'Intervening table', 'Last item')
        text = self.text(root)
        self.assertLess(text.index('First item'), text.index('Intervening table'))
        self.assertLess(text.index('Intervening table'), text.index('Last item'))

    def test_list_nested_anchors_remain_linkable(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><a href="#list">List</a> <a href="#item">Item</a> '
                                    '<a href="#nested">Nested</a> <a href="#last">Last</a></p>'
                                    '<ol id="list"><li id="item"><div id="nested">Item text</div>'
                                    '<ul><li><p id="last">Nested text</p></li></ul></li></ol>',
                                    version=version, navigation=False)
                ids = {n.get('id') for n in root.iter() if n.get('id')}
                self.assertTrue({'list', 'item', 'nested', 'last'}.issubset(ids))
                self.assertIn('Item text', self.text(root))
                self.assertIn('Nested text', self.text(root))

    def test_whitespace_only_inline_formatting_preserves_word_boundary(self):
        root = self.convert('<p>Before<code> </code>after; first<strong> </strong>second; '
                            'one<em> </em>two.</p>', navigation=False)
        paragraph = next(p for p in root.findall('.//f:p', NS) if 'Before' in self.text(p))
        self.assertIn('Before after; first second; one two.', self.text(paragraph))
        for node in paragraph.iter():
            if node.tag.rsplit('}', 1)[-1] in ('code', 'strong', 'emphasis'):
                self.assertTrue(self.text(node).strip())

    def test_html_entity_fallback_keeps_explicit_void_closing_tags(self):
        root = self.convert('<p>Before&nbsp;table</p><table><colgroup><col></col>'
                            '<col></col></colgroup><tr><td>Table text</td><td>Other cell</td>'
                            '</tr></table><p>After table</p>', navigation=False)
        self.assert_once(root, 'Table text', 'Other cell', 'After table')
        self.assertEqual(len(root.findall('.//f:table/f:tr', NS)), 1)

    def test_css_generated_content_on_inline_elements(self):
        root = self.convert('<table><tr><td><i class="direction">Aside</i> words '
                            '<i class="direction muted">Silent</i></td></tr></table>',
                            head='<style>.direction::before { content: "("; font-style: normal } '
                                 '.direction::after { content: ")"; font-style: normal } '
                                 '.muted::before { content: none !important } '
                                 '.muted::after { content: none !important }</style>', navigation=False)
        cell = root.find('.//f:table/f:tr/f:td', NS)
        self.assertEqual(''.join(cell.itertext()), '(Aside) words Silent')
        self.assertEqual([e.text for e in cell.findall('f:emphasis', NS)], ['Aside', 'Silent'])

        root = self.convert('<p><span class="a">One</span> <span class="b">Two</span> '
                            '<span class="c">Three</span></p>',
                            head='<style>.a::before { content: "old" } '
                                 'span.a::before { content: "[;]" !important } '
                                 '.a::before { content: "wrong" } '
                                 '.b::before { content: "hidden"; display: none } '
                                 '.c::before { content: "wrong" } '
                                 '.c::before { content: attr(title) !important }</style>', navigation=False)
        self.assertIn('[;]One Two Three', self.text(root))
        self.assertIn('Unsupported CSS generated content', self.diagnostics)

        root = self.convert('<p><span class="quoted">word</span></p>',
                            head=r'<style>.quoted::before {content: "\201C"} '
                                 r'.quoted::after {content: "\201D\20 \1F600"}</style>', navigation=False)
        self.assertIn('“word” 😀', self.text(root))
        root = self.convert('<p><span class="quoted">word</span></p>',
                            head=r'<style>.quoted::before {content: "\"\\\0 \1 "}</style>', navigation=False)
        self.assertIn('"\\��word', self.text(root))
        self.assertIn('Invalid CSS generated character was replaced', self.diagnostics)

        root = self.convert('<p>Before<span class="marker"></span>after '
                            '<span class="marker" style="display:none"></span> end.</p>',
                            head='<style>.marker::before {content: "★"}</style>', navigation=False)
        self.assertIn('Before★after end.', ' '.join(self.text(root).split()))

        root = self.convert('<p><a class="ref" href="https://example.com/">link</a></p>',
                            head='<style>.ref::before {content: "["} .ref::after {content: "]"}</style>',
                            navigation=False)
        link = root.find('.//f:a', NS)
        self.assertEqual(''.join(link.itertext()), '[link]')

    def test_table_cell_keeps_inline_text_and_block_boundaries(self):
        root = self.convert('<table><tr><td><strong>if</strong>\u200b err := run();</td>'
                            '<td>one <em>two</em> three</td><td><p>first</p><p>second</p></td>'
                            '<td>before<br/>after</td></tr></table>', navigation=False)
        cells = root.findall('.//f:table/f:tr/f:td', NS)
        self.assertEqual([''.join(cell.itertext()) for cell in cells],
                         ['if err := run();', 'one two three', 'first; second', 'before; after'])
        self.assertIsNotNone(cells[0].find('f:strong', NS))
        self.assertIsNotNone(cells[1].find('f:emphasis', NS))

    def test_table_row_and_cell_anchors_remain_linkable(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><a href="#table">Table</a> <a href="#row">Row</a> '
                                    '<a href="#cell">Cell</a> <a href="#inner">Inner</a></p>'
                                    '<table id="table"><tr id="row"><td id="cell" rowspan="2">'
                                    '<span id="inner">Alpha</span></td><td>Beta</td></tr>'
                                    '<tr><td>Gamma</td></tr></table>', version=version, navigation=False)
                ids = {n.get('id') for n in root.iter() if n.get('id')}
                self.assertTrue({'table', 'row', 'cell', 'inner'}.issubset(ids))
                self.assertEqual(len(root.findall('.//f:table/f:tr', NS)), 2)
                self.assertTrue(any(n.get('rowspan') == '2' for n in root.findall('.//f:td', NS)))
                self.assertEqual(self.text(next(n for n in root.findall('.//f:td', NS) if n.get('id') == 'row')), 'Beta')
                self.assertEqual(self.text(next(n for n in root.findall('.//f:td', NS) if n.get('id') == 'cell')), 'Alpha')
                self.assertIn('Alpha', self.text(root))
                self.assertIn('Gamma', self.text(root))

    def test_definition_list_nested_anchors_remain_linkable(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><a href="#defs">Definitions</a> <a href="#term">Term</a> '
                                    '<a href="#definition">Definition</a> <a href="#nested">Nested</a></p>'
                                    '<dl id="defs"><dt id="term">Term</dt><dd><p id="definition">'
                                    'Definition text</p><dl><dt id="nested">Nested term</dt>'
                                    '<dd>Nested explanation</dd></dl></dd></dl>', version=version, navigation=False)
                ids = {n.get('id') for n in root.iter() if n.get('id')}
                self.assertTrue({'defs', 'term', 'definition', 'nested'}.issubset(ids))
                for text in ('Term', 'Definition text', 'Nested term', 'Nested explanation'):
                    self.assertIn(text, self.text(root))

    def test_citation_restarts_after_attribution_preserving_order(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><a href="#quotation">Read quotation</a></p>'
                                    '<blockquote id="quotation"><p>First quotation</p>'
                                    '<p class="text-author">First author</p><p>Second quotation</p>'
                                    '<p class="text-author">Second author</p></blockquote>',
                                    version=version, navigation=False)
                cites = root.findall('.//f:cite', NS)
                self.assertEqual(len(cites), 2)
                self.assertEqual([self.text(n).strip() for n in cites[0].findall('f:text-author', NS)], ['First author'])
                self.assertEqual([self.text(n).strip() for n in cites[1].findall('f:text-author', NS)], ['Second author'])
                text = self.text(root)
                positions = [text.index(x) for x in ('First quotation', 'First author', 'Second quotation', 'Second author')]
                self.assertEqual(positions, sorted(positions))
                self.assertTrue(any(n.get('id') == 'quotation' for n in cites))

    def test_content_filenames_containing_service_words_are_not_skipped(self):
        extra = {
            'Text/navigator.xhtml': ('navchapter', 'application/xhtml+xml', '<p>Navigation story content</p>'),
            'Text/stockroom.xhtml': ('tocchapter', 'application/xhtml+xml', '<p>Stockroom story content</p>'),
            'Text/discovery.xhtml': ('coverchapter', 'application/xhtml+xml', '<p>Discovery story content</p>'),
            'Text/titlepage-diary.xhtml': ('titlechapter', 'application/xhtml+xml', '<p>Diary story content</p>'),
        }
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p>Opening content</p>', extra, version=version,
                                    spine=['chapter', 'navchapter', 'tocchapter', 'coverchapter', 'titlechapter'],
                                    options=['--skip-service-pages', '--remove-service-sections'])
                for phrase in ('Opening content', 'Navigation story content', 'Stockroom story content',
                               'Discovery story content', 'Diary story content'):
                    self.assertEqual(self.text(root).count(phrase), 1)

    def test_copyright_content_is_kept_by_default_and_removable_explicitly(self):
        extra = {'Text/copyright.xhtml': ('credits', 'application/xhtml+xml', '<p>Publication credits text</p>')}
        for version in ('2.0', '3.0'):
            for options, kept in (([], True), (['--remove-service-sections'], False)):
                with self.subTest(version=version, options=options):
                    root = self.convert('<p>Opening content</p>', extra, version=version,
                                        spine=['chapter', 'credits'], options=options)
                    self.assertEqual('Publication credits text' in self.text(root), kept)
                    self.assertIn('Opening content', self.text(root))

    def test_nested_heading_anchors_remain_linkable(self):
        for version in ('2.0', '3.0'):
            for options in ([], ['--no-split-sections']):
                with self.subTest(version=version, options=options):
                    root = self.convert('<p><a href="#first">First</a> <a href="#second">Second</a></p>'
                                        '<div><h2><a id="first"/><span id="second">Nested heading</span></h2>'
                                        '<p>Heading body</p></div>', version=version, options=options,
                                        navigation=False)
                    ids = {n.get('id') for n in root.iter() if n.get('id')}
                    self.assertTrue({'first', 'second'}.issubset(ids))
                    self.assertIn('Nested heading', self.text(root))
                    self.assertIn('Heading body', self.text(root))

    def test_decimal_series_position_preserves_original_metadata(self):
        for position, expected in (('1.0', '1'), ('2.000', '2'), ('2.5', None), ('unknown', None)):
            with self.subTest(position=position):
                metadata = ('<meta name="calibre:series" content="Sample series"/>'
                            f'<meta name="calibre:series_index" content="{position}"/>')
                root = self.convert('<p>Story</p>', version='2.0', metadata_extra=metadata)
                sequence = root.find('f:description/f:title-info/f:sequence', NS)
                self.assertIsNotNone(sequence)
                self.assertEqual(sequence.get('number'), expected)
                extra = root.find('f:description/f:custom-info[@info-type="EPUB series position"]', NS)
                self.assertIsNotNone(extra)
                self.assertEqual(extra.text, position)
                if expected is None:
                    self.assertIn('series position is not an integer', self.diagnostics)

    def test_page_break_anchor_survives_both_page_break_options(self):
        for version in ('2.0', '3.0'):
            for options in (['--pagebreaks'], ['--no-pagebreaks']):
                with self.subTest(version=version, options=options):
                    root = self.convert('<p><a href="#page">Page reference</a> '
                                        '<a href="#page2">Next page</a></p>'
                                        '<div><hr id="page"/><span epub:type="pagebreak" id="page2"/>'
                                        '<p>Following text</p></div>',
                                        version=version, options=options, navigation=False)
                    self.assertTrue(any(n.get('id') == 'page' for n in root.iter()))
                    self.assertTrue(any(n.get('id') == 'page2' for n in root.iter()))
                    self.assertIn('Following text', self.text(root))

    def test_unknown_underscore_genre_is_preserved_as_subject(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p>Story</p>', version=version,
                                    metadata_extra='<dc:subject>sf_action</dc:subject>'
                                                   '<dc:subject>made_up_genre</dc:subject>')
                genres = [n.text for n in root.findall('f:description/f:title-info/f:genre', NS)]
                self.assertIn('sf_action', genres)
                self.assertNotIn('made_up_genre', genres)
                subjects = root.find('f:description/f:custom-info[@info-type="EPUB subjects"]', NS)
                self.assertIn('made_up_genre', subjects.text)
                self.assertIn('Unrecognized FB2-like genre', self.diagnostics)

    def test_nested_book_cover_is_not_discarded_as_main_cover(self):
        extra = {'Text/cover.xhtml': ('front-page', 'application/xhtml+xml',
                 '<img src="../images/cover.png"/>'),
                 'Text/sub/cover.xhtml': ('nested-page', 'application/xhtml+xml',
                 '<img src="../../images/nested.jpg"/>'),
                 'images/cover.png': ('cover-image', 'image/png', PNG),
                 'images/nested.jpg': ('nested-image', 'image/jpeg', JPEG)}
        root = self.convert('<p>Following chapter</p>', extra,
                            spine=['front-page', 'nested-page', 'chapter'], cover_id='cover-image',
                            options=['--skip-service-pages'], navigation=False)
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertIn(JPEG, {binaries[n.get(HREF)[1:]] for n in root.findall('.//f:body//f:image', NS)})
        self.assertEqual(binaries[root.find('.//f:coverpage/f:image', NS).get(HREF)[1:]], PNG)

    def test_no_cover_still_keeps_nested_book_cover(self):
        extra = {'Text/cover.xhtml': ('front-page', 'application/xhtml+xml', '<img src="../images/cover.png"/>'),
                 'Text/sub/cover.xhtml': ('nested-page', 'application/xhtml+xml', '<img src="../../images/nested.jpg"/>'),
                 'images/cover.png': ('cover-image', 'image/png', PNG),
                 'images/nested.jpg': ('nested-image', 'image/jpeg', JPEG)}
        root = self.convert('<p>Following chapter</p>', extra, spine=['front-page', 'nested-page', 'chapter'],
                            cover_id='cover-image', options=['--skip-service-pages', '--no-cover'], navigation=False)
        binaries = {base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertNotIn(PNG, binaries)
        self.assertIn(JPEG, binaries)
        self.assertIsNone(root.find('.//f:coverpage', NS))

    def test_cover_page_with_multiple_images_keeps_all_images(self):
        extra = {'Text/cover.xhtml': ('cover-page', 'application/xhtml+xml',
                 '<div><img src="../images/cover.png"/></div><div><img src="../images/extra.jpg"/></div>'),
                 'images/cover.png': ('cover-image', 'image/png', PNG),
                 'images/extra.jpg': ('extra-image', 'image/jpeg', JPEG)}
        root = self.convert('<p>Following chapter</p>', extra, spine=['cover-page', 'chapter'],
                            cover_id='cover-page', options=['--skip-service-pages'], navigation=False)
        binaries = {n.get('id'): base64.b64decode(n.text or '') for n in root.findall('f:binary', NS)}
        self.assertTrue({PNG, JPEG}.issubset(set(binaries.values())))
        self.assertIn(JPEG, {binaries[n.get(HREF)[1:]] for n in root.findall('.//f:body//f:image', NS)})

    def test_cover_labelled_xhtml_with_text_is_not_discarded(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                extra = {'Text/cover-story.xhtml': ('cover-page', 'application/xhtml+xml',
                         '<p>Cover page text</p><p>Unique opening passage</p>')}
                root = self.convert('<p>Following chapter</p>', extra, version=version,
                                    spine=['cover-page', 'chapter'], cover_id='cover-page',
                                    options=['--skip-service-pages'], navigation=False)
                for phrase in ('Cover page text', 'Unique opening passage', 'Following chapter'):
                    self.assertEqual(self.text(root).count(phrase), 1)
                self.assertIn('Cover-labelled XHTML contains text', self.diagnostics)

    def test_empty_note_candidate_does_not_consume_heading_anchor(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><sup><a href="#alias">1</a></sup> '
                                    '<a href="#alias">Jump to heading</a></p>'
                                    '<h1 id="heading"><a id="alias"/>Heading text</h1>'
                                    '<p>Body text</p>', version=version, navigation=False)
                ids = {n.get('id') for n in root.iter() if n.get('id')}
                self.assertTrue({'heading', 'alias'}.issubset(ids))
                links = [a.get(HREF) for a in root.findall('.//f:a', NS)]
                self.assertIn('#alias', links)
                self.assertIn('Heading text', self.text(root))
                self.assertIn('Body text', self.text(root))

    def test_ordinary_link_to_collected_note_alias_resolves_to_note(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                extra = {'notes.xhtml': ('notes', 'application/xhtml+xml',
                         '<ol><li id="outer"><a id="inner"/>Note text</li></ol>')}
                root = self.convert('<p><sup><a href="../notes.xhtml#inner">1</a></sup> '
                                    '<a href="../notes.xhtml#inner">Read note</a></p>',
                                    extra, version=version, navigation=False)
                note, = self.notes(root)
                link = next(a for a in root.findall('.//f:a', NS) if self.text(a) == 'Read note')
                self.assertEqual(link.get(HREF), '#' + note.get('id'))
                self.assertNotEqual(link.get('type'), 'note')
                self.assertIn('Note text', self.text(note))

    def test_poem_retains_nested_page_anchor(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><a href="#page">Page reference</a></p>'
                                    '<p class="poem" id="verse"><span epub:type="pagebreak" id="page"/>'
                                    'Poem text</p>', version=version, navigation=False)
                ids = {n.get('id') for n in root.iter() if n.get('id')}
                self.assertTrue({'verse', 'page'}.issubset(ids))
                self.assertIn('Poem text', self.text(root))

    def test_unmarked_anchor_inside_list_note_remains_linkable(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                extra = {'notes.xhtml': ('notes', 'application/xhtml+xml',
                         '<ol><li id="note" epub:type="footnote"><a id="alias"/>Note text</li></ol>')}
                root = self.convert('<p><a epub:type="noteref" href="../notes.xhtml#note">1</a> '
                                    '<a href="../notes.xhtml#alias">Read detail</a></p>', extra,
                                    version=version, navigation=False)
                note, = self.notes(root)
                link = next(a for a in root.findall('.//f:a', NS) if self.text(a) == 'Read detail')
                self.assertTrue(any('#' + (n.get('id') or '') == link.get(HREF) for n in note.iter()))
                self.assertIn('Note text', self.text(note))

    def test_document_link_to_chapter_with_preface_before_heading(self):
        for version in ('2.0', '3.0'):
            for heading in ('<h1>Heading text</h1>', '<h1 id="heading">Heading text</h1>'):
                with self.subTest(version=version, heading=heading):
                    links = '<a href="later.xhtml">Whole chapter</a>'
                    if 'id=' in heading:
                        links += ' <a href="later.xhtml#heading">Heading link</a>'
                    root = self.convert('<p>' + links + '</p>',
                                        {'Text/later.xhtml': ('later', 'application/xhtml+xml',
                                         '<div><p>Chapter preface</p></div>' + heading + '<p>Chapter text</p>')},
                                        version=version, spine=['chapter', 'later'], navigation=False)
                    text = self.text(root)
                    self.assertLess(text.index('Chapter preface'), text.index('Heading text'))
                    self.assertLess(text.index('Heading text'), text.index('Chapter text'))

    def test_semantic_subheading_keeps_nested_anchor(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><a href="#sub-anchor">Subheading link</a></p>'
                                    '<div><h2 class="subheading"><a id="sub-anchor"/>'
                                    'Subheading text</h2><p>Following text</p></div>',
                                    version=version, navigation=False)
                self.assertTrue(any(n.get('id') == 'sub-anchor' for n in root.iter()))
                self.assertIn('Subheading text', self.text(root))
                self.assertIn('Following text', self.text(root))

    def test_empty_quote_preserves_anchor_target(self):
        for version in ('2.0', '3.0'):
            with self.subTest(version=version):
                root = self.convert('<p><a href="#empty-quote">Quote link</a></p>'
                                    '<blockquote><p id="empty-quote"><span/></p></blockquote>'
                                    '<p>Following text</p>', version=version, navigation=False)
                self.assertTrue(any(n.get('id') == 'empty-quote' for n in root.iter()))
                self.assertIn('Following text', self.text(root))

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
