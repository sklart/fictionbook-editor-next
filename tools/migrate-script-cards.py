"""Insert reviewed catalog data as embedded cards without changing JS code bytes.

The archive is an input, never a runtime dependency. Run without --apply first.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import runpy
import subprocess
import sys
import zipfile
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "runtime" / "Scripts"
CATALOG_NAME = "FBE-Next-Script-Cards/fbe-script-cards.ru-RU.json"
MANIFEST_NAME = "FBE-Next-Script-Cards/manifest.json"
EXPECTED_TREE = "64c413305ea8f953981f3aa1a2c79a1baff4086f"
EXPECTED_REF = "b8312c4842f8910890c2473771b0d7ba5f268cda"
CARD_TOOL = runpy.run_path(str(ROOT / "tools" / "script-card-tool.py"))
insert_card = CARD_TOOL["insert_card"]
read_document = CARD_TOOL["read_document"]
CardError = CARD_TOOL["CardError"]


def git(*arguments: str) -> subprocess.CompletedProcess[str]:
    native = Path(os.environ.get("ProgramFiles", "")) / "Git" / "cmd" / "git.exe"
    executable = os.environ.get("GIT_EXE") or (str(native) if native.is_file() else "git")
    return subprocess.run([executable, *arguments], cwd=ROOT, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)


def load_catalog(archive: Path) -> dict:
    with zipfile.ZipFile(archive) as bundle:
        manifest = json.loads(bundle.read(MANIFEST_NAME))
        if manifest.get("ref") != EXPECTED_REF:
            raise ValueError("catalog source commit differs from the audited commit")
        payload = bundle.read(CATALOG_NAME)
        expected = manifest["files"]["fbe-script-cards.ru-RU.json"]["sha256"]
        if hashlib.sha256(payload).hexdigest() != expected:
            raise ValueError("catalog checksum differs from its manifest")
        catalog = json.loads(payload)
    if catalog.get("schemaVersion") is None or len(catalog.get("scripts", {})) != 580:
        raise ValueError("expected exactly 580 source catalog scripts")
    return catalog["scripts"]


def authors(entry: dict) -> list[dict]:
    attribution = entry["authorship"]
    result: list[dict] = []
    known = {"author", "original_author", "coauthor", "modifier", "contributor", "ai_assistance"}
    for contributor in attribution.get("contributors", []):
        name = contributor.get("name")
        if not name:
            continue
        for original_role in contributor.get("roles", []):
            role = original_role if original_role in known else "contributor"
            item = {"name": name, "role": role}
            if role != original_role:
                item["contribution"] = original_role
            if item not in result:
                result.append(item)
    if attribution.get("status") != "declared" or not result:
        result.append({"role": "unknown"})
    return result


def dependencies(entry: dict) -> tuple[list[dict], int]:
    result: list[dict] = []
    unresolved = 0
    for dependency in entry.get("dependencies", []):
        resolved = dependency.get("resolvedPath")
        if not isinstance(resolved, str) or not resolved.startswith("runtime/"):
            unresolved += 1
            continue
        path = resolved[len("runtime/"):]
        if not path or "\\" in path or ":" in path or any(part in ("", ".", "..") for part in path.split("/")):
            unresolved += 1
            continue
        target = ROOT / "runtime" / Path(*path.split("/"))
        if not target.is_file():
            unresolved += 1
            continue
        item = {"path": path, "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                "optional": bool(dependency.get("condition"))}
        if item not in result:
            result.append(item)
    return result, unresolved


def make_card(entry: dict, source: bytes) -> tuple[dict, int]:
    linked, unresolved = dependencies(entry)
    scope = entry.get("scope") or {}
    scope_text = scope.get("description") or scope.get("kind") or "Не установлена"
    description = entry.get("description") or entry.get("summary") or "Описание требует проверки."
    card = {
        "schema": 1,
        "title": entry.get("name") or Path(entry["path"]).stem,
        "version": entry.get("version") or "Не указана",
        "authors": authors(entry),
        "authorshipStatus": entry["authorship"].get("status", "not_stated"),
        "summary": entry.get("summary") or "Назначение требует уточнения.",
        "description": description,
        "scope": scope_text,
        "modifiesDocument": entry.get("modifiesDocument"),
        "requiresSelection": entry.get("requiresSelection"),
        "usage": entry.get("usage") or [],
        "warnings": entry.get("warnings") or [],
        "dependencies": linked,
        "dependenciesComplete": False,
        "sourceRevision": {"algorithm": "sha256-exclude-fbe-card-v1",
                           "sha256": hashlib.sha256(source).hexdigest(), "reviewStatus": "generated"},
    }
    if unresolved:
        card["unverifiedDependencyCount"] = unresolved
    for key in ("tags", "examples"):
        if entry.get(key):
            card[key] = entry[key]
    if entry.get("undo"):
        card["undo"] = {key: entry["undo"][key] for key in ("status", "description")
                        if isinstance(entry["undo"].get(key), str)}
    if entry.get("settings"):
        card["settings"] = [
            {key: setting[key] for key in ("name", "value", "expression", "description", "valueMeaning")
             if key in setting and setting[key] is not None}
            for setting in entry["settings"]
        ]
    return card, unresolved


def prepare(catalog: dict) -> tuple[list[tuple[Path, bytes, bytes]], dict]:
    paths = {path.relative_to(SCRIPTS).as_posix(): path for path in SCRIPTS.rglob("*.js")}
    if set(paths) != set(catalog):
        raise ValueError(f"catalog paths differ: missing={len(set(catalog) - set(paths))}, extra={len(set(paths) - set(catalog))}")
    prepared: list[tuple[Path, bytes, bytes]] = []
    counts = Counter()
    for relative, entry in sorted(catalog.items()):
        path = paths[relative]
        original = path.read_bytes()
        if read_document(original).span is not None:
            raise ValueError(f"script already has a card: {relative}")
        card, unresolved = make_card(entry, original)
        try:
            updated = insert_card(original, card)
            extracted = read_document(updated)
        except (CardError, UnicodeError) as error:
            raise ValueError(f"invalid card for {relative}: {error}") from error
        if extracted.code_bytes != original or extracted.sha256 != card["sourceRevision"]["sha256"]:
            raise ValueError(f"code bytes changed for {relative}")
        prepared.append((path, original, updated))
        counts[entry["authorship"].get("status", "not_stated")] += 1
        counts["unresolved_dependencies"] += unresolved
        counts["linked_dependencies"] += len(card["dependencies"])
        counts["unknown_version"] += not bool(entry.get("version"))
    return prepared, dict(counts)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--apply", action="store_true", help="insert every prevalidated card")
    args = parser.parse_args()
    if git("rev-parse", "HEAD:runtime/Scripts").stdout.strip() != EXPECTED_TREE:
        raise ValueError("runtime/Scripts Git tree differs from the audited source")
    if git("diff", "--name-only", "--", "runtime/Scripts").stdout.strip() or \
            git("diff", "--cached", "--name-only", "--", "runtime/Scripts").stdout.strip():
        raise ValueError("runtime/Scripts contains uncommitted changes")
    prepared, counts = prepare(load_catalog(args.archive))
    print(json.dumps({"scripts": len(prepared), "bytes_added": sum(len(after) - len(before) for _, before, after in prepared),
                      **counts}, ensure_ascii=False, sort_keys=True))
    if args.apply:
        written = []
        try:
            for path, original, updated in prepared:
                path.write_bytes(updated)
                written.append((path, original))
        except OSError:
            for path, original in reversed(written):
                path.write_bytes(original)
            raise
        print(f"Inserted {len(written)} cards; original JS bytes remain recoverable by exact card removal.")
    else:
        print("Dry run only. Use --apply after reviewing the counts.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        print(f"migration failed: {error}", file=sys.stderr)
        sys.exit(1)
