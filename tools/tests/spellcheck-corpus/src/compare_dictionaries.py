from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))

from corpus_common import find_context, iter_jsonl, iter_tokens, normalized_token, write_json  # noqa: E402
from hunspell_cli import list_misspelled, suggestions  # noqa: E402


def parse_dictionary(value: str) -> tuple[str, Path]:
    if "=" not in value:
        raise argparse.ArgumentTypeError("Dictionary must be NAME=PATH_TO_ru_RU")
    name, path = value.split("=", 1)
    name = name.strip()
    if not name:
        raise argparse.ArgumentTypeError("Dictionary name is empty")
    return name, Path(path).expanduser().resolve()


def is_russian_token(token: str) -> bool:
    return any("а" <= char.casefold() <= "я" or char in "Ёё" for char in token)


def sha256_file(path: Path, chunk_size: int = 1024 * 1024) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(chunk_size), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def describe_file(path: Path) -> dict[str, Any]:
    resolved = path.expanduser().resolve()
    if not resolved.is_file():
        raise FileNotFoundError(f"File not found: {resolved}")
    return {
        "path": str(resolved),
        "size": resolved.stat().st_size,
        "sha256": sha256_file(resolved),
    }


def describe_dictionary(path: Path) -> dict[str, Any]:
    resolved = path.expanduser().resolve()
    base = resolved.with_suffix("") if resolved.suffix.lower() in {".aff", ".dic"} else resolved
    aff = base.with_suffix(".aff")
    dic = base.with_suffix(".dic")
    if not aff.is_file() or not dic.is_file():
        raise FileNotFoundError(f"Dictionary pair not found: {aff} / {dic}")
    return {
        "base": str(base),
        "aff": describe_file(aff),
        "dic": describe_file(dic),
    }


def collect_vocabulary(clean_dir: Path, max_contexts: int = 3) -> tuple[Counter[str], dict[str, Counter[str]], dict[str, list[str]]]:
    frequencies: Counter[str] = Counter()
    corpus_frequencies: dict[str, Counter[str]] = defaultdict(Counter)
    contexts: dict[str, list[str]] = defaultdict(list)
    for path in sorted(clean_dir.glob("*.jsonl")):
        for row in iter_jsonl(path):
            text = str(row.get("text", ""))
            corpus_id = str(row.get("corpus_id", path.stem))
            for raw_token in iter_tokens(text):
                token = normalized_token(raw_token)
                if len(token) < 2 or not is_russian_token(token):
                    continue
                frequencies[token] += 1
                corpus_frequencies[corpus_id][token] += 1
                if len(contexts[token]) < max_contexts:
                    snippet = find_context(text, raw_token)
                    if snippet and snippet not in contexts[token]:
                        contexts[token].append(snippet)
    return frequencies, dict(corpus_frequencies), dict(contexts)


def collect_annotated_corpora(annotated_dir: Path) -> dict[str, dict[str, dict[str, Any]]]:
    corpora: dict[str, dict[str, dict[str, Any]]] = defaultdict(dict)
    if not annotated_dir.is_dir():
        return {}
    for path in sorted(annotated_dir.glob("*.jsonl")):
        for row in iter_jsonl(path):
            word = normalized_token(str(row.get("word", "")))
            # Annotated RNC corpora intentionally retain valid one-character
            # Russian words (я, и, в, к, с, о, у, а).  The ordinary text
            # corpus vocabulary still keeps the historical len>=2 filter so
            # high-frequency function words do not dominate coverage metrics.
            if not word or not is_russian_token(word):
                continue
            corpus_id = str(row.get("corpus_id", path.stem))
            corpora[corpus_id][word] = row
    return {corpus_id: dict(rows) for corpus_id, rows in corpora.items()}


def _int_counts(value: Any) -> dict[str, int]:
    if not isinstance(value, dict):
        return {}
    result: dict[str, int] = {}
    for key, raw in value.items():
        try:
            count = int(raw)
        except (TypeError, ValueError):
            continue
        if count > 0:
            result[str(key)] = count
    return result


def evaluate_annotated_corpora(
    corpora: dict[str, dict[str, dict[str, Any]]],
    misspelled: dict[str, set[str]],
    dictionary_names: list[str],
) -> dict[str, Any]:
    summary: dict[str, Any] = {}
    for corpus_id, rows in sorted(corpora.items()):
        class_totals: dict[str, Counter[str]] = defaultdict(Counter)
        pos_totals: dict[str, Counter[str]] = defaultdict(Counter)
        source_group_totals: dict[str, Counter[str]] = defaultdict(Counter)
        total_tokens = 0
        for word, row in rows.items():
            frequency = int(row.get("frequency", 0) or 0)
            total_tokens += frequency
            for class_name, count in _int_counts(row.get("class_counts")).items():
                stats = class_totals[class_name]
                stats["tokens"] += count
                stats["unique"] += 1
                for name in dictionary_names:
                    if word in misspelled[name]:
                        stats[f"rejected_tokens:{name}"] += count
                        stats[f"rejected_unique:{name}"] += 1
            for pos, count in _int_counts(row.get("pos_counts")).items():
                stats = pos_totals[pos]
                stats["tokens"] += count
                stats["unique"] += 1
                for name in dictionary_names:
                    if word in misspelled[name]:
                        stats[f"rejected_tokens:{name}"] += count
                        stats[f"rejected_unique:{name}"] += 1
            for source_group, count in _int_counts(row.get("source_groups")).items():
                stats = source_group_totals[source_group]
                stats["tokens"] += count
                stats["unique"] += 1
                for name in dictionary_names:
                    if word in misspelled[name]:
                        stats[f"rejected_tokens:{name}"] += count
                        stats[f"rejected_unique:{name}"] += 1

        def serialize(groups: dict[str, Counter[str]]) -> dict[str, Any]:
            output: dict[str, Any] = {}
            for group_name, stats in sorted(groups.items()):
                tokens = int(stats["tokens"])
                unique = int(stats["unique"])
                dictionaries: dict[str, Any] = {}
                for name in dictionary_names:
                    rejected_tokens = int(stats[f"rejected_tokens:{name}"])
                    rejected_unique = int(stats[f"rejected_unique:{name}"])
                    dictionaries[name] = {
                        "rejected_tokens": rejected_tokens,
                        "rejected_unique": rejected_unique,
                        "rejection_rate": (rejected_tokens / tokens) if tokens else 0.0,
                        "unique_rejection_rate": (rejected_unique / unique) if unique else 0.0,
                    }
                output[group_name] = {
                    "tokens": tokens,
                    "unique_words": unique,
                    "dictionaries": dictionaries,
                }
            return output

        summary[corpus_id] = {
            "tokens": total_tokens,
            "unique_words": len(rows),
            "classes": serialize(class_totals),
            "pos": serialize(pos_totals),
            "source_groups": serialize(source_group_totals),
        }
    return summary


def write_annotated_disagreements_csv(
    path: Path,
    corpora: dict[str, dict[str, dict[str, Any]]],
    misspelled: dict[str, set[str]],
    dictionary_names: list[str],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "corpus",
        "word",
        "frequency",
        "class_counts",
        "pos_counts",
        "lemmas",
        "grammar_examples",
        "source_groups",
        *[f"accepted_{name}" for name in dictionary_names],
        "contexts",
    ]
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for corpus_id, rows in sorted(corpora.items()):
            ordered = sorted(
                rows.items(),
                key=lambda item: (-int(item[1].get("frequency", 0) or 0), item[0].casefold(), item[0]),
            )
            for word, row in ordered:
                statuses = [word not in misspelled[name] for name in dictionary_names]
                if len(set(statuses)) <= 1:
                    continue
                item: dict[str, Any] = {
                    "corpus": corpus_id,
                    "word": word,
                    "frequency": int(row.get("frequency", 0) or 0),
                    "class_counts": "; ".join(
                        f"{key}={value}" for key, value in sorted(_int_counts(row.get("class_counts")).items())
                    ),
                    "pos_counts": "; ".join(
                        f"{key}={value}" for key, value in sorted(_int_counts(row.get("pos_counts")).items())
                    ),
                    "lemmas": " | ".join(str(value) for value in row.get("lemmas", []) if value),
                    "grammar_examples": " | ".join(
                        str(value) for value in row.get("grammar_examples", []) if value
                    ),
                    "source_groups": "; ".join(
                        f"{key}={value}" for key, value in sorted(_int_counts(row.get("source_groups")).items())
                    ),
                    "contexts": " || ".join(str(value) for value in row.get("contexts", []) if value),
                }
                for name, status in zip(dictionary_names, statuses, strict=True):
                    item[f"accepted_{name}"] = int(status)
                writer.writerow(item)


def evaluate_benchmark(
    benchmark_dir: Path,
    hunspell_exe: Path,
    dictionaries: list[tuple[str, Path]],
    suggestion_limit: int,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    cases: list[dict[str, Any]] = []
    for path in sorted(benchmark_dir.glob("*.jsonl")):
        for row in iter_jsonl(path):
            for case in row.get("typo_cases", []):
                wrong = normalized_token(str(case.get("wrong", "")))
                correct = normalized_token(str(case.get("correct", "")))
                if not wrong or not correct or not is_russian_token(wrong):
                    continue
                cases.append(
                    {
                        "wrong": wrong,
                        "correct": correct,
                        "source": row.get("source", ""),
                        "correction": row.get("correction", ""),
                        "domain": row.get("domain"),
                    }
                )

    unique_wrong = list(dict.fromkeys(case["wrong"] for case in cases))
    unique_correct = list(dict.fromkeys(case["correct"] for case in cases))
    benchmark_words = list(dict.fromkeys([*unique_wrong, *unique_correct]))
    suggestion_words = unique_wrong[:suggestion_limit] if suggestion_limit >= 0 else unique_wrong
    all_suggestions: dict[str, dict[str, list[str]]] = {}
    benchmark_misspelled: dict[str, set[str]] = {}
    for name, path in dictionaries:
        benchmark_misspelled[name] = list_misspelled(hunspell_exe, path, benchmark_words)
        all_suggestions[name] = suggestions(hunspell_exe, path, suggestion_words)

    summary: dict[str, Any] = {"cases": len(cases), "dictionaries": {}}
    detailed: list[dict[str, Any]] = []
    for name, _ in dictionaries:
        misspelled = benchmark_misspelled[name]
        stats = Counter()
        for case in cases:
            wrong = case["wrong"]
            correct = case["correct"]
            wrong_rejected = wrong in misspelled
            correct_accepted = correct not in misspelled
            proposed = all_suggestions[name].get(wrong, [])
            correct_fold = correct.casefold()
            ranks = [index + 1 for index, item in enumerate(proposed) if item.casefold() == correct_fold]
            rank = ranks[0] if ranks else None
            stats["wrong_rejected"] += int(wrong_rejected)
            stats["wrong_accepted"] += int(not wrong_rejected)
            stats["correct_accepted"] += int(correct_accepted)
            stats["correct_rejected"] += int(not correct_accepted)
            stats["suggest_top1"] += int(rank == 1)
            stats["suggest_top5"] += int(rank is not None and rank <= 5)
            detailed.append(
                {
                    **case,
                    "dictionary": name,
                    "wrong_rejected": wrong_rejected,
                    "correct_accepted": correct_accepted,
                    "suggestion_rank": rank,
                    "suggestions": proposed[:10],
                }
            )
        summary["dictionaries"][name] = dict(stats)
    return summary, detailed


def write_word_csv(
    path: Path,
    words: list[str],
    frequencies: Counter[str],
    contexts: dict[str, list[str]],
    misspelled: dict[str, set[str]],
    dictionary_names: list[str],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = ["word", "frequency", *[f"accepted_{name}" for name in dictionary_names], "contexts"]
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for word in words:
            statuses = [word not in misspelled[name] for name in dictionary_names]
            if len(set(statuses)) <= 1:
                continue
            row: dict[str, Any] = {
                "word": word,
                "frequency": frequencies[word],
                "contexts": " || ".join(contexts.get(word, [])),
            }
            for name, status in zip(dictionary_names, statuses, strict=True):
                row[f"accepted_{name}"] = int(status)
            writer.writerow(row)


def write_benchmark_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "dictionary",
        "wrong",
        "correct",
        "wrong_rejected",
        "correct_accepted",
        "suggestion_rank",
        "domain",
        "suggestions",
        "source",
        "correction",
    ]
    with path.open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            item = dict(row)
            item["suggestions"] = " | ".join(item.get("suggestions", []))
            writer.writerow({field: item.get(field) for field in fields})


def generate_report(
    path: Path,
    dictionaries: list[tuple[str, Path]],
    frequencies: Counter[str],
    corpus_frequencies: dict[str, Counter[str]],
    misspelled: dict[str, set[str]],
    benchmark: dict[str, Any],
    annotated: dict[str, Any] | None = None,
    dictionary_files: dict[str, Any] | None = None,
    hunspell_probe: dict[str, Any] | None = None,
) -> None:
    names = [name for name, _ in dictionaries]
    lines = [
        "# Сравнение русских словарей Hunspell",
        "",
        f"Уникальных русских токенов: **{len(frequencies):,}**.",
        f"Всего словоупотреблений: **{sum(frequencies.values()):,}**.",
        "",
    ]

    dictionary_files = dictionary_files or {}
    if dictionary_files or hunspell_probe:
        lines.extend(["## Воспроизводимость", ""])
        if dictionary_files:
            lines.extend([
                "| Словарь | База | SHA-256 `.aff` | SHA-256 `.dic` |",
                "|---|---|---|---|",
            ])
            for name in names:
                info = dictionary_files.get(name, {})
                aff = info.get("aff", {})
                dic = info.get("dic", {})
                lines.append(
                    "| " + " | ".join([
                        name,
                        f"`{info.get('base', '')}`",
                        f"`{aff.get('sha256', '')}`",
                        f"`{dic.get('sha256', '')}`",
                    ]) + " |"
                )
        if hunspell_probe:
            lines.extend([
                "",
                f"Hunspell probe: `{hunspell_probe.get('path', '')}`  ",
                f"SHA-256: `{hunspell_probe.get('sha256', '')}`",
            ])
        lines.append("")

    lines.extend([
        "## Сводка по корпусам",
        "",
        "| Корпус | Словоупотреблений | " + " | ".join(f"Не принято `{name}`" for name in names) + " |",
        "|---|---:|" + "---:|" * len(names),
    ])
    for corpus_id, counts in sorted(corpus_frequencies.items()):
        cells = [corpus_id, f"{sum(counts.values()):,}"]
        for name in names:
            rejected = sum(count for word, count in counts.items() if word in misspelled[name])
            cells.append(f"{rejected:,}")
        lines.append("| " + " | ".join(cells) + " |")

    annotated = annotated or {}
    if annotated:
        lines.extend(
            [
                "",
                "## Размеченные корпуса НКРЯ",
                "",
                "Для Морфологического стандарта класс `strict_clean` используется как строгий контрольный набор: "
                "из него исключены неоднозначные, искажённые, аномальные, сокращённые, цифровые/инициальные, "
                "явно помеченные как имена собственные и редкие служебные типы разметки.",
                "",
                "| Корпус / класс | Словоупотреблений | Уникальных слов | "
                + " | ".join(
                    f"Не принято `{name}` | Доля `{name}`" for name in names
                )
                + " |",
                "|---|---:|---:|" + "---:|---:|" * len(names),
            ]
        )
        preferred_order = {
            "strict_clean": 0,
            "all_lexical": 1,
            "proper_name": 2,
            "abbreviation": 3,
            "nonstandard": 4,
            "ambiguous": 5,
            "other_annotation": 6,
        }
        for corpus_id, corpus_stats in sorted(annotated.items()):
            classes = corpus_stats.get("classes", {})
            ordered_classes = sorted(
                classes.items(),
                key=lambda item: (preferred_order.get(item[0], 100), item[0]),
            )
            for class_name, class_stats in ordered_classes:
                tokens = int(class_stats.get("tokens", 0))
                cells = [
                    f"{corpus_id} / {class_name}",
                    f"{tokens:,}",
                    f"{int(class_stats.get('unique_words', 0)):,}",
                ]
                for name in names:
                    d = class_stats.get("dictionaries", {}).get(name, {})
                    rejected = int(d.get("rejected_tokens", 0))
                    rate = float(d.get("rejection_rate", 0.0))
                    cells.extend([f"{rejected:,}", f"{rate:.2%}"])
                lines.append("| " + " | ".join(cells) + " |")

        lines.extend(
            [
                "",
                "### Разбивка размеченных корпусов по частям речи",
                "",
                "| Корпус / POS | Словоупотреблений | Уникальных слов | "
                + " | ".join(f"Не принято `{name}` | Доля `{name}`" for name in names)
                + " |",
                "|---|---:|---:|" + "---:|---:|" * len(names),
            ]
        )
        for corpus_id, corpus_stats in sorted(annotated.items()):
            pos_groups = corpus_stats.get("pos", {})
            ordered_pos = sorted(
                pos_groups.items(),
                key=lambda item: (-int(item[1].get("tokens", 0)), item[0]),
            )
            for pos, pos_stats in ordered_pos:
                tokens = int(pos_stats.get("tokens", 0))
                cells = [
                    f"{corpus_id} / {pos}",
                    f"{tokens:,}",
                    f"{int(pos_stats.get('unique_words', 0)):,}",
                ]
                for name in names:
                    d = pos_stats.get("dictionaries", {}).get(name, {})
                    cells.extend(
                        [
                            f"{int(d.get('rejected_tokens', 0)):,}",
                            f"{float(d.get('rejection_rate', 0.0)):.2%}",
                        ]
                    )
                lines.append("| " + " | ".join(cells) + " |")

        lines.extend(
            [
                "",
                "### Разбивка размеченных корпусов по группам источников",
                "",
                "Для Морфологического стандарта группы соответствуют каталогам жанров "
                "(`fiction`, `science`, `public`, `speech`, `blogs_2013` и т. п.); "
                "для СинТагРус — каталогам годов/коллекций.",
                "",
                "| Корпус / группа | Словоупотреблений | Уникальных слов | "
                + " | ".join(f"Не принято `{name}` | Доля `{name}`" for name in names)
                + " |",
                "|---|---:|---:|" + "---:|---:|" * len(names),
            ]
        )
        for corpus_id, corpus_stats in sorted(annotated.items()):
            groups = corpus_stats.get("source_groups", {})
            ordered_groups = sorted(
                groups.items(),
                key=lambda item: (-int(item[1].get("tokens", 0)), item[0]),
            )
            for group_name, group_stats in ordered_groups:
                tokens = int(group_stats.get("tokens", 0))
                cells = [
                    f"{corpus_id} / {group_name}",
                    f"{tokens:,}",
                    f"{int(group_stats.get('unique_words', 0)):,}",
                ]
                for name in names:
                    d = group_stats.get("dictionaries", {}).get(name, {})
                    cells.extend(
                        [
                            f"{int(d.get('rejected_tokens', 0)):,}",
                            f"{float(d.get('rejection_rate', 0.0)):.2%}",
                        ]
                    )
                lines.append("| " + " | ".join(cells) + " |")

    lines.extend(["", "## Размеченные опечатки", ""])
    total_cases = int(benchmark.get("cases", 0))
    lines.append(f"Однословных пар «ошибка → исправление»: **{total_cases:,}**.")
    lines.append("")
    lines.append("| Словарь | Ошибка обнаружена | Ошибка пропущена | Исправленное слово принято | Top-1 | Top-5 |")
    lines.append("|---|---:|---:|---:|---:|---:|")
    for name in names:
        stats = benchmark.get("dictionaries", {}).get(name, {})
        lines.append(
            "| "
            + " | ".join(
                [
                    name,
                    str(stats.get("wrong_rejected", 0)),
                    str(stats.get("wrong_accepted", 0)),
                    str(stats.get("correct_accepted", 0)),
                    str(stats.get("suggest_top1", 0)),
                    str(stats.get("suggest_top5", 0)),
                ]
            )
            + " |"
        )

    lines.extend(
        [
            "",
            "## Интерпретация",
            "",
            "- Для обычных текстовых, OCR и диахронических корпусов доля непринятых слов измеряет прежде всего покрытие; она сама по себе не доказывает нормативность принятых форм.",
            "- Для `rnc_morphological_standard / strict_clean` доля отклонений является наиболее надёжной метрикой ложных подчёркиваний среди подключённых корпусов.",
            "- Рост числа пропущенных размеченных опечаток является регрессией, даже если словарь подчёркивает меньше слов в книгах.",
            "- `annotated-disagreements.csv` и `spellcheck-review.fb2` могут содержать фрагменты локально лицензированных данных НКРЯ; их не следует публиковать без проверки условий соглашения.",
            "",
        ]
    )
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare Hunspell dictionaries on prepared corpora")
    parser.add_argument("--prepared", type=Path, required=True, help="Directory produced by prepare_corpora.py")
    parser.add_argument("--hunspell-exe", type=Path, required=True)
    parser.add_argument("--dictionary", action="append", type=parse_dictionary, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--suggestion-limit", type=int, default=5000, help="-1 means all typo words")
    args = parser.parse_args()

    if len(args.dictionary) < 2:
        parser.error("Specify at least two --dictionary NAME=PATH values")
    args.output.mkdir(parents=True, exist_ok=True)

    frequencies, corpus_frequencies, contexts = collect_vocabulary(args.prepared / "clean")
    words = sorted(frequencies, key=lambda word: (-frequencies[word], word.casefold(), word))
    print(f"Collected {len(words)} unique Russian tokens from text corpora")

    annotated_corpora = collect_annotated_corpora(args.prepared / "annotated")
    annotated_words = sorted(
        {word for rows in annotated_corpora.values() for word in rows},
        key=lambda word: (word.casefold(), word),
    )
    if annotated_words:
        print(f"Collected {len(annotated_words)} unique Russian tokens from annotated corpora")

    dictionary_files = {name: describe_dictionary(path) for name, path in args.dictionary}
    hunspell_probe = describe_file(args.hunspell_exe)

    misspelled: dict[str, set[str]] = {}
    annotated_misspelled: dict[str, set[str]] = {}
    for name, dictionary in args.dictionary:
        print(f"Checking dictionary {name}: {dictionary}")
        misspelled[name] = list_misspelled(args.hunspell_exe, dictionary, words)
        print(f"  text corpora rejected: {len(misspelled[name])}")
        annotated_misspelled[name] = (
            list_misspelled(args.hunspell_exe, dictionary, annotated_words)
            if annotated_words
            else set()
        )
        if annotated_words:
            print(f"  annotated corpora rejected: {len(annotated_misspelled[name])}")

    benchmark, benchmark_rows = evaluate_benchmark(
        args.prepared / "benchmarks",
        args.hunspell_exe,
        args.dictionary,
        args.suggestion_limit,
    )

    dictionary_names = [name for name, _ in args.dictionary]
    annotated_summary = evaluate_annotated_corpora(
        annotated_corpora, annotated_misspelled, dictionary_names
    )
    write_word_csv(args.output / "dictionary-disagreements.csv", words, frequencies, contexts, misspelled, dictionary_names)
    write_annotated_disagreements_csv(
        args.output / "annotated-disagreements.csv",
        annotated_corpora,
        annotated_misspelled,
        dictionary_names,
    )
    write_benchmark_csv(args.output / "typo-benchmark.csv", benchmark_rows)
    write_json(
        args.output / "results.json",
        {
            "dictionaries": {name: str(path) for name, path in args.dictionary},
            "dictionary_files": dictionary_files,
            "hunspell_probe": hunspell_probe,
            "unique_tokens": len(words),
            "token_occurrences": sum(frequencies.values()),
            "rejected_unique": {name: len(values) for name, values in misspelled.items()},
            "annotated": annotated_summary,
            "benchmark": benchmark,
        },
    )
    generate_report(
        args.output / "report.md",
        args.dictionary,
        frequencies,
        corpus_frequencies,
        misspelled,
        benchmark,
        annotated_summary,
        dictionary_files,
        hunspell_probe,
    )
    print(f"Results written to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
