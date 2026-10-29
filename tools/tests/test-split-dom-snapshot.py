"""Independent regression tests for the read-only Split DOM snapshot oracle."""
import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ORACLE = HERE / "inspect-split-dom-snapshot.py"
FIXTURES = HERE / "fixtures" / "split-dom-oracle"
spec = importlib.util.spec_from_file_location("split_dom_snapshot", ORACLE)
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)

class SnapshotTests(unittest.TestCase):
    def test_valid_structure_text_and_local_reference(self):
        result = oracle.analyze(FIXTURES / "valid.html")
        self.assertEqual(result["full_text"].strip(), "First Backlink")
        self.assertEqual(result["id_counts"], {"source": 1})
        self.assertEqual(result["duplicate_ids"], {})
        self.assertEqual(result["structure_violations"], [])
        self.assertEqual(result["internal_links"], [{"href": "#source", "target": "source", "matching_ids": 1}])
        self.assertEqual(result["tag_counts"]["p"], 1)

    def test_duplicate_ids_and_paragraph_outside_section(self):
        duplicate = oracle.analyze(FIXTURES / "duplicate-id.html")
        orphan = oracle.analyze(FIXTURES / "orphan-paragraph.html")
        self.assertEqual(duplicate["duplicate_ids"], {"source": 2})
        self.assertEqual(duplicate["structure_violations"], [])
        self.assertEqual(orphan["structure_violations"], [{"kind": "paragraph_directly_in_body", "parent": "div", "index": 1}])

    def test_attribute_order_and_case_have_same_canonical_hash(self):
        with tempfile.TemporaryDirectory() as temp:
            a, b = Path(temp) / "a.html", Path(temp) / "b.html"
            a.write_text('<DIV CLASS="section" ID="x"><P>Text</P></DIV>', encoding="utf-8")
            b.write_text('<div id="x" class="section"><p>Text</p></div>', encoding="utf-8")
            self.assertEqual(oracle.analyze(a)["canonical_sha256"], oracle.analyze(b)["canonical_sha256"])

    def test_malformed_and_incomplete_are_rejected(self):
        with self.assertRaises(ValueError):
            oracle.analyze(FIXTURES / "malformed.html")
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "truncated.html"
            path.write_text("<DIV", encoding="utf-8")
            with self.assertRaises(ValueError):
                oracle.analyze(path)

    def test_duplicate_attribute_with_missing_value_does_not_crash(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "attributes.html"
            path.write_text('<DIV checked checked="yes"></DIV>', encoding="utf-8")
            self.assertEqual(oracle.analyze(path)["tag_counts"]["div"], 1)

    def test_external_fragments_are_not_internal_links(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "links.html"
            path.write_text('<DIV><A href="https://example.org/#x">a</A><A href="#missing">b</A></DIV>', encoding="utf-8")
            result = oracle.analyze(path)
            self.assertEqual(len(result["fragment_links"]), 2)
            self.assertEqual(result["internal_links"], [{"href": "#missing", "target": "missing", "matching_ids": 0}])

    def test_cli_preserves_input_and_rejects_same_output(self):
        with tempfile.TemporaryDirectory() as temp:
            source, output = Path(temp) / "input.html", Path(temp) / "result.json"
            source.write_bytes((FIXTURES / "valid.html").read_bytes())
            before = hashlib.sha256(source.read_bytes()).hexdigest()
            subprocess.run([sys.executable, str(ORACLE), str(source), "--output", str(output)], check=True, capture_output=True)
            self.assertTrue(json.loads(output.read_text(encoding="utf-8"))["canonical_sha256"])
            fail = subprocess.run([sys.executable, str(ORACLE), str(source), "--output", str(source)], capture_output=True)
            self.assertNotEqual(fail.returncode, 0)
            self.assertEqual(before, hashlib.sha256(source.read_bytes()).hexdigest())

if __name__ == "__main__":
    unittest.main()