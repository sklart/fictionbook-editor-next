"""Convert unique inventory entries in disposable copies; never write to source."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

from lxml import etree


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def inspect_fb2(path):
    ids, links, counts = set(), [], {'sections': 0, 'paragraphs': 0, 'images': 0,
                                    'binaries': 0, 'links': 0, 'note_links': 0, 'duplicate_ids': 0}
    href_name = '{http://www.w3.org/1999/xlink}href'
    for _, node in ET.iterparse(path, events=('end',)):
        tag = node.tag.rsplit('}', 1)[-1]
        for name, key in (('section', 'sections'), ('p', 'paragraphs'), ('image', 'images'), ('binary', 'binaries'), ('a', 'links')):
            if tag == name: counts[key] += 1
        if tag == 'a' and node.get('type') == 'note': counts['note_links'] += 1
        ident, href = node.get('id'), node.get(href_name, '')
        if ident:
            counts['duplicate_ids'] += ident in ids
            ids.add(ident)
        if href.startswith('#'):
            links.append(href[1:])
        node.clear()
    counts['unresolved_links'] = sum(ident not in ids for ident in links)
    return counts


def shard_rows(rows, shard_index, shard_count):
    return [row for index, row in enumerate(rows) if index % shard_count == shard_index]


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--inventory', type=Path, required=True)
    p.add_argument('--batch', type=Path, required=True)
    p.add_argument('--schema', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--timeout', type=int, default=180)
    p.add_argument('--limit', type=int, default=0)
    p.add_argument('--max-analysis-mib', type=int, default=64)
    p.add_argument('--source-revision', required=True)
    p.add_argument('--importer-source-sha256', help='Source hash recorded when these binaries were built')
    p.add_argument('--exclude-results', type=Path, action='append', default=[])
    p.add_argument('--comparator', type=Path)
    p.add_argument('--shard-count', type=int, default=1)
    p.add_argument('--shard-index', type=int, default=0)
    args = p.parse_args()
    root, batch, output = args.root.resolve(strict=True), args.batch.resolve(strict=True), args.output.resolve()
    if output == root or root in output.parents or output.exists():
        p.error('output must be a new directory outside the source tree')
    if (args.timeout < 1 or args.limit < 0 or args.max_analysis_mib < 1 or
            args.shard_count < 1 or not 0 <= args.shard_index < args.shard_count):
        p.error('invalid timeout or limit')
    compare_source = (args.comparator or (Path(__file__).resolve().parents[2] / 'compare-epub-fb2.py')).resolve(strict=True)
    db = sqlite3.connect(f'{args.inventory.resolve().as_uri()}?mode=ro', uri=True)
    rows = db.execute('SELECT sha256, MIN(relative_path), COUNT(*) FROM books WHERE sha256 IS NOT NULL GROUP BY sha256 ORDER BY MIN(relative_path)').fetchall()
    db.close()
    excluded = set()
    for previous in args.exclude_results:
        previous = previous.resolve(strict=True)
        prior = json.loads((previous / 'manifest.json').read_text(encoding='utf-8'))
        if (prior['batch_sha256'] != sha256(batch) or
                prior['plugin_sha256'] != sha256(batch.parent / 'Plugins' / 'ImportEPUB.dll') or
                prior['svg_sha256'] != sha256(batch.parent / 'Plugins' / 'ImportEPUBLunaSVG.dll') or
                prior['schema_sha256'] != sha256(args.schema) or
                prior.get('comparator_sha256') != sha256(compare_source)):
            raise ValueError('previous run uses different binaries, schema or comparator')
        for result_file in previous.rglob('result.json'):
            record = json.loads(result_file.read_text(encoding='utf-8'))
            if record.get('status') in ('REVIEW', 'FAIL', 'IMPORT ERROR', 'UNSUPPORTED'):
                excluded.add(record['source_sha256'])
    rows = [row for row in rows if row[0] not in excluded]
    rows = shard_rows(rows, args.shard_index, args.shard_count)
    if args.limit: rows = rows[:args.limit]
    output.mkdir(parents=True)
    schema = etree.XMLSchema(etree.parse(str(args.schema.resolve(strict=True))))
    compare = output / 'compare-epub-fb2.snapshot.py'
    shutil.copyfile(compare_source, compare)
    manifest = {'source_revision': args.source_revision,
                'importer_source_sha256': args.importer_source_sha256 or sha256(Path(__file__).resolve().parents[4] / 'src/import-epub/EpubImport.cpp'),
                'fbe_sha256': sha256(batch.parent / 'FBE.exe'),
                'batch_path': str(batch), 'batch_sha256': sha256(batch),
                'plugin_sha256': sha256(batch.parent / 'Plugins' / 'ImportEPUB.dll'),
                'svg_sha256': sha256(batch.parent / 'Plugins' / 'ImportEPUBLunaSVG.dll'),
                'schema_sha256': sha256(args.schema), 'inventory_sha256': sha256(args.inventory),
                'comparator_sha256': sha256(compare),
                'timeout_seconds': args.timeout, 'max_analysis_mib': args.max_analysis_mib,
                'shard_count': args.shard_count, 'shard_index': args.shard_index,
                'excluded_completed_sha256': len(excluded), 'selected_unique_sources': len(rows)}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    for n, (sha, relative, duplicates) in enumerate(rows, 1):
        if shutil.disk_usage(output).free < 4 * 1024 ** 3:
            print(f'STOP: disk reserve at {n}/{len(rows)}', flush=True)
            return 2
        case = output / sha[:2] / sha
        case.mkdir(parents=True)
        source = (root / relative).resolve(strict=True)
        if root not in source.parents:
            raise ValueError('inventory source escapes corpus')
        copy, target = case / 'input.epub', case / 'output.fb2'
        record = {'source_sha256': sha, 'source_relative_path': relative, 'source_paths': duplicates,
                  'status': 'NOT RUN', 'xsd': 'NOT RUN'}
        started = time.monotonic()
        try:
            with source.open('rb') as original:
                signature = original.read(6)
            if signature == b'7z\xbc\xaf\x27\x1c':
                record['status'] = 'UNSUPPORTED'
                record['error'] = '7z container, not EPUB ZIP; identified by signature without per-file import'
                record['import_attempted'] = False
                record['seconds'] = round(time.monotonic() - started, 3)
                (case / 'result.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
                if n % 10 == 0 or n == len(rows):
                    print(f'processed={n}/{len(rows)} status={record["status"]}', flush=True)
                continue
            shutil.copyfile(source, copy)
            if sha256(copy) != sha:
                raise ValueError('source hash changed since inventory or copy failed')
            proc = subprocess.run([str(batch), str(copy), str(target), '--stats'], cwd=case,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=args.timeout,
                                  creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            (case / 'stdout.txt').write_bytes(proc.stdout)
            (case / 'stderr.txt').write_bytes(proc.stderr)
            record['exit_code'] = proc.returncode
            record['import_attempted'] = True
            record['status'] = 'REVIEW' if proc.returncode == 0 and target.exists() else 'IMPORT ERROR'
            if target.exists():
                record['fb2_size'] = target.stat().st_size
                record['fb2_sha256'] = sha256(target)
                if target.stat().st_size > args.max_analysis_mib * 1024 * 1024:
                    record['xsd'] = 'NOT RUN: size limit'
                    record['comparison'] = 'NOT RUN: size limit'
                else:
                  try:
                    # FB2 binaries are one base64 text node and legitimately
                    # exceed libxml's 10 MiB default text-node limit. The
                    # separate maximum file size bounds this parsing step.
                    parser = etree.XMLParser(resolve_entities=False, no_network=True, huge_tree=True)
                    document = etree.parse(str(target), parser)
                    record['xsd'] = 'PASS' if schema.validate(document) else 'FAIL'
                    if record['xsd'] == 'FAIL': record['xsd_error'] = str(schema.error_log.last_error)[:1000]
                    del document
                    record['fb2'] = inspect_fb2(target)
                    if record['xsd'] == 'FAIL' or record['fb2']['duplicate_ids'] or record['fb2']['unresolved_links']:
                        record['status'] = 'FAIL'
                  except (etree.XMLSyntaxError, ET.ParseError, ValueError) as error:
                    record['xsd'] = 'ERROR'
                    record['xsd_error'] = str(error)[:1000]
                  try:
                    audit = subprocess.run([sys.executable, str(compare), '--epub', str(copy), '--fb2', str(target),
                                            '--report', str(case / 'comparison.json')], capture_output=True,
                                           timeout=args.timeout, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
                    record['comparison'] = 'REVIEW' if audit.returncode == 0 else 'ERROR'
                    if audit.returncode: record['comparison_error'] = audit.stderr.decode('utf-8', 'replace')[-2000:]
                  except subprocess.TimeoutExpired:
                    record['comparison'] = 'TIMEOUT'
        except subprocess.TimeoutExpired:
            record['status'] = 'IMPORT ERROR'
            record['error'] = 'TIMEOUT'
        except (OSError, ValueError) as error:
            record['status'] = 'NOT RUN'
            record['error'] = f'{type(error).__name__}: {error}'
        finally:
            copy.unlink(missing_ok=True)
        record['seconds'] = round(time.monotonic() - started, 3)
        (case / 'result.json').write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding='utf-8')
        if n % 10 == 0 or n == len(rows):
            print(f'processed={n}/{len(rows)} status={record["status"]}', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
