"""Build a deterministic search index from embedded JS cards, or check freshness."""

from __future__ import annotations

import argparse
import json
import runpy
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "runtime" / "Scripts"
OUTPUT = SCRIPTS / "script-cards.index.json"
CARD_TOOL = runpy.run_path(str(ROOT / "tools" / "script-card-tool.py"))
inspect_file = CARD_TOOL["inspect_file"]


def build() -> bytes:
    entries = []
    for path in sorted(SCRIPTS.rglob("*.js"), key=lambda item: item.relative_to(SCRIPTS).as_posix()):
        document, result = inspect_file(path, None, None)
        if result["status"] != "current":
            raise ValueError(f"{path}: card is {result['status']}")
        card = document.card
        relative = path.relative_to(SCRIPTS).as_posix()
        entries.append({
            "path": relative,
            "category": path.parent.relative_to(SCRIPTS).as_posix() if path.parent != SCRIPTS else "",
            "title": card["title"],
            "summary": card["summary"],
            "description": card["description"],
            "authors": card["authors"],
            "tags": card.get("tags", []),
            "codeSha256": result["codeSha256"],
        })
    return (json.dumps({"schema": 1, "source": "embedded-fbe-next-card-v1", "scripts": entries},
                       ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--generate", action="store_true")
    args = parser.parse_args()
    if args.check == args.generate:
        parser.error("choose exactly one of --check and --generate")
    data = build()
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_bytes() != data:
            print(f"stale script card index: {OUTPUT}", file=sys.stderr)
            return 1
        print("Script card index is current.")
    else:
        OUTPUT.write_bytes(data)
        print(f"Generated {OUTPUT} ({len(json.loads(data)['scripts'])} scripts).")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
