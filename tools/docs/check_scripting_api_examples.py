"""Check handbook structure, source-derived arity and extract runnable examples.

This is a static check, not proof of execution or of dialog/Clipboard behavior.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re


LABELS = ('Назначение', 'Сигнатура и параметры', 'Возвращаемое значение',
          'Особенности, ограничения и ошибки', 'Пример → результат', 'Ожидаемый результат')
COMPLEX = ('complex-report', 'complex-nbsp', 'complex-images', 'complex-title',
           'complex-selection', 'complex-backup', 'complex-source-export',
           'complex-binary-report', 'complex-safe-clipboard', 'complex-preferences')
HISTORICAL = ('getdocumentfilepath getdocumentfilename getdocumentdirectory getstylepath '
              'getprogramversion getuuid getnbsp isfastmode getviewwidth getviewheight '
              'beginundounit endundounit inflateblock inflateparagraphs setstyleex '
              'работа-с-исходным-xml-документа msgbox askyesno inputbox getmodalresult '
              'genrepopup getextendedstyle descshowelement descshowmenu setstatusbartext '
              'getbinarysize savebinary getimagedimsbypath getimagedimsbydata binary-api-limits').split()


def sections(document: str, prefix: str) -> dict[str, str]:
    return {m[1]: m[2] for m in re.finditer(
        r'<a id="(' + prefix + r'[^"]+)"></a>([\s\S]*?)(?=\n<a id=|\n## |\Z)', document)}


def argument_limits(root: Path, methods: dict[str, int]) -> dict[str, tuple[int, int]]:
    """Read legacy parameter lists and the current Stage2 arity table.

    Remaining dispatcher groups have fixed contracts keyed by their existing IDs.
    Unknown IDs fail rather than inheriting an assumed zero-argument signature.
    """
    from check_scripting_api import without_comments
    idl = without_comments((root / 'src/contracts/fbe.idl').read_text(encoding='utf-8-sig'))
    legacy = idl.split('interface IExternalHelper : IDispatch', 1)[1]
    limits = {}
    for name, args in re.findall(r'HRESULT\s+(\w+)\s*\(([^;]+)\);', legacy):
        count = len(re.findall(r'\[in\]', args))
        if name not in limits:  # indexed inflateBlock getter, not its setter
            limits[name] = (count, count)
    stage2 = (root / 'src/fbe/scripts/ScriptingStage2.inl').read_text(encoding='utf-8-sig')
    table = re.search(r'const UINT expected\[\] = \{([\d,]+)\}', stage2)
    if not table:
        raise ValueError('Stage2 arity table changed; inspect the dispatcher')
    counts = [int(n) for n in table[1].split(',')]
    for name, number in methods.items():
        if number < 100:
            continue
        if number <= 118:
            minimum = maximum = 2 if number in (111, 112) else 1 if number in (101, 113, 115, 116, 117, 118) else 0
        elif number <= 142:
            minimum = counts[number - 119]
            maximum = minimum + (2 if number == 141 else 1 if number in (131, 132, 142) else 0)
        elif number <= 146:
            minimum = maximum = 1 if number == 143 else 2 if number == 144 else 0
        elif number <= 152:
            minimum = 2 if number in (149, 150) else 1
            maximum = 3 if number in (149, 150) else 2 if number in (151, 152) else 1
        elif number <= 163:
            minimum = maximum = 4 if number == 153 else 0 if number in (157, 159, 163) else 2 if number == 160 else 1
        elif number <= 168:
            minimum = maximum = 0 if number in (164, 167) else 2
        else:
            raise ValueError(f'Unknown method ID: {number}')
        limits[name] = (minimum, maximum)
    return limits


def call_count(source: str, start: int) -> int:
    """Count positional arguments; strings, comments and nested calls stay opaque."""
    depth, count, content, quote, escaped = 1, 0, False, None, False
    i = start
    while i < len(source):
        c = source[i]
        if quote:
            if escaped:
                escaped = False
            elif c == '\\':
                escaped = True
            elif c == quote:
                quote = None
        elif source.startswith('//', i):
            end = source.find('\n', i)
            i = len(source) if end < 0 else end
            continue
        elif source.startswith('/*', i):
            end = source.find('*/', i + 2)
            i = len(source) if end < 0 else end + 2
            continue
        elif c in "\"'":
            quote, content = c, True
        elif c in '([{':
            depth += 1
            content = True
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                return count + int(content)
        elif c == ',' and depth == 1:
            count += 1
        elif not c.isspace():
            content = True
        i += 1
    raise ValueError('Unterminated API call')


def check_examples(root: Path, document: str, methods: dict[str, int]) -> list[str]:
    errors = []
    entries = sections(document, 'api-')
    limits = argument_limits(root, methods)
    for name, number in methods.items():
        body = entries.get('api-' + name.lower(), '')
        positions = [body.find('**' + label) for label in LABELS]
        if min(positions) < 0 or positions != sorted(positions):
            errors.append(f'{name}: missing or unordered handbook fields')
        if not re.search(r'DISPID \*\*' + str(number) + r'\*\*', body):
            errors.append(f'{name}: wrong or absent DISPID')
        version = ('исходный интерфейс' if number < 100 else 'API ' +
                   next(v for upper, v in ((118, '1.0.0'), (142, '1.1.0'), (146, '1.2.0'),
                                           (152, '1.3.0'), (163, '1.4.0'), (168, '1.5.0')) if number <= upper))
        if f'DISPID **{number}** · {version}.' not in body:
            errors.append(f'{name}: incorrect introduction version')
        signature = re.search(r'### `\w+\(([^`]*)\)`', body)
        if signature:
            args = re.findall(r'[A-Za-z_]\w*', signature[1])
            required = len(re.findall(r'[A-Za-z_]\w*', signature[1].split('[', 1)[0]))
            if (required, len(args)) != limits[name]:
                errors.append(f'{name}: signature arity differs from dispatcher {limits[name]}')
        codes = re.findall(r'```javascript\n([\s\S]*?)```', body)
        if not codes or any(not re.search(r'function\s+Run\s*\(\s*\)', code) for code in codes):
            errors.append(f'{name}: example needs a complete Run()')
    for anchor in HISTORICAL:
        if f'<a id="{anchor}"></a>' not in document:
            errors.append(f'Historical anchor removed: {anchor}')
    complex_entries = sections(document, 'complex-')
    for anchor in COMPLEX:
        body = complex_entries.get(anchor, '')
        if not body or f'](#{anchor})' not in document:
            errors.append(f'Missing complex scenario or TOC link: {anchor}')
        if anchor in COMPLEX[5:]:
            for field in ('**Задача и методы:**', '**Условия и ограничения:**', '**Исходные данные → ожидаемый результат:**'):
                if field not in body:
                    errors.append(f'{anchor}: absent {field}')
            if 'finally' not in body or 'catch' not in body:
                errors.append(f'{anchor}: missing cleanup/error handling')
    if document.count('## Сложные скрипты: сочетание методов API') != 1:
        errors.append('Complex scripts section must occur exactly once')
    # Check literal calls in executable blocks, not prose or signatures.
    for index, code in enumerate(re.findall(r'```javascript\n([\s\S]*?)```', document)):
        for call in re.finditer(r'\b(?:ext|e|window\.external)\.(\w+)\s*\(', code):
            if call[1] not in limits:
                errors.append(f'Block {index}: unknown public method {call[1]}')
            else:
                count = call_count(code, call.end())
                low, high = limits[call[1]]
                if not low <= count <= high:
                    errors.append(f'Block {index}: {call[1]} takes {low}..{high} arguments, got {count}')
    return errors


def main() -> int:
    from check_scripting_api import expected_methods
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--extract-dir', type=Path)
    args = parser.parse_args()
    root = args.root
    document = (root / 'docs/scripting-api.md').read_text(encoding='utf-8-sig')
    methods = expected_methods((root / 'src/contracts/fbe.idl').read_text(encoding='utf-8-sig'),
                               (root / 'src/fbe/ExternalHelper.cpp').read_text(encoding='utf-8-sig'))
    errors = check_examples(root, document, methods)
    if errors:
        print('\n'.join(errors))
        return 1
    if args.extract_dir:
        args.extract_dir.mkdir(parents=True, exist_ok=True)
        for anchor, body in {**sections(document, 'api-'), **sections(document, 'complex-')}.items():
            codes = re.findall(r'```javascript\n([\s\S]*?)```', body)
            for index, code in enumerate(codes):
                (args.extract_dir / f'{anchor}-{index}.js').write_text(code, encoding='utf-8-sig')
    print(f'PASS: {len(methods)} structured methods; {len(COMPLEX)} complex scenarios; call arity matches contracts')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
