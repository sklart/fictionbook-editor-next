"""Migrate embedded script cards to UTF-8 JSON without changing script code bytes."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import runpy
import tempfile

ROOT = Path(__file__).resolve().parent.parent
TOOL = runpy.run_path(str(ROOT / 'tools' / 'script-card-tool.py'))
RAW_ALGORITHM = 'sha256-raw-js-v1'


def atomic_write(path: Path, data: bytes, *, create_only: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix='.fbe-card-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        if create_only:
            # An atomic no-replace operation protects a card created by a
            # second process after our initial existence check.
            os.link(name, path)
        else:
            os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def migrate(scripts: Path, cards: Path, apply: bool) -> dict:
    scripts = scripts.resolve(strict=True)
    cards = cards.resolve()
    if scripts == cards or scripts in cards.parents or cards in scripts.parents:
        raise ValueError('script and card roots must be separate')
    rows = []
    for path in sorted(scripts.rglob('*.js'), key=lambda p: p.relative_to(scripts).as_posix().casefold()):
        relative = path.relative_to(scripts)
        target = cards / relative.with_suffix('.json')
        row = {'script': relative.as_posix(), 'card': relative.with_suffix('.json').as_posix()}
        try:
            if path.is_symlink() or not path.resolve().is_relative_to(scripts):
                raise ValueError('script resolves outside its root')
            doc = TOOL['read_document'](path.read_bytes())
            if doc.card is None:
                row['status'] = 'already-migrated' if target.is_file() else 'missing'
            else:
                problems = TOOL['validate_schema'](doc.card)
                if problems:
                    raise ValueError('; '.join(problems))
                expected = doc.card['sourceRevision']['sha256']
                digest = hashlib.sha256(doc.code_bytes).hexdigest()
                if expected is not None and expected != digest:
                    raise ValueError('embedded expected digest differs from code bytes')
                card = copy.deepcopy(doc.card)
                card['sourceRevision']['algorithm'] = RAW_ALGORITHM
                payload = (json.dumps(card, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
                if len(payload) > TOOL['MAX_CARD_BYTES']:
                    raise ValueError('external UTF-8 card exceeds 64 KiB')
                if target.exists():
                    # A pre-existing card may have been edited by its user.
                    # Never remove the embedded block unless it is identical.
                    if target.is_symlink() or target.read_bytes() != payload:
                        row['status'] = 'conflict'
                        rows.append(row)
                        continue
                ancestor = target.parent
                while not ancestor.exists() and ancestor != cards:
                    ancestor = ancestor.parent
                if not ancestor.resolve().is_relative_to(cards):
                    raise ValueError('card destination escapes its root')
                if apply:
                    if not target.exists():
                        atomic_write(target, payload, create_only=True)
                    atomic_write(path, doc.code_bytes)
                    if path.read_bytes() != doc.code_bytes or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                        raise ValueError('script byte preservation check failed')
                row.update(status='migrated' if apply else 'ready', sha256=digest)
        except (OSError, UnicodeError, ValueError, KeyError) as error:
            row.update(status='error', error=str(error))
        rows.append(row)
    counts = {}
    for row in rows:
        counts[row['status']] = counts.get(row['status'], 0) + 1
    return {'scripts': len(rows), 'counts': counts, 'applied': apply, 'items': rows}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scripts', type=Path, required=True)
    parser.add_argument('--cards', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    report = migrate(args.scripts, args.cards, args.apply)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'scripts': report['scripts'], 'counts': report['counts']}, ensure_ascii=False))
    return 1 if any(x in report['counts'] for x in ('error', 'conflict')) else 0


if __name__ == '__main__':
    raise SystemExit(main())
