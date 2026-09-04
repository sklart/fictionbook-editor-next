from __future__ import annotations

import gzip
import json
import random
import re
import xml.etree.ElementTree as ET
from collections import Counter
from collections.abc import Iterator
from pathlib import Path
from typing import Any

from corpus_common import clean_text, iter_tokens, normalized_token

RNC_LOCAL_LOADERS = {
    "rnc_morphological_standard",
    "rnc_syntagrus",
    "rnc_diachronic_gzip",
    "rnc_multilingual_json",
}
RNC_ANNOTATED_LOADERS = {
    "rnc_morphological_standard",
    "rnc_syntagrus",
}

MORPH_NONSTANDARD_FLAGS = {"distort", "anom", "nonlex"}
MORPH_ABBREVIATION_FLAGS = {"abbr", "ciph"}
MORPH_PROPER_FLAGS = {"persn", "famn", "patrn", "zoon", "topon", "geo", "orgn"}
MORPH_STRICT_POS = {
    "S",
    "V",
    "PR",
    "CONJ",
    "S-PRO",
    "PART",
    "ADV",
    "A",
    "ADV-PRO",
    "NUM",
    "A-PRO",
    "PRAEDIC",
    "PARENTH",
    "ANUM",
    "INTJ",
    "PRAEDIC-PRO",
    "A-NUM",
}
POS_ALIASES = {
    "SPRO": "S-PRO",
    "APRO": "A-PRO",
    "АPRO": "A-PRO",  # legacy corpus typo: Cyrillic А
    "ADVPRO": "ADV-PRO",
    "PRAEDICPRO": "PRAEDIC-PRO",
}
PAREN_PREFIX_RE = re.compile(r"^(?:\([^)]*\))+\s*")
RNC_STRESS_MARKS_RE = re.compile(r"[`\u00b4\u0300\u0301]")


def profile_value(entry: dict[str, Any], profile: str, key: str, default: Any = None) -> Any:
    return entry.get("profiles", {}).get(profile, {}).get(key, default)


def is_rnc_loader(loader: str) -> bool:
    return loader in RNC_LOCAL_LOADERS


def is_annotated_rnc_loader(loader: str) -> bool:
    return loader in RNC_ANNOTATED_LOADERS


def _local_path(rnc_root: Path, entry: dict[str, Any]) -> Path:
    relative = entry.get("relative_path")
    if not relative:
        raise ValueError(f"RNC entry {entry.get('id')} does not define relative_path")
    path = (rnc_root / str(relative)).resolve()
    if not path.exists():
        raise FileNotFoundError(
            f"RNC source for {entry.get('id')} not found: {path}. "
            f"Check --rnc-root and extracted dataset layout."
        )
    return path


def _selected_files(root: Path, pattern: str, limit: int | None, seed: int) -> list[Path]:
    files = sorted(path for path in root.rglob(pattern) if path.is_file())
    if not files:
        raise FileNotFoundError(f"No {pattern} files found below {root}")
    if limit is None or limit >= len(files):
        return files
    rng = random.Random(seed)
    rng.shuffle(files)
    return sorted(files[:limit])


def _exact_spellcheck_token(value: str, *, strip_stress_marks: bool = False) -> str | None:
    token = normalized_token(clean_text(value))
    if strip_stress_marks:
        # The Morphological Standard marks stress in surface forms with an
        # ASCII grave accent before the stressed vowel (for example
        # ``К`амень`` or ```ели``).  Some exports may use Unicode combining
        # acute/grave marks instead.  They are annotation, not spelling, so
        # remove them before sending the word to Hunspell.
        token = RNC_STRESS_MARKS_RE.sub("", token)
    if not token:
        return None
    parts = list(iter_tokens(token))
    if len(parts) != 1 or parts[0] != token:
        return None
    if not any("а" <= ch.casefold() <= "я" or ch in "Ёё" for ch in token):
        return None
    return token


def _xml_text(element: ET.Element) -> str:
    return clean_text("".join(element.itertext()))


def _source_group_from_path(path: Path, root: Path, *, marker: str | None = None) -> str:
    """Return a stable high-level source group for an annotated RNC file.

    The Morphological Standard is commonly distributed below
    ``sample_ar/TEXTS/<genre>/...``.  When the configured root points at the
    extraction directory, simply taking the first relative component would
    collapse every document into ``sample_ar``.  Prefer the directory after
    the requested marker (``TEXTS``) and fall back to the first useful
    relative directory.
    """
    try:
        relative = path.relative_to(root)
        parts = relative.parts
    except ValueError:
        return path.parent.name

    if marker:
        marker_folded = marker.casefold()
        for index, part in enumerate(parts[:-1]):
            if part.casefold() == marker_folded and index + 1 < len(parts) - 1:
                return parts[index + 1]

    if len(parts) > 1:
        return parts[0]
    return path.parent.name


def _base_morph_pos(gr: str) -> str:
    value = PAREN_PREFIX_RE.sub("", gr.strip())
    head = value.split(",", 1)[0].strip()
    head = head.split("=", 1)[0].strip()
    return POS_ALIASES.get(head, head or "UNKNOWN")


def _morph_flags(gr: str) -> set[str]:
    lowered = gr.casefold()
    return {
        flag
        for flag in MORPH_NONSTANDARD_FLAGS | MORPH_ABBREVIATION_FLAGS | MORPH_PROPER_FLAGS
        if flag in lowered
    }


def _morph_class(analyses: list[ET.Element]) -> str:
    if len(analyses) != 1:
        return "ambiguous"
    gr = analyses[0].get("gr", "")
    flags = _morph_flags(gr)
    pos = _base_morph_pos(gr)
    if flags & MORPH_NONSTANDARD_FLAGS or pos == "NONLEX":
        return "nonstandard"
    if flags & MORPH_ABBREVIATION_FLAGS or pos == "INIT":
        return "abbreviation"
    if flags & MORPH_PROPER_FLAGS:
        return "proper_name"
    if pos not in MORPH_STRICT_POS:
        return "other_annotation"
    return "strict_clean"


def _new_aggregate() -> dict[str, Any]:
    return {
        "frequency": 0,
        "class_counts": Counter(),
        "pos_counts": Counter(),
        "lemma_counts": Counter(),
        "grammar_counts": Counter(),
        "source_group_counts": Counter(),
        "contexts": [],
    }


def _add_aggregate(
    aggregate: dict[str, Any],
    *,
    class_name: str,
    pos: str,
    lemma: str | None,
    grammar: str | None,
    source_group: str | None,
    context: str | None,
    max_contexts: int = 3,
) -> None:
    aggregate["frequency"] += 1
    aggregate["class_counts"]["all_lexical"] += 1
    if class_name != "all_lexical":
        aggregate["class_counts"][class_name] += 1
    if pos:
        aggregate["pos_counts"][pos] += 1
    if lemma:
        aggregate["lemma_counts"][lemma] += 1
    if grammar:
        aggregate["grammar_counts"][grammar] += 1
    if source_group:
        aggregate["source_group_counts"][source_group] += 1
    if context and context not in aggregate["contexts"] and len(aggregate["contexts"]) < max_contexts:
        aggregate["contexts"].append(context)


def _write_aggregated_rows(
    output_path: Path,
    corpus_id: str,
    entry: dict[str, Any],
    words: dict[str, dict[str, Any]],
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    ordered = sorted(words.items(), key=lambda item: (-item[1]["frequency"], item[0].casefold(), item[0]))
    with output_path.open("w", encoding="utf-8", newline="\n") as handle:
        for word, data in ordered:
            row = {
                "corpus_id": corpus_id,
                "quality_tier": entry.get("quality_tier"),
                "license": entry.get("license"),
                "word": word,
                "frequency": int(data["frequency"]),
                "class_counts": dict(data["class_counts"]),
                "pos_counts": dict(data["pos_counts"]),
                "lemmas": [item for item, _ in data["lemma_counts"].most_common(5)],
                "grammar_examples": [item for item, _ in data["grammar_counts"].most_common(5)],
                "source_groups": dict(data["source_group_counts"].most_common(8)),
                "contexts": data["contexts"],
            }
            handle.write(json.dumps(row, ensure_ascii=False) + "\n")


def process_rnc_morphological_standard(
    entry: dict[str, Any],
    profile: str,
    output_dir: Path,
    rnc_root: Path,
    seed: int,
) -> dict[str, Any]:
    corpus_id = entry["id"]
    root = _local_path(rnc_root, entry)
    limit = profile_value(entry, profile, "max_source_files")
    limit = None if limit is None else int(limit)
    files = _selected_files(root, "*.xhtml", limit, seed)
    words: dict[str, dict[str, Any]] = {}
    totals = Counter()
    pos_totals = Counter()
    parse_errors: list[str] = []

    for path in files:
        try:
            tree = ET.parse(path)
        except ET.ParseError as exc:
            parse_errors.append(f"{path}: {exc}")
            continue
        source_group = _source_group_from_path(path, root, marker="TEXTS")

        for sentence in tree.iter("se"):
            context = _xml_text(sentence)
            for w in sentence.iter("w"):
                raw_surface = _xml_text(w)
                totals["source_tokens"] += 1
                if RNC_STRESS_MARKS_RE.search(raw_surface):
                    totals["stress_marked_tokens"] += 1
                surface = _exact_spellcheck_token(raw_surface, strip_stress_marks=True)
                if not surface:
                    totals["tokens_filtered"] += 1
                    continue
                analyses = list(w.findall("ana"))
                class_name = _morph_class(analyses)
                if len(analyses) == 1:
                    gr = analyses[0].get("gr", "")
                    lemma = analyses[0].get("lex")
                    pos = _base_morph_pos(gr)
                else:
                    gr = " | ".join(a.get("gr", "") for a in analyses if a.get("gr"))
                    lemma = next((a.get("lex") for a in analyses if a.get("lex")), None)
                    pos = "AMBIG"

                aggregate = words.setdefault(surface, _new_aggregate())
                _add_aggregate(
                    aggregate,
                    class_name=class_name,
                    pos=pos,
                    lemma=lemma,
                    grammar=gr,
                    source_group=source_group,
                    context=context,
                )
                totals["tokens"] += 1
                totals[class_name] += 1
                pos_totals[pos] += 1

    if parse_errors:
        raise RuntimeError(
            f"RNC Morphological Standard XML parse failures: {len(parse_errors)}. "
            f"First error: {parse_errors[0]}"
        )

    output_path = output_dir / "annotated" / f"{corpus_id}.jsonl"
    _write_aggregated_rows(output_path, corpus_id, entry, words)
    if entry.get("require_nonempty") and not words:
        raise RuntimeError(f"RNC annotated corpus {corpus_id} produced no usable tokens")
    return {
        "id": corpus_id,
        "kind": "annotated",
        "revision": None,
        "local_source": str(entry.get("relative_path")),
        "source_files_seen": len(files),
        "source_tokens": int(totals["source_tokens"]),
        "tokens": int(totals["tokens"]),
        "tokens_filtered": int(totals["tokens_filtered"]),
        "stress_marked_tokens": int(totals["stress_marked_tokens"]),
        "unique_words": len(words),
        "classes": {
            key: int(value)
            for key, value in sorted(totals.items())
            if key not in {"source_tokens", "tokens", "tokens_filtered", "stress_marked_tokens"}
        },
        "pos": dict(pos_totals.most_common()),
        "output": str(output_path),
    }


def process_rnc_syntagrus(
    entry: dict[str, Any],
    profile: str,
    output_dir: Path,
    rnc_root: Path,
    seed: int,
) -> dict[str, Any]:
    corpus_id = entry["id"]
    root = _local_path(rnc_root, entry)
    limit = profile_value(entry, profile, "max_source_files")
    limit = None if limit is None else int(limit)
    files = _selected_files(root, "*.tgt", limit, seed)
    words: dict[str, dict[str, Any]] = {}
    totals = Counter()
    pos_totals = Counter()
    parse_errors: list[str] = []

    for path in files:
        try:
            tree = ET.parse(path)
        except ET.ParseError as exc:
            parse_errors.append(f"{path}: {exc}")
            continue
        source_group = _source_group_from_path(path, root)

        for sentence in tree.iter("S"):
            context = _xml_text(sentence)
            for w in sentence.iter("W"):
                totals["source_tokens"] += 1
                surface = _exact_spellcheck_token(_xml_text(w))
                if not surface:
                    totals["tokens_filtered"] += 1
                    continue
                feat = w.get("FEAT", "")
                pos = feat.split()[0] if feat else "UNKNOWN"
                lemma = w.get("LEMMA")
                aggregate = words.setdefault(surface, _new_aggregate())
                _add_aggregate(
                    aggregate,
                    class_name="all_lexical",
                    pos=pos,
                    lemma=lemma,
                    grammar=feat,
                    source_group=source_group,
                    context=context,
                )
                totals["tokens"] += 1
                totals["all_lexical"] += 1
                pos_totals[pos] += 1

    if parse_errors:
        raise RuntimeError(
            f"RNC SynTagRus XML parse failures: {len(parse_errors)}. First error: {parse_errors[0]}"
        )

    output_path = output_dir / "annotated" / f"{corpus_id}.jsonl"
    _write_aggregated_rows(output_path, corpus_id, entry, words)
    if entry.get("require_nonempty") and not words:
        raise RuntimeError(f"RNC annotated corpus {corpus_id} produced no usable tokens")
    return {
        "id": corpus_id,
        "kind": "annotated",
        "revision": None,
        "local_source": str(entry.get("relative_path")),
        "source_files_seen": len(files),
        "source_tokens": int(totals["source_tokens"]),
        "tokens": int(totals["tokens"]),
        "tokens_filtered": int(totals["tokens_filtered"]),
        "unique_words": len(words),
        "classes": {"all_lexical": int(totals["all_lexical"])},
        "pos": dict(pos_totals.most_common()),
        "output": str(output_path),
    }


def process_rnc_annotated(
    entry: dict[str, Any],
    profile: str,
    output_dir: Path,
    rnc_root: Path,
    seed: int,
) -> dict[str, Any]:
    loader = entry["loader"]
    if loader == "rnc_morphological_standard":
        return process_rnc_morphological_standard(entry, profile, output_dir, rnc_root, seed)
    if loader == "rnc_syntagrus":
        return process_rnc_syntagrus(entry, profile, output_dir, rnc_root, seed)
    raise ValueError(f"Unsupported annotated RNC loader: {loader}")


def stream_rnc_diachronic_documents(
    entry: dict[str, Any],
    profile: str,
    rnc_root: Path,
) -> Iterator[dict[str, Any]]:
    path = _local_path(rnc_root, entry)
    encoding = entry.get("encoding", "utf-8")
    target_words = int(entry.get("aggregate_target_words", 5000))
    max_records = profile_value(entry, profile, "max_source_records")
    max_records = None if max_records is None else int(max_records)
    buffer: list[str] = []
    buffer_words = 0
    buffer_start = 1

    with gzip.open(path, "rt", encoding=encoding, errors="strict") as handle:
        for line_number, line in enumerate(handle, 1):
            if max_records is not None and line_number > max_records:
                break
            text = clean_text(line)
            count = sum(1 for _ in iter_tokens(text))
            if count == 0:
                continue
            if not buffer:
                buffer_start = line_number
            buffer.append(text)
            buffer_words += count
            if buffer_words < target_words:
                continue
            yield {
                "text": "\n".join(buffer),
                "source_record": f"lines:{buffer_start}-{line_number}",
                "title": entry.get("name"),
                "author": None,
                "source_url": entry.get("source_url"),
                "source_metadata": {
                    "source_file": path.name,
                    "period": entry.get("period"),
                    "encoding": encoding,
                },
            }
            buffer = []
            buffer_words = 0

    if buffer:
        yield {
            "text": "\n".join(buffer),
            "source_record": f"lines:{buffer_start}-end",
            "title": entry.get("name"),
            "author": None,
            "source_url": entry.get("source_url"),
            "source_metadata": {
                "source_file": path.name,
                "period": entry.get("period"),
                "encoding": encoding,
            },
        }


def stream_rnc_multilingual_documents(
    entry: dict[str, Any],
    profile: str,
    rnc_root: Path,
) -> Iterator[dict[str, Any]]:
    path = _local_path(rnc_root, entry)
    encoding = entry.get("encoding", "utf-8-sig")
    target_words = int(entry.get("aggregate_target_words", 5000))
    max_records = profile_value(entry, profile, "max_source_records")
    max_records = None if max_records is None else int(max_records)

    with path.open("r", encoding=encoding) as handle:
        data = json.load(handle)
    if not isinstance(data, list):
        raise ValueError(f"Expected a JSON list in {path}, got {type(data).__name__}")

    buffer: list[str] = []
    buffer_words = 0
    buffer_start = 0
    last_index = -1
    for index, record in enumerate(data):
        if max_records is not None and index >= max_records:
            break
        last_index = index
        if not isinstance(record, dict):
            continue
        ru = record.get("ru")
        if not isinstance(ru, list):
            continue
        for segment in ru:
            text = clean_text(segment)
            count = sum(1 for _ in iter_tokens(text))
            if count == 0:
                continue
            if not buffer:
                buffer_start = index
            buffer.append(text)
            buffer_words += count
            if buffer_words < target_words:
                continue
            yield {
                "text": "\n".join(buffer),
                "source_record": f"rows:{buffer_start}-{index}",
                "title": entry.get("name"),
                "author": None,
                "source_url": entry.get("source_url"),
                "source_metadata": {"source_file": path.name, "language": "ru"},
            }
            buffer = []
            buffer_words = 0

    if buffer:
        yield {
            "text": "\n".join(buffer),
            "source_record": f"rows:{buffer_start}-{last_index}",
            "title": entry.get("name"),
            "author": None,
            "source_url": entry.get("source_url"),
            "source_metadata": {"source_file": path.name, "language": "ru"},
        }
