from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from prepare_corpora import (
    _hf_dataset_file_url,
    _request_json,
    main,
    process_benchmark,
    stream_hf_text_documents,
    stream_wikisource_documents,
)


class FakeResponse:
    def __init__(self, lines: list[bytes]) -> None:
        self._lines = lines
        self.closed = False

    def raise_for_status(self) -> None:
        return None

    def iter_lines(self, **_: Any):
        yield from self._lines

    def close(self) -> None:
        self.closed = True


class FakeSession:
    def __init__(self, response: FakeResponse) -> None:
        self.response = response
        self.headers: dict[str, str] = {}
        self.requested_url: str | None = None

    def get(self, url: str, **_: Any) -> FakeResponse:
        self.requested_url = url
        return self.response

    def close(self) -> None:
        return None


class FakeJsonResponse:
    def __init__(
        self,
        payload: dict[str, Any],
        *,
        status_code: int = 200,
        headers: dict[str, str] | None = None,
    ) -> None:
        self._payload = payload
        self.status_code = status_code
        self.headers = headers or {}
        self.closed = False

    def raise_for_status(self) -> None:
        if self.status_code >= 400:
            raise RuntimeError(f"HTTP {self.status_code}")

    def json(self) -> dict[str, Any]:
        return self._payload

    def close(self) -> None:
        self.closed = True


class FakeJsonSession:
    def __init__(self, responses: list[FakeJsonResponse]) -> None:
        self.responses = responses
        self.headers: dict[str, str] = {}
        self.calls: list[dict[str, Any]] = []
        self.closed = False

    def get(self, url: str, **kwargs: Any) -> FakeJsonResponse:
        self.calls.append({"url": url, **kwargs})
        if not self.responses:
            raise AssertionError("No fake response left")
        return self.responses.pop(0)

    def close(self) -> None:
        self.closed = True


class PrepareCorporaTests(unittest.TestCase):

    def test_main_reads_manifest_and_writes_lock(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            manifest = root / "corpora.json"
            output = root / "prepared"
            manifest.write_text(
                json.dumps({"corpora": []}, ensure_ascii=False),
                encoding="utf-8",
            )
            argv = [
                "prepare_corpora.py",
                "--manifest",
                str(manifest),
                "--output",
                str(output),
                "--profile",
                "smoke",
            ]
            with patch.object(sys, "argv", argv):
                self.assertEqual(main(), 0)
            self.assertTrue((output / "SOURCES.lock.json").is_file())
            self.assertTrue((output / "SOURCES.generated.md").is_file())

    def test_hf_file_url_pins_revision_and_file(self) -> None:
        url = _hf_dataset_file_url(
            "Imperius/ru-classic", "corpus_clean.txt", "abc123"
        )
        self.assertIn("/datasets/Imperius/ru-classic/resolve/abc123/corpus_clean.txt", url)

    def test_utf8_text_stream_does_not_use_windows_ansi_codepage(self) -> None:
        # UTF-8 for «И» contains byte 0x98, which is undefined in cp1251 and
        # reproduces the failure seen on Windows when the locale encoding leaks in.
        response = FakeResponse([
            "Иван увидел зелёную ёлку.".encode("utf-8"),
            "Ирина подошла к нему.".encode("utf-8"),
        ])
        session = FakeSession(response)
        entry = {
            "repo_id": "Imperius/ru-classic",
            "filename": "corpus_clean.txt",
            "encoding": "utf-8",
            "encoding_errors": "strict",
            "source_url": "https://huggingface.co/datasets/Imperius/ru-classic",
            "aggregate_small_records": True,
            "aggregate_target_words": 3,
            "profiles": {"smoke": {"max_source_records": 10}},
            "_revision": "abc123",
        }

        rows = list(stream_hf_text_documents(entry, "smoke", session=session))

        self.assertEqual(len(rows), 2)
        self.assertIn("Иван", rows[0]["text"])
        self.assertIn("ёлку", rows[0]["text"])
        self.assertEqual(rows[0]["source_metadata"]["encoding"], "utf-8")
        self.assertIn("corpus_clean.txt", session.requested_url or "")
        self.assertTrue(response.closed)

    def test_mediawiki_429_honors_retry_after(self) -> None:
        session = FakeJsonSession(
            [
                FakeJsonResponse({}, status_code=429, headers={"Retry-After": "7"}),
                FakeJsonResponse({"query": {"pages": []}}),
            ]
        )
        sleeps: list[float] = []

        payload = _request_json(
            session,
            "https://example.test/w/api.php",
            {"action": "query"},
            sleep_fn=sleeps.append,
        )

        self.assertEqual(payload, {"query": {"pages": []}})
        self.assertEqual(sleeps, [7.0])
        self.assertEqual(len(session.calls), 2)
        self.assertEqual(session.calls[0]["params"]["maxlag"], 5)

    def test_process_benchmark_accepts_json_and_jsonl_files(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp = Path(temp_dir)
            regular_json = temp / "regular.json"
            jsonl_with_json_suffix = temp / "lines.json"
            regular_json.write_text(
                json.dumps(
                    [{"source": "сабака", "correction": "собака", "domain": "test"}],
                    ensure_ascii=False,
                ),
                encoding="utf-8",
            )
            jsonl_with_json_suffix.write_text(
                '{"source":"превет","correction":"привет","domain":"test"}\n'
                '{"source":"малако","correction":"молоко","domain":"test"}\n',
                encoding="utf-8",
            )
            entry = {"id": "benchmark", "repo_id": "example/benchmark", "files": []}
            with patch(
                "prepare_corpora.download_benchmark_files",
                return_value=([regular_json, jsonl_with_json_suffix], "abc123"),
            ):
                result = process_benchmark(entry, temp / "output")

            output_rows = [
                json.loads(line)
                for line in Path(result["output"]).read_text(encoding="utf-8").splitlines()
                if line.strip()
            ]

        self.assertEqual(result["pairs"], 3)
        self.assertEqual(len(output_rows), 3)
        self.assertEqual(output_rows[1]["source"], "превет")

    def test_mediawiki_non_retryable_api_error_is_not_silenced(self) -> None:
        session = FakeJsonSession([
            FakeJsonResponse({
                "error": {
                    "code": "invalidparammix",
                    "info": "Multiple full extracts are not supported",
                }
            })
        ])

        with self.assertRaisesRegex(RuntimeError, "invalidparammix"):
            _request_json(
                session,
                "https://example.test/w/api.php",
                {"action": "query"},
                sleep_fn=lambda _: None,
            )

    def test_wikisource_uses_action_parse_for_full_page_text(self) -> None:
        category_payload = {
            "query": {
                "categorymembers": [
                    {"pageid": 1, "title": "Перевод один"},
                    {"pageid": 2, "title": "Перевод два"},
                    {"pageid": 3, "title": "Перевод три"},
                ]
            }
        }
        parsed_payloads = [
            {
                "parse": {
                    "title": f"Перевод {index}",
                    "pageid": index,
                    "revid": 100 + index,
                    "displaytitle": f"Перевод {index}",
                    "text": "<p>" + ("Это достаточно длинный русский литературный текст. " * 20) + "</p>",
                    "categories": [{"category": "Переводы"}],
                }
            }
            for index in range(1, 4)
        ]
        session = FakeJsonSession(
            [FakeJsonResponse(category_payload)]
            + [FakeJsonResponse(payload) for payload in parsed_payloads]
        )
        entry = {
            "api": "https://example.test/w/api.php",
            "categories": ["Переводы"],
            "candidate_factor": 1,
            "request_delay_seconds": 0,
            "parse_request_delay_seconds": 0,
            "min_document_words": 20,
            "min_cyrillic_ratio": 0.5,
            "profiles": {"smoke": {"pages_per_category": 3}},
        }

        with patch("prepare_corpora.random.Random.shuffle", lambda self, values: None):
            rows = list(
                stream_wikisource_documents(
                    entry,
                    "smoke",
                    seed=51,
                    session=session,
                    sleep_fn=lambda _: None,
                )
            )

        self.assertEqual(len(rows), 3)
        self.assertEqual(len(session.calls), 4)
        self.assertTrue(all(call["params"]["action"] == "parse" for call in session.calls[1:]))
        self.assertEqual(session.calls[1]["params"]["prop"], "text|revid|categories|displaytitle")
        self.assertIn("SpellcheckCorpusBot/1.8", session.headers["User-Agent"])

    def test_wikisource_parse_supports_legacy_text_wrapper(self) -> None:
        category_payload = {
            "query": {"categorymembers": [{"pageid": 1, "title": "Старый ответ API"}]}
        }
        parsed_payload = {
            "parse": {
                "title": "Старый ответ API",
                "pageid": 1,
                "revid": 202,
                "displaytitle": {"*": "Старый ответ API"},
                "text": {"*": "<div><p>" + ("Это полный русский литературный перевод. " * 30) + "</p></div>"},
                "categories": [{"category": "Переводы с английского языка"}],
            }
        }
        session = FakeJsonSession([
            FakeJsonResponse(category_payload),
            FakeJsonResponse(parsed_payload),
        ])
        entry = {
            "api": "https://example.test/w/api.php",
            "categories": ["Переводы"],
            "candidate_factor": 1,
            "request_delay_seconds": 0,
            "parse_request_delay_seconds": 0,
            "min_document_chars": 100,
            "min_document_words": 20,
            "min_cyrillic_ratio": 0.5,
            "profiles": {"smoke": {"pages_per_category": 1}},
        }

        rows = list(
            stream_wikisource_documents(
                entry,
                "smoke",
                seed=51,
                session=session,
                sleep_fn=lambda _: None,
            )
        )

        self.assertEqual(len(rows), 1)
        self.assertIn("полный русский литературный перевод", rows[0]["text"])
        self.assertEqual(rows[0]["source_metadata"]["revision_id"], 202)
        self.assertEqual(rows[0]["source_metadata"]["content_api"], "action=parse")
        self.assertEqual(len(session.calls), 2)

    def test_wikisource_keeps_scanning_after_unusable_candidate(self) -> None:
        category_payload = {
            "query": {
                "categorymembers": [
                    {"pageid": 1, "title": "Короткий перевод"},
                    {"pageid": 2, "title": "Полный перевод"},
                ]
            }
        }
        short_parse = {
            "parse": {
                "title": "Короткий перевод",
                "pageid": 1,
                "revid": 11,
                "text": "<p>Тоже коротко.</p>",
                "categories": [{"category": "Переводы"}],
            }
        }
        long_parse = {
            "parse": {
                "title": "Полный перевод",
                "pageid": 2,
                "revid": 22,
                "text": "<p>" + ("Это полный русский литературный перевод. " * 30) + "</p>",
                "categories": [{"category": "Переводы"}],
            }
        }
        session = FakeJsonSession([
            FakeJsonResponse(category_payload),
            FakeJsonResponse(short_parse),
            FakeJsonResponse(long_parse),
        ])
        entry = {
            "api": "https://example.test/w/api.php",
            "categories": ["Переводы"],
            "candidate_factor": 2,
            "request_delay_seconds": 0,
            "parse_request_delay_seconds": 0,
            "min_document_chars": 100,
            "min_document_words": 20,
            "min_cyrillic_ratio": 0.5,
            "profiles": {"smoke": {"pages_per_category": 1}},
        }

        with patch("prepare_corpora.random.Random.shuffle", lambda self, values: None):
            rows = list(
                stream_wikisource_documents(
                    entry,
                    "smoke",
                    seed=51,
                    session=session,
                    sleep_fn=lambda _: None,
                )
            )

        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["title"], "Полный перевод")
        self.assertEqual(len(session.calls), 3)

    def test_wikisource_excludes_translation_list_and_continues(self) -> None:
        category_payload = {
            "query": {
                "categorymembers": [
                    {"pageid": 1, "title": "Список переводов"},
                    {"pageid": 2, "title": "Текст перевода"},
                ]
            }
        }
        excluded_page = {
            "parse": {
                "title": "Список переводов",
                "pageid": 1,
                "revid": 1,
                "text": "<p>" + ("Описание переводов. " * 30) + "</p>",
                "categories": [{"category": "Списки переводов"}],
            }
        }
        valid_page = {
            "parse": {
                "title": "Текст перевода",
                "pageid": 2,
                "revid": 2,
                "text": "<p>" + ("Это художественный перевод произведения. " * 30) + "</p>",
                "categories": [{"category": "Переводы с английского языка"}],
            }
        }
        session = FakeJsonSession([
            FakeJsonResponse(category_payload),
            FakeJsonResponse(excluded_page),
            FakeJsonResponse(valid_page),
        ])
        entry = {
            "api": "https://example.test/w/api.php",
            "categories": ["Переводы"],
            "candidate_factor": 2,
            "request_delay_seconds": 0,
            "parse_request_delay_seconds": 0,
            "min_document_chars": 100,
            "min_document_words": 20,
            "min_cyrillic_ratio": 0.5,
            "excluded_page_categories": ["Списки переводов"],
            "profiles": {"smoke": {"pages_per_category": 1}},
        }

        with patch("prepare_corpora.random.Random.shuffle", lambda self, values: None):
            rows = list(
                stream_wikisource_documents(
                    entry,
                    "smoke",
                    seed=51,
                    session=session,
                    sleep_fn=lambda _: None,
                )
            )

        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["title"], "Текст перевода")
        self.assertEqual(len(session.calls), 3)


if __name__ == "__main__":
    unittest.main()
