from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from compare_dictionaries import (
    collect_annotated_corpora,
    describe_dictionary,
    describe_file,
    evaluate_annotated_corpora,
    evaluate_benchmark,
    generate_report,
    summarize_benchmark_delta,
    summarize_vocabulary_delta,
    write_benchmark_csv,
)
from corpus_common import write_json


class CompareDictionariesAnnotatedTests(unittest.TestCase):


    def test_dictionary_and_probe_identities_include_sha256(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            base = root / "ru_RU-1.0.8"
            aff = base.with_name(base.name + ".aff")
            dic = base.with_name(base.name + ".dic")
            probe = root / "hunspell-probe.exe"
            aff.write_bytes(b"AFF-DATA\n")
            dic.write_bytes(b"DIC-DATA\n")
            probe.write_bytes(b"PROBE-DATA\n")

            dictionary = describe_dictionary(base)
            executable = describe_file(probe)

            self.assertEqual(dictionary["base"], str(base.resolve()))
            self.assertEqual(
                dictionary["aff"]["sha256"],
                hashlib.sha256(b"AFF-DATA\n").hexdigest().upper(),
            )
            self.assertEqual(
                dictionary["dic"]["sha256"],
                hashlib.sha256(b"DIC-DATA\n").hexdigest().upper(),
            )
            self.assertEqual(dictionary["aff"]["size"], len(b"AFF-DATA\n"))
            self.assertEqual(
                executable["sha256"],
                hashlib.sha256(b"PROBE-DATA\n").hexdigest().upper(),
            )

    def test_collect_annotated_keeps_one_character_russian_words(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            annotated = Path(tmp)
            path = annotated / "rnc_morphological_standard.jsonl"
            rows = [
                {
                    "corpus_id": "rnc_morphological_standard",
                    "word": "я",
                    "frequency": 7,
                    "class_counts": {"strict_clean": 7},
                    "pos_counts": {"S-PRO": 7},
                },
                {
                    "corpus_id": "rnc_morphological_standard",
                    "word": "и",
                    "frequency": 11,
                    "class_counts": {"strict_clean": 11},
                    "pos_counts": {"CONJ": 11},
                },
                {
                    "corpus_id": "rnc_morphological_standard",
                    "word": "A",
                    "frequency": 3,
                    "class_counts": {"strict_clean": 3},
                    "pos_counts": {"UNKNOWN": 3},
                },
            ]
            with path.open("w", encoding="utf-8", newline="\n") as handle:
                for row in rows:
                    handle.write(json.dumps(row, ensure_ascii=False) + "\n")

            corpora = collect_annotated_corpora(annotated)
            words = corpora["rnc_morphological_standard"]
            self.assertIn("я", words)
            self.assertIn("и", words)
            self.assertNotIn("A", words)

    def test_annotated_summary_uses_occurrence_and_unique_counts(self) -> None:
        corpora = {
            "rnc_morphological_standard": {
                "слово": {
                    "frequency": 3,
                    "class_counts": {"all_lexical": 3, "strict_clean": 2, "proper_name": 1},
                    "pos_counts": {"S": 3},
                    "source_groups": {"fiction": 2, "public": 1},
                },
                "текст": {
                    "frequency": 2,
                    "class_counts": {"all_lexical": 2, "strict_clean": 2},
                    "pos_counts": {"S": 2},
                    "source_groups": {"fiction": 2},
                },
            }
        }
        misspelled = {
            "current": {"слово"},
            "candidate": set(),
        }

        summary = evaluate_annotated_corpora(corpora, misspelled, ["current", "candidate"])
        corpus = summary["rnc_morphological_standard"]

        strict = corpus["classes"]["strict_clean"]
        self.assertEqual(strict["tokens"], 4)
        self.assertEqual(strict["unique_words"], 2)
        self.assertEqual(strict["dictionaries"]["current"]["rejected_tokens"], 2)
        self.assertEqual(strict["dictionaries"]["current"]["rejected_unique"], 1)
        self.assertEqual(strict["dictionaries"]["current"]["unique_rejection_rate"], 0.5)
        self.assertEqual(strict["dictionaries"]["candidate"]["rejected_tokens"], 0)

        noun = corpus["pos"]["S"]
        self.assertEqual(noun["tokens"], 5)
        self.assertEqual(noun["dictionaries"]["current"]["rejected_tokens"], 3)

        fiction = corpus["source_groups"]["fiction"]
        self.assertEqual(fiction["tokens"], 4)
        self.assertEqual(fiction["unique_words"], 2)
        self.assertEqual(fiction["dictionaries"]["current"]["rejected_tokens"], 2)
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / "report.md"
            generate_report(report, [(name, Path(name)) for name in misspelled], {}, {},
                            misspelled, {}, annotated=summary)
            content = report.read_text(encoding="utf-8")
        for group, tokens, rejected, rate in (
            ("strict_clean", 4, 2, "50.00%"),
            ("S", 5, 3, "60.00%"),
            ("fiction", 4, 2, "50.00%"),
        ):
            self.assertIn(
                f"| rnc_morphological_standard / {group} | {tokens} | 2 | {rejected} / 1 | {rate} / 50.00% |",
                content,
            )
        self.assertIn("(tokens / unique)", content)


    def test_report_includes_all_suggestion_metrics_and_delta(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / "report.md"
            generate_report(
                report,
                [("old", Path("old")), ("new", Path("new"))],
                {"слово": 4},
                {"fixture": {"слово": 4}},
                {"old": {"слово"}, "new": set()},
                {
                    "cases": 2,
                    "dictionaries": {
                        "old": {"wrong_rejected": 1, "wrong_accepted": 1, "correct_accepted": 2, "correct_rejected": 0, "suggest_top1": 1, "suggest_top3": 1, "suggest_top5": 1, "suggest_top8": 1, "suggest_any": 1, "suggest_mrr": 0.5},
                        "new": {"wrong_rejected": 2, "wrong_accepted": 0, "correct_accepted": 2, "correct_rejected": 0, "suggest_top1": 2, "suggest_top3": 2, "suggest_top5": 2, "suggest_top8": 2, "suggest_any": 2, "suggest_mrr": 1.0},
                    },
                },
                {"newly_accepted": {"unique": 1, "tokens": 4}, "newly_rejected": {"unique": 0, "tokens": 0}},
                {"fixed_false_accepts": 1, "suggestion_rank_improved": 1},
            )
            content = report.read_text(encoding="utf-8")
            self.assertIn("Top-3", content)
            self.assertIn("Top-8", content)
            self.assertIn("Any", content)
            self.assertIn("MRR", content)
            self.assertIn("Исправленные ложные принятия", content)
    def test_vocabulary_delta_counts_unique_and_occurrences(self) -> None:
        delta = summarize_vocabulary_delta(
            {"старое", "общее"},
            {"новое", "общее"},
            {"старое": 7, "новое": 3, "общее": 11},
        )
        self.assertEqual(delta["newly_accepted"], {"unique": 1, "tokens": 7})
        self.assertEqual(delta["newly_rejected"], {"unique": 1, "tokens": 3})

    def test_benchmark_delta_keeps_absent_suggestion_distinct_from_rank(self) -> None:
        rows = [
            {"dictionary": "old", "wrong": "ошбка", "correct": "ошибка", "source": "x", "correction": "", "domain": "", "wrong_rejected": False, "correct_accepted": False, "suggestion_rank": None},
            {"dictionary": "new", "wrong": "ошбка", "correct": "ошибка", "source": "x", "correction": "", "domain": "", "wrong_rejected": True, "correct_accepted": True, "suggestion_rank": 3},
            {"dictionary": "old", "wrong": "корета", "correct": "карета", "source": "x", "correction": "", "domain": "", "wrong_rejected": True, "correct_accepted": True, "suggestion_rank": 2},
            {"dictionary": "new", "wrong": "корета", "correct": "карета", "source": "x", "correction": "", "domain": "", "wrong_rejected": False, "correct_accepted": False, "suggestion_rank": None},
        ]
        for row in rows:
            row["suggestion_evaluated"] = True
        delta = summarize_benchmark_delta(rows, "old", "new")
        self.assertEqual(delta["fixed_false_accepts"], 1)
        self.assertEqual(delta["new_false_accepts"], 1)
        self.assertEqual(delta["fixed_false_rejects"], 1)
        self.assertEqual(delta["new_false_rejects"], 1)
        self.assertEqual(delta["correct_suggestion_appeared"], 1)
        self.assertEqual(delta["correct_suggestion_disappeared"], 1)

    def test_benchmark_delta_excludes_unqueried_suggestions_only(self) -> None:
        for before, after in ((False, True), (True, False), (False, False)):
            with self.subTest(before=before, after=after):
                rows = [
                    {"dictionary": "old", "wrong": "ошбка", "correct": "ошибка",
                     "wrong_rejected": False, "correct_accepted": False,
                     "suggestion_evaluated": before, "suggestion_rank": None},
                    {"dictionary": "new", "wrong": "ошбка", "correct": "ошибка",
                     "wrong_rejected": True, "correct_accepted": True,
                     "suggestion_evaluated": after, "suggestion_rank": 1},
                ]
                delta = summarize_benchmark_delta(rows, "old", "new")
                self.assertEqual(delta["fixed_false_accepts"], 1)
                self.assertEqual(delta["fixed_false_rejects"], 1)
                self.assertEqual(delta.get("correct_suggestion_appeared", 0), 0)
                self.assertEqual(delta.get("suggestion_cases_compared", 0), 0)

    def check_suggestion_limit(self, limit, unique_count, case_count, mrr, top1, top3):
        cases = [
            {"wrong": "ашибка", "correct": "ошибка"},
            {"wrong": "малако", "correct": "молоко"},
            {"wrong": "ашибка", "correct": "ошибки"},
            {"wrong": "сабака", "correct": "собака"},
        ]
        proposed = {"ашибка": ["ошибка", "ошибки"], "малако": [], "сабака": ["собака"]}
        dictionaries = [("old", Path("old")), ("new", Path("new"))]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "cases.jsonl").write_text(json.dumps({"typo_cases": cases}), encoding="utf-8")
            with patch("compare_dictionaries.list_misspelled", return_value={"ашибка", "малако", "сабака"}), patch(
                "compare_dictionaries.suggestions",
                side_effect=lambda exe, dic, words: {word: proposed[word] for word in words},
            ) as query:
                summary, rows = evaluate_benchmark(root, Path("probe"), dictionaries, limit)
            self.assertEqual(query.call_args.args[2], list(proposed)[:unique_count])
            self.assertEqual(summary["cases"], 4)
            self.assertEqual(summary["suggestion_limit"], limit)
            self.assertEqual(summary["suggestion_unique_words_evaluated"], unique_count)
            self.assertEqual(summary["suggestion_cases_evaluated"], case_count)
            write_json(root / "results.json", {"benchmark": summary})
            def reject_nonfinite(value):
                self.fail(f"Non-standard JSON number: {value}")
            serialized = json.loads((root / "results.json").read_text(encoding="utf-8"),
                                    parse_constant=reject_nonfinite)["benchmark"]
            if not case_count:
                for stats in serialized["dictionaries"].values():
                    for key in ("suggest_top1", "suggest_top3", "suggest_top5", "suggest_top8",
                                "suggest_any", "suggest_mrr"):
                        self.assertIsNone(stats[key])
                    self.assertEqual(stats["wrong_rejected"] + stats["wrong_accepted"], len(cases))
                    self.assertEqual(stats["correct_accepted"] + stats["correct_rejected"], len(cases))
            for stats in summary["dictionaries"].values():
                self.assertEqual([stats[key] for key in (
                    "wrong_rejected", "wrong_accepted", "correct_accepted", "correct_rejected"
                )], [4, 0, 4, 0])
                self.assertEqual(stats["suggestion_cases_evaluated"], case_count)
                self.assertEqual(stats["suggest_mrr"], mrr)
                self.assertEqual(stats["suggest_top1"], top1)
                for key in ("suggest_top3", "suggest_top5", "suggest_top8", "suggest_any"):
                    self.assertEqual(stats[key], top3)
            self.assertEqual(sum(row["suggestion_evaluated"] for row in rows), case_count * 2)
            write_benchmark_csv(root / "detail.csv", rows)
            self.assertIn("suggestion_evaluated", (root / "detail.csv").read_text(encoding="utf-8-sig"))
            generate_report(root / "report.md", dictionaries, {}, {}, {"old": set(), "new": set()}, summary)
            report = (root / "report.md").read_text(encoding="utf-8")
            self.assertIn(f"случаев с оценкой подсказок: **{case_count}**", report)
            self.assertIn("suggestion_cases_evaluated", report)
            if not case_count:
                self.assertIn("| N/A | N/A | N/A | N/A | N/A | N/A |", report)

    def test_suggestion_limit_counts_repeated_wrong_cases_and_mrr(self):
        self.check_suggestion_limit(1, 1, 2, 0.75, 1, 2)

    def test_suggestion_limit_all_includes_queried_misses(self):
        self.check_suggestion_limit(-1, 3, 4, 0.625, 2, 3)

    def test_suggestion_limit_zero_is_not_evaluated(self):
        self.check_suggestion_limit(0, 0, 0, None, None, None)

    def test_suggestion_limit_above_vocabulary_checks_all(self):
        self.check_suggestion_limit(100, 3, 4, 0.625, 2, 3)


if __name__ == "__main__":
    unittest.main()
