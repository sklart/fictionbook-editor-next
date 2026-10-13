"""Fast byte-preservation and status tests for embedded script cards."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / "script-card-tool.py"
TEST_OUTPUT = Path(__file__).resolve().parents[2] / "out" / "tests"
spec = importlib.util.spec_from_file_location("script_card_tool", TOOL)
cards = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = cards
spec.loader.exec_module(cards)


def sample_card():
    return {
        "schema": 1, "title": "Проверка", "version": "1.0",
        "authors": [{"name": None, "role": "unknown"}],
        "summary": "Непроверенное описание.", "description": "Тестовая карточка.",
        "scope": "document", "modifiesDocument": None, "requiresSelection": None,
        "usage": ["Откройте книгу."], "warnings": [], "dependencies": [],
        "sourceRevision": {"algorithm": cards.ALGORITHM, "sha256": None, "reviewStatus": "generated"},
    }


class ScriptCardToolTests(unittest.TestCase):
    def test_insertion_round_trip_for_original_byte_encodings(self):
        for encoding, bom in (("utf-8", b"\xef\xbb\xbf"), ("cp1251", b""),
                              ("utf-16-le", b"\xff\xfe"), ("utf-16-be", b"\xfe\xff")):
            with self.subTest(encoding=encoding):
                source = bom + '// Автор\r\nfunction Run(){return "Привет";}\r\n'.encode(encoding)
                card = sample_card()
                card["sourceRevision"]["sha256"] = cards.read_document(source, encoding).sha256
                updated = cards.insert_card(source, card, encoding)
                parsed = cards.read_document(updated, encoding)
                self.assertEqual(parsed.code_bytes, source)
                self.assertEqual(parsed.sha256, card["sourceRevision"]["sha256"])
                self.assertEqual(parsed.card["authors"][0]["role"], "unknown")
                changed_description = dict(card, description="Новое описание")
                self.assertEqual(cards.read_document(
                    bom + cards.serialize(changed_description, encoding, "\r\n") + source[len(bom):], encoding
                ).sha256, parsed.sha256)
                self.assertNotEqual(cards.read_document(updated + b" ", encoding).sha256, parsed.sha256)

    def test_rejects_unsafe_or_invalid_card(self):
        card = sample_card()
        card["description"] = "bad */ comment"
        with self.assertRaises(cards.CardError):
            cards.insert_card(b"function Run() {}\n", card)
        card["description"] = "safe"
        card["schema"] = 2
        with self.assertRaises(cards.CardError):
            cards.insert_card(b"function Run() {}\n", card)
        card["schema"] = 1
        card["description"] = "a" * 65536
        with self.assertRaises(cards.CardError):
            cards.insert_card(b"function Run() {}\n", card)
        invalid = (cards.OPEN + '\n{"schema":1,"schema":1}\n' + cards.CLOSE + '\n').encode()
        with self.assertRaises(cards.CardError):
            cards.read_document(invalid)
        with self.assertRaises(cards.CardError):
            cards.read_document(b"// header\n" + cards.serialize(sample_card(), "utf-8", "\n"))
        with self.assertRaises(cards.CardError):
            cards.read_document(cards.serialize(sample_card(), "utf-8", "\n") * 2)

    def test_stamp_is_explicit_and_preserves_source_bytes(self):
        with tempfile.TemporaryDirectory(dir=TEST_OUTPUT) as directory:
            path = Path(directory) / "example.js"
            original = b"function Run() { return 1; }\n"
            path.write_bytes(cards.insert_card(original, sample_card()))
            rejected = subprocess.run([sys.executable, str(TOOL), "stamp", str(path)], capture_output=True)
            self.assertEqual(rejected.returncode, 2)
            self.assertIsNone(cards.read_document(path.read_bytes()).card["sourceRevision"]["sha256"])
            accepted = subprocess.run([sys.executable, str(TOOL), "stamp", str(path), "--accept-revision"], capture_output=True)
            self.assertEqual(accepted.returncode, 0, accepted.stderr)
            parsed = cards.read_document(path.read_bytes())
            self.assertEqual(parsed.code_bytes, original)
            self.assertEqual(parsed.card["sourceRevision"]["sha256"], parsed.sha256)
            self.assertEqual(parsed.card["sourceRevision"]["reviewStatus"], "generated")
            self.assertEqual(subprocess.run([sys.executable, str(TOOL), "validate", str(path)], capture_output=True).returncode, 0)
            path.write_bytes(path.read_bytes() + b" ")
            self.assertEqual(subprocess.run([sys.executable, str(TOOL), "validate", str(path)], capture_output=True).returncode, 1)

    def test_dependency_status_is_separate_and_confined(self):
        with tempfile.TemporaryDirectory(dir=TEST_OUTPUT) as directory:
            root = Path(directory)
            asset = root / "HTML" / "helper.html"
            asset.parent.mkdir()
            asset.write_bytes(b"old")
            card = sample_card()
            card["dependenciesComplete"] = True
            card["dependencies"] = [{"path": "HTML/helper.html", "sha256": cards.hashlib.sha256(b"old").hexdigest(), "optional": False}]
            self.assertEqual(cards.dependency_status(card, root)[0], "current")
            asset.write_bytes(b"new")
            self.assertEqual(cards.dependency_status(card, root)[0], "changed")
            script = root / "script.js"
            source = b"function Run() {}\n"
            card["sourceRevision"]["sha256"] = cards.hashlib.sha256(source).hexdigest()
            script.write_bytes(cards.insert_card(source, card))
            self.assertEqual(subprocess.run([sys.executable, str(TOOL), "validate", str(script),
                                             "--resource-root", str(root)], capture_output=True).returncode, 1)
            card["dependencies"][0]["path"] = "../outside.html"
            self.assertEqual(cards.dependency_status(card, root)[0], "invalid")
            self.assertIn("dependency path leaves the resource root", cards.validate_schema(card))
            card["dependencies"][0] = {"path": "HTML/helper.html", "sha256": "broken", "optional": "no"}
            self.assertIn("dependency.optional must be boolean", cards.validate_schema(card))
            self.assertIn("dependency.sha256 must be lowercase SHA-256 or null", cards.validate_schema(card))
            card["dependencies"] = []
            card.pop("dependenciesComplete")
            self.assertEqual(cards.dependency_status(card, root)[0], "unverified")


if __name__ == "__main__":
    unittest.main()
