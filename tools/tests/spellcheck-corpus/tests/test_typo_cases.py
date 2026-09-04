from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from typo_cases import extract_typo_cases


class TypoCaseTests(unittest.TestCase):
    def test_single_word_replacement(self) -> None:
        cases = extract_typo_cases("Это сабака.", "Это собака.")
        self.assertEqual([(case.wrong, case.correct) for case in cases], [("сабака", "собака")])

    def test_space_change_is_not_single_word_typo(self) -> None:
        cases = extract_typo_cases("ктобы пришёл", "кто бы пришёл")
        self.assertEqual(cases, [])

    def test_multiple_independent_typos(self) -> None:
        cases = extract_typo_cases("превет, сабака", "привет, собака")
        self.assertEqual(len(cases), 2)


if __name__ == "__main__":
    unittest.main()
