"""Inspect, validate and explicitly stamp embedded FBE Next script cards."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import tempfile
from dataclasses import dataclass

OPEN = '/*<FBE-NEXT-CARD version="1">'
CLOSE = '</FBE-NEXT-CARD>*/'
ALGORITHM = "sha256-exclude-fbe-card-v1"
MAX_CARD_BYTES = 64 * 1024
REQUIRED = (
    "schema", "title", "version", "authors", "summary", "description",
    "scope", "modifiesDocument", "requiresSelection", "usage", "warnings",
    "dependencies", "sourceRevision",
)
ROLES = {"author", "original_author", "coauthor", "modifier", "contributor", "ai_assistance", "unknown"}
REVIEW_STATUSES = {"generated", "reviewed", "unknown"}


class CardError(ValueError):
    pass


def _unique_pairs(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise CardError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


@dataclass(frozen=True)
class Document:
    data: bytes
    encoding: str
    bom_size: int
    span: tuple[int, int] | None
    card: dict | None

    @property
    def code_bytes(self) -> bytes:
        if self.span is None:
            return self.data
        start, end = self.span
        return self.data[:start] + self.data[end:]

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.code_bytes).hexdigest()


def _encoding(data: bytes, override: str | None) -> tuple[str, int]:
    if data.startswith(b"\xef\xbb\xbf"):
        detected, bom_size = "utf-8", 3
    elif data.startswith(b"\xff\xfe"):
        detected, bom_size = "utf-16-le", 2
    elif data.startswith(b"\xfe\xff"):
        detected, bom_size = "utf-16-be", 2
    else:
        bom_size = 0
        try:
            data.decode("utf-8", errors="strict")
            detected = "utf-8"
        except UnicodeDecodeError:
            detected = "cp1251"
    if override:
        selected = override.lower().replace("windows-1251", "cp1251")
        if bom_size and selected != detected:
            raise CardError("explicit encoding conflicts with BOM")
        return selected, bom_size
    return detected, bom_size


def read_document(data: bytes, encoding: str | None = None) -> Document:
    codec, bom_size = _encoding(data, encoding)
    opener = OPEN.encode(codec)
    closer = CLOSE.encode(codec)
    start = bom_size
    if not data.startswith(opener, start):
        if data.find(opener, start) >= 0:
            raise CardError("card opening marker is not immediately after the BOM")
        return Document(data, codec, bom_size, None, None)
    limit = min(len(data), start + MAX_CARD_BYTES)
    close = data.find(closer, start + len(opener), limit)
    if close < 0:
        raise CardError("card closing marker missing or card exceeds 64 KiB")
    end = close + len(closer)
    for line_ending in ("\r\n", "\n", "\r"):
        encoded_line_ending = line_ending.encode(codec)
        if data.startswith(encoded_line_ending, end):
            end += len(encoded_line_ending)
            break
    else:
        raise CardError("card must end with exactly one line ending")
    if end - start > MAX_CARD_BYTES:
        raise CardError("card exceeds 64 KiB")
    if data.find(opener, end) >= 0:
        raise CardError("more than one card opening marker")
    try:
        body = data[start + len(opener):close].decode(codec, errors="strict")
    except UnicodeError as error:
        raise CardError(f"invalid card encoding: {error}") from error
    if "*/" in body:
        raise CardError("unsafe comment terminator in card body")
    try:
        card = json.loads(body, object_pairs_hook=_unique_pairs)
    except json.JSONDecodeError as error:
        raise CardError(f"invalid card JSON: {error}") from error
    if not isinstance(card, dict):
        raise CardError("card JSON must be an object")
    return Document(data, codec, bom_size, (start, end), card)


def validate_schema(card: dict) -> list[str]:
    problems = [f"missing {key}" for key in REQUIRED if key not in card]
    if card.get("schema") != 1 or isinstance(card.get("schema"), bool):
        problems.append("schema must be 1")
    for key in ("title", "version", "summary", "description", "scope"):
        if not isinstance(card.get(key), str) or not card[key].strip():
            problems.append(f"{key} must be nonempty text")
    for key in ("modifiesDocument", "requiresSelection"):
        if card.get(key) is not None and not isinstance(card.get(key), bool):
            problems.append(f"{key} must be boolean or null")
    authors = card.get("authors")
    if not isinstance(authors, list) or not authors:
        problems.append("authors must name participants or explicitly state unknown")
    else:
        for author in authors:
            if not isinstance(author, dict) or author.get("role") not in ROLES or (
                author.get("role") != "unknown" and
                (not isinstance(author.get("name"), str) or not author["name"].strip())
            ):
                problems.append("invalid author or role")
                break
    for key in ("usage", "warnings"):
        if not isinstance(card.get(key), list) or any(not isinstance(value, str) for value in card[key]):
            problems.append(f"{key} must be a text array")
    if not isinstance(card.get("dependencies"), list):
        problems.append("dependencies must be an array")
    else:
        for dependency in card["dependencies"]:
            if not isinstance(dependency, dict):
                problems.append("dependency must be an object")
                break
            path = dependency.get("path")
            digest = dependency.get("sha256")
            if not isinstance(path, str) or not path or "\x00" in path or "\\" in path or ":" in path:
                problems.append("dependency path is invalid")
            elif PurePosixPath(path).is_absolute() or any(part in (".", "..", "") for part in path.split("/")):
                problems.append("dependency path leaves the resource root")
            if not isinstance(dependency.get("optional"), bool):
                problems.append("dependency.optional must be boolean")
            if digest is not None and (not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest)):
                problems.append("dependency.sha256 must be lowercase SHA-256 or null")
    if "dependenciesComplete" in card and not isinstance(card["dependenciesComplete"], bool):
        problems.append("dependenciesComplete must be boolean")
    revision = card.get("sourceRevision")
    if not isinstance(revision, dict):
        problems.append("sourceRevision must be an object")
    else:
        if revision.get("algorithm") != ALGORITHM:
            problems.append("unsupported sourceRevision algorithm")
        digest = revision.get("sha256")
        if digest is not None and (not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest)):
            problems.append("sourceRevision.sha256 must be lowercase SHA-256 or null")
        if revision.get("reviewStatus") not in REVIEW_STATUSES:
            problems.append("invalid sourceRevision.reviewStatus")
    return problems


def serialize(card: dict, encoding: str, newline: str) -> bytes:
    body = json.dumps(card, ensure_ascii=True, indent=2)
    if "*/" in body:
        raise CardError("card JSON contains an unsafe comment terminator")
    text = OPEN + newline + body.replace("\n", newline) + newline + CLOSE + newline
    encoded = text.encode(encoding, errors="strict")
    if len(encoded) > MAX_CARD_BYTES:
        raise CardError("card exceeds 64 KiB")
    return encoded


def insert_card(source: bytes, card: dict, encoding: str | None = None) -> bytes:
    document = read_document(source, encoding)
    if document.span is not None:
        raise CardError("script already contains a card")
    problems = validate_schema(card)
    if problems:
        raise CardError("; ".join(problems))
    data = source[document.bom_size:]
    newline = "\r\n" if "\r\n".encode(document.encoding) in data[:4096] else "\n"
    result = source[:document.bom_size] + serialize(card, document.encoding, newline) + data
    if read_document(result, document.encoding).code_bytes != source:
        raise CardError("byte preservation invariant failed")
    return result


def dependency_status(card: dict, resource_root: Path | None) -> tuple[str, list[dict]]:
    results: list[dict] = []
    for dependency in card.get("dependencies", []):
        if not isinstance(dependency, dict):
            results.append({"status": "invalid", "reason": "dependency must be an object"})
            continue
        path = dependency.get("path")
        if not isinstance(path, str) or not path or "\x00" in path or "\\" in path or ":" in path or not isinstance(dependency.get("optional"), bool):
            results.append({"path": path, "status": "invalid"})
            continue
        relative = PurePosixPath(path)
        if relative.is_absolute() or any(part in (".", "..") for part in path.split("/")):
            results.append({"path": path, "status": "invalid"})
            continue
        if resource_root is None:
            results.append({"path": path, "status": "unverified"})
            continue
        root = resource_root.resolve()
        target = (root / Path(*relative.parts)).resolve()
        if not target.is_relative_to(root):
            results.append({"path": path, "status": "invalid"})
        elif not target.is_file():
            results.append({"path": path, "status": "missing" if not dependency.get("optional") else "optional-missing"})
        elif not re.fullmatch(r"[0-9a-f]{64}", str(dependency.get("sha256") or "")):
            results.append({"path": path, "status": "unverified"})
        else:
            # An embedded card is documentation, not part of a linked JS
            # script's executable revision. Keep this consistent with the
            # sourceRevision algorithm so migrating a dependency's card does
            # not make every caller appear stale.
            actual = (read_document(target.read_bytes()).sha256 if target.suffix.lower() == ".js"
                      else hashlib.sha256(target.read_bytes()).hexdigest())
            results.append({"path": path, "status": "current" if actual == dependency["sha256"] else "changed", "actualSha256": actual})
    statuses = {entry["status"] for entry in results}
    if "invalid" in statuses:
        overall = "invalid"
    elif "changed" in statuses or "missing" in statuses:
        overall = "changed"
    elif "unverified" in statuses or not card.get("dependenciesComplete", False):
        overall = "unverified"
    else:
        overall = "current"
    return overall, results


def inspect_file(path: Path, encoding: str | None, resource_root: Path | None) -> tuple[Document, dict]:
    document = read_document(path.read_bytes(), encoding)
    result: dict = {"path": str(path), "encoding": document.encoding, "codeSha256": document.sha256}
    if document.card is None:
        result["status"] = "missing"
        return document, result
    problems = validate_schema(document.card)
    expected = document.card.get("sourceRevision", {}).get("sha256") if isinstance(document.card.get("sourceRevision"), dict) else None
    result.update({"status": "invalid" if problems else "unchecked" if expected is None else "current" if expected == document.sha256 else "changed",
                   "expectedSha256": expected, "reviewStatus": document.card.get("sourceRevision", {}).get("reviewStatus") if isinstance(document.card.get("sourceRevision"), dict) else None,
                   "problems": problems})
    result["dependencyStatus"], result["dependencies"] = dependency_status(document.card, resource_root)
    return document, result


def _atomic_write(path: Path, data: bytes) -> None:
    fd, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary, stat.S_IMODE(path.stat().st_mode))
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inspect", "validate", "stamp"))
    parser.add_argument("file", type=Path)
    parser.add_argument("--encoding", help="Override detection for files without a BOM")
    parser.add_argument("--resource-root", type=Path, help="Root for literal dependency paths")
    parser.add_argument("--accept-revision", action="store_true", help="Explicitly accept the current source revision for stamp")
    args = parser.parse_args()
    try:
        document, result = inspect_file(args.file, args.encoding, args.resource_root)
        if args.command == "stamp":
            if not args.accept_revision:
                raise CardError("stamp requires --accept-revision after reviewing the description")
            if document.card is None or result["problems"]:
                raise CardError("stamp requires a valid embedded card")
            document.card["sourceRevision"]["sha256"] = document.sha256
            old_card = document.data[document.span[0]:document.span[1]]
            newline = "\r\n" if "\r\n".encode(document.encoding) in old_card else "\n"
            replacement = serialize(document.card, document.encoding, newline)
            updated = document.data[:document.span[0]] + replacement + document.data[document.span[1]:]
            if read_document(updated, args.encoding).code_bytes != document.code_bytes:
                raise CardError("stamp would change executable source bytes")
            _atomic_write(args.file, updated)
            _, result = inspect_file(args.file, args.encoding, args.resource_root)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        if args.command == "validate" and (result["status"] != "current" or result["dependencyStatus"] in ("changed", "invalid")):
            return 1
        return 0
    except (CardError, OSError, UnicodeError, LookupError) as error:
        print(json.dumps({"status": "invalid", "error": str(error)}, ensure_ascii=False))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
