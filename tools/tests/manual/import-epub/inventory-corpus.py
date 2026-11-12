"""Inventory an EPUB tree without changing its contents.

Metadata and content-derived flags are diagnostic, not validation results.
The output database must be outside the scanned tree. Historical results are
matched by content hash, never by pathname alone.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import posixpath
import sqlite3
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET
import zipfile


def local(tag):
    return tag.rsplit('}', 1)[-1].lower()


def hash_file(path):
    sha = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            sha.update(chunk)
    return sha.hexdigest()


def metadata(path):
    result = {'version': None, 'title': [], 'creator': [], 'language': [],
              'spine': None, 'images': None, 'features': [], 'inspection_error': None}
    try:
        with path.open('rb') as stream:
            signature = stream.read(6)
        if signature == b'7z\xbc\xaf\x27\x1c':
            result['features'] = ['7z-not-epub']
            result['inspection_error'] = 'UNSUPPORTED: 7z container, not EPUB ZIP'
            return result
        with zipfile.ZipFile(path) as archive:
            def read(name, limit=16 * 1024 * 1024):
                info = archive.getinfo(name)
                if info.file_size > limit:
                    raise ValueError('resource too large for inventory')
                return archive.read(info)
            container = ET.fromstring(read('META-INF/container.xml'))
            rootfile = next(x for x in container.iter() if local(x.tag) == 'rootfile')
            package_path = rootfile.attrib['full-path']
            package = ET.fromstring(read(package_path))
            result['version'] = package.get('version')
            for element in package.iter():
                kind = local(element.tag)
                if kind in ('title', 'creator', 'language') and element.text:
                    result[kind].append(element.text.strip())
            items = [x for x in package.iter() if local(x.tag) == 'item']
            result['spine'] = sum(local(x.tag) == 'itemref' for x in package.iter())
            result['images'] = sum(x.get('media-type', '').startswith('image/') for x in items)
            features = set()
            if 'META-INF/encryption.xml' in archive.namelist():
                features.add('encryption-metadata')  # May also be font obfuscation.
            for item in items:
                media = item.get('media-type', '').lower()
                href = item.get('href', '')
                if media == 'image/svg+xml': features.add('svg')
                if media == 'text/css': features.add('css')
                if 'nav' in item.get('properties', '').split(): features.add('nav')
                if media not in ('application/xhtml+xml', 'text/html'):
                    continue
                name = posixpath.normpath(posixpath.join(posixpath.dirname(package_path), unquote(urlsplit(href).path)))
                if name.startswith('../') or name.startswith('/'):
                    features.add('invalid-resource-path')
                    continue
                try:
                    raw = read(name, 4 * 1024 * 1024).lower()
                except (KeyError, ValueError, RuntimeError):
                    features.add('xhtml-not-inspected')
                    continue
                for marker, flag in ((b'<math', 'mathml'), (b'<svg', 'inline-svg'),
                                     (b'<table', 'table'), (b'<ul', 'list'),
                                     (b'<ol', 'list'), (b'footnote', 'note-marker'),
                                     (b'endnote', 'note-marker'), (b'<style', 'inline-css')):
                    if marker in raw: features.add(flag)
            result['features'] = sorted(features)
    except (OSError, zipfile.BadZipFile, ET.ParseError, KeyError, StopIteration, ValueError, RuntimeError, NotImplementedError) as error:
        result['inspection_error'] = type(error).__name__ + ': ' + str(error)[:300]
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--historical-index', type=Path)
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    output = args.output.resolve()
    if output == root or root in output.parents:
        parser.error('output must be outside the source tree')
    if output.exists():
        parser.error('output already exists; choose a new run identifier')
    historical = {}
    if args.historical_index:
        with args.historical_index.open(encoding='utf-8-sig', newline='') as stream:
            for row in csv.DictReader(stream):
                historical.setdefault(row.get('source_sha256', '').lower(), []).append(row)
    output.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(output)
    connection.execute('CREATE TABLE books (relative_path TEXT PRIMARY KEY, size INTEGER, sha256 TEXT, metadata_json TEXT, historical_json TEXT, error TEXT)')
    connection.execute('CREATE TABLE scan_errors (path TEXT, error TEXT)')
    connection.execute('CREATE INDEX books_sha256 ON books(sha256)')
    count = 0
    try:
        def scan_error(error):
            connection.execute('INSERT INTO scan_errors VALUES (?,?)', (str(error.filename), str(error)))
        for current, dirs, files in os.walk(root, followlinks=False, onerror=scan_error):
            dirs[:] = sorted(d for d in dirs if not (Path(current) / d).is_symlink()
                             and not getattr(Path(current) / d, 'is_junction', lambda: False)())
            for name in sorted(files):
                path = Path(current) / name
                if path.suffix.lower() != '.epub' or path.is_symlink():
                    continue
                rel = str(path.relative_to(root))
                try:
                    size = path.stat().st_size
                    sha = hash_file(path)
                    info = metadata(path)
                    previous = historical.get(sha, [])
                    error = None
                except OSError as exc:
                    size, sha, info, previous = None, None, {}, []
                    error = type(exc).__name__ + ': ' + str(exc)[:300]
                connection.execute('INSERT INTO books VALUES (?,?,?,?,?,?)',
                                   (rel, size, sha, json.dumps(info, ensure_ascii=False),
                                    json.dumps(previous, ensure_ascii=False), error))
                count += 1
                if count % 50 == 0:
                    connection.commit()
                    print(f'inventoried={count}', flush=True)
        connection.commit()
        print(f'complete={count}', flush=True)
    finally:
        connection.close()


if __name__ == '__main__':
    main()
