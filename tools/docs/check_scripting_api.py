"""Static documentation coverage for the public IDL and additive dispatcher.

Extracts names from current sources, not a frozen method list. Does not execute
examples, prove signatures/ABI, or replace Windows runtime checks.
"""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

from doc_links import mask_code


def without_comments(text: str) -> str:
    pattern = r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|/\*[\s\S]*?\*/|//[^\r\n]*'
    return re.sub(pattern, lambda m: m[0] if m.group(1) else ' ', text)


def expected_methods(idl: str, helper: str) -> dict[str, int]:
    source = without_comments(idl)
    interface = re.search(r'interface\s+IExternalHelper\s*:\s*IDispatch\s*\{([\s\S]*?)\};', source)
    if not interface:
        raise ValueError('IExternalHelper not found; inspect the changed contract')
    methods = {}
    for number, name in re.findall(
            r'\[[^\]]*\bid\(\s*(\d+)\s*\)[^\]]*\]\s*HRESULT\s+(\w+)\s*\(', interface[1]):
        # propget/propput for inflateBlock intentionally share one public name.
        if name in methods and methods[name] != int(number):
            raise ValueError(f'Duplicate IDL name with different IDs: {name}')
        methods[name] = int(number)
    source = without_comments(helper)
    table = re.search(r'\bScriptingMethods\s*\[\s*\]\s*=\s*\{([\s\S]*?)\};', source)
    if not table:
        raise ValueError('ScriptingMethods[] not found; inspect the changed dispatcher')
    entries = table[1].split(',')
    extensions = []
    for entry in entries:
        if not entry.strip():
            continue
        literal = re.fullmatch(r'\s*L"([A-Za-z_]\w*)"\s*', entry)
        if not literal:
            raise ValueError('Unrecognized dispatcher entry; do not silently skip a public method')
        extensions.append(literal[1])
    if not methods or not extensions:
        raise ValueError('Empty contract extraction is not a PASS')
    names = [name.casefold() for name in methods] + [name.casefold() for name in extensions]
    if len(names) != len(set(names)):
        raise ValueError('Duplicate public method names (case insensitive)')
    if not re.search(r'return\s+100\s*\+\s*static_cast<DISPID>\(index\)', source):
        raise ValueError('Dispatcher ID assignment changed; inspect before updating the check')
    methods.update((name, 100 + index) for index, name in enumerate(extensions))
    return methods


def check(idl: str, helper: str, documentation: str, implementation: str) -> dict:
    methods = expected_methods(idl, helper)
    visible = mask_code(documentation, inline=False)
    ids = re.findall(r'<a\s+[^>]*id=["\']api-([^"\']+)["\']', visible, re.I)
    toc = re.findall(r'\]\(#api-([^)]*)\)', mask_code(documentation))
    expected = {name.lower() for name in methods}
    problems = {
        'missing_anchors': sorted(expected - set(ids)),
        'missing_toc': sorted(expected - set(toc)),
        'unexpected_api_anchors': sorted(set(ids) - expected),
        'unexpected_toc': sorted(set(toc) - expected),
        'duplicate_anchors': sorted(name for name, n in Counter(ids).items() if n > 1),
    }
    banner = re.search(r'Актуальный публичный контракт:\s*API\s+(\d+\.\d+\.\d+),\s*(\d+)\s+метод', visible)
    toc_count = re.search(r'^## Оглавление: все (\d+) метод', visible, re.M)
    version = re.search(r'case\s+100\s*:\s*value\s*=\s*L"(\d+\.\d+\.\d+)"', without_comments(implementation))
    capabilities = re.search(r'case\s+102\s*:\s*value\s*=\s*L("(?:\\.|[^"\\])*")', without_comments(implementation))
    if not version or not capabilities:
        raise ValueError('API version/capabilities extraction failed; inspect the implementation')
    caps = json.loads(json.loads(capabilities[1]))
    problems['metadata_errors'] = []
    if not banner or int(banner[2]) != len(methods):
        problems['metadata_errors'].append('Banner method count differs from public source methods')
    if not toc_count or int(toc_count[1]) != len(methods):
        problems['metadata_errors'].append('TOC method count differs from public source methods')
    if not banner or banner[1] != version[1] or caps['apiVersion'] != version[1]:
        problems['metadata_errors'].append('Documented/API/capabilities versions differ')
    cap_section = re.search(r'<a id="api-getscriptingcapabilities"></a>([\s\S]*?)(?=<a id="api-)', visible)
    for name, enabled in caps['capabilities'].items():
        if enabled and (not cap_section or f'`{name}`' not in cap_section[1]):
            problems['metadata_errors'].append(f'Enabled capability not documented: {name}')
    return {
        'status': 'FAIL' if any(problems.values()) else 'PASS',
        'api_version': version[1], 'method_count': len(methods),
        'legacy_count': sum(i < 100 for i in methods.values()),
        'extension_count': sum(i >= 100 for i in methods.values()),
        **problems,
        'scope': 'Static names, counts, version, capabilities and navigation; no runtime/ABI/example execution.',
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    root = args.root
    def read(name):
        return (root / name).read_text(encoding='utf-8-sig')
    documentation = read('docs/scripting-api.md')
    result = check(read('src/contracts/fbe.idl'), read('src/fbe/ExternalHelper.cpp'),
                   documentation, read('src/fbe/scripts/ScriptingApi.inl'))
    from check_scripting_api_examples import check_examples
    result['example_errors'] = check_examples(args.root, documentation,
        expected_methods(read('src/contracts/fbe.idl'), read('src/fbe/ExternalHelper.cpp')))
    if result['example_errors']:
        result['status'] = 'FAIL'
    data = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    print(data, end='')
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(data, encoding='utf-8')
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as exc:
        print(f'API check failed: {exc}', file=sys.stderr)
        raise SystemExit(2)
