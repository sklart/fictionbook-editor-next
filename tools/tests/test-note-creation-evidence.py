"""Check the focused stage-1 evidence against current sources and fixture outcomes."""
from pathlib import Path
import hashlib
import json
import re
import runpy

ROOT = Path(__file__).resolve().parents[2]
READ = runpy.run_path(str(ROOT / "tools/script-card-tool.py"))["read_document"]


def main():
    report = json.loads((ROOT / "docs/note-creation-undo-stage1-results.json").read_text(encoding="utf-8"))
    results = report["runtimeResults"]
    assert report["result"] == results["result"] == "pass"
    expected = []
    for kind in ("notes", "comments"):
        for variant in range(1, 7):
            modes = ["ordinary", "existing", "missing-body", "bad-link", "nested",
                     "middle" if variant <= 3 else "last-refusal"]
            if variant in (2, 5):
                modes += ["cancel", "retry-cancel"]
            if variant == 2:
                modes += ["fault", "fault-before-body", "empty-body", "id-collision", "fault-middle", "selected", "selected-fault"]
            expected += [f"{kind}-{variant:02}-{mode}" for mode in modes]
    assert results["fixtures"] == str(len(expected)) == "94"
    for name in expected:
        for suffix in ("", "-behavior", "-closed", "-undo", "-redo"):
            assert results[name + suffix] == "1", name + suffix
    paths = {p.relative_to(ROOT / "runtime/Scripts").as_posix()
             for folder in ("02_Примечания", "03_Комментарии")
             for p in (ROOT / "runtime/Scripts" / folder).glob("0[1-6]_*.js")}
    assert len(paths) == len(report["sources"]) == 12 and set(report["sources"]) == paths
    current = json.loads((ROOT / "docs/note-creation-lint-metadata-results.json").read_text(encoding="utf-8"))
    assert current["previousEvidence"] == "note-creation-undo-stage1-results.json"
    assert current["previousScenarioCount"] == 94 and current["currentScenarioCount"] == 2
    assert set(current["sources"]) == paths
    for path, source in report["sources"].items():
        document = READ((ROOT / "runtime/Scripts" / path).read_bytes())
        assert current["sources"][path]["previousCodeSha256"] == source["codeSha256"], path
        assert document.sha256 == current["sources"][path]["codeSha256"] == document.card["sourceRevision"]["sha256"], path
        assert source["codeSha256"] != document.sha256, path
        prefix = document.code_bytes.replace(b"\r\n", b"\n").split(" // Подготовка не изменяет книгу".encode("utf-8"))[0]
        assert hashlib.sha256(prefix).hexdigest() == current["sources"][path]["configurationPrefixSha256"], path
        assert document.encoding == source["encoding"] and document.bom_size == source["bomBytes"], path
    followup = json.loads((ROOT / "docs/note-creation-runtime-followup-2026-10-31.json").read_text(encoding="utf-8"))
    assert followup["schema"] == 1 and followup["previousEvidence"] == "note-creation-lint-metadata-results.json"
    assert re.fullmatch(r"[0-9a-f]{40}", followup["sourceCommit"])
    assert re.fullmatch(r"[0-9a-f]{64}", followup["fbeExeSha256"])
    assert followup["dispatcherPath"] == "src/fbe/testing/RuntimeTestPortableState.inl"
    assert followup["dispatcherPath"] in current["harnessSourceSha256"]
    for path, digest in current["harnessSourceSha256"].items():
        if path == followup["dispatcherPath"]:
            assert digest == followup["previousDispatcherSha256"], path
            digest = followup["dispatcherSha256"]
        raw = (ROOT / path).read_bytes()
        lf = raw.replace(b"\r\n", b"\n")
        assert digest in {hashlib.sha256(data).hexdigest() for data in (raw, lf, lf.replace(b"\n", b"\r\n"))}, path
    latest = current["runtimeResults"]
    assert latest["fixtures"] == "2" and latest["result"] == "pass"
    for name in ("notes-02-ordinary", "comments-02-ordinary"):
        for suffix in ("", "-behavior", "-closed", "-undo", "-redo"):
            assert latest[name + suffix] == "1", name + suffix
    repeated = followup["runtimeResults"]
    assert repeated["fixtures"] == "2" and repeated["result"] == "pass"
    for name in ("notes-02-ordinary", "comments-02-ordinary"):
        for suffix in ("", "-behavior", "-closed", "-undo", "-redo"):
            assert repeated[name + suffix] == "1", name + suffix
    classification = json.loads((ROOT / "docs/script-card-undo-classification.json").read_text(encoding="utf-8"))
    fixed = {item["path"] for item in classification["scripts"] if item.get("resolution", {}).get("status") == "fixed"}
    assert fixed == paths and classification["resolvedCount"] == 12 and classification["remainingCount"] == 79
    assert classification["scriptCount"] == 91
    for item in classification["scripts"]:
        if item["path"] in fixed:
            assert item["resolution"]["codeSha256"] == report["sources"][item["path"]]["codeSha256"]
    print("Note creation evidence: PASS (historical 94 scenarios; current 12 SHA and two repeated runtime scenarios).")


if __name__ == "__main__":
    main()
