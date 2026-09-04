from __future__ import annotations

import difflib
from dataclasses import dataclass

from corpus_common import iter_tokens, normalized_token


@dataclass(frozen=True)
class TypoCase:
    wrong: str
    correct: str
    source: str
    correction: str


def extract_typo_cases(source: str, correction: str) -> list[TypoCase]:
    source_tokens = [normalized_token(token) for token in iter_tokens(source)]
    correction_tokens = [normalized_token(token) for token in iter_tokens(correction)]
    matcher = difflib.SequenceMatcher(a=source_tokens, b=correction_tokens, autojunk=False)
    cases: list[TypoCase] = []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag != "replace":
            continue
        wrong = source_tokens[i1:i2]
        correct = correction_tokens[j1:j2]
        if len(wrong) == len(correct) and wrong:
            for wrong_token, correct_token in zip(wrong, correct, strict=True):
                if wrong_token.casefold() != correct_token.casefold():
                    cases.append(TypoCase(wrong_token, correct_token, source, correction))
    return cases
