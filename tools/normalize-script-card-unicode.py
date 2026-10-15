"""Use readable Unicode and current markers in embedded cards without touching JS code.

Run without --write to check for printable Unicode escapes or legacy markers.
The card JSON structure, source bytes outside the card, and code SHA stay identical.
"""

from __future__ import annotations

import argparse
import json
import runpy
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "runtime" / "Scripts"
CARD_TOOL = runpy.run_path(str(ROOT / "tools" / "script-card-tool.py"))
OPEN = CARD_TOOL["OPEN"].encode("utf-8")
CLOSE = CARD_TOOL["CLOSE"].encode("utf-8")
LEGACY_OPEN = CARD_TOOL["LEGACY_OPEN"].encode("utf-8")
LEGACY_CLOSE = CARD_TOOL["LEGACY_CLOSE"].encode("utf-8")
read_document = CARD_TOOL["read_document"]
HEX = frozenset("0123456789abcdefABCDEF")


def printable(value: int) -> bool:
    return value >= 0xA0 and value not in (0x2028, 0x2029) and not 0xD800 <= value <= 0xDFFF


def unescape_string(token: str) -> str:
    result = ['"']
    index = 1
    while index < len(token) - 1:
        if token[index] != "\\":
            result.append(token[index])
            index += 1
            continue
        if index + 5 < len(token) - 1 and token[index + 1] == "u" and all(
            digit in HEX for digit in token[index + 2:index + 6]
        ):
            value = int(token[index + 2:index + 6], 16)
            consumed = 6
            if 0xD800 <= value <= 0xDBFF and token[index + 6:index + 8] == "\\u" and all(
                digit in HEX for digit in token[index + 8:index + 12]
            ):
                lower = int(token[index + 8:index + 12], 16)
                if 0xDC00 <= lower <= 0xDFFF:
                    value = 0x10000 + (value - 0xD800) * 0x400 + lower - 0xDC00
                    consumed = 12
            if printable(value):
                result.append(chr(value))
                index += consumed
                continue
        result.append(token[index:index + 2])
        index += 2
    result.append('"')
    return "".join(result)


def readable_json(body: str) -> str:
    result = []
    index = 0
    while index < len(body):
        if body[index] != '"':
            result.append(body[index])
            index += 1
            continue
        end = index + 1
        while end < len(body):
            if body[end] == "\\":
                end += 2
            elif body[end] == '"':
                break
            else:
                end += 1
        if end >= len(body):
            raise ValueError("unterminated JSON string")
        result.append(unescape_string(body[index:end + 1]))
        index = end + 1
    return "".join(result)


def normalized(data: bytes) -> bytes:
    document = read_document(data)
    if document.span is None or document.card is None or document.encoding != "utf-8":
        raise ValueError("expected an embedded UTF-8 script card")
    start, end = document.span
    segment = data[start:end]
    old_open, old_close = (OPEN, CLOSE) if segment.startswith(OPEN) else (LEGACY_OPEN, LEGACY_CLOSE)
    close = segment.find(old_close)
    if close < 0:
        raise ValueError("card closing marker missing")
    body = segment[len(old_open):close].decode("utf-8")
    updated = readable_json(body)
    if updated == body and old_open == OPEN:
        return data
    if json.loads(updated) != document.card:
        raise ValueError("card JSON changed meaning")
    replacement = data[:start] + OPEN + updated.encode("utf-8") + CLOSE + segment[close + len(old_close):] + data[end:]
    checked = read_document(replacement)
    if checked.card != document.card or checked.code_bytes != document.code_bytes or checked.sha256 != document.sha256:
        raise ValueError("card normalization changed script code or metadata")
    return replacement


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="update the script cards")
    args = parser.parse_args()
    paths = sorted(SCRIPTS.rglob("*.js"))
    if len(paths) != 579:
        raise SystemExit(f"expected 579 scripts, found {len(paths)}")
    changed = []
    for path in paths:
        original = path.read_bytes()
        updated = normalized(original)
        if updated != original:
            changed.append(path)
            if args.write:
                path.write_bytes(updated)
    print(f"Script cards checked: {len(paths)}; cards requiring normalization: {len(changed)}")
    if changed and not args.write:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
