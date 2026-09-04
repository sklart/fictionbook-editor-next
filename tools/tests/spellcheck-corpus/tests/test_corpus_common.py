from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from corpus_common import (
    chunk_text,
    clean_text,
    cyrillic_ratio,
    iter_json_file_records,
    iter_json_records,
    iter_tokens,
    normalized_token,
)


class CorpusCommonTests(unittest.TestCase):
    def test_clean_text_preserves_yo_and_hyphen(self) -> None:
        value = "<p>Ёлка — по-прежнему зелёная.</p> https://example.org"
        result = clean_text(value)
        self.assertIn("Ёлка", result)
        self.assertIn("по-прежнему", result)
        self.assertIn("зелёная", result)
        self.assertNotIn("example.org", result)

    def test_tokenizer_keeps_compounds(self) -> None:
        tokens = list(iter_tokens("По-прежнему всё-таки идёт."))
        self.assertEqual(tokens, ["По-прежнему", "всё-таки", "идёт"])

    def test_normalized_token_unifies_dash_and_apostrophe(self) -> None:
        self.assertEqual(normalized_token("из‑за"), "из-за")
        self.assertEqual(normalized_token("д’Артаньян"), "д'Артаньян")

    def test_cyrillic_ratio(self) -> None:
        self.assertGreater(cyrillic_ratio("Русский текст"), 0.99)
        self.assertLess(cyrillic_ratio("English text"), 0.01)

    def test_chunk_text(self) -> None:
        paragraphs = [" ".join([f"слово{i}" for i in range(60)]) for _ in range(4)]
        chunks = list(chunk_text("\n\n".join(paragraphs), max_words=130, min_words=50))
        self.assertEqual(len(chunks), 2)
        self.assertTrue(all(chunk.word_count >= 100 for chunk in chunks))

    def test_iter_json_records_column_layout(self) -> None:
        rows = list(iter_json_records({"source": ["а", "б"], "correction": ["в", "г"]}))
        self.assertEqual(rows[1]["correction"], "г")

    def test_iter_json_file_records_regular_json(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "records.json"
            path.write_text(
                json.dumps([{"source": "сабака", "correction": "собака"}], ensure_ascii=False),
                encoding="utf-8",
            )
            rows = list(iter_json_file_records(path))
        self.assertEqual(rows[0]["correction"], "собака")

    def test_iter_json_file_records_jsonl_with_json_extension(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "records.json"
            path.write_text(
                '{"source":"сабака","correction":"собака"}\n'
                '{"source":"превет","correction":"привет"}\n',
                encoding="utf-8",
            )
            rows = list(iter_json_file_records(path))
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[1]["source"], "превет")


if __name__ == "__main__":
    unittest.main()
