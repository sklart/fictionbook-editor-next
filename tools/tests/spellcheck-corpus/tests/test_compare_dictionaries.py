from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from compare_dictionaries import (
    collect_annotated_corpora,
    describe_dictionary,
    describe_file,
    evaluate_annotated_corpora,
)


class CompareDictionariesAnnotatedTests(unittest.TestCase):


    def test_dictionary_and_probe_identities_include_sha256(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            base = root / "ru_RU"
            aff = base.with_suffix(".aff")
            dic = base.with_suffix(".dic")
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


if __name__ == "__main__":
    unittest.main()
