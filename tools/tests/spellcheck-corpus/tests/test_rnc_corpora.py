from __future__ import annotations

import gzip
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from corpus_common import iter_jsonl
from rnc_corpora import (
    process_rnc_morphological_standard,
    process_rnc_syntagrus,
    stream_rnc_diachronic_documents,
    stream_rnc_multilingual_documents,
)


class RncCorporaTests(unittest.TestCase):
    def test_morphological_standard_preserves_annotations_and_clean_class(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            rnc_root = Path(temp_dir)
            source = rnc_root / "morphological-standard" / "extracted" / "sample_ar" / "TEXTS" / "fiction"
            source.mkdir(parents=True)
            xhtml = '''<?xml version="1.0" encoding="windows-1251"?>
<html><body><se>
<w><ana lex="вопрос" gr="S,inan,loc,m,sg"></ana>вопросе</w>
<w><ana lex="Пушкин" gr="S,anim,famn,m,nom,sg"></ana>Пушкин</w>
<w><ana lex="тыща" gr="S,abbr"></ana>тыща</w>
<w><ana lex="сказать" gr="V,distort"></ana>скозать</w>
<w><ana lex="замок" gr="S,inan,m,nom,sg"></ana><ana lex="замок" gr="S,inan,m,acc,sg"></ana>замок</w>
<w><ana lex="метка" gr="norm"></ana>метка</w>
<w><ana lex="камень" gr="S,inan,m,nom,sg"></ana>К`амень</w>
<w><ana lex="есть" gr="V,act,indic,pl,praes"></ana>`ели</w>
<w><ana lex="и" gr="CONJ"></ana>и</w>
</se></body></html>'''
            (source / "sample.xhtml").write_bytes(xhtml.encode("cp1251"))
            entry = {
                "id": "rnc_morphological_standard",
                "loader": "rnc_morphological_standard",
                "relative_path": "morphological-standard/extracted",
                "quality_tier": "annotated_clean",
                "license": "test",
                "profiles": {"smoke": {"max_source_files": None}},
            }
            result = process_rnc_morphological_standard(entry, "smoke", rnc_root / "out", rnc_root, 51)
            rows = {row["word"]: row for row in iter_jsonl(Path(result["output"]))}

        self.assertEqual(result["source_tokens"], 9)
        self.assertEqual(result["tokens"], 9)
        self.assertEqual(result["tokens_filtered"], 0)
        self.assertEqual(result["stress_marked_tokens"], 2)
        self.assertEqual(rows["вопросе"]["class_counts"]["strict_clean"], 1)
        self.assertEqual(rows["Пушкин"]["class_counts"]["proper_name"], 1)
        self.assertEqual(rows["тыща"]["class_counts"]["abbreviation"], 1)
        self.assertEqual(rows["скозать"]["class_counts"]["nonstandard"], 1)
        self.assertEqual(rows["замок"]["class_counts"]["ambiguous"], 1)
        self.assertEqual(rows["метка"]["class_counts"]["other_annotation"], 1)
        self.assertEqual(rows["Камень"]["class_counts"]["strict_clean"], 1)
        self.assertEqual(rows["ели"]["class_counts"]["strict_clean"], 1)
        self.assertEqual(rows["и"]["class_counts"]["strict_clean"], 1)
        self.assertNotIn("К`амень", rows)
        self.assertNotIn("`ели", rows)
        self.assertEqual(rows["вопросе"]["lemmas"], ["вопрос"])
        self.assertEqual(rows["вопросе"]["source_groups"], {"fiction": 1})

    def test_syntagrus_reads_all_lemma_and_feat_tokens(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            rnc_root = Path(temp_dir)
            source = rnc_root / "syntagrus" / "extracted" / "syntagrus" / "SynTagRus2022" / "2003"
            source.mkdir(parents=True)
            tgt = '''<?xml version="1.0" encoding="utf-8"?>
<text><body><S ID="1">
<W FEAT="S ЕД ЖЕН РОД НЕОД" LEMMA="БУРЯ">бури</W>
<W FEAT="V НЕСОВ ИНФ" LEMMA="ДЕЙСТВОВАТЬ">действовать</W>
<W FEAT="CONJ" LEMMA="И">и</W>
</S></body></text>'''
            (source / "sample.tgt").write_text(tgt, encoding="utf-8")
            entry = {
                "id": "rnc_syntagrus",
                "loader": "rnc_syntagrus",
                "relative_path": "syntagrus/extracted/syntagrus/SynTagRus2022",
                "quality_tier": "annotated_clean",
                "license": "test",
                "profiles": {"smoke": {"max_source_files": None}},
            }
            result = process_rnc_syntagrus(entry, "smoke", rnc_root / "out", rnc_root, 51)
            rows = {row["word"]: row for row in iter_jsonl(Path(result["output"]))}

        self.assertEqual(result["source_tokens"], 3)
        self.assertEqual(result["tokens"], 3)
        self.assertEqual(result["tokens_filtered"], 0)
        self.assertEqual(rows["бури"]["pos_counts"], {"S": 1})
        self.assertEqual(rows["действовать"]["lemmas"], ["ДЕЙСТВОВАТЬ"])
        self.assertEqual(rows["и"]["pos_counts"], {"CONJ": 1})
        self.assertEqual(rows["бури"]["source_groups"], {"2003": 1})

    def test_diachronic_gzip_is_streamed_without_unpacking(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            rnc_root = Path(temp_dir)
            source = rnc_root / "diachronic"
            source.mkdir(parents=True)
            path = source / "rnc_soviet.txt.gz"
            with gzip.open(path, "wt", encoding="utf-8") as handle:
                handle.write("Это первое достаточно длинное русское предложение для теста.\n")
                handle.write("..\n")
                handle.write("Это второе русское предложение для проверки потокового чтения.\n")
            entry = {
                "id": "rnc_diachronic_soviet",
                "relative_path": "diachronic/rnc_soviet.txt.gz",
                "aggregate_target_words": 5,
                "encoding": "utf-8",
                "profiles": {"smoke": {}},
                "name": "test",
                "source_url": "https://example.test",
                "period": "soviet",
            }
            rows = list(stream_rnc_diachronic_documents(entry, "smoke", rnc_root))

        self.assertGreaterEqual(len(rows), 1)
        self.assertTrue(all(".." != row["text"].strip() for row in rows))
        self.assertEqual(rows[0]["source_metadata"]["period"], "soviet")

    def test_multilingual_loader_extracts_only_russian_side(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            rnc_root = Path(temp_dir)
            source = rnc_root / "multilingual" / "extracted"
            source.mkdir(parents=True)
            path = source / "многоязычный_датасет_полный.json"
            path.write_text(
                json.dumps(
                    [
                        {"en": ["Hello"], "ru": ["Доброе утро и хороший день"]},
                        {"en": ["World"], "ru": ["Русский текст для проверки корпуса"]},
                    ],
                    ensure_ascii=False,
                ),
                encoding="utf-8",
            )
            entry = {
                "id": "rnc_multilingual_ru",
                "relative_path": "multilingual/extracted/многоязычный_датасет_полный.json",
                "aggregate_target_words": 3,
                "encoding": "utf-8-sig",
                "profiles": {"smoke": {}},
                "name": "test",
                "source_url": "https://example.test",
            }
            rows = list(stream_rnc_multilingual_documents(entry, "smoke", rnc_root))

        combined = "\n".join(row["text"] for row in rows)
        self.assertIn("Доброе утро", combined)
        self.assertIn("Русский текст", combined)
        self.assertNotIn("Hello", combined)
        self.assertNotIn("World", combined)


if __name__ == "__main__":
    unittest.main()
