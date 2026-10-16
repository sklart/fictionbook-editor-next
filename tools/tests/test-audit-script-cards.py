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
    regexp_mask = audit["mask_non_code"]('var re=/["\'/]/g; var option=true; if(option) use();')
    assert 'option=true' in regexp_mask and 'if(option)' in regexp_mask
    assert not audit["linked_resource_candidates"](audit["Source"](
        ['// window.open("HTML/no.html")'], '// window.open("HTML/no.html")',
        audit["mask_non_code"]('// window.open("HTML/no.html")'), 0))
    def resources(code):
        return audit["linked_resource_candidates"](audit["Source"](
            code.splitlines(), code, audit["mask_non_code"](code), 0))
    dead = resources('function Run(){function dead(){window.open("HTML/dead.html");} /* dead(); */}')
    assert dead[0]["reachability"] == "not_referenced" and dead[0]["required"] is None
    live = resources('function Run(){load();} function load(){window.open("HTML/live.html");}')
    assert live[0]["reachability"] == "reachable" and live[0]["required"] is None
    dynamic = resources('function Run(){eval(input);} function load(){window.open("HTML/maybe.html");}')
    assert dynamic[0]["reachability"] == "unknown"
    escaped = resources('function Run(){var callback=load; callback();} function load(){window.open("HTML/maybe.html");}')
    assert escaped[0]["reachability"] == "unknown"
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
        assert result["structurallyValid"] and result["sourceHashCurrent"]
        assert result["contentReview"] == "requires_review"
        card["settings"] = [{"name": "ProcessingEnded", "value": False,
                             "description": "Внутреннее состояние"}]
        card["authors"] = [{"name": "Same", "role": "author"},
                           {"name": "same", "role": "modifier"}]
        card["undo"]["description"] = "Полный откат не проверен."
        code = b'function Run(){var ProcessingEnded=false; window.open("HTML/input.html");}\n'
        card["sourceRevision"]["sha256"] = hashlib.sha256(code).hexdigest()
        synthetic.write_bytes(card_tool["insert_card"](code, card))
        result = inspect(synthetic)
        categories = {item["category"] for item in result["findings"]}
        assert {"setting_title", "unused_setting", "internal_setting", "duplicate_author",
                "incorrect_undo", "unlisted_dependency"} <= categories, categories
        # The confirmed status is exempt from the technical API-name warning.
        card["settings"] = []
        card["undo"] = {"status": "verified_single_action",
                        "description": "Изменения можно отменить одним действием (Ctrl+Z)"}
        synthetic.write_bytes(card_tool["insert_card"](code, card))
        assert not any(item["category"] == "incorrect_undo" for item in inspect(synthetic)["findings"])
        code = b'function Run(){window.external.inflateBlock(element)=true;}\n'
        card["warnings"] = ["Ошибочное присваивание результату вызова прерывает выполнение."]
        card["sourceRevision"]["sha256"] = hashlib.sha256(code).hexdigest()
        synthetic.write_bytes(card_tool["insert_card"](code, card))
        assert any(item["category"] == "incorrect_jscript_claim" for item in inspect(synthetic)["findings"])

    scripts_root = ROOT / "runtime" / "Scripts"
    proposals = json.loads((ROOT / "docs" / "script-card-reviewed-proposals.json").read_text(encoding="utf-8"))
    files = {path.relative_to(scripts_root).as_posix() for path in scripts_root.rglob("*.js")}
    assert len(proposals) == len(files) == 579
    assert {proposal["path"] for proposal in proposals} == files
    previous_notes = json.loads((ROOT / "docs/note-creation-undo-stage1-results.json").read_text(encoding="utf-8"))["sources"]
    current_notes = json.loads((ROOT / "docs/note-creation-lint-metadata-results.json").read_text(encoding="utf-8"))["sources"]
    for proposal in proposals:
        assert isinstance(proposal["codeIssues"], list), proposal["path"]
        document = card_tool["read_document"]((scripts_root / proposal["path"]).read_bytes())
        if proposal["path"] in previous_notes:
            # These proposals retain their reviewed historical source revision.
            # The newer COM fix has only two rerun scenarios, not the prior 94.
            assert proposal["codeSha256"] == previous_notes[proposal["path"]]["codeSha256"], proposal["path"]
            assert document.sha256 == current_notes[proposal["path"]]["codeSha256"], proposal["path"]
        else:
            assert proposal["codeSha256"] == document.sha256, proposal["path"]
        for setting in document.card.get("settings", []):
            assert setting.get("title", "").strip(), (proposal["path"], setting)
            assert setting["name"] not in audit["INTERNAL_SETTINGS"], (proposal["path"], setting)
        assert not any(any(item.startswith(prefix) for prefix in audit["GENERIC_USAGE"])
                       for item in document.card["usage"]), proposal["path"]
        assert not audit["GENERIC_UNDO"].search(document.card.get("undo", {}).get("description", "")), proposal["path"]
        # Older records can use physical file lines, while newer records use
        # executable-code lines after the embedded card.
        line_count = len(document.data.decode(document.encoding).splitlines())
        for issue in proposal["codeIssues"]:
            assert isinstance(issue["line"], int) and 1 <= issue["line"] <= line_count
            assert isinstance(issue["issue"], str) and issue["issue"].strip()
    print("Script card audit tests passed.")

    classification = json.loads((ROOT / "docs" / "script-card-undo-classification.json").read_text(encoding="utf-8"))
    undo_source = (ROOT / "docs" / "script-card-undo-defects.md").read_bytes()
    assert classification["sourceSha256"] == hashlib.sha256(undo_source).hexdigest()
    import re
    source_paths = set(re.findall(r"^- `([^`]+)` —", undo_source.decode("utf-8"), re.M))
    classified_paths = [item["path"] for item in classification["scripts"]]
    assert len(classified_paths) == len(set(classified_paths)) == classification["scriptCount"] == 91
    assert set(classified_paths) == source_paths
    assert set(classification["categories"]) == set("ABCDEFG")
    for item in classification["scripts"]:
        assert item["categories"] and set(item["categories"]) <= set("ABCDEFG")
        assert item["priority"] in ("P1", "P2", "P3") and item["recommendedFix"]
    print("Undo classification: all 91 source records covered.")
    runtime = json.loads((ROOT / "docs" / "script64-runtime-results.json").read_text(encoding="utf-8"))
    script64 = next(scripts_root.rglob("64_Замена символов по списку из буфера обмена.js"))
    assert runtime["sourceCodeSha256"] == card_tool["read_document"](script64.read_bytes()).sha256
    results = runtime["runtimeResults"]
    assert results["harness-ran"] == results["undo"] == results["clipboard-restored"] == "1"
    checks = ("ordinary", "special", "backslash", "quote", "replacement-special",
              "empty-selection", "empty-list", "invalid-list", "mixed-special", "newlines",
              "cyrillic", "no-text", "invalid-late", "nodes", "textarea", "numeric-order",
              "duplicates", "cascade", "utf16-reject", "parser-negative", "undo", "redo",
              "undo-single", "refusal-no-undo", "exception-cleanup")
    checks += ("id-collision-0", "id-collision-1", "id-collision-2", "id-collision-3",
               "id-collision-4", "cleanup-mode-0", "cleanup-mode-1", "cleanup-mode-2",
               "textarea-history-observed", "textarea-undo", "textarea-redo")
    expected_status = "pass" if all(results[key] == "1" for key in checks) else "fail"
    assert runtime["result"] == results["result"] == expected_status
    assert expected_status == "pass"
    assert runtime["probeSha256"] == hashlib.sha256((ROOT / "tools/tests/script64-runtime-probe.js").read_bytes()).hexdigest()
    for path, digest in runtime["harnessSourceSha256"].items():
        if path == "tools/tests/test-audit-script-cards.py":
            # The script64 result records the historical version of this test.
            assert len(digest) == 64
            continue
        source = (ROOT / path).read_bytes()
        # Git can materialize the same tested source with LF or CRLF. Keep the
        # original runtime hash, accepting only that checkout transformation.
        lf = source.replace(b"\r\n", b"\n")
        checkout_hashes = {hashlib.sha256(data).hexdigest()
                           for data in (source, lf, lf.replace(b"\n", b"\r\n"))}
        assert digest in checkout_hashes, path
    from urllib.parse import unquote
    assert unquote(results["replacement-special-actual"]) == '$\\"'
    assert results["duplicates-actual"] == "CB" and results["cascade-actual"] == "cc"
    assert results["numeric-order-actual"] == "1"
    code64 = card_tool["read_document"](script64.read_bytes()).code_bytes.decode("utf-8")
    assert not re.search(r"\b(?:eval|Function)\s*\(", audit["mask_non_code"](code64))
    baseline = json.loads((ROOT / "docs/script-card-code-baseline.json").read_text(encoding="utf-8"))
    script64_changes = [item for item in baseline["executableChanges"]
                        if item["path"] == script64.relative_to(scripts_root).as_posix()]
    update = script64_changes[-1]
    assert update["path"] == script64.relative_to(scripts_root).as_posix()
    assert update["previousCodeSha256"] == script64_changes[-2]["codeSha256"]
    assert update["previousCodeSha256"] == "3d38de988b6294c81d63edc9af50b9f341474289417b11e2c4aee28f7a0cab96"
    assert update["codeSha256"] == runtime["sourceCodeSha256"]
    for path in files:
        card = card_tool["read_document"]((scripts_root / path).read_bytes()).card
        assert not re.search(r"ссылк[аи] на (?:примечаний|комментариев)", str(card["usage"]) + str(card["warnings"])), path
    print("Script 64 runtime evidence hash and reported outcome are consistent.")


if __name__ == "__main__":
    main()
