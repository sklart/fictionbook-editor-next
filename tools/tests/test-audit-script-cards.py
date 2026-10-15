"""Focused checks for audit evidence and byte-preserving reviewed apply."""

from __future__ import annotations

import copy
import hashlib
import json
import runpy
from pathlib import Path
from tempfile import TemporaryDirectory


ROOT = Path(__file__).resolve().parents[2]
audit = runpy.run_path(str(ROOT / "tools" / "audit-script-cards.py"))
card_tool = runpy.run_path(str(ROOT / "tools" / "script-card-tool.py"))


def main() -> None:
    masked = audit["mask_non_code"]("// ShellExecute(\nvar namespace='http://www.w3.org/1999/xlink';\n"
                                    "/* XMLHTTP( */\nwindow.external.BeginUndoUnit(document);\n")
    assert "ShellExecute" not in masked and "http://" not in masked and "XMLHTTP" not in masked
    assert "BeginUndoUnit" in masked
    script = ROOT / "runtime" / "Scripts" / "03_Комментарии" / "01_Добавить комментарий, остаться на его ссылке.js"
    original = script.read_bytes()
    original_document = card_tool["read_document"](original)
    record = audit["inspect"](script)
    assert record["codeSha256"] == original_document.sha256
    assert len(record["claims"]["settings"]) == 11
    assert not any("сетевого адреса" in str(item["previous"]) for item in record["findings"])
    with TemporaryDirectory() as temporary:
        root = Path(temporary)
        copied_script = root / "example.js"
        copied_script.write_bytes(original)
        apply = audit["apply_proposals"]
        apply.__globals__["SCRIPTS"] = root
        edited_summary = original_document.card["summary"] + " "
        proposal = {"path": "example.js", "codeSha256": original_document.sha256,
                    "evidence": "Tested exact source and metadata-only edit",
                    "changes": [{"field": "summary", "before": original_document.card["summary"],
                                 "after": edited_summary}]}
        assert apply([proposal], [{"path": "example.js", "result": "requires_review"}]) == 1
        updated = card_tool["read_document"](copied_script.read_bytes())
        assert updated.card["summary"] == edited_summary
        assert updated.code_bytes == original_document.code_bytes
        assert updated.sha256 == original_document.sha256
        assert updated.encoding == original_document.encoding and updated.bom_size == original_document.bom_size
        assert apply([proposal], [{"path": "example.js", "result": "requires_review"}]) == 0
        reduced_card = copy.deepcopy(updated.card)
        reduced_card.pop("settings")
        start, end = updated.span
        copied_script.write_bytes(updated.data[:start] +
                                  card_tool["serialize"](reduced_card, updated.encoding, "\r\n") +
                                  updated.data[end:])
        added_field = {"path": "example.js", "codeSha256": original_document.sha256,
                       "evidence": "Verified optional field addition",
                       "changes": [{"field": "settings", "before": None, "after": []}]}
        assert apply([added_field], [{"path": "example.js", "result": "requires_review"}]) == 1
        assert card_tool["read_document"](copied_script.read_bytes()).card["settings"] == []
        assert apply([added_field], [{"path": "example.js", "result": "requires_review"}]) == 0
        proposal["changes"][0]["after"] = "Different new value"
        try:
            apply([proposal], [{"path": "example.js", "result": "requires_review"}])
        except ValueError as error:
            assert "stale original" in str(error)
        else:
            raise AssertionError("stale proposal was accepted")
        synthetic = root / "synthetic.js"
        code = ("function Run() { var ns='http://www.w3.org/1999/xlink'; "
                "window.external.BeginUndoUnit(document); if (!document.body) return; "
                "window.external.EndUndoUnit(document); }\n").encode("utf-8")
        card = copy.deepcopy(original_document.card)
        card["settings"] = []
        card["usage"] = ["Запустите команду и ответьте на её диалоги."]
        card["warnings"] = ["Есть запуск внешнего приложения или открытие сетевого адреса; проверьте сформированную команду и передаваемые данные."]
        card["sourceRevision"]["sha256"] = hashlib.sha256(code).hexdigest()
        synthetic.write_bytes(card_tool["insert_card"](code, card))
        inspect = audit["inspect"]
        inspect.__globals__["SCRIPTS"] = root
        result = inspect(synthetic)
        assert not result["evidence"]["network"] and not result["evidence"]["process"]
        assert result["namespaceUrls"]
        assert any(item["field"] == "usage" and "диалога" in item["reason"]
                   for item in result["findings"])
        assert any(item["field"] == "undo" and item["evidence"]
                   for item in result["findings"])

    scripts_root = ROOT / "runtime" / "Scripts"
    proposals = json.loads((ROOT / "docs" / "script-card-reviewed-proposals.json").read_text(encoding="utf-8"))
    files = {path.relative_to(scripts_root).as_posix() for path in scripts_root.rglob("*.js")}
    assert len(proposals) == len(files) == 579
    assert {proposal["path"] for proposal in proposals} == files
    for proposal in proposals:
        assert isinstance(proposal["codeIssues"], list), proposal["path"]
        document = card_tool["read_document"]((scripts_root / proposal["path"]).read_bytes())
        assert proposal["codeSha256"] == document.sha256, proposal["path"]
        # Older records can use physical file lines, while newer records use
        # executable-code lines after the embedded card.
        line_count = len(document.data.decode(document.encoding).splitlines())
        for issue in proposal["codeIssues"]:
            assert isinstance(issue["line"], int) and 1 <= issue["line"] <= line_count
            assert isinstance(issue["issue"], str) and issue["issue"].strip()
    print("Script card audit tests passed.")


if __name__ == "__main__":
    main()
