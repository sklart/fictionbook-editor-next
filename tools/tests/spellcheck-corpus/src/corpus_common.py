from __future__ import annotations

import hashlib
import html
import json
import re
import unicodedata
from collections.abc import Iterable, Iterator
from dataclasses import dataclass
from pathlib import Path
from typing import Any

LETTER_RE = re.compile(r"[A-Za-zА-Яа-яЁё]")
CYRILLIC_RE = re.compile(r"[А-Яа-яЁё]")
TOKEN_RE = re.compile(
    r"(?<![A-Za-zА-Яа-яЁё])"
    r"[A-Za-zА-Яа-яЁё]+(?:[\-‐‑‒–—'’ʼ][A-Za-zА-Яа-яЁё]+)*"
    r"(?![A-Za-zА-Яа-яЁё])"
)
URL_RE = re.compile(r"\b(?:https?://|www\.)\S+", re.IGNORECASE)
EMAIL_RE = re.compile(r"\b[\w.+-]+@[\w.-]+\.[A-Za-z]{2,}\b")
TAG_RE = re.compile(r"<[^>]+>")
CONTROL_RE = re.compile(r"[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f]")
SPACES_RE = re.compile(r"[ \t\u00a0\u2007\u202f]+")
BLANKS_RE = re.compile(r"\n{3,}")


@dataclass(frozen=True)
class TextChunk:
    text: str
    word_count: int


def clean_text(value: Any) -> str:
    """Conservative cleanup for spellchecking corpora.

    It deliberately preserves ё/е, case, hyphens and apostrophes. Only markup,
    URLs, controls and spacing noise are removed.
    """
    if value is None:
        return ""
    text = str(value)
    text = html.unescape(text)
    text = unicodedata.normalize("NFC", text)
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = text.replace("\u200b", "").replace("\ufeff", "")
    text = TAG_RE.sub(" ", text)
    text = URL_RE.sub(" ", text)
    text = EMAIL_RE.sub(" ", text)
    text = CONTROL_RE.sub("", text)
    lines: list[str] = []
    for line in text.split("\n"):
        line = SPACES_RE.sub(" ", line).strip()
        lines.append(line)
    text = "\n".join(lines)
    text = BLANKS_RE.sub("\n\n", text)
    return text.strip()


def iter_tokens(text: str) -> Iterator[str]:
    for match in TOKEN_RE.finditer(text):
        yield match.group(0)


def normalized_token(token: str) -> str:
    token = unicodedata.normalize("NFC", token)
    return (
        token.replace("‐", "-")
        .replace("‑", "-")
        .replace("‒", "-")
        .replace("–", "-")
        .replace("—", "-")
        .replace("’", "'")
        .replace("ʼ", "'")
    )


def cyrillic_ratio(text: str) -> float:
    letters = LETTER_RE.findall(text)
    if not letters:
        return 0.0
    return len(CYRILLIC_RE.findall(text)) / len(letters)


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def chunk_text(text: str, max_words: int = 1200, min_words: int = 80) -> Iterator[TextChunk]:
    """Split on paragraphs while keeping chunks useful for context review."""
    paragraphs = [p.strip() for p in re.split(r"\n\s*\n", text) if p.strip()]
    current: list[str] = []
    current_words = 0

    def flush() -> TextChunk | None:
        nonlocal current, current_words
        if not current:
            return None
        joined = "\n\n".join(current).strip()
        count = sum(1 for _ in iter_tokens(joined))
        current = []
        current_words = 0
        if count < min_words:
            return None
        return TextChunk(joined, count)

    for paragraph in paragraphs:
        p_words = sum(1 for _ in iter_tokens(paragraph))
        if p_words == 0:
            continue
        if p_words > max_words:
            result = flush()
            if result:
                yield result
            words = paragraph.split()
            for start in range(0, len(words), max_words):
                piece = " ".join(words[start : start + max_words]).strip()
                count = sum(1 for _ in iter_tokens(piece))
                if count >= min_words:
                    yield TextChunk(piece, count)
            continue
        if current and current_words + p_words > max_words:
            result = flush()
            if result:
                yield result
        current.append(paragraph)
        current_words += p_words

    result = flush()
    if result:
        yield result


def read_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def iter_json_file_records(path: Path) -> Iterator[dict[str, Any]]:
    """Read records from either a regular JSON document or JSON Lines.

    Some upstream datasets use the ``.json`` suffix for newline-delimited JSON.
    Trying to load such a file with :func:`json.load` raises ``Extra data`` on
    the second line. We first try the ordinary JSON layouts supported by
    :func:`iter_json_records`, then transparently fall back to JSONL.
    """
    try:
        value = read_json(path)
    except json.JSONDecodeError as json_error:
        try:
            yielded = False
            for record in iter_jsonl(path):
                yielded = True
                yield from iter_json_records(record)
            if not yielded:
                raise ValueError(f"JSON/JSONL file contains no records: {path}")
        except (ValueError, json.JSONDecodeError) as jsonl_error:
            raise ValueError(
                f"Cannot parse {path} as JSON or JSONL. "
                f"JSON error: {json_error}; JSONL error: {jsonl_error}"
            ) from jsonl_error
        return

    yield from iter_json_records(value)


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(value, handle, ensure_ascii=False, indent=2)
        handle.write("\n")


def iter_json_records(value: Any) -> Iterator[dict[str, Any]]:
    """Flatten common JSON dataset layouts into dictionaries."""
    if isinstance(value, list):
        for item in value:
            if isinstance(item, dict):
                yield item
        return
    if isinstance(value, dict):
        if all(isinstance(v, list) for v in value.values()) and value:
            lengths = {len(v) for v in value.values()}
            if len(lengths) == 1:
                keys = list(value)
                for index in range(next(iter(lengths))):
                    yield {key: value[key][index] for key in keys}
                return
        for key in ("data", "records", "items", "examples"):
            nested = value.get(key)
            if isinstance(nested, list):
                yield from iter_json_records(nested)
                return
        if "source" in value or "text" in value:
            yield value


def iter_jsonl(path: Path) -> Iterator[dict[str, Any]]:
    with path.open("r", encoding="utf-8-sig") as handle:
        for line_number, line in enumerate(handle, 1):
            line = line.strip()
            if not line:
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"Invalid JSONL at {path}:{line_number}: {exc}") from exc
            if isinstance(value, dict):
                yield value


def find_context(text: str, token: str, radius: int = 110) -> str:
    lowered = text.casefold()
    pos = lowered.find(token.casefold())
    if pos < 0:
        return text[: radius * 2].replace("\n", " ")
    start = max(0, pos - radius)
    end = min(len(text), pos + len(token) + radius)
    snippet = text[start:end].replace("\n", " ")
    if start:
        snippet = "…" + snippet
    if end < len(text):
        snippet += "…"
    return SPACES_RE.sub(" ", snippet).strip()


def batched(items: Iterable[str], size: int) -> Iterator[list[str]]:
    batch: list[str] = []
    for item in items:
        batch.append(item)
        if len(batch) >= size:
            yield batch
            batch = []
    if batch:
        yield batch
