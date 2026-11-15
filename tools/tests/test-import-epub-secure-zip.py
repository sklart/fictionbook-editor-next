"""Exercise the experimental EPUB ZIP reader through the production batch API."""
import argparse
import io
import os
from pathlib import Path
import stat
import struct
import subprocess
import tempfile
import unittest
from urllib.parse import quote
import zipfile
import zlib

parser = argparse.ArgumentParser()
parser.add_argument('--batch', type=Path, required=True)
args, rest = parser.parse_known_args()
BATCH = args.batch.resolve()


class SecureZip(unittest.TestCase):
    def run_case(self, extra=(), damage=False, expect_success=False, legacy_name=False, unicode_extra=False, truncate=False, nul_name=False, local_nul=False, zip64=False, cp1251=False, descriptor=False, zip64_offset=False):
        with tempfile.TemporaryDirectory(prefix='epub-secure-') as directory:
            root = Path(directory)
            temp = root / 'temp'; temp.mkdir()
            epub = root / 'input.epub'; output = root / 'output.fb2'
            chapter = 'глава.xhtml' if cp1251 else ('café.xhtml' if legacy_name else 'Текст/глава.xhtml')
            stored_chapter = 'aaaaa.xhtml' if cp1251 else ('cafX.xhtml' if legacy_name or unicode_extra else chapter)
            class NonSeekable(io.BytesIO):
                def seek(self, *args): raise io.UnsupportedOperation('seek')
            stream = NonSeekable() if descriptor else None
            with zipfile.ZipFile(stream if descriptor else epub, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
                archive.writestr('mimetype', 'application/epub+zip', compress_type=zipfile.ZIP_STORED)
                archive.writestr('META-INF/container.xml', '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
                archive.writestr('OEBPS/book.opf', '<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">fixture</dc:identifier><dc:title>Fixture</dc:title><dc:language>ru</dc:language></metadata><manifest><item id="chapter" href="' + quote(chapter, safe='/') + '" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="chapter"/></spine></package>')
                entry = 'OEBPS/' + stored_chapter
                if unicode_extra:
                    raw_name = entry.encode('ascii'); translated = ('OEBPS/' + chapter).encode('utf-8')
                    payload = b'\x01' + struct.pack('<I', zlib.crc32(raw_name)) + translated
                    entry = zipfile.ZipInfo(entry); entry.extra = struct.pack('<HH', 0x7075, len(payload)) + payload
                archive.writestr(entry, '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Глава</title></head><body><h1>Глава</h1><p>Сохранённый текст.</p></body></html>')
                for name, data in extra:
                    archive.writestr(name, data)
            if descriptor:
                epub.write_bytes(stream.getvalue())
            if cp1251:
                data = epub.read_bytes()
                self.assertEqual(data.count(b'aaaaa.xhtml'), 2)
                epub.write_bytes(data.replace(b'aaaaa.xhtml', 'глава.xhtml'.encode('cp1251')))
            if legacy_name:
                data = epub.read_bytes()
                self.assertEqual(data.count(b'cafX.xhtml'), 2)
                epub.write_bytes(data.replace(b'cafX.xhtml', b'caf\x82.xhtml'))
            if damage:
                data = bytearray(epub.read_bytes())
                with zipfile.ZipFile(epub) as check:
                    info = check.getinfo('OEBPS/' + chapter)
                name_length, extra_length = struct.unpack_from('<HH', data, info.header_offset + 26)
                position = info.header_offset + 30 + name_length + extra_length + info.compress_size // 2
                data[position] ^= 0x80
                epub.write_bytes(data)
            if zip64_offset:
                data = bytearray(epub.read_bytes()); end = len(data) - 22
                directory_size, directory_offset = struct.unpack_from('<II', data, end + 12)
                self.assertEqual(data[directory_offset:directory_offset+4], b'PK\x01\x02')
                name_size, extra_size = struct.unpack_from('<HH', data, directory_offset + 28)
                local_offset = struct.unpack_from('<I', data, directory_offset + 42)[0]
                struct.pack_into('<I', data, directory_offset + 42, 0xffffffff)
                extra = struct.pack('<HHQ', 1, 8, local_offset)
                struct.pack_into('<H', data, directory_offset + 30, extra_size + len(extra))
                insert = directory_offset + 46 + name_size + extra_size
                data[insert:insert] = extra
                struct.pack_into('<I', data, end + len(extra) + 12, directory_size + len(extra))
                epub.write_bytes(data)
            if zip64:
                data = epub.read_bytes()
                end = len(data) - 22
                self.assertEqual(data[end:end + 4], b'PK\x05\x06')
                _, disk, start, disk_count, count, size, offset, comment = struct.unpack_from('<IHHHHIIH', data, end)
                record = struct.pack('<IQHHIIQQQQ', 0x06064b50, 44, 45, 45, disk, start, disk_count, count, size, offset)
                locator = struct.pack('<IIQI', 0x07064b50, 0, end, 1)
                ending = struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, 0xffff, 0xffff, 0xffffffff, 0xffffffff, comment)
                epub.write_bytes(data[:end] + record + locator + ending)
            if truncate:
                epub.write_bytes(epub.read_bytes()[:-22])
            if nul_name:
                data = epub.read_bytes()
                self.assertEqual(data.count(b'badXname.txt'), 2)
                epub.write_bytes(data.replace(b'badXname.txt', b'bad\x00name.txt'))
            if local_nul:
                data = epub.read_bytes()
                self.assertEqual(data.count(b'badXname.txt'), 2)
                epub.write_bytes(data.replace(b'badXname.txt', b'bad\x00name.txt', 1))
            env = {**os.environ, 'TEMP': str(temp), 'TMP': str(temp), 'FBE_NEXT_TEST_MODE': '1'}
            result = subprocess.run([str(BATCH), str(epub), str(output), '--stats'], env=env, capture_output=True, timeout=90)
            self.assertEqual(result.returncode == 0, expect_success, result.stdout.decode(errors='replace') + result.stderr.decode(errors='replace'))
            if expect_success:
                self.assertIn('Сохранённый текст.', output.read_text(encoding='utf-8-sig'))
            else:
                self.assertFalse(output.exists())
            self.assertFalse((temp / 'escape.txt').exists())

    def test_unicode_nested_paths(self): self.run_case(expect_success=True)
    def test_streaming_data_descriptors(self): self.run_case(descriptor=True, expect_success=True)
    def test_zip64_local_offset(self): self.run_case(zip64_offset=True, expect_success=True)
    def test_zip64_directory(self): self.run_case(zip64=True, expect_success=True)
    def test_many_resources(self): self.run_case([('OEBPS/resources/%04d.dat' % i, b'x' * 4096) for i in range(1200)], expect_success=True)
    def test_ambiguous_cp1251_is_not_silently_accepted(self): self.run_case(cp1251=True)
    def test_cp437_filename(self): self.run_case(legacy_name=True, expect_success=True)
    def test_unicode_path_extra_field(self): self.run_case(unicode_extra=True, expect_success=True)
    def test_parent_traversal(self): self.run_case([('../../escape.txt', b'escape')])
    def test_absolute_path(self): self.run_case([('/absolute.txt', b'escape')])
    def test_windows_stream(self): self.run_case([('OEBPS/name.txt:stream', b'escape')])
    def test_reserved_name(self): self.run_case([('OEBPS/CON.txt', b'escape')])
    def test_case_collision(self): self.run_case([('OEBPS/One.txt', b'a'), ('OEBPS/one.txt', b'b')])
    def test_file_directory_conflict(self): self.run_case([('OEBPS/conflict', b'a'), ('OEBPS/conflict/nested.txt', b'b')])
    def test_symlink(self):
        info = zipfile.ZipInfo('OEBPS/link'); info.create_system = 3
        info.external_attr = (stat.S_IFLNK | 0o777) << 16
        self.run_case([(info, b'../../escape.txt')])
    def test_7z_signature_has_distinct_diagnostic(self):
        with tempfile.TemporaryDirectory(prefix='epub-7z-') as directory:
            root = Path(directory); epub = root / 'input.epub'; output = root / 'output.fb2'
            epub.write_bytes(b'7z\xbc\xaf\x27\x1c' + bytes(32))
            result = subprocess.run([str(BATCH), str(epub), str(output)], capture_output=True, timeout=20)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'7z:', result.stdout + result.stderr)
            self.assertFalse(output.exists())
    def test_corrupt_crc(self): self.run_case(damage=True)
    def test_missing_end_record(self): self.run_case(truncate=True)
    def test_embedded_nul_in_name(self): self.run_case([('OEBPS/badXname.txt', b'x')], nul_name=True)
    def test_local_header_nul_in_name(self): self.run_case([('OEBPS/badXname.txt', b'x')], local_nul=True)
    def test_nested_parent_traversal(self): self.run_case([('OEBPS/a/../escape.txt', b'x')])


if __name__ == '__main__': unittest.main(argv=[__file__, *rest])
