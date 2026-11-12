import csv
from contextlib import closing
import hashlib
import importlib.util
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).parent / 'manual/import-epub/inventory-corpus.py'


class InventoryTest(unittest.TestCase):
    def test_shards_cover_remaining_unique_rows_once(self):
        path = SCRIPT.with_name('run-corpus.py')
        spec = importlib.util.spec_from_file_location('epub_corpus_run', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        rows = [(i,) for i in range(103)]
        shards = [module.shard_rows(rows, index, 4) for index in range(4)]
        combined = [row for shard in shards for row in shard]
        self.assertCountEqual(combined, rows)
        self.assertEqual(len(set(combined)), len(rows))

    def test_duplicate_hashes_historical_match_and_features(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / 'source'
            root.mkdir()
            epub = root / 'one.epub'
            with zipfile.ZipFile(epub, 'w') as archive:
                archive.writestr('META-INF/container.xml', '<container><rootfile full-path="OPS/book.opf"/></container>')
                archive.writestr('OPS/book.opf', '<package version="3.0"><metadata><title>Example</title><language>en</language></metadata><manifest><item id="c" media-type="application/xhtml+xml" href="my%20chapter.xhtml"/></manifest><spine><itemref idref="c"/></spine></package>')
                archive.writestr('OPS/my chapter.xhtml', '<html><body><p>Example</p><table/></body></html>')
            (root / 'two.epub').write_bytes(epub.read_bytes())
            (root / 'broken.epub').write_bytes(b'not a ZIP')
            (root / 'seven.epub').write_bytes(b'7z\xbc\xaf\x27\x1c' + b'test')
            digest = hashlib.sha256(epub.read_bytes()).hexdigest()
            history = base / 'history.csv'
            with history.open('w', newline='', encoding='utf-8') as stream:
                writer = csv.DictWriter(stream, fieldnames=['source_sha256', 'xsd'])
                writer.writeheader()
                writer.writerow({'source_sha256': digest, 'xsd': 'PASS'})
            output = base / 'inventory.sqlite3'
            subprocess.run([sys.executable, str(SCRIPT), '--root', str(root), '--output', str(output),
                            '--historical-index', str(history)], check=True, capture_output=True)
            with closing(sqlite3.connect(output)) as db:
                rows = db.execute('SELECT relative_path,sha256,metadata_json,historical_json FROM books ORDER BY relative_path').fetchall()
                self.assertEqual(db.execute('SELECT COUNT(*) FROM scan_errors').fetchone()[0], 0)
            self.assertEqual(len(rows), 4)
            self.assertIn('BadZipFile', json.loads(rows[0][2])['inspection_error'])
            for row in (rows[1], rows[3]):
                self.assertEqual(row[1], digest)
                metadata = json.loads(row[2])
                self.assertEqual(metadata['version'], '3.0')
                self.assertIn('table', metadata['features'])
                self.assertNotIn('xhtml-not-inspected', metadata['features'])
                self.assertEqual(json.loads(row[3])[0]['xsd'], 'PASS')
            self.assertIn('UNSUPPORTED', json.loads(rows[2][2])['inspection_error'])

    def test_refuses_output_inside_source_and_existing_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / 'source'
            root.mkdir()
            for output in (root/'result.db', base/'existing.db'):
                if output.parent == base:
                    output.write_text('keep', encoding='utf-8')
                result = subprocess.run([sys.executable, str(SCRIPT), '--root', str(root), '--output', str(output)], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
            self.assertEqual((base/'existing.db').read_text(encoding='utf-8'), 'keep')

    def test_registry_keeps_duplicate_paths_and_unsupported_separate(self):
        with tempfile.TemporaryDirectory() as temporary:
            base=Path(temporary); root=base/'source'; root.mkdir()
            inventory=base/'inventory.db'; containers=base/'containers.db'
            with closing(sqlite3.connect(inventory)) as db:
                db.execute('CREATE TABLE books (relative_path TEXT, size INTEGER, sha256 TEXT, metadata_json TEXT, historical_json TEXT, error TEXT)')
                source_sha=hashlib.sha256(b'epub').hexdigest()
                db.executemany('INSERT INTO books VALUES (?,?,?,?,?,?)',[
                    ('one.epub',10,source_sha,'{}','[]',None),('copy.epub',10,source_sha,'{}','[]',None),
                    ('wrapped.epub',12,'bbb','{}','[]',None)])
                db.commit()
            with closing(sqlite3.connect(containers)) as db:
                db.execute('CREATE TABLE metadata(key TEXT,value TEXT)')
                db.execute('INSERT INTO metadata VALUES (?,?)',('root',str(root)))
                db.execute('CREATE TABLE containers(sha256 TEXT,status TEXT)')
                db.execute('INSERT INTO containers VALUES (?,?)',('bbb','UNSUPPORTED_7Z'));db.commit()
            run=base/'run';run.mkdir()
            (run/'manifest.json').write_text(json.dumps({key:key for key in ('plugin_sha256','batch_sha256','comparator_sha256','schema_sha256')}),encoding='utf8')
            (run/'result.json').write_text(json.dumps({'source_sha256':source_sha,'status':'IMPORT ERROR','xsd':'NOT RUN','comparison':'NOT RUN',
                                                       'fb2_sha256':hashlib.sha256(b'fb2').hexdigest()}),encoding='utf8')
            retry=base/'retry';retry.mkdir()
            for name,data in [('input.epub',b'epub'),('output.fb2',b'fb2'),('comparison.json',b'{}')]:
                (retry/name).write_bytes(data)
            evidence={'source_sha256':source_sha,'output_sha256':hashlib.sha256(b'fb2').hexdigest(),
                      'comparison_sha256':hashlib.sha256(b'{}').hexdigest(),'plugin_sha256':'plugin_sha256',
                      'comparator_sha256':'comparator_sha256','schema_sha256':'schema_sha256',
                      'status':'REVIEW','xsd':'PASS','comparison':'REVIEW'}
            (retry/'retry.json').write_text(json.dumps(evidence),encoding='utf8')
            xsd=base/'xsd.json'
            xsd.write_text(json.dumps({'input_sha256':evidence['output_sha256'],'schema_sha256':'schema_sha256','status':'PASS'}),encoding='utf8')
            output,csv_output=base/'registry.db',base/'registry.csv'
            script=SCRIPT.with_name('build-registry.py')
            result=subprocess.run([sys.executable,str(script),'--inventory',str(inventory),'--containers',str(containers),
                                   '--run',str(run),'--retry',str(retry/'retry.json'),'--xsd-recheck',str(xsd),'--output',str(output),'--csv',str(csv_output)],capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr.decode('utf8','replace'))
            with closing(sqlite3.connect(output)) as db:
                self.assertEqual(db.execute("SELECT count(*) FROM books WHERE status='IMPORT ERROR' AND retry_path IS NOT NULL AND xsd_recheck='PASS'").fetchone()[0],2)
                self.assertEqual(db.execute("SELECT count(*) FROM books WHERE status='UNSUPPORTED'").fetchone()[0],1)
            self.assertTrue(csv_output.exists())
            other=base/'other-run';other.mkdir()
            (other/'manifest.json').write_text(json.dumps({**{key:key for key in ('plugin_sha256','batch_sha256','comparator_sha256','schema_sha256')},
                                                            'svg_sha256':'different'}),encoding='utf8')
            mismatch=subprocess.run([sys.executable,str(script),'--inventory',str(inventory),'--containers',str(containers),
                                     '--run',str(run),'--run',str(other),'--output',str(base/'mixed.db'),
                                     '--csv',str(base/'mixed.csv')],capture_output=True)
            self.assertNotEqual(mismatch.returncode,0)
            (retry/'output.fb2').write_bytes(b'tampered')
            invalid=subprocess.run([sys.executable,str(script),'--inventory',str(inventory),'--containers',str(containers),
                                    '--run',str(run),'--retry',str(retry/'retry.json'),'--output',str(base/'bad.db'),
                                    '--csv',str(base/'bad.csv')],capture_output=True)
            self.assertNotEqual(invalid.returncode,0)


if __name__ == '__main__':
    unittest.main()
