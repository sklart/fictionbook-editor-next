"""Run a small, isolated sample from an inventory against the real batch tool."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
import time


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--inventory', type=Path, required=True)
    p.add_argument('--batch', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--timeout', type=int, default=120)
    args = p.parse_args()
    root, output, batch = args.root.resolve(strict=True), args.output.resolve(), args.batch.resolve(strict=True)
    if output == root or root in output.parents or output.exists():
        p.error('output must be a new directory outside the source')
    conn = sqlite3.connect(f'{args.inventory.resolve().as_uri()}?mode=ro', uri=True)
    rows = conn.execute('SELECT relative_path,sha256,metadata_json,historical_json FROM books WHERE sha256 IS NOT NULL ORDER BY size').fetchall()
    conn.close()
    selected = []
    for flag in ('table', 'note-marker', 'svg', 'mathml', 'inline-css', None):
        for path, sha, meta, history in rows:
            info = json.loads(meta)
            if sha in {x[1] for x in selected} or info.get('inspection_error'):
                continue
            if flag is None or flag in info.get('features', []):
                selected.append((path, sha, history, flag))
                break
    output.mkdir(parents=True)
    compare_path = Path(__file__).resolve().parents[2] / 'compare-epub-fb2.py'
    spec = importlib.util.spec_from_file_location('epub_compare', compare_path)
    compare = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(compare)
    for index, (relative, expected, history, flag) in enumerate(selected, 1):
        original = (root / relative).resolve(strict=True)
        if root not in original.parents:
            raise ValueError('source path outside corpus')
        case = output / f'{index:03d}-{expected[:12]}'
        case.mkdir()
        copied, fb2 = case / 'input.epub', case / 'output.fb2'
        shutil.copyfile(original, copied)
        if sha256(copied) != expected:
            raise ValueError(f'source changed or copy corrupted: {relative}')
        start = time.monotonic()
        result = {'source_relative_path': relative, 'source_sha256': expected,
                  'selection': flag, 'historical': json.loads(history), 'status': 'NOT RUN'}
        try:
            proc = subprocess.run([str(batch), str(copied), str(fb2), '--stats'],
                                  cwd=case, capture_output=True, timeout=args.timeout)
            (case / 'stdout.txt').write_bytes(proc.stdout)
            (case / 'stderr.txt').write_bytes(proc.stderr)
            result['exit_code'] = proc.returncode
            result['status'] = 'REVIEW' if proc.returncode == 0 and fb2.exists() else 'IMPORT ERROR'
            if fb2.exists():
                result['fb2_size'] = fb2.stat().st_size
                result['fb2_sha256'] = sha256(fb2)
                try:
                    report = compare.compare(copied, fb2)
                    (case / 'comparison.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
                except Exception as exc:
                    result['comparison_error'] = f'{type(exc).__name__}: {exc}'
        except subprocess.TimeoutExpired:
            result['status'] = 'IMPORT ERROR'
            result['error'] = 'timeout'
        result['seconds'] = round(time.monotonic() - start, 3)
        (case / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        print(f'{index}/{len(selected)} {result["status"]} {result["seconds"]}s', flush=True)


if __name__ == '__main__':
    main()
