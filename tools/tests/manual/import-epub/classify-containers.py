"""Classify previously uninspectable EPUB paths by their container signature."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import sqlite3


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--inventory',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--resume',action='store_true')
    parser.add_argument('--workers',type=int,default=8)
    args=parser.parse_args()
    root,out=args.root.resolve(strict=True),args.output.resolve()
    if out==root or root in out.parents or (out.exists() and not args.resume) or args.workers<1 or args.workers>16:
        parser.error('output must be outside source; existing output requires --resume; workers must be 1..16')
    source=sqlite3.connect(f'{args.inventory.resolve().as_uri()}?mode=ro',uri=True)
    out.parent.mkdir(parents=True,exist_ok=True)
    dest=sqlite3.connect(out)
    dest.execute('CREATE TABLE IF NOT EXISTS containers(relative_path TEXT PRIMARY KEY, sha256 TEXT, status TEXT, signature_hex TEXT, error TEXT)')
    dest.execute('CREATE TABLE IF NOT EXISTS metadata(key TEXT PRIMARY KEY, value TEXT NOT NULL)')
    with args.inventory.open('rb') as stream:
        inventory_hash=hashlib.file_digest(stream,'sha256').hexdigest()
    old=dict(dest.execute('SELECT key,value FROM metadata'))
    if old and (old.get('root')!=str(root) or old.get('inventory_sha256')!=inventory_hash):
        raise ValueError('resume inventory or root differs from the original run')
    if not old:
        dest.executemany('INSERT INTO metadata VALUES (?,?)',[('root',str(root)),('inventory_sha256',inventory_hash)])
        dest.commit()
    completed={row[0] for row in dest.execute('SELECT relative_path FROM containers')}
    rows=[row for row in source.execute("SELECT relative_path,sha256 FROM books WHERE json_extract(metadata_json,'$.inspection_error') IS NOT NULL") if row[0] not in completed]
    def classify(row):
        relative,sha=row
        try:
            path=(root/relative).resolve(strict=True)
            if root not in path.parents: raise ValueError('path escapes source')
            with path.open('rb') as stream: sig=stream.read(8)
            status='UNSUPPORTED_7Z' if sig.startswith(b'7z\xbc\xaf\x27\x1c') else 'INVALID_OR_UNSUPPORTED'
            error=None
        except (OSError,ValueError) as exc:
            sig=b'';status='NOT_RUN';error=f'{type(exc).__name__}: {exc}'
        return relative,sha,status,sig.hex(),error
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        for i,record in enumerate(pool.map(classify,rows),1):
            dest.execute('INSERT INTO containers VALUES (?,?,?,?,?)',record)
            if i%500==0:
                dest.commit();print(f'classified={len(completed)+i}',flush=True)
    counts=Counter(dict(dest.execute('SELECT status,count(*) FROM containers GROUP BY status')))
    dest.commit();dest.close();source.close()
    print(json.dumps(counts),flush=True)


if __name__=='__main__':main()
