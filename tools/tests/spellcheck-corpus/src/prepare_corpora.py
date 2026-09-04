from __future__ import annotations

import argparse
import json
import os
import random
import sys
import time
from collections import Counter
from collections.abc import Iterable, Iterator
from datetime import datetime, timezone
from email.utils import parsedate_to_datetime
from pathlib import Path
from typing import Any
from urllib.parse import quote

sys.path.insert(0, str(Path(__file__).resolve().parent))

from corpus_common import (  # noqa: E402
    chunk_text,
    clean_text,
    cyrillic_ratio,
    iter_json_file_records,
    iter_tokens,
    read_json,
    sha256_text,
    write_json,
)
from typo_cases import extract_typo_cases  # noqa: E402
from rnc_corpora import (  # noqa: E402
    is_annotated_rnc_loader,
    is_rnc_loader,
    process_rnc_annotated,
    stream_rnc_diachronic_documents,
    stream_rnc_multilingual_documents,
)


def profile_value(entry: dict[str, Any], profile: str, key: str, default: Any = None) -> Any:
    profiles = entry.get("profiles", {})
    value = profiles.get(profile, {}).get(key, default)
    return value


def resolve_revision(repo_id: str) -> str | None:
    try:
        from huggingface_hub import HfApi

        return HfApi().dataset_info(repo_id).sha
    except Exception as exc:  # network metadata is helpful but not mandatory
        print(f"[warning] Cannot resolve revision for {repo_id}: {exc}", file=sys.stderr)
        return None


def stream_hf_documents(entry: dict[str, Any], profile: str) -> Iterator[dict[str, Any]]:
    try:
        from datasets import load_dataset
    except ImportError as exc:
        raise RuntimeError("Install requirements.txt before downloading Hugging Face corpora") from exc

    repo_id = entry["repo_id"]
    split = entry.get("split", "train")
    config = entry.get("config")
    kwargs: dict[str, Any] = {"split": split, "streaming": True}
    if entry.get("_revision"):
        kwargs["revision"] = entry["_revision"]
    if entry.get("data_dir"):
        kwargs["data_dir"] = entry["data_dir"]
    if entry.get("data_files"):
        kwargs["data_files"] = entry["data_files"]
    if entry.get("encoding"):
        # Pass the encoding explicitly. On Windows/Python 3.14 some versions of
        # datasets may otherwise inherit the active ANSI code page for remote
        # text files instead of the TextConfig UTF-8 default.
        kwargs["encoding"] = entry["encoding"]
    if entry.get("encoding_errors"):
        kwargs["encoding_errors"] = entry["encoding_errors"]
    if entry.get("sample_by"):
        kwargs["sample_by"] = entry["sample_by"]
    dataset = load_dataset(repo_id, config, **kwargs) if config else load_dataset(repo_id, **kwargs)
    max_records = profile_value(entry, profile, "max_source_records")
    text_field = entry.get("text_field", "text")
    aggregate = bool(entry.get("aggregate_small_records"))
    aggregate_words = int(entry.get("aggregate_target_words", 5000))
    buffer: list[str] = []
    buffer_words = 0
    buffer_start = 0

    for index, record in enumerate(dataset):
        if max_records is not None and index >= int(max_records):
            break
        raw_text = clean_text(record.get(text_field, ""))
        if not raw_text:
            continue
        if aggregate:
            if not buffer:
                buffer_start = index
            buffer.append(raw_text)
            buffer_words += sum(1 for _ in iter_tokens(raw_text))
            if buffer_words < aggregate_words:
                continue
            yield {
                "text": "\n".join(buffer),
                "source_record": f"rows:{buffer_start}-{index}",
                "title": entry.get("title", repo_id),
                "author": None,
                "source_url": entry["source_url"],
            }
            buffer = []
            buffer_words = 0
            continue

        yield {
            "text": raw_text,
            "source_record": index,
            "title": record.get(entry.get("title_field", "title")) or f"{repo_id}:{index}",
            "author": record.get(entry.get("author_field", "author")),
            "source_url": record.get(entry.get("source_url_field", "source_url")) or entry["source_url"],
            "source_metadata": {
                key: record.get(key)
                for key in entry.get("metadata_fields", [])
                if record.get(key) is not None
            },
        }

    if buffer:
        yield {
            "text": "\n".join(buffer),
            "source_record": f"rows:{buffer_start}-end",
            "title": entry.get("title", repo_id),
            "author": None,
            "source_url": entry["source_url"],
        }



def _hf_dataset_file_url(repo_id: str, filename: str, revision: str | None) -> str:
    """Build a pinned direct-download URL for one file in a dataset repo."""
    encoded_repo = quote(repo_id, safe="/")
    encoded_revision = quote(revision or "main", safe="")
    encoded_filename = quote(filename, safe="/")
    return (
        f"https://huggingface.co/datasets/{encoded_repo}/resolve/"
        f"{encoded_revision}/{encoded_filename}?download=true"
    )


def stream_hf_text_documents(
    entry: dict[str, Any],
    profile: str,
    session: Any | None = None,
) -> Iterator[dict[str, Any]]:
    """Stream one explicitly selected text file with an explicit encoding.

    This avoids two problems with repository auto-detection for text datasets:
    it can include several copies of the corpus (normalized and per-author files),
    and on some Windows/Python combinations the remote file object may inherit
    the active ANSI code page. Bytes are decoded here using the encoding from
    the manifest, normally UTF-8.
    """
    try:
        import requests
    except ImportError as exc:
        raise RuntimeError("Install requests from requirements.txt") from exc

    repo_id = entry["repo_id"]
    filename = entry["filename"]
    encoding = entry.get("encoding", "utf-8")
    encoding_errors = entry.get("encoding_errors", "strict")
    revision = entry.get("_revision")
    url = _hf_dataset_file_url(repo_id, filename, revision)
    max_records = profile_value(entry, profile, "max_source_records")
    aggregate = bool(entry.get("aggregate_small_records"))
    aggregate_words = int(entry.get("aggregate_target_words", 5000))

    own_session = session is None
    if session is None:
        session = requests.Session()
    headers = {
        "User-Agent": "FictionBook-Editor-Next spellcheck corpus builder/1.3",
        "Accept-Encoding": "identity",
    }
    token = os.environ.get("HF_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    session.headers.update(headers)

    response = None
    delay = 1.0
    for attempt in range(4):
        try:
            response = session.get(url, stream=True, timeout=(30, 120), allow_redirects=True)
            response.raise_for_status()
            break
        except Exception:
            if response is not None:
                response.close()
            if attempt == 3:
                if own_session:
                    session.close()
                raise
            time.sleep(delay)
            delay *= 2
    assert response is not None

    buffer: list[str] = []
    buffer_words = 0
    buffer_start = 0
    last_index = -1
    try:
        for index, raw_line in enumerate(
            response.iter_lines(chunk_size=1024 * 1024, decode_unicode=False)
        ):
            last_index = index
            if max_records is not None and index >= int(max_records):
                break
            try:
                line = raw_line.decode(encoding, errors=encoding_errors)
            except UnicodeDecodeError as exc:
                raise RuntimeError(
                    f"Cannot decode {repo_id}/{filename} as {encoding} at source line "
                    f"{index + 1}: {exc}. Check the corpus manifest encoding."
                ) from exc
            raw_text = clean_text(line)
            if not raw_text:
                continue
            if aggregate:
                if not buffer:
                    buffer_start = index
                buffer.append(raw_text)
                buffer_words += sum(1 for _ in iter_tokens(raw_text))
                if buffer_words < aggregate_words:
                    continue
                yield {
                    "text": "\n".join(buffer),
                    "source_record": f"{filename}:lines:{buffer_start + 1}-{index + 1}",
                    "title": entry.get("title", repo_id),
                    "author": None,
                    "source_url": entry["source_url"],
                    "source_metadata": {
                        "source_file": filename,
                        "encoding": encoding,
                    },
                }
                buffer = []
                buffer_words = 0
                continue

            yield {
                "text": raw_text,
                "source_record": f"{filename}:line:{index + 1}",
                "title": entry.get("title", repo_id),
                "author": None,
                "source_url": entry["source_url"],
                "source_metadata": {
                    "source_file": filename,
                    "encoding": encoding,
                },
            }

        if buffer:
            yield {
                "text": "\n".join(buffer),
                "source_record": f"{filename}:lines:{buffer_start + 1}-{last_index + 1}",
                "title": entry.get("title", repo_id),
                "author": None,
                "source_url": entry["source_url"],
                "source_metadata": {
                    "source_file": filename,
                    "encoding": encoding,
                },
            }
    finally:
        response.close()
        if own_session:
            session.close()

def _retry_after_seconds(response: Any, default: float, maximum: float = 120.0) -> float:
    """Return a bounded delay from Retry-After (seconds or HTTP date)."""
    headers = getattr(response, "headers", {}) or {}
    raw = headers.get("Retry-After")
    if raw:
        try:
            return max(0.0, min(float(raw), maximum))
        except (TypeError, ValueError):
            try:
                target = parsedate_to_datetime(str(raw))
                if target.tzinfo is None:
                    target = target.replace(tzinfo=timezone.utc)
                seconds = (target - datetime.now(timezone.utc)).total_seconds()
                return max(0.0, min(seconds, maximum))
            except (TypeError, ValueError, OverflowError):
                pass
    return max(0.0, min(default, maximum))


def _request_json(
    session: Any,
    endpoint: str,
    params: dict[str, Any],
    retries: int = 8,
    sleep_fn: Any = time.sleep,
) -> dict[str, Any]:
    """Call the MediaWiki API politely and survive temporary throttling.

    Wikimedia may answer with HTTP 429/503 or a JSON ``maxlag`` error.  Honor
    Retry-After when present and otherwise use bounded exponential backoff.
    """
    request_params = dict(params)
    request_params.setdefault("maxlag", 5)
    delay = 2.0

    for attempt in range(retries):
        try:
            response = session.get(endpoint, params=request_params, timeout=(30, 120))
        except Exception:
            if attempt == retries - 1:
                raise
            wait = min(delay, 120.0)
            print(
                f"[warning] Wikimedia request failed; retrying in {wait:.1f}s "
                f"({attempt + 1}/{retries})",
                file=sys.stderr,
            )
            sleep_fn(wait)
            delay = min(delay * 2, 120.0)
            continue

        status = int(getattr(response, "status_code", 200))
        if status in {429, 503}:
            if attempt == retries - 1:
                response.raise_for_status()
            wait = _retry_after_seconds(response, delay)
            print(
                f"[warning] Wikimedia returned HTTP {status}; retrying in {wait:.1f}s "
                f"({attempt + 1}/{retries})",
                file=sys.stderr,
            )
            response.close()
            sleep_fn(wait)
            delay = min(max(delay * 2, wait), 120.0)
            continue

        response.raise_for_status()
        payload = response.json()
        response.close()
        error = payload.get("error") if isinstance(payload, dict) else None
        if isinstance(error, dict):
            if error.get("code") == "maxlag":
                if attempt == retries - 1:
                    raise RuntimeError(f"MediaWiki maxlag persisted after {retries} attempts: {error}")
                lag = error.get("lag")
                try:
                    lag_delay = float(lag) if lag is not None else delay
                except (TypeError, ValueError):
                    lag_delay = delay
                wait = max(2.0, min(max(delay, lag_delay), 120.0))
                print(
                    f"[warning] Wikimedia maxlag; retrying in {wait:.1f}s "
                    f"({attempt + 1}/{retries})",
                    file=sys.stderr,
                )
                sleep_fn(wait)
                delay = min(delay * 2, 120.0)
                continue
            code = str(error.get("code") or "unknown")
            info = str(error.get("info") or error)
            raise RuntimeError(f"MediaWiki API error {code}: {info}")
        return payload

    raise AssertionError("unreachable")


def _batches(values: list[dict[str, Any]], size: int) -> Iterator[list[dict[str, Any]]]:
    for index in range(0, len(values), size):
        yield values[index:index + size]


def _wikisource_page_url(endpoint: str, title: str) -> str:
    base = endpoint.split("/w/api.php", 1)[0].rstrip("/")
    encoded = quote(title.replace(" ", "_"), safe="/()")
    return f"{base}/wiki/{encoded}"


def _fetch_wikisource_parsed_page(
    session: Any,
    endpoint: str,
    title: str,
    *,
    sleep_fn: Any = time.sleep,
) -> dict[str, Any] | None:
    """Fetch the fully rendered text of one Wikisource page.

    TextExtracts cannot return full extracts for several titles in one query.
    Wikisource works are also often assembled through ProofreadPage
    transclusion, so ``action=parse`` is used directly for each candidate.
    """
    params = {
        "action": "parse",
        "format": "json",
        "formatversion": 2,
        "page": title,
        "prop": "text|revid|categories|displaytitle",
        "redirects": 1,
    }
    payload = _request_json(session, endpoint, params, sleep_fn=sleep_fn)
    parsed = payload.get("parse") if isinstance(payload, dict) else None
    if not isinstance(parsed, dict):
        return None
    raw_text = parsed.get("text", "")
    if isinstance(raw_text, dict):
        raw_text = raw_text.get("*", "")
    text = clean_text(raw_text)
    categories: list[str] = []
    for item in parsed.get("categories", []) or []:
        if not isinstance(item, dict):
            continue
        value = item.get("category") or item.get("*")
        if value:
            categories.append(str(value).removeprefix("Категория:"))
    raw_display_title = parsed.get("displaytitle") or parsed.get("title") or title
    if isinstance(raw_display_title, dict):
        raw_display_title = raw_display_title.get("*", title)
    page_title = clean_text(raw_display_title)
    return {
        "text": text,
        "source_record": parsed.get("pageid"),
        "title": page_title or title,
        "author": None,
        "source_url": _wikisource_page_url(endpoint, parsed.get("title") or title),
        "source_metadata": {
            "revision_id": parsed.get("revid"),
            "categories": categories,
            "content_api": "action=parse",
        },
    }


def _wikisource_page_is_excluded(
    categories: list[str],
    excluded_categories: set[str],
) -> bool:
    normalized = {clean_text(value).casefold() for value in categories if value}
    return any(value.casefold() in normalized for value in excluded_categories)


def _wikisource_text_is_usable(
    text: str,
    *,
    min_document_chars: int,
    min_document_words: int,
    min_ratio: float,
) -> bool:
    if len(text) < min_document_chars:
        return False
    if cyrillic_ratio(text) < min_ratio:
        return False
    return sum(1 for _ in iter_tokens(text)) >= min_document_words


def stream_wikisource_documents(
    entry: dict[str, Any],
    profile: str,
    seed: int,
    session: Any | None = None,
    sleep_fn: Any = time.sleep,
) -> Iterator[dict[str, Any]]:
    try:
        import requests
    except ImportError as exc:
        raise RuntimeError("Install requests from requirements.txt") from exc

    endpoint = entry.get("api", "https://ru.wikisource.org/w/api.php")
    per_category = int(profile_value(entry, profile, "pages_per_category", 25))
    candidate_factor = max(1, int(entry.get("candidate_factor", 10)))
    request_delay = max(0.0, float(entry.get("request_delay_seconds", 0.35)))
    parse_delay = max(0.0, float(entry.get("parse_request_delay_seconds", max(request_delay, 0.75))))
    min_document_chars = max(1, int(entry.get("min_document_chars", 200)))
    min_document_words = max(
        1,
        int(entry.get("min_document_words", entry.get("min_chunk_words", 40))),
    )
    min_ratio = float(entry.get("min_cyrillic_ratio", 0.55))
    excluded_categories = {
        str(value).removeprefix("Категория:")
        for value in entry.get("excluded_page_categories", [])
    }
    own_session = session is None
    if session is None:
        session = requests.Session()
    user_agent = os.environ.get(
        "FBE_WIKIMEDIA_USER_AGENT",
        "FictionBookEditorNext-SpellcheckCorpusBot/1.8 "
        "(https://github.com/sklart/fictionbook-editor-next/issues/51) requests",
    )
    session.headers.update(
        {
            "User-Agent": user_agent,
            "Api-User-Agent": user_agent,
            "Accept": "application/json",
        }
    )
    rng = random.Random(seed)

    try:
        for category in entry["categories"]:
            candidates: list[dict[str, Any]] = []
            continuation: str | None = None
            target_candidates = max(per_category, per_category * candidate_factor)
            while len(candidates) < target_candidates:
                params: dict[str, Any] = {
                    "action": "query",
                    "format": "json",
                    "formatversion": 2,
                    "list": "categorymembers",
                    "cmtitle": f"Категория:{category}",
                    "cmnamespace": 0,
                    "cmtype": "page",
                    "cmlimit": min(500, max(target_candidates - len(candidates), 50)),
                }
                if continuation:
                    params["cmcontinue"] = continuation
                payload = _request_json(session, endpoint, params, sleep_fn=sleep_fn)
                candidates.extend(payload.get("query", {}).get("categorymembers", []))
                continuation = payload.get("continue", {}).get("cmcontinue")
                if not continuation:
                    break
                if request_delay:
                    sleep_fn(request_delay)

            unique_candidates = list({item["title"]: item for item in candidates}.values())
            rng.shuffle(unique_candidates)
            yielded_for_category = 0
            parsed_candidates = 0
            rejected_short = 0
            rejected_category = 0
            rejected_missing = 0
            rejected_api = 0

            # TextExtracts explicitly supports multiple extracts only with
            # exintro=true, which returns only the introduction.  A previous
            # batched full-text request therefore produced an API error and an
            # empty pages list.  Parse each candidate directly instead; this
            # also expands ProofreadPage transclusions used by Wikisource.
            for candidate in unique_candidates:
                title = str(candidate.get("title") or "").strip()
                if not title:
                    rejected_missing += 1
                    continue
                try:
                    parsed_page = _fetch_wikisource_parsed_page(
                        session,
                        endpoint,
                        title,
                        sleep_fn=sleep_fn,
                    )
                except RuntimeError as exc:
                    rejected_api += 1
                    print(
                        f"[warning] Wikisource page {title!r} was skipped: {exc}",
                        file=sys.stderr,
                    )
                    if rejected_api >= 10 and yielded_for_category == 0:
                        raise RuntimeError(
                            f"Wikisource category {category!r} produced repeated API errors; "
                            "aborting instead of silently creating an empty corpus."
                        ) from exc
                    continue
                finally:
                    if parse_delay:
                        sleep_fn(parse_delay)

                parsed_candidates += 1
                if not parsed_page:
                    rejected_missing += 1
                    continue

                parsed_categories = list(
                    parsed_page.get("source_metadata", {}).get("categories", [])
                )
                if _wikisource_page_is_excluded(parsed_categories, excluded_categories):
                    rejected_category += 1
                    continue

                text = clean_text(parsed_page.get("text", ""))
                if not _wikisource_text_is_usable(
                    text,
                    min_document_chars=min_document_chars,
                    min_document_words=min_document_words,
                    min_ratio=min_ratio,
                ):
                    rejected_short += 1
                    continue

                parsed_page["text"] = text
                parsed_page["source_metadata"]["category"] = category
                parsed_page["source_metadata"]["candidate_pool_size"] = len(unique_candidates)
                parsed_page["source_metadata"]["content_api"] = "action=parse"
                yield parsed_page
                yielded_for_category += 1
                if yielded_for_category >= per_category:
                    break

            if yielded_for_category < per_category:
                print(
                    f"[warning] Wikisource category {category!r}: yielded "
                    f"{yielded_for_category}/{per_category} usable documents from "
                    f"{len(unique_candidates)} candidates; parsed={parsed_candidates}, "
                    f"rejected_short={rejected_short}, rejected_category={rejected_category}, "
                    f"rejected_missing={rejected_missing}, rejected_api={rejected_api}",
                    file=sys.stderr,
                )
    finally:
        if own_session:
            session.close()

def download_benchmark_files(entry: dict[str, Any]) -> tuple[list[Path], str | None]:
    try:
        from huggingface_hub import hf_hub_download
    except ImportError as exc:
        raise RuntimeError("Install huggingface_hub from requirements.txt") from exc
    revision = resolve_revision(entry["repo_id"])
    paths: list[Path] = []
    for filename in entry["files"]:
        path = hf_hub_download(
            repo_id=entry["repo_id"],
            filename=filename,
            repo_type="dataset",
            revision=revision,
        )
        paths.append(Path(path))
    return paths, revision


def process_benchmark(entry: dict[str, Any], output_dir: Path) -> dict[str, Any]:
    paths, revision = download_benchmark_files(entry)
    output_path = output_dir / "benchmarks" / f"{entry['id']}.jsonl"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    seen: set[tuple[str, str]] = set()
    pair_count = 0
    case_count = 0
    with output_path.open("w", encoding="utf-8", newline="\n") as handle:
        for path in paths:
            for record in iter_json_file_records(path):
                source = clean_text(record.get("source", ""))
                correction = clean_text(record.get("correction", ""))
                if not source or not correction:
                    continue
                key = (source, correction)
                if key in seen:
                    continue
                seen.add(key)
                pair_count += 1
                cases = extract_typo_cases(source, correction)
                case_count += len(cases)
                row = {
                    "corpus_id": entry["id"],
                    "source": source,
                    "correction": correction,
                    "domain": record.get("domain"),
                    "source_file": str(path.name),
                    "typo_cases": [
                        {"wrong": case.wrong, "correct": case.correct} for case in cases
                    ],
                }
                handle.write(json.dumps(row, ensure_ascii=False) + "\n")
    return {
        "id": entry["id"],
        "kind": "benchmark",
        "revision": revision,
        "pairs": pair_count,
        "single_token_typo_cases": case_count,
        "output": str(output_path),
    }


def process_text_corpus(
    entry: dict[str, Any],
    profile: str,
    output_dir: Path,
    seed: int,
    global_hashes: set[str],
    rnc_root: Path | None = None,
) -> dict[str, Any]:
    corpus_id = entry["id"]
    output_path = output_dir / "clean" / f"{corpus_id}.jsonl"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    max_chunks = profile_value(entry, profile, "max_chunks")
    min_words = int(entry.get("min_chunk_words", 80))
    max_words = int(entry.get("max_chunk_words", 1200))
    min_ratio = float(entry.get("min_cyrillic_ratio", 0.55))
    loader = entry["loader"]
    if loader in {"hf_stream", "hf_text_stream"}:
        revision = resolve_revision(entry["repo_id"])
        resolved_entry = dict(entry)
        resolved_entry["_revision"] = revision
        if loader == "hf_text_stream":
            documents = stream_hf_text_documents(resolved_entry, profile)
        else:
            documents = stream_hf_documents(resolved_entry, profile)
    elif loader == "wikisource_categories":
        documents = stream_wikisource_documents(entry, profile, seed)
        revision = None
    elif loader == "rnc_diachronic_gzip":
        if rnc_root is None:
            raise ValueError(f"RNC root is required for {corpus_id}")
        documents = stream_rnc_diachronic_documents(entry, profile, rnc_root)
        revision = None
    elif loader == "rnc_multilingual_json":
        if rnc_root is None:
            raise ValueError(f"RNC root is required for {corpus_id}")
        documents = stream_rnc_multilingual_documents(entry, profile, rnc_root)
        revision = None
    else:
        raise ValueError(f"Unsupported loader: {loader}")

    stats = Counter()
    dedupe_scope = str(entry.get("dedupe_scope", "global"))
    if dedupe_scope not in {"global", "corpus"}:
        raise ValueError(f"Unsupported dedupe_scope for {corpus_id}: {dedupe_scope}")
    dedupe_hashes = global_hashes if dedupe_scope == "global" else set()
    with output_path.open("w", encoding="utf-8", newline="\n") as handle:
        stop = False
        for document in documents:
            stats["source_documents_seen"] += 1
            text = clean_text(document.get("text", ""))
            if not text or cyrillic_ratio(text) < min_ratio:
                stats["documents_filtered"] += 1
                continue
            for chunk_index, chunk in enumerate(chunk_text(text, max_words=max_words, min_words=min_words)):
                digest = sha256_text(chunk.text)
                if digest in dedupe_hashes:
                    stats["duplicates"] += 1
                    continue
                dedupe_hashes.add(digest)
                row = {
                    "id": f"{corpus_id}:{stats['chunks_written'] + 1}",
                    "corpus_id": corpus_id,
                    "quality_tier": entry["quality_tier"],
                    "license": entry["license"],
                    "title": document.get("title"),
                    "author": document.get("author"),
                    "source_url": document.get("source_url") or entry["source_url"],
                    "source_record": document.get("source_record"),
                    "source_metadata": document.get("source_metadata", {}),
                    "chunk_index": chunk_index,
                    "word_count": chunk.word_count,
                    "cyrillic_ratio": round(cyrillic_ratio(chunk.text), 6),
                    "sha256": digest,
                    "text": chunk.text,
                }
                handle.write(json.dumps(row, ensure_ascii=False) + "\n")
                stats["chunks_written"] += 1
                stats["words_written"] += chunk.word_count
                if max_chunks is not None and stats["chunks_written"] >= int(max_chunks):
                    stop = True
                    break
            if stop:
                break

    result = {
        "id": corpus_id,
        "kind": "text",
        "revision": revision,
        "output": str(output_path),
        "source_documents_seen": int(stats["source_documents_seen"]),
        "documents_filtered": int(stats["documents_filtered"]),
        "duplicates": int(stats["duplicates"]),
        "chunks_written": int(stats["chunks_written"]),
        "words_written": int(stats["words_written"]),
        "dedupe_scope": dedupe_scope,
    }
    if is_rnc_loader(loader):
        result["local_source"] = str(entry.get("relative_path", ""))
    if entry.get("require_nonempty") and result["chunks_written"] == 0:
        raise RuntimeError(
            f"Corpus {corpus_id} produced no usable text. "
            "The output file is empty; check the source API or loader filters."
        )
    return result


def generate_sources_markdown(manifest: dict[str, Any], results: list[dict[str, Any]], output: Path) -> None:
    by_id = {item["id"]: item for item in results}
    lines = [
        "# Источники тестовых корпусов",
        "",
        "Файл сгенерирован `prepare_corpora.py`. Полные тексты не предназначены для коммита в основной репозиторий.",
        "",
    ]
    for entry in manifest["corpora"]:
        result = by_id.get(entry["id"], {})
        lines.extend(
            [
                f"## {entry['name']}",
                "",
                f"- Идентификатор: `{entry['id']}`",
                f"- Назначение: {entry['purpose']}",
                f"- Источник: {entry['source_url']}",
                f"- Лицензия/статус: {entry['license']}",
                f"- Зафиксированная ревизия: `{result.get('revision') or 'указывается в каждой записи/не применимо'}`",
                f"- Результат: `{result.get('output', 'не загружен')}`",
                "",
                entry.get("license_note", ""),
                "",
            ]
        )
    output.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Download and prepare spellcheck corpora for FBE Next")
    parser.add_argument("--manifest", type=Path, default=Path(__file__).resolve().parents[1] / "corpora.json")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "data" / "prepared")
    parser.add_argument("--profile", choices=("smoke", "standard", "full"), default="smoke")
    parser.add_argument("--only", action="append", help="Corpus id; may be specified multiple times")
    parser.add_argument(
        "--rnc-root",
        type=Path,
        help="Root directory of the locally licensed RNC offline datasets",
    )
    parser.add_argument("--seed", type=int, default=51)
    args = parser.parse_args()

    manifest = read_json(args.manifest)
    selected = set(args.only or [])
    args.output.mkdir(parents=True, exist_ok=True)
    results: list[dict[str, Any]] = []
    global_hashes: set[str] = set()

    rnc_root = args.rnc_root.expanduser().resolve() if args.rnc_root else None
    if rnc_root is not None and not rnc_root.is_dir():
        parser.error(f"--rnc-root is not a directory: {rnc_root}")

    for entry in manifest["corpora"]:
        if selected and entry["id"] not in selected:
            continue
        if not entry.get("enabled", True):
            continue
        loader = entry["loader"]
        if is_rnc_loader(loader) and rnc_root is None:
            if selected and entry["id"] in selected:
                parser.error(f"--rnc-root is required for local RNC corpus {entry['id']}")
            print(f"[skip] {entry['id']}: local RNC data; specify --rnc-root to enable")
            continue
        print(f"[prepare] {entry['id']}: {entry['name']}")
        if loader == "benchmark_files":
            result = process_benchmark(entry, args.output)
        elif is_annotated_rnc_loader(loader):
            assert rnc_root is not None
            result = process_rnc_annotated(entry, args.profile, args.output, rnc_root, args.seed)
        else:
            result = process_text_corpus(
                entry,
                args.profile,
                args.output,
                args.seed,
                global_hashes,
                rnc_root=rnc_root,
            )
        results.append(result)
        print(json.dumps(result, ensure_ascii=False, indent=2))

    lock = {
        "schema_version": 2,
        "profile": args.profile,
        "seed": args.seed,
        "rnc_enabled": rnc_root is not None,
        "corpora": results,
    }
    write_json(args.output / "SOURCES.lock.json", lock)
    generate_sources_markdown(manifest, results, args.output / "SOURCES.generated.md")
    print(f"Prepared data: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
