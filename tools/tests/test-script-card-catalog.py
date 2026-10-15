"""Short regression for all embedded script cards and the derived index."""

from __future__ import annotations

import runpy
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "runtime" / "Scripts"
TOOL = runpy.run_path(str(ROOT / "tools" / "script-card-tool.py"))
inspect_file = TOOL["inspect_file"]
read_document = TOOL["read_document"]


def main() -> None:
    files = list(SCRIPTS.rglob("*.js"))
    assert len(files) == 579, f"expected 579 JS files, got {len(files)}"
    authors = Counter()
    dependencies = Counter()
    missing_resources = []
    for path in files:
        document, result = inspect_file(path, None, ROOT / "runtime")
        assert result["status"] == "current", (path, result)
        assert result["dependencyStatus"] != "invalid", (path, result)
        for dependency in result["dependencies"]:
            assert dependency["status"] != "changed", (path, dependency)
            if dependency["status"] == "missing":
                missing_resources.append((path.name, dependency["path"]))
        assert document.card["sourceRevision"]["reviewStatus"] == "generated", path
        assert document.card["authors"], path
        # Git normalizes tracked JS to LF and the checkout applies .gitattributes.
        # Confirm that a fresh Windows checkout retains the expected raw code SHA.
        relative = path.relative_to(SCRIPTS).as_posix()
        canonical = document.data.replace(b"\r\n", b"\n")
        checkout = (canonical if relative.startswith("32_") and path.name.startswith("52_")
                    else canonical.replace(b"\n", b"\r\n"))
        assert read_document(checkout).sha256 == document.card["sourceRevision"]["sha256"], path
        authors[document.card["authorshipStatus"]] += 1
        dependencies[result["dependencyStatus"]] += 1
    assert authors == Counter({"declared": 523, "partial_attribution": 23, "not_stated": 33}), authors
    # Known source defect is explicitly recorded, never treated as a complete
    # runnable package merely because the card and executable hashes match.
    assert missing_resources == [("15_Расставить иллюстрации по заданным местам.js",
                                  "HTML/Расставить иллюстрации по заданным местам.html")], missing_resources
    index = runpy.run_path(str(ROOT / "tools" / "generate-script-card-index.py"))
    assert index["OUTPUT"].read_bytes() == index["build"](), "derived index is stale"
    print(f"Script cards: {len(files)} current; attribution {dict(authors)}; dependencies {dict(dependencies)}; index current.")


if __name__ == "__main__":
    main()
