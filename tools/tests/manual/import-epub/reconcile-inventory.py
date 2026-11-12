"""Check a completed inventory against a fresh directory enumeration.

The read-only scan records errors and symlinks explicitly. It does not hash
again and does not treat a difference as proof that source contents changed.
"""
import argparse
import json
import os
from pathlib import Path
import sqlite3


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--inventory', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    args = p.parse_args()
    root, report = args.root.resolve(strict=True), args.report.resolve()
    if report == root or root in report.parents or report.exists():
        p.error('report must be new and outside the source tree')
    with sqlite3.connect(f'{args.inventory.resolve().as_uri()}?mode=ro', uri=True) as db:
        indexed = {r[0] for r in db.execute('SELECT relative_path FROM books')}
    seen, skipped_symlinks, errors = set(), [], []
    def onerror(error):
        errors.append({'path': str(error.filename), 'error': str(error)})
    for current, dirs, files in os.walk(root, followlinks=False, onerror=onerror):
        for directory in list(dirs):
            path = Path(current) / directory
            if path.is_symlink() or getattr(path, 'is_junction', lambda: False)():
                skipped_symlinks.append(str(path.relative_to(root)))
                dirs.remove(directory)
        for filename in files:
            path = Path(current) / filename
            if path.suffix.lower() == '.epub':
                if path.is_symlink():
                    skipped_symlinks.append(str(path.relative_to(root)))
                else:
                    seen.add(str(path.relative_to(root)))
    result = {'enumerated': len(seen), 'indexed': len(indexed),
              'missing_from_inventory': sorted(seen - indexed),
              'missing_from_disk': sorted(indexed - seen),
              'skipped_symlinks': skipped_symlinks, 'scan_errors': errors}
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'enumerated={len(seen)} indexed={len(indexed)} new={len(seen-indexed)} missing={len(indexed-seen)} symlinks={len(skipped_symlinks)} errors={len(errors)}')


if __name__ == '__main__':
    main()
